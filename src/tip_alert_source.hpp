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
  std::string animation_path;
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
