/**
 * DLUTNetwork — 大连理工大学校园网连接库
 *
 * 从 wifi.cpp 提取:
 *   WiFi 连接 → Portal 认证(RSA) → 苏宁时间同步
 *
 * 用法:
 *   DLUTNetwork net;
 *   net.setPortalAccount("学号", "密码");
 *   net.begin();                     // 一键上线
 *   LocalTime t = net.getLocalTime(); // 获取北京时间
 */

#ifndef DLUT_NETWORK_H
#define DLUT_NETWORK_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define DLUT_WIFI_SSID     "DLUT-LingShui"
#define DLUT_WIFI_PASSWORD ""
#define DLUT_TIME_API      "http://f.m.suning.com/api/ct.do"

#define DLUT_RSA_E "010001"
#define DLUT_RSA_M "94dd2a8675fb779e6b9f7103698634cd400f27a154afa67af6166a43fc2641722" \
                   "2a79506d34cacc7641946abda1785b7acf9910ad6a0978c91ec84d40b71d28913" \
                   "79af19ffb333e7517e390bd26ac312fe940c340466b4a5d4af1d65c3b5944078f9" \
                   "6a1a51a5a53e4bc302818b7c9f63c4a1b07bd7d874cef1c3d4b2f5eb7871"

enum LoginResult {
    LOGIN_OK = 0, LOGIN_ALREADY_ONLINE, LOGIN_WRONG_PASSWORD,
    LOGIN_USER_NOT_FOUND, LOGIN_ACCOUNT_LOCKED,
    LOGIN_NETWORK_ERROR, LOGIN_RSA_ERROR, LOGIN_UNKNOWN_ERROR,
};

struct LocalTime {
    int year, mon, day, hour, min, sec, ms, wday;
};

class DLUTNetwork {
public:
    DLUTNetwork();

    void setWiFi(const char *ssid, const char *password = "");
    void setPortalAccount(const char *username, const char *password);
    void setVerbose(bool on) { _verbose = on; }

    /// 仅连接 WiFi
    bool connectWiFi();

    /// 仅 Portal 认证
    LoginResult portalLogin();

    /// 一键上线: WiFi → Portal → 时间同步
    bool begin();

    /// Ping 百度检测连通性, -1 = 不通
    int pingTest();

    /// 同步苏宁时间
    bool syncTime(int maxRetries = 5);

    /// 毫秒时间戳 (补偿 millis 流逝)
    String getTimestamp();

    /// 北京时间
    LocalTime getLocalTime();

    bool   isOnline()    const { return _online; }
    String getLastError() const { return _lastError; }

    // === 公开工具 ===
    static String rsaEncrypt(const String &plain,
                             const char *eHex = DLUT_RSA_E,
                             const char *mHex = DLUT_RSA_M);
    static String extractBody(const String &http);
    static String extract(const String &s, const String &left, const String &right);

private:
    String _wifiSSID, _wifiPass, _portalUser, _portalPass;
    String _baseUrl, _lastError, _cookieJar;
    bool   _online = false, _verbose = false;
    unsigned long long _apiTime = 0;
    unsigned long      _apiMillis = 0;
    bool               _timeOk = false;

    String _readPortal(WiFiClient &client, int timeoutMs);
    bool   _fetchRedirectUrl(String &redirectUrl, String &queryString, String &mac);
    String _postLogin(const String &redirectUrl, const String &queryString,
                      const String &username, const String &encrypted);

    // 内部 HTTP (无 Cookie, 供 Portal 专用)
    String _httpPost(const char *host, int port, const String &path, const String &body);
};

#endif
