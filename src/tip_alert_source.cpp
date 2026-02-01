#include "tip_alert_source.hpp"

#include <cstdint>
#include <string>

#include <obs-module.h>
#include <graphics/graphics.h>

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

static void tip_alert_defaults(obs_data_t* settings)
{
  // Tier defaults
  obs_data_set_default_double(settings, "tier1_threshold", 0.0);
  obs_data_set_default_double(settings, "tier2_threshold", 10.0);
  obs_data_set_default_double(settings, "tier3_threshold", 50.0);

  // Legacy + tier media
  obs_data_set_default_string(settings, "animation", "");
  obs_data_set_default_string(settings, "tier1_media", "");
  obs_data_set_default_string(settings, "tier2_media", "");
  obs_data_set_default_string(settings, "tier3_media", "");

  // Text UI defaults
  obs_data_set_default_int(settings,  "text_color",   0x00FFFF00); // yellow (0xRRGGBB)
  obs_data_set_default_int(settings,  "text_size",    36);
  obs_data_set_default_bool(settings, "text_outline", true);
  obs_data_set_default_int(settings,  "outline_size", 2);
  obs_data_set_default_string(settings, "font_face", "Arial");

  obs_data_set_default_int(settings, "text_position", 0); // top
  obs_data_set_default_int(settings, "text_margin", 40);

  obs_data_set_default_double(settings, "text_fade_in",  0.20);
  obs_data_set_default_double(settings, "text_fade_out", 0.25);

  obs_data_set_default_string(
    settings,
    "text_template",
    "{user} tipped {amount} {symbol}\n{message}"
  );

  obs_data_set_default_double(settings, "duration", 3.0);
}

// Read a string from current source settings (works even before user clicks OK)
static std::string get_setting_str(obs_source_t* src, const char* key)
{
  obs_data_t* st = obs_source_get_settings(src);
  std::string v = obs_data_get_string(st, key);
  obs_data_release(st);
  return v;
}

// ---- template helpers ----
static void replace_all(std::string& s, const std::string& from, const std::string& to)
{
  if (from.empty()) return;
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
}

static std::string format_tip_text(const tip_alert_source* s, const TipEvent& ev)
{
  std::string out = s->text_template.empty()
    ? std::string("{user} tipped {amount} {symbol}\n{message}")
    : s->text_template;

  replace_all(out, "{user}", ev.from_username);
  replace_all(out, "{amount}", ev.amount_str);
  replace_all(out, "{symbol}", ev.symbol);
  replace_all(out, "{message}", ev.message);

  return out;
}

// Apply consistent styling to Text (GDI+)
static void apply_tip_text_style(
  obs_data_t* d,
  uint32_t color_rgb,
  const std::string& face,
  int text_size,
  bool outline_enabled,
  int outline_size)
{
  obs_data_t* font = obs_data_create();
  obs_data_set_string(font, "face", face.empty() ? "Arial" : face.c_str());
  obs_data_set_int(font, "size", text_size);
  obs_data_set_int(font, "flags", 0);
  obs_data_set_obj(d, "font", font);
  obs_data_release(font);

  // color is 0xRRGGBB for text_gdiplus
  obs_data_set_int(d, "color", (int)color_rgb);

  obs_data_set_bool(d, "outline", outline_enabled);
  obs_data_set_int(d, "outline_size", outline_enabled ? outline_size : 0);
  obs_data_set_int(d, "outline_color", 0x000000); // black 0xRRGGBB

  // align left/top
  obs_data_set_int(d, "align", 0);
  obs_data_set_int(d, "valign", 0);
}

// Update only opacity for fade
static void set_text_opacity(tip_alert_source* s, int opacity_0_100)
{
  if (!s || !s->text) return;

  if (opacity_0_100 < 0) opacity_0_100 = 0;
  if (opacity_0_100 > 100) opacity_0_100 = 100;

  if (s->last_opacity == opacity_0_100)
    return;

  obs_data_t* td = obs_source_get_settings(s->text);
  obs_data_set_int(td, "opacity", opacity_0_100);
  obs_source_update(s->text, td);
  obs_data_release(td);

  s->last_opacity = opacity_0_100;
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

  // TDLib thread -> UI thread auth state callback
  s->tg.set_on_auth_state([s](const std::string& st) {
    if (!s || !s->source) return;

    blog(LOG_INFO, "[TWICH] UI auth callback: %s", st.c_str());

    std::string ui =
      std::string("TDLib state: ") + (st.empty() ? "(empty)" : st) + "\n" +
      format_auth_status(st);

    queue_auth_status_update(s->source, ui, true);
  });

  // Start TDLib and parse incoming messages into TipEvent queue
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

  queue_auth_status_update(s->source, "Starting Telegram… (TDLib launching)", true);
}

