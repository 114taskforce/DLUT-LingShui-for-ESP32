// ====================================================================
// 校园网自动登录主程序
//   上电自动登录; 按键重新登录; 每30分钟检测在线状态, 掉线自动重连
//   登录成功打印: 余额 / 剩余流量 / 在线设备
//   LED: 蓝=登录中, 绿闪=成功, 红=失败
// ====================================================================

#include <WiFi.h>
#include "campusnet.h"
#include "Freenove_WS2812_Lib_for_ESP32.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern TaskHandle_t loopTaskHandle;   // 框架 loop 任务句柄(查栈余量用)

// ==================== WiFi ====================
const char* WIFI_SSID = "DLUT-LingShui";
const char* WIFI_PSWD = "";

// ==================== 硬件 ====================
#define TRIGGER_BUTTON_PIN 0      // 按键: 重新登录
#define LEDS_COUNT  3
#define LEDS_PIN    48
#define CHANNEL     0

// ==================== 周期 ====================
#define CHECK_INTERVAL 1800000UL  // 30分钟在线检测
#define DEBOUNCE_DELAY 200

Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_GRB);
CampusInfo gInfo;

void ledColor(int r, int g, int b) { strip.setAllLedsColor(r, g, b); }

bool doLogin()
{
    Serial.println("\n[开始登录校园网]");
    ledColor(0, 0, 255);          // 蓝: 登录中

    if (!campusLogin(gInfo)) {
        Serial.println("=========== 登录失败 ===========");
        ledColor(255, 0, 0);      // 红: 失败
        return false;
    }

    Serial.println("=========== 登录成功 ===========");
    Serial.printf("用户名   : %s\n", gInfo.userName.c_str());
    Serial.printf("余额     : %s\n", gInfo.balance.c_str());
    Serial.printf("剩余流量 : %s\n", gInfo.traffic.c_str());
    Serial.printf("在线设备 : %s\n", gInfo.devices.c_str());

    for (int i = 0; i < 3; i++) { // 绿闪: 成功
        ledColor(0, 255, 0);
        delay(150);
        ledColor(0, 0, 0);
        delay(150);
    }
    return true;
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    pinMode(TRIGGER_BUTTON_PIN, INPUT_PULLUP);
    strip.begin();
    strip.setBrightness(20);

    Serial.println("\nESP32-S3 校园网自动登录");
    Serial.printf("[诊断] PSRAM: %s(%u字节), 空闲堆: %u/%u 字节\n",
                  psramFound() ? "有" : "无",
                  (unsigned)ESP.getPsramSize(),
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getHeapSize());
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PSWD);
    Serial.print("连接 WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("WiFi 已连接: " + WiFi.localIP().toString());

    campusInit();
    doLogin();
    Serial.printf("[诊断] 登录后空闲堆: %u 字节, loop任务栈余量: %u 字节\n",
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)uxTaskGetStackHighWaterMark(loopTaskHandle));
}

unsigned long lastCheck = 0;
bool lastButtonState = HIGH;
unsigned long lastPress = 0;

void loop()
{
    // 按键: 重新登录
    bool s = digitalRead(TRIGGER_BUTTON_PIN);
    if (lastButtonState == HIGH && s == LOW && millis() - lastPress > DEBOUNCE_DELAY) {
        lastPress = millis();
        Serial.println("\n[按键] 重新刷新");
        campusRefreshAccount(gInfo);
        Serial.printf("用户名   : %s\n", gInfo.userName.c_str());
        Serial.printf("余额     : %s\n", gInfo.balance.c_str());
        Serial.printf("剩余流量 : %s\n", gInfo.traffic.c_str());
        Serial.printf("在线设备 : %s\n", gInfo.devices.c_str());
    }
    lastButtonState = s;

    // 定时检测在线状态
    if (millis() - lastCheck >= CHECK_INTERVAL) {
        lastCheck = millis();
        if (!campusCheckOnline()) {
            Serial.println("[定时] 已掉线, 自动重新登录");
            doLogin();
        } else {
            Serial.println("[定时] 在线状态正常");
        }
    }
    delay(10);
}
