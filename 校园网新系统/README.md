# 校园网新系统

适用于大连理工**新认证系统**（eportal v2 + CAS 统一身份认证）的 ESP32-S3 校园网自动登录固件。

> 旧认证系统（锐捷 Portal）的代码见仓库根目录 `src/`。

## 功能

- 上电自动登录；按键（GPIO0）重新登录；每 30 分钟检测在线状态，掉线自动重连
- 登录成功打印：余额 / 剩余流量 / 在线设备
- LED（WS2812）：蓝=登录中，绿闪=成功，红=失败

## 使用

1. 在 `src/campusnet.cpp` 顶部填入你的学号和统一身份认证密码（已替换为占位符）：

   ```cpp
   const char* USERNAME = "2024XXXXXXX";   // 学号
   const char* PASSWORD = "你的密码";        // 统一身份认证密码
   ```

2. 用 PlatformIO 打开本文件夹（platform: espressif32@6.9.0，Arduino core 2.0.17），编译上传。

## 硬件

- ESP32-S3（16MB Flash 版本开发板）
- 按键：GPIO0（上拉）；WS2812：GPIO48 × 3

## 说明

- 不使用 PSRAM（mbedTLS 硬件 AES/SHA/RSA 走 DMA 无法访问 PSRAM，启用会导致 TLS 阶段堆损坏），堆全部落在 320KB 内部 RAM