// -------------------- OBS callbacks --------------------
static void* tip_alert_create(obs_data_t* settings, obs_source_t* source)
{
  auto* s = new tip_alert_source();
  s->source = source;

  // tiers
  s->tier1_threshold = obs_data_get_double(settings, "tier1_threshold");
  s->tier2_threshold = obs_data_get_double(settings, "tier2_threshold");
  s->tier3_threshold = obs_data_get_double(settings, "tier3_threshold");

  if (s->tier2_threshold < s->tier1_threshold)
    s->tier2_threshold = s->tier1_threshold;
  if (s->tier3_threshold < s->tier2_threshold)
    s->tier3_threshold = s->tier2_threshold;

  s->tier1_media = obs_data_get_string(settings, "tier1_media");
  s->tier2_media = obs_data_get_string(settings, "tier2_media");
  s->tier3_media = obs_data_get_string(settings, "tier3_media");

  // legacy
  s->animation_path = obs_data_get_string(settings, "animation");
  if (s->tier1_media.empty() && !s->animation_path.empty())
    s->tier1_media = s->animation_path;

  // text config
  s->text_color    = (uint32_t)obs_data_get_int(settings, "text_color");
  s->text_size     = (int)obs_data_get_int(settings, "text_size");
  s->text_outline  = obs_data_get_bool(settings, "text_outline");
  s->outline_size  = (int)obs_data_get_int(settings, "outline_size");
  s->font_face     = obs_data_get_string(settings, "font_face");

  s->text_position = (int)obs_data_get_int(settings, "text_position");
  s->text_margin   = (int)obs_data_get_int(settings, "text_margin");

  s->text_fade_in  = (float)obs_data_get_double(settings, "text_fade_in");
  s->text_fade_out = (float)obs_data_get_double(settings, "text_fade_out");

  s->text_template = obs_data_get_string(settings, "text_template");

  s->duration_sec = (float)obs_data_get_double(settings, "duration");

  // telegram fields
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

  // remove active children before releasing
  if (s->source) {
    if (s->media) obs_source_remove_active_child(s->source, s->media);
    if (s->text)  obs_source_remove_active_child(s->source, s->text);
  }

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

// -------------------- Credentials UI (Save only) --------------------
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

  // Read-only status box
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

  // Tiered animation settings
  obs_properties_t* media_grp = obs_properties_create();
  obs_properties_add_text(
    media_grp,
    "tier_help",
    "Highest tier whose threshold is met will be played.\n"
    "Defaults are 0 / 10 / 50.",
    OBS_TEXT_INFO
  );

  obs_properties_add_float(media_grp, "tier1_threshold", "Tier 1 threshold", 0.0, 1e9, 0.1);
  obs_properties_add_path (media_grp, "tier1_media", "Tier 1 media (WebM)",
                           OBS_PATH_FILE, "WebM Files (*.webm)", nullptr);

  obs_properties_add_float(media_grp, "tier2_threshold", "Tier 2 threshold", 0.0, 1e9, 0.1);
  obs_properties_add_path (media_grp, "tier2_media", "Tier 2 media (WebM)",
                           OBS_PATH_FILE, "WebM Files (*.webm)", nullptr);

  obs_properties_add_float(media_grp, "tier3_threshold", "Tier 3 threshold", 0.0, 1e9, 0.1);
  obs_properties_add_path (media_grp, "tier3_media", "Tier 3 media (WebM)",
                           OBS_PATH_FILE, "WebM Files (*.webm)", nullptr);

  obs_properties_add_group(props, "tiered_media", "Alert Media (by Amount)", OBS_GROUP_NORMAL, media_grp);

  // Text config
  obs_properties_add_color(props, "text_color", "Tip text color");
  obs_properties_add_int(props, "text_size", "Tip text size", 16, 96, 1);

  obs_property_t* p_font = obs_properties_add_list(
    props,
    "font_face",
    "Tip font",
    OBS_COMBO_TYPE_LIST,
    OBS_COMBO_FORMAT_STRING
  );
  obs_property_list_add_string(p_font, "Arial", "Arial");
  obs_property_list_add_string(p_font, "Segoe UI", "Segoe UI");
  obs_property_list_add_string(p_font, "Roboto", "Roboto");

  obs_properties_add_bool(props, "text_outline", "Text outline");
  obs_properties_add_int(props, "outline_size", "Outline size", 0, 10, 1);

  obs_property_t* p_pos = obs_properties_add_list(
    props,
    "text_position",
    "Text position",
    OBS_COMBO_TYPE_LIST,
    OBS_COMBO_FORMAT_INT
  );
  obs_property_list_add_int(p_pos, "Top", 0);
  obs_property_list_add_int(p_pos, "Center", 1);
  obs_property_list_add_int(p_pos, "Bottom", 2);

  obs_properties_add_int(props, "text_margin", "Text margin (px)", 0, 400, 1);

  obs_properties_add_float(props, "text_fade_in",  "Text fade-in (sec)",  0.0, 5.0, 0.05);
  obs_properties_add_float(props, "text_fade_out", "Text fade-out (sec)", 0.0, 5.0, 0.05);

  obs_properties_add_text(props, "text_template", "Text template", OBS_TEXT_MULTILINE);

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
      ev.amount_str = "12.500";
      ev.symbol = "TWICH";
      ev.message = "Test tip message";
      ev.from_username = "tester";
      ev.dedupe_key = "test";
      std::lock_guard<std::mutex> lk(s->queue_mutex);
      s->queue.push(ev);
      return true;
    }
  );

  // Advanced group (creds + help)
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

  // tiers
  s->tier1_threshold = obs_data_get_double(settings, "tier1_threshold");
  s->tier2_threshold = obs_data_get_double(settings, "tier2_threshold");
  s->tier3_threshold = obs_data_get_double(settings, "tier3_threshold");

  if (s->tier2_threshold < s->tier1_threshold)
    s->tier2_threshold = s->tier1_threshold;
  if (s->tier3_threshold < s->tier2_threshold)
    s->tier3_threshold = s->tier2_threshold;

  s->tier1_media = obs_data_get_string(settings, "tier1_media");
  s->tier2_media = obs_data_get_string(settings, "tier2_media");
  s->tier3_media = obs_data_get_string(settings, "tier3_media");

  // legacy
  s->animation_path = obs_data_get_string(settings, "animation");
  if (s->tier1_media.empty() && !s->animation_path.empty())
    s->tier1_media = s->animation_path;

  // text
  s->text_color    = (uint32_t)obs_data_get_int(settings, "text_color");
  s->text_size     = (int)obs_data_get_int(settings, "text_size");
  s->text_outline  = obs_data_get_bool(settings, "text_outline");
  s->outline_size  = (int)obs_data_get_int(settings, "outline_size");
  s->font_face     = obs_data_get_string(settings, "font_face");

  s->text_position = (int)obs_data_get_int(settings, "text_position");
  s->text_margin   = (int)obs_data_get_int(settings, "text_margin");

  s->text_fade_in  = (float)obs_data_get_double(settings, "text_fade_in");
  s->text_fade_out = (float)obs_data_get_double(settings, "text_fade_out");

  s->text_template = obs_data_get_string(settings, "text_template");

  s->duration_sec  = (float)obs_data_get_double(settings, "duration");

  s->tg_phone = obs_data_get_string(settings, "tg_phone");
  s->tg_code  = obs_data_get_string(settings, "tg_code");
  s->tg_pass  = obs_data_get_string(settings, "tg_pass");
}

