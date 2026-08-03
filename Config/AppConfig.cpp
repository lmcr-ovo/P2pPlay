//
// Created by ASUS on 2026/8/3.
//

#include "AppConfig.h"

AppConfig AppConfig::defaultHost() {
    AppConfig config;
    config.p2p.localUdpPort = 10000;
    return config;
};

AppConfig AppConfig::defaultGuest() {
    AppConfig config;
    config.p2p.localUdpPort = 10001;
    return config;
}