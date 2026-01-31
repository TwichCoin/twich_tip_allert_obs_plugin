#include <obs-module.h>
#include "tip_alert_source.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("twich-tip-alert", "en-US")

extern obs_source_info tip_alert_source_info;
extern void init_tip_alert_source_info();

bool obs_module_load(void)
{
  blog(LOG_INFO, "[TWICH] LOADED BUILD %s %s", __DATE__, __TIME__);
  init_tip_alert_source_info();
  obs_register_source(&tip_alert_source_info);
  return true;
}

const char* obs_module_description(void)
{
  return "TWICH Telegram Tip Alerts";
}
