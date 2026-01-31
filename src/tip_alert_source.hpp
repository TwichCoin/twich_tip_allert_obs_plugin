#pragma once

#include <obs-module.h>

#include <mutex>
#include <queue>
#include <string>

#include "event_parse.hpp"
#include "telegram_tdlib.hpp"

struct tip_alert_source
{
  obs_source_t* source = nullptr;

  // settings
  // Back-compat (old single file setting). We keep it so older scene collections
  // still load, but the new UI uses tier{1,2,3}_media.
  std::string animation_path;

  // Tiered media selection (in TWICH units)
  // Defaults are provided via get_defaults: 0 / 10 / 50
  double tier1_threshold = 0.0;
  double tier2_threshold = 10.0;
  double tier3_threshold = 50.0;

  std::string tier1_media;
  std::string tier2_media;
  std::string tier3_media;

  float duration_sec = 3.0f;

  // telegram
  TelegramTdLibClient tg;
  std::string tg_phone;
  std::string tg_code;
  std::string tg_pass;

  // queued tip events
  std::mutex queue_mutex;
  std::queue<TipEvent> queue;

  // child sources
  obs_source_t* media = nullptr;
  obs_source_t* text  = nullptr;

  // playback state
  bool playing = false;
  float time_left = 0.0f;
};

extern struct obs_source_info tip_alert_source_info;
