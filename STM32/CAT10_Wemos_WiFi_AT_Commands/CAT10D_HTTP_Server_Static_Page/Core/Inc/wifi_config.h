/**
  * @file    wifi_config.h
  * @brief   Wi-Fi credentials for this project.
  *
  *          The real SSID/password live in "wifi_secrets.h", which is NOT committed
  *          to git (see .gitignore). To use your own network, create
  *          Core/Inc/wifi_secrets.h next to this file containing:
  *
  *              #define WIFI_SSID      "your-network"
  *              #define WIFI_PASSWORD  "your-password"
  *
  *          Without that file the project still builds, using the placeholders below.
  */
#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#if defined(__has_include)
#  if __has_include("wifi_secrets.h")
#    include "wifi_secrets.h"
#  endif
#endif

#ifndef WIFI_SSID
#define WIFI_SSID      "YOUR_WIFI_SSID"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"
#endif

#endif /* WIFI_CONFIG_H */
