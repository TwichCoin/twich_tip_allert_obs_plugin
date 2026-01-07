#include "tip_alert_source.hpp"

#include <string>

#include <obs-module.h>

#include "config.hpp"
#include "event_parse.hpp"

// Build a session dir next to config.json (portable, writable)
static std::string session_dir_from_config_path()
{
  std::string cfg = twich_config_path();
  auto pos = cfg.find_last_of("\\/");
  std::string folder = (pos == std::string::npos) ? "." : cfg.substr(0, pos);

#ifdef _WIN32
  return folder + "\\tg_session";
#else
  return folder + "/tg_session";
#endif
}

static const char* tip_alert_get_name(void*)
{
  return "TWICH Tip Alerts (Telegram)";
}

// Read a string from current source settings (works even before user clicks OK)
static std::string get_setting_str(obs_source_t* src, const char* key)
{
  obs_data_t* st = obs_source_get_settings(src);
  std::string v = obs_data_get_string(st, key);
  obs_data_release(st);
  return v;
}

// ---------- Status formatting ----------
static std::string format_auth_status(const std::string& st)
{
  if (st == "authorizationStateWaitPhoneNumber")
    return "Waiting for phone number\nNext: Enter phone and click \"Set Phone\".";
  if (st == "authorizationStateWaitCode")
    return "Waiting for login code\nNext: Enter code and click \"Submit Code\".";
  if (st == "authorizationStateWaitPassword")
    return "Waiting for 2FA password\nNext: Enter password and click \"Submit Password\".";
  if (st == "authorizationStateReady")
    return "READY (logged in)\nNext: Tips should appear when the bot sends events.";
  if (st == "authorizationStateWaitTdlibParameters")
    return "Initializing Telegram (TDLib)…\nNext: Wait a moment.";
  if (st == "authorizationStateClosing")
    return "Telegram closing…";
  if (st == "authorizationStateClosed")
    return "Telegram closed\nNext: Click \"Restart TDLib\".";
  if (st.empty())
    return "NO AUTH STATE YET\n(if logs show Ready, callback/UI update is not running)";
  return st;
}

// ---------- UI-thread update plumbing ----------
struct AuthStatusTask
{
  obs_source_t* source = nullptr; // ref-counted
  std::string text;
  bool refresh_props = true;
};

static void ui_set_auth_status(void* param)
{
  auto* t = (AuthStatusTask*)param;
  if (!t) return;

  if (t->source) {
    obs_data_t* d = obs_source_get_settings(t->source);

    const char* prev_c = obs_data_get_string(d, "tg_auth_status");
    const std::string prev = prev_c ? prev_c : "";

    if (prev != t->text) {
      obs_data_set_string(d, "tg_auth_status", t->text.c_str());
      obs_source_update(t->source, d);
    }

    obs_data_release(d);

    // Force Properties dialog repaint (safe: UI thread, not inside get_properties())
    if (t->refresh_props) {
      obs_source_update_properties(t->source);
    }

    obs_source_release(t->source);
    t->source = nullptr;
  }

  delete t;
}

static void queue_auth_status_update(obs_source_t* source, const std::string& text, bool refresh_props)
{
  if (!source) return;

  auto* t = new AuthStatusTask();
  t->text = text;
  t->refresh_props = refresh_props;

  t->source = obs_source_get_ref(source);
  if (!t->source) {
    delete t;
    return;
  }

  obs_queue_task(OBS_TASK_UI, ui_set_auth_status, t, false);
}

