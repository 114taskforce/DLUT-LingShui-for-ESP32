/**
 * DLUTNetwork 实现 — 全部逻辑来自 wifi.cpp
 */

#include "DLUTNetwork.h"
#include <HTTPClient.h>
#include <mbedtls/bignum.h>

DLUTNetwork::DLUTNetwork()
    : _wifiSSID(DLUT_WIFI_SSID), _wifiPass(DLUT_WIFI_PASSWORD)
{}

void DLUTNetwork::setWiFi(const char *ssid, const char *password) {
    _wifiSSID = ssid;
    _wifiPass = password ? password : "";
}

void DLUTNetwork::setPortalAccount(const char *username, const char *password) {
    _portalUser = username;
    _portalPass = password;
}

// ==================== WiFi ====================

bool DLUTNetwork::connectWiFi() {
    Serial.printf("\n[WiFi] 连接 %s ...", _wifiSSID.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(_wifiSSID.c_str(), _wifiPass.c_str());
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println();
    Serial.printf("[WiFi] IP: %s  网关: %s  DNS: %s\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.gatewayIP().toString().c_str(),
                  WiFi.dnsIP().toString().c_str());
    return true;
}

// ==================== Portal 认证 (wifi.cpp portalLogin) ====================

LoginResult DLUTNetwork::portalLogin() {
    if (WiFi.status() != WL_CONNECTED) { _lastError = "WiFi 未连接"; return LOGIN_NETWORK_ERROR; }
    if (_portalUser.length() == 0)     { _lastError = "未设置账号"; return LOGIN_NETWORK_ERROR; }

    _lastError = ""; _online = false;
    Serial.println("\n[Portal] 开始校园网认证...");

    // Step 1: GET 索引页
    String redirectUrl, queryString, mac;
    if (!_fetchRedirectUrl(redirectUrl, queryString, mac)) {
        if (pingTest() >= 0) { _online = true; _lastError = "已在线"; return LOGIN_ALREADY_ONLINE; }
        _lastError = "Portal 不可达"; return LOGIN_NETWORK_ERROR;
    }

    // Step 2: RSA 加密
    String encrypted = rsaEncrypt(_portalPass + ">" + mac);
    if (encrypted.length() == 0) { _lastError = "RSA 失败"; return LOGIN_RSA_ERROR; }
    if (_verbose) Serial.printf("[Portal] mac=%s  密文(%d chars)\n", mac.c_str(), encrypted.length());

    // Step 3: POST 登录
    String response = _postLogin(redirectUrl, queryString, _portalUser, encrypted);
    if (response.length() == 0) { _lastError = "POST 无响应"; return LOGIN_NETWORK_ERROR; }

    // Step 4: 检查结果 (wifi.cpp: 200/302/success)
    if (response.indexOf("200 OK") != -1 || response.indexOf("302") != -1 || response.indexOf("success") != -1) {
        _online = true;
        Serial.println("[Portal] ✅ 认证成功");
        return LOGIN_OK;
    }

    // 失败解析
    String body = extractBody(response);
    String msg  = extract(body, "\"message\":\"", "\"");
    if (msg.length() == 0) msg = extract(body, "\"msg\":\"", "\"");
    _lastError = msg.length() > 0 ? msg : extract(body, "\"result\":\"", "\"");
    if (_verbose) Serial.printf("[Portal] ❌ %s\n", _lastError.c_str());
    if (msg.indexOf("密码") >= 0 || msg.indexOf("password") >= 0) return LOGIN_WRONG_PASSWORD;
    if (msg.indexOf("用户") >= 0 || msg.indexOf("不存在") >= 0) return LOGIN_USER_NOT_FOUND;
    if (msg.indexOf("锁定") >= 0 || msg.indexOf("禁用") >= 0) return LOGIN_ACCOUNT_LOCKED;
    return LOGIN_UNKNOWN_ERROR;
}

// --- Portal 内部 ---

String DLUTNetwork::_readPortal(WiFiClient &client, int timeoutMs) {
    String res;
    unsigned long to = millis() + timeoutMs;
    while (millis() < to) {
        while (client.available()) { res += (char)client.read(); to = millis() + 1000; }
        if (!client.connected() && !client.available()) break;
        delay(10);
    }
    return res;
}

bool DLUTNetwork::_fetchRedirectUrl(String &redirectUrl, String &queryString, String &mac) {
    Serial.print("[Portal] GET http://123.123.123.123/ ...\n");
    WiFiClient client;
    if (!client.connect("123.123.123.123", 80)) { Serial.println("[Portal] ❌ 无法连接"); return false; }
    client.print("GET / HTTP/1.1\r\nHost: 123.123.123.123\r\n"
                 "User-Agent: Mozilla/5.0\r\nConnection: close\r\n\r\n");
    String res = _readPortal(client, 5000);
    client.stop();
    if (res.length() == 0) { Serial.println("[Portal] ❌ 无响应"); return false; }

    String body   = extractBody(res);
    String script = extract(body, "<script>", "</script>");
    redirectUrl   = extract(script, "'", "'");
    if (redirectUrl.length() == 0) redirectUrl = extract(script, "\"", "\"");
    Serial.printf("[Portal] 重定向: %s\n", redirectUrl.c_str());

    int qm = redirectUrl.indexOf('?');
    _baseUrl = (qm != -1) ? redirectUrl.substring(0, qm) : redirectUrl;
    String qs = (qm != -1) ? redirectUrl.substring(qm + 1) : "";
    queryString = qs;

    int mi = qs.indexOf("mac=");
    if (mi != -1) {
        int me = qs.indexOf('&', mi);
        mac = qs.substring(mi + 4, (me == -1) ? qs.length() : me);
    }
    if (mac.length() == 0) mac = "111111111";
    Serial.printf("[Portal] mac = %s\n", mac.c_str());
    return true;
}

String DLUTNetwork::_postLogin(const String &redirectUrl, const String &queryString,
                               const String &username, const String &encrypted) {
    // wifi.cpp 行 332-333: 先去掉 ?query, 再替换
    String loginUrl = redirectUrl.substring(0, redirectUrl.indexOf('?'));
    loginUrl.replace("index.jsp", "InterFace.do?method=login");

    String encQS = queryString;
    encQS.replace("&", "%26"); encQS.replace("=", "%3D");

    String postBody;
    postBody += "userId=" + username;
    postBody += "&password=" + encrypted;
    postBody += "&service=&queryString=" + encQS;
    postBody += "&operatorPwd=&operatorUserId=&validcode=";
    postBody += "&passwordEncrypt=true";

    String host = loginUrl;
    host.replace("http://", "");
    int slash = host.indexOf('/');
    String path = (slash != -1) ? host.substring(slash) : "/";
    host = host.substring(0, slash);

    Serial.printf("[Portal] POST %s%s\n", host.c_str(), path.c_str());
    return _httpPost(host.c_str(), 80, path, postBody);
}

String DLUTNetwork::_httpPost(const char *host, int port, const String &path, const String &body) {
    WiFiClient client;
    if (!client.connect(host, port)) { Serial.printf("[E] connect %s:%d fail\n", host, port); return ""; }
    client.print("POST " + path + " HTTP/1.1\r\nHost: " + host +
                 "\r\nContent-Type: application/x-www-form-urlencoded\r\n" +
                 "Content-Length: " + body.length() +
                 "\r\nUser-Agent: Mozilla/5.0\r\nConnection: close\r\n\r\n" + body);
    String res = _readPortal(client, 5000);
    client.stop();
    return res;
}

// ==================== 一键上线 ====================

bool DLUTNetwork::begin() {
    connectWiFi();
    LoginResult r = portalLogin();
    if (r != LOGIN_OK && r != LOGIN_ALREADY_ONLINE)
        Serial.printf("[Init] ❌ Portal: %s\n", _lastError.c_str());
    if (_online || r == LOGIN_ALREADY_ONLINE) {
        if (!syncTime()) Serial.println("[诊断] ❌ 时间同步失败!");
        else             Serial.println("[诊断] ✅ 网络已通");
    }
    Serial.println("\n[系统] 就绪");
    return _online;
}

// ==================== 网络检测 ====================

int DLUTNetwork::pingTest() {
    WiFiClient c; c.setTimeout(3000);
    unsigned long t0 = millis();
    bool ok = c.connect("www.baidu.com", 80);
    unsigned long t1 = millis(); c.stop();
    return ok ? (int)(t1 - t0) : -1;
}

// ==================== 时间 (wifi.cpp syncTime + getTimestamp) ====================

bool DLUTNetwork::syncTime(int maxRetries) {
    Serial.println("\n[Time] 同步网络时间...");
    for (int i = 0; i < maxRetries; i++) {
        HTTPClient http; http.setTimeout(10000); http.begin(DLUT_TIME_API);
        if (http.GET() == 200) {
            String payload = http.getString(); http.end();
            int idx = payload.indexOf("\"currentTime\":");
            if (idx != -1) {
                int s = idx + 14, e = payload.indexOf(",", s);
                if (e == -1) e = payload.indexOf("}", s);
                _apiTime = strtoull(payload.substring(s, e).c_str(), NULL, 10);
                _apiMillis = millis(); _timeOk = true;
                Serial.printf("[Time] %llu\n", _apiTime);
                return true;
            }
        }
        http.end();
        delay(1000 * (i + 1));
    }
    Serial.println("[Time] ❌ FAIL");
    return false;
}

String DLUTNetwork::getTimestamp() {
    if (!_timeOk) return "0";
    return String(_apiTime + (millis() - _apiMillis));
}

LocalTime DLUTNetwork::getLocalTime() {
    LocalTime lt = {0};
    if (!_timeOk) return lt;
    unsigned long long nowMs = _apiTime + (millis() - _apiMillis);
    time_t nowSec = (time_t)(nowMs / 1000);
    lt.ms = (int)(nowMs % 1000);
    struct tm *t = gmtime(&nowSec);
    lt.year = t->tm_year + 1900; lt.mon = t->tm_mon + 1; lt.day = t->tm_mday;
    lt.hour = (t->tm_hour + 8) % 24; lt.min = t->tm_min; lt.sec = t->tm_sec;
    lt.wday = t->tm_wday;
    return lt;
}

// ==================== 字符串工具 ====================

String DLUTNetwork::extractBody(const String &http) {
    int idx = http.indexOf("\r\n\r\n");
    return (idx == -1) ? http : http.substring(idx + 4);
}

String DLUTNetwork::extract(const String &s, const String &left, const String &right) {
    int l = s.indexOf(left); if (l == -1) return "";
    l += left.length();
    int r = s.indexOf(right, l);
    return (r == -1) ? "" : s.substring(l, r);
}

// ==================== RSA (wifi.cpp rsaEncrypt) ====================

String DLUTNetwork::rsaEncrypt(const String &plain, const char *eHex, const char *mHex) {
    mbedtls_mpi E, M, P, C;
    mbedtls_mpi_init(&E); mbedtls_mpi_init(&M);
    mbedtls_mpi_init(&P); mbedtls_mpi_init(&C);
    mbedtls_mpi_read_string(&E, 16, eHex);
    mbedtls_mpi_read_string(&M, 16, mHex);
    mbedtls_mpi_read_binary(&P, (const unsigned char *)plain.c_str(), plain.length());
    mbedtls_mpi_exp_mod(&C, &P, &E, &M, nullptr);
    size_t keyLen = (mbedtls_mpi_bitlen(&M) + 7) / 8;
    unsigned char *buf = new unsigned char[keyLen];
    mbedtls_mpi_write_binary(&C, buf, keyLen);
    String result; result.reserve(keyLen * 2);
    for (size_t i = 0; i < keyLen; i++) { char h[3]; snprintf(h, sizeof(h), "%02x", buf[i]); result += h; }
    delete[] buf;
    mbedtls_mpi_free(&E); mbedtls_mpi_free(&M);
    mbedtls_mpi_free(&P); mbedtls_mpi_free(&C);
    return result;
}
