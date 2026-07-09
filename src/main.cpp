/**
 * ESP32-S3 校园网 + 网络时间
 *
 * 流程: WiFi → Portal(RSA) → 苏宁时间 → 每 10s 打印
 */

#include <Arduino.h>
#include "DLUTNetwork.h"

const char *PORTAL_USER = "2024XXXXXXX";
const char *PORTAL_PASS = "password";

DLUTNetwork net;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n╔══════════════════════════════╗");
    Serial.println("║  ESP32-S3  校园网 + 时间同步 ║");
    Serial.println("╚══════════════════════════════╝");

    net.setPortalAccount(PORTAL_USER, PORTAL_PASS);
    net.setVerbose(true);
    net.begin();
}

void loop() {
    if (net.getTimestamp() != "0") {
        LocalTime t = net.getLocalTime();
        const char *wd[] = {"日", "一", "二", "三", "四", "五", "六"};
        Serial.printf("[时间] %04d-%02d-%02d (星期%s) %02d:%02d:%02d.%03d  |  戳: %s\n",
                      t.year, t.mon, t.day, wd[t.wday],
                      t.hour, t.min, t.sec, t.ms,
                      net.getTimestamp().c_str());
    } else {
        Serial.println("[时间] 尚未同步");
    }
    delay(10000);
}
