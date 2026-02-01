#pragma once

#include <obs-module.h>

#include <cstdint>
#include <mutex>
#include <queue>
#include <string>

#include "event_parse.hpp"
#include "telegram_tdlib.hpp"

struct tip_alert_source
{
  obs_source_t* source = nullptr;

  // --- settings / legacy ---
  std::string animation_path; // legacy key "animation"
  float duration_sec = 3.0f;

  // --- Telegram ---
  TelegramTdLibClient tg;
  std::string tg_phone;
  std::string tg_code;
  std::string tg_pass;

  // --- queued tip events ---
  std::mutex queue_mutex;
  std::queue<TipEvent> queue;

  // --- tiered media ---
  double tier1_threshold = 0.0;
  double tier2_threshold = 10.0;
  double tier3_threshold = 50.0;

  std::string tier1_media;
  std::string tier2_media;
  std::string tier3_media;

  // --- child sources ---
  obs_source_t* media = nullptr; // ffmpeg_source
  obs_source_t* text  = nullptr; // text_gdiplus

  // --- playback state ---
  bool playing = false;
  float time_left = 0.0f;

  // --- text UI config ---
  uint32_t text_color = 0x00FFFF00; // 0xRRGGBB
  int text_size = 36;
  bool text_outline = true;
  int outline_size = 2;
  std::string font_face = "Arial";

  // position preset
  int text_position = 0; // 0=top 1=center 2=bottom
  int text_margin = 40;

  // fade
  float text_fade_in  = 0.20f;
  float text_fade_out = 0.25f;
  float alert_elapsed = 0.0f;
  int last_opacity = -1;

  // template
  std::string text_template = "{user} tipped {amount} {symbol}\n{message}";
};

extern obs_source_info tip_alert_source_info;
void init_tip_alert_source_info(void);
