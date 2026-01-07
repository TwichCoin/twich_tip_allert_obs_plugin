#include "event_parse.hpp"
#include <string>
#include <cctype>
#include <sstream>
#include <iomanip>
#include "nlohmann_json.hpp" // external/nlohmann_json.hpp

using nlohmann::json;

static std::optional<std::string> extract_event_json(const std::string& text) {
  auto p = text.find("#EVENT");
  if (p == std::string::npos) return std::nullopt;

  auto brace = text.find('{', p);
  if (brace == std::string::npos) return std::nullopt;

  // Simple brace matching (robust enough for single JSON object)
  int depth = 0;
  for (size_t i = brace; i < text.size(); ++i) {
    if (text[i] == '{') depth++;
    else if (text[i] == '}') {
      depth--;
      if (depth == 0) {
        return text.substr(brace, i - brace + 1);
      }
    }
  }
  return std::nullopt;
}

static std::string format_amount_9dp(const std::string& amount_twits_str) {
  // amount_twits is integer string. TWICH assumed 9 decimals.
  // Display 3 decimals by default.
  long long v = 0;
  try { v = std::stoll(amount_twits_str); } catch (...) { return "0.000"; }

  const long long denom = 1000000000LL;
  long long whole = v / denom;
  long long frac  = llabs(v % denom);

  // Convert to 3 decimals (rounded)
  // frac is 0..999,999,999; take first 3 decimals with rounding from 4th decimal
  long long frac3 = (frac + 500000LL) / 1000000LL; // +0.0005 for rounding at 4th digit
  if (frac3 >= 1000) { whole += 1; frac3 -= 1000; }

  std::ostringstream oss;
  oss << whole << "." << std::setw(3) << std::setfill('0') << frac3;
  return oss.str();
}

std::optional<TipEvent> parse_tip_event_from_message(const std::string& text) {
  auto jtxt = extract_event_json(text);
  if (!jtxt) return std::nullopt;

  json j;
  try { j = json::parse(*jtxt); }
  catch (...) { return std::nullopt; }

  if (!j.contains("type") || j["type"].get<std::string>() != "TWICH_TIP")
    return std::nullopt;

  TipEvent ev;
  ev.symbol = "TWICH";

  if (j.contains("from_username")) ev.from_username = j["from_username"].get<std::string>();
  if (j.contains("message")) ev.message = j["message"].get<std::string>();
  if (j.contains("ts")) ev.ts_ms = j["ts"].get<long long>();

  std::string amount_twits = j.value("amount_twits", "0");
  ev.amount_str = format_amount_9dp(amount_twits);

  // Dedupe key: ts + from + amount (good enough; better if you add event_id)
  ev.dedupe_key = std::to_string(ev.ts_ms) + "|" + ev.from_username + "|" + amount_twits;

  // Truncate message for overlay sanity
  if (ev.message.size() > 140) ev.message.resize(140);

  return ev;
}