// Start/Restart TDLib based on config.json
static void start_tdlib(tip_alert_source* s)
{
  TgAppCreds creds = load_tg_creds();
  if (!creds.valid) {
    blog(LOG_ERROR, "[TWICH] Telegram API creds missing/invalid: %s", creds.error.c_str());
    blog(LOG_ERROR, "[TWICH] TDLib NOT started. Enter API ID/HASH and click Save.");

    queue_auth_status_update(
      s->source,
      "Telegram NOT started\n"
      "Reason: Missing/invalid API credentials.\n"
      "Next: Open Advanced, enter API ID/HASH, click \"Save credentials\".",
      true
    );
    return;
  }

  const std::string session_dir = session_dir_from_config_path();

  blog(LOG_INFO, "[TWICH] Starting TDLib. session_dir=%s api_id=%s",
       session_dir.c_str(),
       creds.api_id.c_str());

  s->tg.set_allowed_bot_username("EddieLives_bot");

  // TDLib thread -> UI thread
  s->tg.set_on_auth_state([s](const std::string& st) {
    if (!s || !s->source) return;

    // Critical debug: proves UI callback is actually reached
    blog(LOG_INFO, "[TWICH] UI auth callback: %s", st.c_str());

    // show raw + mapped (so you can see EXACT state string)
    std::string ui =
      std::string("TDLib state: ") + (st.empty() ? "(empty)" : st) + "\n" +
      format_auth_status(st);

    queue_auth_status_update(s->source, ui, true);
  });

  s->tg.start(
    creds.api_id,
    creds.api_hash,
    session_dir,
    [s](long long /*chat_id*/, const std::string& text) {
      auto ev = parse_tip_event_from_message(text);
      if (!ev) return;
      std::lock_guard<std::mutex> lk(s->queue_mutex);
      s->queue.push(*ev);
    }
  );

  // Set immediately (TDLib will overwrite as soon as it emits state)
  queue_auth_status_update(s->source, "Starting Telegram… (TDLib launching)", true);
}

// -------------------- OBS callbacks --------------------
static void* tip_alert_create(obs_data_t* settings, obs_source_t* source)
{
  auto* s = new tip_alert_source();
  s->source = source;

  s->animation_path = obs_data_get_string(settings, "animation");
  s->duration_sec   = (float)obs_data_get_double(settings, "duration");

  s->tg_phone = obs_data_get_string(settings, "tg_phone");
  s->tg_code  = obs_data_get_string(settings, "tg_code");
  s->tg_pass  = obs_data_get_string(settings, "tg_pass");

  // Initial status value stored in settings
  obs_data_set_string(settings, "tg_auth_status", "Starting Telegram…");
  obs_source_update(source, settings);

  start_tdlib(s);
  return s;
}

static void tip_alert_destroy(void* data)
{
  auto* s = (tip_alert_source*)data;

  if (s->media) obs_source_release(s->media);
  if (s->text)  obs_source_release(s->text);

  s->tg.stop();
  delete s;
}

// -------------------- Telegram UI buttons --------------------
static bool on_set_phone(obs_properties_t*, obs_property_t*, void* data)
{
  auto* s = (tip_alert_source*)data;

  const std::string phone = get_setting_str(s->source, "tg_phone");
  const std::string st = s->tg.auth_state();

  blog(LOG_INFO, "[TWICH] Set Phone clicked. auth_state=%s phone='%s'", st.c_str(), phone.c_str());

  if (st == "authorizationStateWaitPhoneNumber") {
    s->tg.send_phone_now(phone);
    blog(LOG_INFO, "[TWICH] phone sent");
  } else {
    blog(LOG_INFO, "[TWICH] not waiting for phone, ignoring");
  }

  // UI will update automatically via callback, but keep instant feedback too:
  std::string ui =
    std::string("TDLib state: ") + (st.empty() ? "(empty)" : st) + "\n" +
    format_auth_status(st);
  queue_auth_status_update(s->source, ui, true);

  return true;
}

static bool on_submit_code(obs_properties_t*, obs_property_t*, void* data)
{
  auto* s = (tip_alert_source*)data;

  const std::string code = get_setting_str(s->source, "tg_code");
  const std::string st = s->tg.auth_state();

  blog(LOG_INFO, "[TWICH] Submit Code clicked. auth_state=%s code_len=%d",
       st.c_str(), (int)code.size());

  if (st == "authorizationStateWaitCode") {
    s->tg.send_code_now(code);
    blog(LOG_INFO, "[TWICH] code sent");
  } else {
    blog(LOG_INFO, "[TWICH] not waiting for code, ignoring");
  }

  std::string ui =
    std::string("TDLib state: ") + (st.empty() ? "(empty)" : st) + "\n" +
    format_auth_status(st);
  queue_auth_status_update(s->source, ui, true);

  return true;
}

static bool on_submit_pass(obs_properties_t*, obs_property_t*, void* data)
{
  auto* s = (tip_alert_source*)data;

  const std::string pass = get_setting_str(s->source, "tg_pass");
  const std::string st = s->tg.auth_state();

  blog(LOG_INFO, "[TWICH] Submit Password clicked. auth_state=%s pass_len=%d",
       st.c_str(), (int)pass.size());

  if (st == "authorizationStateWaitPassword") {
    s->tg.send_password_now(pass);
    blog(LOG_INFO, "[TWICH] password sent");
  } else {
    blog(LOG_INFO, "[TWICH] not waiting for password, ignoring");
  }

  std::string ui =
    std::string("TDLib state: ") + (st.empty() ? "(empty)" : st) + "\n" +
    format_auth_status(st);
  queue_auth_status_update(s->source, ui, true);

  return true;
}

