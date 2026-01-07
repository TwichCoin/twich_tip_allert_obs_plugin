#pragma once

#include <string>

struct TgAppCreds {
  std::string api_id;
  std::string api_hash;
  bool valid = false;
  std::string error;
};

// Full path to %APPDATA%\obs-studio\plugin_config\twich_tip_alert\config.json
std::string twich_config_path();

// Validate an api_id/api_hash pair (digits + hex)
bool validate_tg_creds(const std::string& api_id,
                       const std::string& api_hash,
                       std::string& out_error);

// Load creds from config.json.
// If missing/invalid -> valid=false and error filled.
TgAppCreds load_tg_creds();

// Save creds into config.json (creates file if missing).
// Returns false + out_error on validation or write failure.
bool save_tg_creds(const std::string& api_id,
                   const std::string& api_hash,
                   std::string& out_error);
