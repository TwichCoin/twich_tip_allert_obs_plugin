#include "config.hpp"

#include <cctype>
#include <fstream>
#include <string>

#include <obs-module.h>     // obs_module_config_path, blog, bfree
#include <util/platform.h>  // os_mkdirs

#include "nlohmann_json.hpp"
using nlohmann::json;

static std::string parent_dir_of(const std::string& path)
{
  const size_t pos = path.find_last_of("\\/");
  if (pos == std::string::npos) return std::string();
  return path.substr(0, pos);
}

static bool is_digits(const std::string& s)
{
  if (s.empty()) return false;
  for (unsigned char c : s) {
    if (!std::isdigit(c)) return false;
  }
  return true;
}

static bool is_hex(const std::string& s)
{
  if (s.size() < 16) return false;
  for (unsigned char c : s) {
    if (!(std::isdigit(c) ||
          (c >= 'a' && c <= 'f') ||
          (c >= 'A' && c <= 'F')))
      return false;
  }
  return true;
}

static void ensure_trailing_slash(std::string& dir)
{
  if (dir.empty()) return;
  char last = dir.back();
  if (last != '\\' && last != '/')
#ifdef _WIN32
    dir.push_back('\\');
#else
    dir.push_back('/');
#endif
}

std::string twich_config_path()
{
  // OBS-managed writable dir:
  // %APPDATA%\obs-studio\plugin_config\<module-name>\  (on Windows)
  char* dir_c = obs_module_config_path("twich_tip_alert");
  std::string dir = dir_c ? dir_c : "";
  if (dir_c) bfree(dir_c);

  if (!dir.empty()) {
    ensure_trailing_slash(dir);
    os_mkdirs(dir.c_str());
  }

  return dir + "config.json";
}

bool validate_tg_creds(const std::string& api_id,
                       const std::string& api_hash,
                       std::string& out_error)
{
  if (!is_digits(api_id)) {
    out_error =
      "Telegram api_id must be digits only.\n"
      "Get it from: my.telegram.org → API development tools → App api_id.";
    return false;
  }

  if (!is_hex(api_hash)) {
    out_error =
      "Telegram api_hash must be a hex string.\n"
      "Get it from: my.telegram.org → API development tools → App api_hash.";
    return false;
  }

  out_error.clear();
  return true;
}

TgAppCreds load_tg_creds()
{
  TgAppCreds out;
  const std::string path = twich_config_path();

  std::ifstream f(path, std::ios::binary);
  if (!f.good()) {
    out.valid = false;
    out.error = "config.json not found. Please enter Telegram API ID/HASH and click Save.";
    return out;
  }

  try {
    json j;
    f >> j;

    out.api_id   = j.value("api_id", "");
    out.api_hash = j.value("api_hash", "");

    std::string err;
    out.valid = validate_tg_creds(out.api_id, out.api_hash, err);
    out.error = out.valid ? "" : ("config.json invalid: " + err);
    return out;

  } catch (const std::exception& e) {
    out.valid = false;
    out.error = std::string("config.json parse error: ") + e.what();
    return out;
  }
}

bool save_tg_creds(const std::string& api_id,
                   const std::string& api_hash,
                   std::string& out_error)
{
  std::string err;
  if (!validate_tg_creds(api_id, api_hash, err)) {
    out_error = err;
    return false;
  }

  const std::string path = twich_config_path();

  const std::string dir = parent_dir_of(path);
  if (!dir.empty())
    os_mkdirs(dir.c_str());

  json j = {
    {"api_id", api_id},
    {"api_hash", api_hash}
  };

  std::ofstream out(path, std::ios::binary);
  if (!out.good()) {
    out_error = "Failed to write config.json (cannot open for writing).";
    return false;
  }

  out << j.dump(2);
  out_error.clear();

  blog(LOG_INFO, "[TWICH] Saved Telegram config to: %s", path.c_str());
  return true;
}