// -------------------- Credentials UI (Save only; OBS per-field reveal) --------------------
static bool on_save_creds(obs_properties_t*, obs_property_t*, void* data)
{
  auto* s = (tip_alert_source*)data;
  if (!s || !s->source) return true;

  obs_data_t* st = obs_source_get_settings(s->source);

  const std::string api_id   = obs_data_get_string(st, "tg_api_id");
  const std::string api_hash = obs_data_get_string(st, "tg_api_hash");

  obs_data_release(st);

  std::string err;
  if (!save_tg_creds(api_id, api_hash, err)) {
    blog(LOG_ERROR, "[TWICH] Save credentials FAILED: %s", err.c_str());
    queue_auth_status_update(s->source, std::string("Credentials NOT saved\nReason: ") + err, true);
    return true;
  }

  blog(LOG_INFO, "[TWICH] Saved Telegram API creds to config.json");

  s->tg.stop();
  start_tdlib(s);

  return true;
}

static bool on_restart_tdlib(obs_properties_t*, obs_property_t*, void* data)
{
  auto* s = (tip_alert_source*)data;
  blog(LOG_INFO, "[TWICH] Restart TDLib clicked");
  s->tg.stop();
  start_tdlib(s);
  return true;
}

// -------------------- Properties --------------------
static obs_properties_t* tip_alert_properties(void* data)
{
  obs_properties_t* props = obs_properties_create();

  // Read-only status box (multiline disabled text widget)
  obs_property_t* p_status = obs_properties_add_text(
    props,
    "tg_auth_status",
    "Telegram authentication status",
    OBS_TEXT_MULTILINE
  );
  obs_property_set_enabled(p_status, false);

  // Telegram login UI
  obs_properties_add_text(props, "tg_phone", "Telegram phone", OBS_TEXT_DEFAULT);
  obs_properties_add_button(props, "tg_set_phone", "Set Phone", on_set_phone);

  obs_properties_add_text(props, "tg_code", "Telegram login code", OBS_TEXT_DEFAULT);
  obs_properties_add_button(props, "tg_submit_code", "Submit Code", on_submit_code);

  obs_properties_add_text(props, "tg_pass", "Telegram 2FA password (if enabled)", OBS_TEXT_PASSWORD);
  obs_properties_add_button(props, "tg_submit_pass", "Submit Password", on_submit_pass);

  // Animation settings
  obs_properties_add_path(
    props,
    "animation",
    "Animation (WebM)",
    OBS_PATH_FILE,
    "WebM Files (*.webm)",
    nullptr
  );

  obs_properties_add_float(
    props,
    "duration",
    "Alert Duration (seconds)",
    1.0, 10.0, 0.1
  );

  // Test alert
  obs_properties_add_button(
    props,
    "test_alert",
    "Test Alert",
    [](obs_properties_t*, obs_property_t*, void* data2) {
      auto* s = (tip_alert_source*)data2;
      TipEvent ev;
      ev.amount_str = "0.200";
      ev.symbol = "TWICH";
      ev.message = "Test tip";
      ev.from_username = "tester";
      ev.dedupe_key = "test";
      std::lock_guard<std::mutex> lk(s->queue_mutex);
      s->queue.push(ev);
      return true;
    }
  );

  // --- Advanced group (creds + help) ---
  obs_properties_t* adv = obs_properties_create();

  obs_properties_add_text(
    adv,
    "tg_api_help_title",
    "Telegram API credentials (required)",
    OBS_TEXT_INFO
  );

  obs_properties_add_text(
    adv,
    "tg_api_help",
    "You must create your own Telegram API credentials:\n"
    "1) Open my.telegram.org\n"
    "2) API development tools \xe2\x86\x92 Create application\n"
    "3) Copy App api_id and App api_hash\n"
    "Then paste them below and click Save.\n\n"
    "Safety: these fields are password-type to avoid leaking them on stream.\n"
    "Use OBS's per-field reveal control if you need to see what you typed.",
    OBS_TEXT_INFO
  );

  obs_properties_add_text(adv, "tg_api_id",   "API ID",   OBS_TEXT_PASSWORD);
  obs_properties_add_text(adv, "tg_api_hash", "API HASH", OBS_TEXT_PASSWORD);

  obs_properties_add_button(adv, "tg_save_creds", "Save credentials", on_save_creds);
  obs_properties_add_button(adv, "tg_restart_tdlib", "Restart TDLib", on_restart_tdlib);

  obs_properties_add_group(props, "advanced", "Advanced", OBS_GROUP_NORMAL, adv);

  return props;
}