// -------------------- Tick/render --------------------
static void tip_alert_tick(void* data, float seconds)
{
  auto* s = (tip_alert_source*)data;

  if (s->playing) {
    s->alert_elapsed += seconds;
    s->time_left -= seconds;

    // fade alpha
    float alpha = 1.0f;

    if (s->text_fade_in > 0.0f && s->alert_elapsed < s->text_fade_in)
      alpha = s->alert_elapsed / s->text_fade_in;

    if (s->text_fade_out > 0.0f && s->time_left < s->text_fade_out) {
      float a2 = s->time_left / s->text_fade_out;
      if (a2 < alpha) alpha = a2;
    }

    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    set_text_opacity(s, (int)(alpha * 100.0f));

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

  // choose media for this event
  const std::string* chosen_media = nullptr;
  double amount = 0.0;
  try { amount = std::stod(ev.amount_str); } catch (...) { amount = 0.0; }

  if (amount >= s->tier3_threshold && !s->tier3_media.empty())
    chosen_media = &s->tier3_media;
  else if (amount >= s->tier2_threshold && !s->tier2_media.empty())
    chosen_media = &s->tier2_media;
  else if (amount >= s->tier1_threshold && !s->tier1_media.empty())
    chosen_media = &s->tier1_media;

  // media child
  if (chosen_media && !chosen_media->empty()) {
    if (!s->media) {
      obs_data_t* d = obs_data_create();
      obs_data_set_string(d, "local_file", chosen_media->c_str());
      obs_data_set_bool(d, "is_local_file", true);
      obs_data_set_bool(d, "restart_on_activate", true);
      obs_data_set_bool(d, "close_when_inactive", false);

      s->media = obs_source_create("ffmpeg_source", "tip_anim", d, nullptr);
      obs_data_release(d);

      if (s->media)
        obs_source_add_active_child(s->source, s->media);
    } else {
      obs_data_t* md = obs_source_get_settings(s->media);
      obs_data_set_string(md, "local_file", chosen_media->c_str());
      obs_data_set_bool(md, "is_local_file", true);
      obs_data_set_bool(md, "restart_on_activate", true);
      obs_data_set_bool(md, "close_when_inactive", false);

      obs_source_update(s->media, md);
      obs_data_release(md);
    }
  }

  // text child
  if (!s->text) {
    obs_data_t* d = obs_data_create();
    obs_data_set_string(d, "text", "");

    apply_tip_text_style(d, s->text_color, s->font_face, s->text_size, s->text_outline, s->outline_size);

    s->text = obs_source_create("text_gdiplus", "tip_text", d, nullptr);
    obs_data_release(d);

    if (s->text)
      obs_source_add_active_child(s->source, s->text);
  }

  if (s->text) {
    obs_data_t* td = obs_source_get_settings(s->text);

    const std::string txt = format_tip_text(s, ev);
    obs_data_set_string(td, "text", txt.c_str());

    // force style each alert (so it never "reverts")
    apply_tip_text_style(td, s->text_color, s->font_face, s->text_size, s->text_outline, s->outline_size);

    // start faded if fade-in enabled
    obs_data_set_int(td, "opacity", (s->text_fade_in > 0.0f) ? 0 : 100);

    obs_source_update(s->text, td);
    obs_data_release(td);

    s->last_opacity = -1;
  }

  // play media only if chosen this event (avoid sticky old tier)
  if (chosen_media && !chosen_media->empty() && s->media) {
    obs_source_set_enabled(s->media, true);
    obs_source_media_restart(s->media);
  } else {
    if (s->media) obs_source_set_enabled(s->media, false);
  }

  if (s->text)
    obs_source_set_enabled(s->text, true);

  s->playing = true;
  s->time_left = s->duration_sec;
  s->alert_elapsed = 0.0f;
}

// sizing
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

static void render_child_at(obs_source_t* child, float x, float y)
{
  if (!child) return;
  gs_matrix_push();
  gs_matrix_translate3f(x, y, 0.0f);
  obs_source_video_render(child);
  gs_matrix_pop();
}

static void tip_alert_render(void* data, gs_effect_t*)
{
  auto* s = (tip_alert_source*)data;
  if (!s->playing) return;

  if (s->media)
    obs_source_video_render(s->media);

  if (s->text) {
    const uint32_t W = tip_alert_get_width(data);
    const uint32_t H = tip_alert_get_height(data);

    const uint32_t tw = obs_source_get_width(s->text);
    const uint32_t th = obs_source_get_height(s->text);

    float x = (tw > 0 && W > tw) ? (float)(W - tw) * 0.5f : 0.0f;
    float y = 0.0f;

    if (s->text_position == 0) {          // top
      y = (float)s->text_margin;
    } else if (s->text_position == 1) {   // center
      y = (th > 0 && H > th) ? (float)(H - th) * 0.5f : 0.0f;
    } else {                               // bottom
      y = (th > 0 && H > th) ? (float)(H - th - s->text_margin) : 0.0f;
    }

    render_child_at(s->text, x, y);
  }
}

// ------------------------------------------------------------
// Exported source info: safe init via assignments (no MSVC C7560)
// ------------------------------------------------------------
obs_source_info tip_alert_source_info = {};

void init_tip_alert_source_info(void)
{
  tip_alert_source_info.id           = "twich_tip_alert";
  tip_alert_source_info.type         = OBS_SOURCE_TYPE_INPUT;
  tip_alert_source_info.output_flags = OBS_SOURCE_VIDEO;

  tip_alert_source_info.get_name       = tip_alert_get_name;
  tip_alert_source_info.create         = tip_alert_create;
  tip_alert_source_info.destroy        = tip_alert_destroy;
  tip_alert_source_info.get_width      = tip_alert_get_width;
  tip_alert_source_info.get_height     = tip_alert_get_height;
  tip_alert_source_info.get_defaults   = tip_alert_defaults;
  tip_alert_source_info.get_properties = tip_alert_properties;
  tip_alert_source_info.update         = tip_alert_update;
  tip_alert_source_info.video_tick     = tip_alert_tick;
  tip_alert_source_info.video_render   = tip_alert_render;
}
