#include "telegram_tdlib.hpp"

#include <utility>

#include <obs-module.h>

#include "nlohmann_json.hpp"
#include "td/telegram/td_json_client.h"

using nlohmann::json;

static std::string trim_copy(std::string s)
{
  auto is_ws = [](unsigned char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n'; };
  while (!s.empty() && is_ws((unsigned char)s.front())) s.erase(s.begin());
  while (!s.empty() && is_ws((unsigned char)s.back()))  s.pop_back();
  return s;
}

static std::string normalize_username(std::string u)
{
  // Accept "@EddieLives_bot" or "EddieLives_bot"
  u = trim_copy(std::move(u));
  if (!u.empty() && u[0] == '@') u.erase(u.begin());
  return u;
}

TelegramTdLibClient::TelegramTdLibClient() = default;

TelegramTdLibClient::~TelegramTdLibClient()
{
  stop();
}

void TelegramTdLibClient::set_on_auth_state(OnAuthState cb)
{
  std::lock_guard<std::mutex> lk(auth_cb_mutex_);
  auth_cb_ = std::move(cb);
}

void TelegramTdLibClient::start(const std::string& api_id,
                                const std::string& api_hash,
                                const std::string& session_dir,
                                OnTextMessage cb)
{
  if (running_) return;

  api_id_ = api_id;
  api_hash_ = api_hash;
  session_dir_ = session_dir;
  cb_ = std::move(cb);

  client_ = td_json_client_create();

  const char* ver = td_json_client_execute(nullptr, R"({"@type":"getOption","name":"version"})");
  blog(LOG_INFO, "[TWICH][TDLib] version execute: %s", ver ? ver : "(null)");

  running_ = true;
  thr_ = std::thread(&TelegramTdLibClient::run, this);
}

void TelegramTdLibClient::stop()
{
  if (!running_) return;

  running_ = false;

  if (thr_.joinable())
    thr_.join();

  if (client_) {
    td_json_client_destroy(client_);
    client_ = nullptr;
  }
}

// Optional stored-input helpers
void TelegramTdLibClient::set_phone(const std::string& phone)
{
  std::lock_guard<std::mutex> lk(auth_mutex_);
  phone_ = phone;
}

void TelegramTdLibClient::submit_code(const std::string& code)
{
  std::lock_guard<std::mutex> lk(auth_mutex_);
  code_ = code;
}

void TelegramTdLibClient::submit_password(const std::string& password)
{
  std::lock_guard<std::mutex> lk(auth_mutex_);
  password_ = password;
}

void TelegramTdLibClient::send_json(const std::string& s)
{
  if (!client_) {
    blog(LOG_ERROR, "[TWICH][TDLib] send_json called but client_ is null");
    return;
  }
  td_json_client_send(client_, s.c_str());
}

std::string TelegramTdLibClient::auth_state() const
{
  std::lock_guard<std::mutex> lk(state_mutex_);
  return auth_state_;
}

// Immediate-send methods (use these from OBS UI buttons)
void TelegramTdLibClient::send_phone_now(const std::string& phone)
{
  const std::string c = trim_copy(phone);
  json cmd = {
    {"@type", "setAuthenticationPhoneNumber"},
    {"phone_number", c}
  };

  const std::string payload = cmd.dump();
  blog(LOG_INFO, "[TWICH][TDLib] send_phone_now JSON: %s", payload.c_str());
  send_json(payload);
}

void TelegramTdLibClient::send_code_now(const std::string& code)
{
  const std::string c = trim_copy(code);
  json cmd = {
    {"@type", "checkAuthenticationCode"},
    {"code", c}
  };

  const std::string payload = cmd.dump();
  blog(LOG_INFO, "[TWICH][TDLib] send_code_now JSON: %s", payload.c_str());
  send_json(payload);
}

void TelegramTdLibClient::send_password_now(const std::string& password)
{
  const std::string c = trim_copy(password);

  json cmd = {
    {"@type", "checkAuthenticationPassword"},
    {"password", c}
  };

  const std::string payload = cmd.dump();
  blog(LOG_INFO, "[TWICH][TDLib] send_password_now JSON: %s", payload.c_str());
  send_json(payload);
}

static int to_int_api_id(const std::string& api_id_str)
{
  try {
    return std::stoi(api_id_str);
  } catch (...) {
    return 0;
  }
}

// TDLib v1.8.6+ expects INLINED parameters (no "parameters": {...})
static json build_tdlib_parameters(const std::string& session_dir,
                                  int api_id_int,
                                  const std::string& api_hash)
{
  json p;
  p["@type"] = "setTdlibParameters";

  p["use_test_dc"] = false;
  p["database_directory"] = session_dir;
  p["files_directory"] = session_dir + "/files";

  // empty string = unencrypted local database
  p["database_encryption_key"] = "";

  p["use_file_database"] = true;
  p["use_chat_info_database"] = true;
  p["use_message_database"] = true;
  p["use_secret_chats"] = false;

  p["api_id"] = api_id_int;
  p["api_hash"] = api_hash;

  p["system_language_code"] = "en";
  p["device_model"] = "OBS Plugin";
  p["system_version"] = "Windows";
  p["application_version"] = "1.0";
  p["enable_storage_optimizer"] = true;

  return p;
}

static bool try_extract_private_chat_user_id(const json& chat_obj, long long& out_uid)
{
  out_uid = 0;

  if (!chat_obj.contains("type")) return false;
  const auto& t = chat_obj["type"];
  if (!t.contains("@type")) return false;

  const std::string tt = t.value("@type", "");
  if (tt != "chatTypePrivate") return false;

  out_uid = t.value("user_id", 0LL);
  return out_uid > 0;
}

void TelegramTdLibClient::run()
{
  // Reduce TDLib logging noise
  send_json(R"({"@type":"setLogVerbosityLevel","new_verbosity_level":1})");

  while (running_) {
    const char* resp = td_json_client_receive(client_, 1.0);
    if (!resp)
      continue;

    json u;
    try {
      u = json::parse(resp);
    } catch (...) {
      continue;
    }

    // Log TDLib errors clearly
    if (u.contains("@type") && u["@type"] == "error") {
      int code = u.value("code", 0);
      std::string msg = u.value("message", "");
      blog(LOG_ERROR, "[TWICH][TDLib] ERROR %d: %s", code, msg.c_str());
      continue;
    }

    // Authorization state machine
    if (u.contains("@type") && u["@type"] == "updateAuthorizationState") {
      try {
        std::string st = u["authorization_state"]["@type"].get<std::string>();

        {
          std::lock_guard<std::mutex> lk(state_mutex_);
          auth_state_ = st;
        }

        blog(LOG_INFO, "[TWICH][TDLib] auth state: %s", st.c_str());

        // ---- NEW: notify listener (and prove it) ----
        OnAuthState cb;
        {
          std::lock_guard<std::mutex> lk(auth_cb_mutex_);
          cb = auth_cb_;
        }
        if (cb) {
          blog(LOG_INFO, "[TWICH][TDLib] calling auth_cb_ with state=%s", st.c_str());
          cb(st); // called on TDLib thread
        } else {
          blog(LOG_INFO, "[TWICH][TDLib] auth_cb_ is NOT set (state=%s)", st.c_str());
        }
        // --------------------------------------------

        if (st == "authorizationStateWaitTdlibParameters") {
          const int api_id_int = to_int_api_id(api_id_);
          if (api_id_int <= 0) {
            blog(LOG_ERROR, "[TWICH][TDLib] api_id is not numeric/positive: '%s'", api_id_.c_str());
            continue;
          }
          if (api_hash_.empty()) {
            blog(LOG_ERROR, "[TWICH][TDLib] api_hash is empty");
            continue;
          }

          json p = build_tdlib_parameters(session_dir_, api_id_int, api_hash_);
          const std::string payload = p.dump();
          blog(LOG_INFO, "[TWICH][TDLib] sending INLINED setTdlibParameters: %s", payload.c_str());
          send_json(payload);
          continue;
        }

        // Optional “auto-send stored values” behavior
        if (st == "authorizationStateWaitPhoneNumber") {
          std::string phone;
          {
            std::lock_guard<std::mutex> lk(auth_mutex_);
            phone = phone_;
          }
          phone = trim_copy(phone);
          if (!phone.empty()) {
            json cmd = {{"@type","setAuthenticationPhoneNumber"},{"phone_number",phone}};
            send_json(cmd.dump());
          }
        } else if (st == "authorizationStateWaitCode") {
          std::string code;
          {
            std::lock_guard<std::mutex> lk(auth_mutex_);
            code = code_;
            code_.clear();
          }
          code = trim_copy(code);
          if (!code.empty()) {
            json cmd = {{"@type","checkAuthenticationCode"},{"code",code}};
            send_json(cmd.dump());
          }
        } else if (st == "authorizationStateWaitPassword") {
          std::string pass;
          {
            std::lock_guard<std::mutex> lk(auth_mutex_);
            pass = password_;
            password_.clear();
          }
          pass = trim_copy(pass);
          if (!pass.empty()) {
            json cmd = {{"@type","checkAuthenticationPassword"},{"password",pass}};
            send_json(cmd.dump());
          }
        } else if (st == "authorizationStateReady") {
          // Resolve bot -> user_id once per run
          if (allowed_bot_user_id_.load() <= 0) {
            resolve_allowed_bot();
          }
        }

      } catch (...) {
        // ignore parse errors
      }

      continue;
    }

    // Bot resolution can come as:
    // 1) "@type":"chat" with matching @extra
    // 2) "@type":"updateNewChat" with chat field (may still have @extra inside chat)
    if (u.contains("@type") && (u["@type"] == "chat" || u["@type"] == "updateNewChat")) {
      try {
        json chat_obj;

        if (u["@type"] == "chat") {
          chat_obj = u;
        } else {
          if (!u.contains("chat")) {
            continue;
          }
          chat_obj = u["chat"];
        }

        // Only accept the chat that came back from our resolve request
        if (!chat_obj.contains("@extra") || chat_obj["@extra"] != "resolve_bot")
          continue;

        long long uid = 0;
        if (try_extract_private_chat_user_id(chat_obj, uid)) {
          allowed_bot_user_id_.store(uid);
          blog(LOG_INFO, "[TWICH][TDLib] bot resolved: @%s user_id=%lld",
               allowed_bot_username_.c_str(), uid);
        }
      } catch (...) {
        // ignore
      }
      continue;
    }

    // New incoming text message
    if (u.contains("@type") && u["@type"] == "updateNewMessage") {
      try {
        const auto& msg = u["message"];

        // DROP everything not from the bot
        if (!is_allowed_sender(msg)) {
          continue;
        }

        long long chat_id = msg["chat_id"].get<long long>();
        auto content = msg["content"];

        const std::string ctype = content.value("@type", "");
        if (ctype != "messageText") {
          continue;
        }

        std::string text = content["text"]["text"].get<std::string>();

        if (cb_)
          cb_(chat_id, text);

      } catch (...) {
        // ignore bad updates
      }
    }
  }
}

void TelegramTdLibClient::set_allowed_bot_username(const std::string& username)
{
  allowed_bot_username_ = normalize_username(username);
  allowed_bot_user_id_.store(0);
}

void TelegramTdLibClient::resolve_allowed_bot()
{
  const std::string u = normalize_username(allowed_bot_username_);
  if (u.empty()) return;

  json cmd = {
    {"@type", "searchPublicChat"},
    {"username", u},
    {"@extra", "resolve_bot"}
  };

  blog(LOG_INFO, "[TWICH][TDLib] resolving bot @%s via searchPublicChat...", u.c_str());
  send_json(cmd.dump());
}

bool TelegramTdLibClient::is_allowed_sender(const json& message) const
{
  const long long allowed = allowed_bot_user_id_.load();
  if (allowed <= 0) {
    // Not resolved yet; safest is to DROP until resolved.
    return false;
  }

  if (!message.contains("sender_id")) return false;

  const auto& sid = message["sender_id"];
  const std::string st = sid.value("@type", "");
  if (st != "messageSenderUser") return false;

  const long long uid = sid.value("user_id", 0LL);
  return uid == allowed;
}