static void tip_alert_update(void* data, obs_data_t* settings)
{
  auto* s = (tip_alert_source*)data;
  s->animation_path = obs_data_get_string(settings, "animation");
  s->duration_sec   = (float)obs_data_get_double(settings, "duration");

  s->tg_phone = obs_data_get_string(settings, "tg_phone");
  s->tg_code  = obs_data_get_string(settings, "tg_code");
  s->tg_pass  = obs_data_get_string(settings, "tg_pass");
}

// -------------------- Video/render --------------------
static void tip_alert_tick(void* data, float seconds)
{
  auto* s = (tip_alert_source*)data;

  if (s->playing) {
    s->time_left -= seconds;
    if (s->time_left <= 0.0f) {
      s->playing = false;
      if (s->media) obs_source_set_enabled(s->media, false);
      if (s->text)  obs_source_set_enabled(s->text, false);
    }
    return;
  }

  TipEvent ev;
  {
    std::lock_guard<std::mutex> lk(s->queue_mutex);
    if (s->queue.empty())
      return;
    ev = s->queue.front();
    s->queue.pop();
  }

  // Create child sources lazily
  if (!s->media && !s->animation_path.empty()) {
    obs_data_t* d = obs_data_create();
    obs_data_set_string(d, "local_file", s->animation_path.c_str());
    s->media = obs_source_create("ffmpeg_source", "tip_anim", d, nullptr);
    obs_data_release(d);
  }

  if (!s->text) {
    obs_data_t* d = obs_data_create();
    obs_data_set_string(d, "text", "");
    s->text = obs_source_create("text_gdiplus", "tip_text", d, nullptr);
    obs_data_release(d);
  }

  // Update text
  std::string text =
    "+" + ev.amount_str + " " + ev.symbol + "\n" +
    ev.message;

  if (s->text) {
    obs_data_t* td = obs_source_get_settings(s->text);
    obs_data_set_string(td, "text", text.c_str());
    obs_source_update(s->text, td);
    obs_data_release(td);
  }

  // Restart animation
  if (s->media) {
    obs_source_set_enabled(s->media, true);
    obs_source_media_restart(s->media);
  }

  if (s->text)
    obs_source_set_enabled(s->text, true);

  s->playing = true;
  s->time_left = s->duration_sec;
}

static uint32_t tip_alert_get_width(void* data)
{
  auto* s = (tip_alert_source*)data;
  if (s->media) {
    uint32_t w = obs_source_get_width(s->media);
    if (w) return w;
  }
  if (s->text) {
    uint32_t w = obs_source_get_width(s->text);
    if (w) return w;
  }
  return 1920;
}

static uint32_t tip_alert_get_height(void* data)
{
  auto* s = (tip_alert_source*)data;
  if (s->media) {
    uint32_t h = obs_source_get_height(s->media);
    if (h) return h;
  }
  if (s->text) {
    uint32_t h = obs_source_get_height(s->text);
    if (h) return h;
  }
  return 1080;
}

static void tip_alert_render(void* data, gs_effect_t*)
{
  auto* s = (tip_alert_source*)data;
  if (!s->playing) return;

  if (s->media) obs_source_video_render(s->media);
  if (s->text)  obs_source_video_render(s->text);
}

// Exported source info
struct obs_source_info tip_alert_source_info = {
  .id = "twich_tip_alert",
  .type = OBS_SOURCE_TYPE_INPUT,
  .output_flags = OBS_SOURCE_VIDEO,
  .get_name = tip_alert_get_name,
  .create = tip_alert_create,
  .destroy = tip_alert_destroy,
  .get_width = tip_alert_get_width,
  .get_height = tip_alert_get_height,
  .get_properties = tip_alert_properties,
  .update = tip_alert_update,
  .video_tick = tip_alert_tick,
  .video_render = tip_alert_render,
};
