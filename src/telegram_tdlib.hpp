#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "nlohmann_json.hpp" // IMPORTANT: include, don't forward-declare

class TelegramTdLibClient {
public:
  using OnTextMessage = std::function<void(long long chat_id, const std::string& text)>;
  using OnAuthState  = std::function<void(const std::string& state)>;

  TelegramTdLibClient();
  ~TelegramTdLibClient();

  void start(const std::string& api_id,
             const std::string& api_hash,
             const std::string& session_dir,
             OnTextMessage cb);

  void stop();

  // auth helpers
  std::string auth_state() const;

  // NEW: auth state change callback (called from TDLib thread)
  void set_on_auth_state(OnAuthState cb);

  // immediate send (OBS buttons)
  void send_phone_now(const std::string& phone);
  void send_code_now(const std::string& code);
  void send_password_now(const std::string& password);

  // optional stored-input API
  void set_phone(const std::string& phone);
  void submit_code(const std::string& code);
  void submit_password(const std::string& password);

  // bot-only filtering
  void set_allowed_bot_username(const std::string& username);

private:
  void run();
  void send_json(const std::string& s);

  void resolve_allowed_bot();
  bool is_allowed_sender(const nlohmann::json& message) const;

  // thread / lifecycle
  std::thread thr_;
  std::atomic<bool> running_{false};

  // tdjson client handle (opaque)
  void* client_ = nullptr;

  // config
  std::string api_id_;
  std::string api_hash_;
  std::string session_dir_;

  // login values (optional)
  std::mutex auth_mutex_;
  std::string phone_;
  std::string code_;
  std::string password_;

  // auth state tracking
  mutable std::mutex state_mutex_;
  std::string auth_state_;

  // callback
  OnTextMessage cb_;

  // NEW: auth state callback
  mutable std::mutex auth_cb_mutex_;
  OnAuthState auth_cb_;

  // bot filter state
  std::string allowed_bot_username_ = "EddieLives_bot";
  std::atomic<long long> allowed_bot_user_id_{0};
};
