#pragma once

/*============================================================================
 *  WiFi 配置 - 支持多组，自动连接可用的网络 (按顺序优先级)
 *============================================================================*/
typedef struct {
    const char *ssid;
    const char *password;
} WifiConfig;

// 按优先级排列，扫描周围 WiFi 后自动连接第一个匹配的网络
// ⚠️ 请替换为你的真实 WiFi 名称和密码
static const WifiConfig WIFI_LIST[] = {
    { "your-wifi-1",  "your-password-1" },   // 优先级 1
    { "your-wifi-2",  "your-password-2" },   // 优先级 2
    { "your-wifi-3",  "your-password-3" },   // 优先级 3
};

static const int WIFI_COUNT = sizeof(WIFI_LIST) / sizeof(WIFI_LIST[0]);

/*============================================================================
 *  MQTT 配置
 *============================================================================*/
// ⚠️ 请替换为你自己的 MQTT 服务器信息
#define MQTT_SERVER   "your-mqtt.example.com"     // MQTT Broker 地址
#define MQTT_PORT     8883                         // TLS 加密端口
#define MQTT_USER     "your-username"              // MQTT 用户名
#define MQTT_PASS     "your-password"              // MQTT 密码
#define MQTT_TOPIC    "esp32/office"               // 数据订阅主题
#define MQTT_STATUS   "esp32/state"                // 状态发布主题

/*============================================================================
 *  硬件配置
 *============================================================================*/
#define BOOT_BTN      9
#define CHART_POINT_NUM 50
#define HISTORY_NUM   50     // 启动时从SD卡加载的历史数据条数
#define SD_CS         4
