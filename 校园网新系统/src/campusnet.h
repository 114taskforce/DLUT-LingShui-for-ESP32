#ifndef CAMPUSNET_H
#define CAMPUSNET_H

#include <Arduino.h>

struct CampusInfo {
    bool online = false;
    String userName;   // 学号
    String balance;    // 套餐&余额
    String traffic;    // 剩余流量
    String devices;    // 在线设备
};

// 初始化(NVS 中读取已保存的认证入口地址, 供"已在线"状态下复用)
void campusInit();

// 完整校园网登录流程: 认证跳转 → eportal 会话 → CAS SSO → 上线 → 获取账户信息
// 成功返回 true 并填充 info(余额/剩余流量/在线设备)
bool campusLogin(CampusInfo &info);

// 用当前会话检查是否在线(供定时检测)
bool campusCheckOnline();

// 重新拉取账户信息(余额/剩余流量/在线设备)→ 填充 info。
// 需在 campusLogin 成功后(会话有效、持有 /eportal/ 作用域 JSESSIONID)调用,
// 返回 true 表示成功取到至少一项。
bool campusRefreshAccount(CampusInfo &info);

#endif
