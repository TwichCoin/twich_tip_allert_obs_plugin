#pragma once
#include <string>
#include <optional>

struct TipEvent {
  std::string from_username;
  std::string amount_str;   // display formatted e.g. "0.200"
  std::string symbol;       // "TWICH"
  std::string message;
  long long   ts_ms = 0;
  std::string dedupe_key;
};

std::optional<TipEvent> parse_tip_event_from_message(const std::string& text);
