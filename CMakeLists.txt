cmake_minimum_required(VERSION 3.20)
project(twich_tip_alert_obs LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to your OBS source tree + build output
set(OBS_SRC "D:/p/obs-studio")
set(OBS_BUILD "D:/p/obs-studio/build")

# OBS headers
include_directories(
  "${OBS_SRC}/libobs"
  "${OBS_SRC}/UI/obs-frontend-api"
  "${OBS_BUILD}/config"
  "${OBS_BUILD}/libobs"
  "${OBS_SRC}"
)

# OBS libs
link_directories(
  "${OBS_BUILD}/libobs/Release"
)

add_library(twich_tip_alert MODULE
  src/plugin.cpp
  src/tip_alert_source.cpp
  src/event_parse.cpp
  src/telegram_tdlib.cpp
  src/config.cpp
)

target_compile_features(twich_tip_alert PRIVATE cxx_std_20)
set_target_properties(twich_tip_alert PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED YES
)

target_include_directories(twich_tip_alert PRIVATE external src external/tdlib/include)
target_link_directories(twich_tip_alert PRIVATE external/tdlib/lib)

target_link_libraries(twich_tip_alert
  obs 
  tdjson
)

set_target_properties(twich_tip_alert PROPERTIES
  PREFIX ""
  SUFFIX ".dll"
)

add_custom_command(TARGET twich_tip_alert POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/external/tdlib/bin/tdjson.dll"
    "$<TARGET_FILE_DIR:twich_tip_alert>/tdjson.dll"
)
