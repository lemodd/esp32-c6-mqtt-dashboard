/*******************************************************************************
 * mqtt_helper.ino - ESP32-C6 MQTT 传感器仪表盘 + 折线图
 *
 * 界面1: 2x2 网格仪表盘 (温度/湿度/气压/时间)
 * 界面2: 三条折线图 (温度/湿度/气压历史数据)
 * BOOT 按键 (GPIO9) 切换两个界面
 ******************************************************************************/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "LVGL_Driver.h"
#include "config.h"
#include "SD_Card.h"

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

/* ---- 界面1: 仪表盘控件 ---- */
static lv_obj_t *title_label = NULL;
static lv_obj_t *temp_value_label = NULL;
static lv_obj_t *humi_value_label = NULL;
static lv_obj_t *baro_value_label = NULL;
static lv_obj_t *time_value_label = NULL;

/* ---- 界面2: 折线图控件 (3个独立图表) ---- */
static lv_obj_t *temp_chart = NULL;
static lv_obj_t *humi_chart = NULL;
static lv_obj_t *baro_chart = NULL;
static lv_chart_series_t *temp_series = NULL;
static lv_chart_series_t *humi_series = NULL;
static lv_chart_series_t *baro_series = NULL;
static lv_obj_t *temp_chart_label = NULL;
static lv_obj_t *humi_chart_label = NULL;
static lv_obj_t *baro_chart_label = NULL;
static lv_obj_t *temp_max_label = NULL;
static lv_obj_t *temp_min_label = NULL;
static lv_obj_t *humi_max_label = NULL;
static lv_obj_t *humi_min_label = NULL;
static lv_obj_t *baro_max_label = NULL;
static lv_obj_t *baro_min_label = NULL;
static lv_obj_t *temp_y_top = NULL;
static lv_obj_t *temp_y_mid = NULL;
static lv_obj_t *temp_y_bot = NULL;
static lv_obj_t *humi_y_top = NULL;
static lv_obj_t *humi_y_mid = NULL;
static lv_obj_t *humi_y_bot = NULL;
static lv_obj_t *baro_y_top = NULL;
static lv_obj_t *baro_y_mid = NULL;
static lv_obj_t *baro_y_bot = NULL;

/* ---- 历史数据 ---- */
static float temp_data[CHART_POINT_NUM] = {0};
static float humi_data[CHART_POINT_NUM] = {0};
static float baro_data[CHART_POINT_NUM] = {0};
static int data_count = 0;
static int data_index = 0;

/* ---- 界面切换 ---- */
static lv_obj_t *dashboard_scr = NULL;
static lv_obj_t *temp_scr = NULL;
static lv_obj_t *humi_scr = NULL;
static lv_obj_t *baro_scr = NULL;
static lv_obj_t *sysinfo_scr = NULL;
static int current_screen = 0;  // 0=dashboard, 1=temp, 2=humi, 3=baro, 4=sysinfo

/* ---- 系统信息标签 ---- */
static lv_obj_t *sys_chip_label = NULL;
static lv_obj_t *sys_heap_label = NULL;
static lv_obj_t *sys_wifi_label = NULL;
static lv_obj_t *sys_ip_label = NULL;
static lv_obj_t *sys_uptime_label = NULL;
static lv_obj_t *sys_rx_label = NULL;
static lv_obj_t *sys_csv_label = NULL;

/* ---- MQTT 接收计数 ---- */
static unsigned long rx_count = 0;

/* ---- CSV 记录计数 ---- */
static int csv_record_count = 0;

/* ---- 昨天数据 (历史对比) ---- */
static SensorEntry yesterday_data[CHART_POINT_NUM];
static int yesterday_count = 0;
static lv_chart_series_t *temp_series_yesterday = NULL;
static lv_chart_series_t *humi_series_yesterday = NULL;
static lv_chart_series_t *baro_series_yesterday = NULL;
static bool history_loaded = false;

/* ---- 屏幕旋转 ---- */
static int current_rotation = 0;  // 0=0°, 1=90°, 2=180°, 3=270°
static unsigned long btn_press_time = 0;
static bool btn_pressed = false;
static bool long_press_triggered = false;  // 防止长按后误触短按
#define LONG_PRESS_MS 1000

/* ---- 自动切换 ---- */
static unsigned long switch_timer = 0;
#define AUTO_SWITCH_MS 10000

/* ---- 网络重试节流 ---- */
static unsigned long last_wifi_attempt = 0;
static unsigned long last_mqtt_attempt = 0;
static bool mqtt_configured = false;
static bool wifi_scan_running = false;
#define WIFI_RETRY_MS 10000
#define MQTT_RETRY_MS 5000

typedef struct {
    lv_obj_t *card;
    lv_obj_t *name_label;
    lv_obj_t *value_label;
} SensorCard;

SensorCard createCard(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                      lv_coord_t w, lv_coord_t h,
                      const char *name, lv_color_t accent, const char *value_text) {
    SensorCard sc;
    sc.card = lv_obj_create(parent);
    lv_obj_remove_style_all(sc.card);
    lv_obj_set_size(sc.card, w, h);
    lv_obj_set_pos(sc.card, x, y);
    lv_obj_set_style_bg_color(sc.card, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(sc.card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sc.card, 1, 0);
    lv_obj_set_style_border_color(sc.card, lv_color_hex(0x333355), 0);
    lv_obj_set_style_radius(sc.card, 6, 0);
    lv_obj_set_flex_flow(sc.card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sc.card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(sc.card, 10, 0);

    sc.name_label = lv_label_create(sc.card);
    lv_label_set_text(sc.name_label, name);
    lv_obj_set_style_text_color(sc.name_label, accent, 0);
    lv_obj_set_style_text_font(sc.name_label, &lv_font_montserrat_16, 0);

    sc.value_label = lv_label_create(sc.card);
    lv_label_set_text(sc.value_label, value_text);
    lv_obj_set_style_text_color(sc.value_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(sc.value_label, &lv_font_montserrat_28, 0);

    return sc;
}

void setTitleConnected(bool connected) {
    if (!title_label) return;
    lv_obj_set_style_text_color(title_label,
        connected ? lv_color_hex(0x58A6FF) : lv_color_hex(0xFF4444), 0);
}

/*============================================================================
 *  创建仪表盘界面 (界面1)
 *============================================================================*/
void createDashboard(void) {
    dashboard_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(dashboard_scr, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(dashboard_scr, LV_OPA_COVER, 0);

    title_label = lv_label_create(dashboard_scr);
    lv_label_set_text(title_label, "Sensor Dashboard");
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFF4444), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 14);

    lv_coord_t card_w = 108, card_h = 90, gap = 8;
    lv_coord_t x0 = (240 - card_w * 2 - gap) / 2;
    lv_coord_t y0 = 42;

    SensorCard temp = createCard(dashboard_scr, x0, y0, card_w, card_h,
                                  "Temp(\xC2\xB0" "C)", lv_color_hex(0xFF6B6B), "--.-");
    temp_value_label = temp.value_label;

    SensorCard humi = createCard(dashboard_scr, x0 + card_w + gap, y0, card_w, card_h,
                                  "Humi(%)", lv_color_hex(0x6BCB77), "--.-");
    humi_value_label = humi.value_label;

    SensorCard baro = createCard(dashboard_scr, x0, y0 + card_h + gap, card_w, card_h,
                                  "Baro(hPa)", lv_color_hex(0x4ECDC4), "----.-");
    baro_value_label = baro.value_label;

    SensorCard time = createCard(dashboard_scr, x0 + card_w + gap, y0 + card_h + gap,
                                  card_w, card_h,
                                  "Updated", lv_color_hex(0xBB86FC), "--:--:--");
    lv_obj_set_style_text_font(time.value_label, &lv_font_montserrat_24, 0);
    time_value_label = time.value_label;
}

/*============================================================================
 *  创建折线图界面 (界面2) - 3个图表从上到下排列
 *
 *  ┌──────────────────────────┐
 *  │      Sensor Trends       │
 *  │ ┌──────────────────────┐ │
 *  │ │ Temp(°C)             │ │
 *  │ │ [===red line=========]│ │
 *  │ └──────────────────────┘ │
 *  │ ┌──────────────────────┐ │
 *  │ │ Humi(%)              │ │
 *  │ │ [===green line=======]│ │
 *  │ └──────────────────────┘ │
 *  │ ┌──────────────────────┐ │
 *  │ │ Baro(hPa)            │ │
 *  │ │ [===cyan line========]│ │
 *  │ └──────────────────────┘ │
 *  └──────────────────────────┘
 *============================================================================*/
lv_obj_t *createSingleChart(lv_obj_t *parent, lv_coord_t y, const char *title,
                              lv_color_t color, lv_coord_t y_min, lv_coord_t y_max,
                              lv_chart_series_t **series_out, lv_obj_t **label_out,
                              lv_obj_t **max_out, lv_obj_t **min_out) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 224, 100);
    lv_obj_set_pos(card, 8, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x333355), 0);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_set_style_pad_all(card, 0, 0);

    /* 图表填满整个卡片 */
    lv_obj_t *c = lv_chart_create(card);
    lv_obj_set_size(c, 224, 100);
    lv_obj_align(c, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_chart_set_type(c, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(c, CHART_POINT_NUM);
    lv_chart_set_range(c, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
    lv_chart_set_div_line_count(c, 3, 0);

    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_line_color(c, lv_color_hex(0x222244), LV_PART_MAIN);
    lv_obj_set_style_line_width(c, 1, LV_PART_MAIN);
    lv_obj_set_style_line_width(c, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(c, 0, LV_PART_INDICATOR);

    lv_chart_set_update_mode(c, LV_CHART_UPDATE_MODE_SHIFT);

    *series_out = lv_chart_add_series(c, color, LV_CHART_AXIS_PRIMARY_Y);

    /* 标题叠加在图表左上角 */
    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 4, 2);
    *label_out = lbl;

    /* 最大值叠加在标题下方 */
    lv_obj_t *mx = lv_label_create(card);
    lv_label_set_text(mx, "Max: --");
    lv_obj_set_style_text_color(mx, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(mx, &lv_font_montserrat_12, 0);
    lv_obj_align(mx, LV_ALIGN_TOP_LEFT, 4, 16);
    *max_out = mx;

    /* 最小值叠加在最大值下方 */
    lv_obj_t *mn = lv_label_create(card);
    lv_label_set_text(mn, "Min: --");
    lv_obj_set_style_text_color(mn, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(mn, &lv_font_montserrat_12, 0);
    lv_obj_align(mn, LV_ALIGN_TOP_LEFT, 4, 28);
    *min_out = mn;

    return c;
}

/*============================================================================
 *  创建单传感器大图表界面
 *
 *  ┌──────────────────────────────┐
 *  │     Temperature (°C)         │
 *  │   Current: 29.5              │
 *  │  Max: 32.1   Min: 27.8      │
 *  │27 ┌─────────────────────┐   │
 *  │   │     ╱╲    ╱╲        │   │
 *  │   │    ╱  ╲  ╱  ╲  ╱╲  │   │
 *  │   │   ╱    ╲╱    ╲╱  ╲ │   │
 *  │   │  ╱              ╲  │   │
 *  │26 │ ╱                ╲ │   │
 *  │   └─────────────────────┘   │
 *  └──────────────────────────────┘
 *============================================================================*/
void createSensorScreen(lv_obj_t **scr_out, const char *title,
                         lv_color_t color, lv_coord_t y_min, lv_coord_t y_max,
                         const char *chart_title,
                         lv_chart_series_t **series_out, lv_obj_t **chart_out,
                         lv_obj_t **label_out, lv_obj_t **max_out, lv_obj_t **min_out,
                         lv_obj_t **yt, lv_obj_t **ym, lv_obj_t **yb,
                         lv_chart_series_t **series_yesterday_out) {
    *scr_out = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(*scr_out, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(*scr_out, LV_OPA_COVER, 0);

    /* 标题 (居中) */
    lv_obj_t *scr_title = lv_label_create(*scr_out);
    lv_label_set_text(scr_title, title);
    lv_obj_set_style_text_color(scr_title, lv_color_hex(0x58A6FF), 0);
    lv_obj_set_style_text_font(scr_title, &lv_font_montserrat_20, 0);
    lv_obj_align(scr_title, LV_ALIGN_TOP_MID, 0, 6);

    /* 当前值 (左对齐) */
    lv_obj_t *current_lbl = lv_label_create(*scr_out);
    lv_label_set_text(current_lbl, "Current: --");
    lv_obj_set_style_text_color(current_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(current_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(current_lbl, LV_ALIGN_TOP_LEFT, 8, 30);
    *label_out = current_lbl;

    /* Max 和 Min 在同一行 (左对齐) */
    lv_obj_t *mx = lv_label_create(*scr_out);
    lv_label_set_text(mx, "Max: --");
    lv_obj_set_style_text_color(mx, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(mx, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(mx, 8, 52);
    *max_out = mx;

    lv_obj_t *mn = lv_label_create(*scr_out);
    lv_label_set_text(mn, "Min: --");
    lv_obj_set_style_text_color(mn, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(mn, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(mn, 128, 52);
    *min_out = mn;

    /* 图表卡片 */
    lv_obj_t *card = lv_obj_create(*scr_out);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 224, 160);
    lv_obj_set_pos(card, 8, 72);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x333355), 0);
    lv_obj_set_style_radius(card, 4, 0);
    lv_obj_set_style_pad_all(card, 0, 0);

    /* Y轴刻度标签 (图表左侧外部) */
    lv_obj_t *y_top = lv_label_create(card);
    lv_label_set_text(y_top, "--");
    lv_obj_set_style_text_color(y_top, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(y_top, &lv_font_montserrat_12, 0);
    lv_obj_align(y_top, LV_ALIGN_TOP_LEFT, 2, 4);

    lv_obj_t *y_mid = lv_label_create(card);
    lv_label_set_text(y_mid, "--");
    lv_obj_set_style_text_color(y_mid, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(y_mid, &lv_font_montserrat_12, 0);
    lv_obj_align(y_mid, LV_ALIGN_LEFT_MID, 2, 0);

    lv_obj_t *y_bot = lv_label_create(card);
    lv_label_set_text(y_bot, "--");
    lv_obj_set_style_text_color(y_bot, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(y_bot, &lv_font_montserrat_12, 0);
    lv_obj_align(y_bot, LV_ALIGN_BOTTOM_LEFT, 2, -4);
    *yt = y_top;
    *ym = y_mid;
    *yb = y_bot;

    /* 图表 (留出左侧30px给刻度) */
    lv_obj_t *c = lv_chart_create(card);
    lv_obj_set_size(c, 190, 160);
    lv_obj_set_pos(c, 32, 0);
    lv_chart_set_type(c, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(c, CHART_POINT_NUM);
    lv_chart_set_range(c, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
    lv_chart_set_div_line_count(c, 5, 4);
    lv_chart_set_update_mode(c, LV_CHART_UPDATE_MODE_SHIFT);

    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_line_color(c, lv_color_hex(0x222244), LV_PART_MAIN);
    lv_obj_set_style_line_width(c, 1, LV_PART_MAIN);
    lv_obj_set_style_line_width(c, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(c, 0, LV_PART_INDICATOR);

    *series_out = lv_chart_add_series(c, color, LV_CHART_AXIS_PRIMARY_Y);
    *series_yesterday_out = lv_chart_add_series(c, lv_color_hex(0x666666), LV_CHART_AXIS_PRIMARY_Y);
    *chart_out = c;
}

/*============================================================================
 *  创建系统信息界面 (界面5)
 *============================================================================*/
void createSysinfoScreen(void) {
    sysinfo_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(sysinfo_scr, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_bg_opa(sysinfo_scr, LV_OPA_COVER, 0);

    /* 标题 */
    lv_obj_t *scr_title = lv_label_create(sysinfo_scr);
    lv_label_set_text(scr_title, "System Info");
    lv_obj_set_style_text_color(scr_title, lv_color_hex(0x58A6FF), 0);
    lv_obj_set_style_text_font(scr_title, &lv_font_montserrat_20, 0);
    lv_obj_align(scr_title, LV_ALIGN_TOP_MID, 0, 6);

    /* 信息卡片 */
    lv_obj_t *card = lv_obj_create(sysinfo_scr);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 224, 180);
    lv_obj_set_pos(card, 8, 44);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x333355), 0);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_set_style_pad_all(card, 0, 0);

    /* 信息项: Chip, Heap, WiFi, IP, Uptime, RX Count, CSV Records */
    const char *labels[] = {"Chip", "Heap", "WiFi", "IP", "Uptime", "RX Count", "CSV Records"};
    lv_color_t colors[] = {
        lv_color_hex(0x58A6FF), lv_color_hex(0xBBBBBB), lv_color_hex(0x6BCB77),
        lv_color_hex(0x6BCB77), lv_color_hex(0xBB86FC), lv_color_hex(0xBBBBBB),
        lv_color_hex(0xFF6B6B)
    };
    lv_obj_t **value_labels[] = {
        &sys_chip_label, &sys_heap_label, &sys_wifi_label,
        &sys_ip_label, &sys_uptime_label, &sys_rx_label, &sys_csv_label
    };

    for (int i = 0; i < 7; i++) {
        lv_coord_t y = 4 + i * 24;

        // 标题 (左对齐)
        lv_obj_t *lbl = lv_label_create(card);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_set_style_text_color(lbl, colors[i], 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, y + 1);

        // 数值 (右对齐)
        *value_labels[i] = lv_label_create(card);
        lv_label_set_text(*value_labels[i], "--");
        lv_obj_set_style_text_color(*value_labels[i], lv_color_hex(0xAAAAAA), 0);
        lv_obj_set_style_text_font(*value_labels[i], &lv_font_montserrat_16, 0);
        lv_obj_align(*value_labels[i], LV_ALIGN_TOP_RIGHT, -8, y + 1);

        // 横线分隔
        if (i < 6) {
            lv_obj_t *line = lv_obj_create(card);
            lv_obj_remove_style_all(line);
            lv_obj_set_size(line, 216, 1);
            lv_obj_set_pos(line, 4, y + 20);
            lv_obj_set_style_bg_color(line, lv_color_hex(0x333355), 0);
            lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
        }
    }
}

/*============================================================================
 *  更新系统信息
 *============================================================================*/
void updateSysinfo(void) {
    char buf[64];
    uint32_t heap_total = ESP.getHeapSize();
    uint32_t heap_free = ESP.getFreeHeap();
    uint8_t heap_pct = (uint8_t)((heap_total - heap_free) * 100 / heap_total);

    lv_label_set_text(sys_chip_label, "ESP32-C6");

    snprintf(buf, sizeof(buf), "%lu KB (%u%%)", heap_free / 1024, 100 - heap_pct);
    lv_label_set_text(sys_heap_label, buf);

    lv_label_set_text(sys_wifi_label, WiFi.status() == WL_CONNECTED ? WiFi.SSID().c_str() : "Disconnected");
    lv_label_set_text(sys_ip_label, WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "--");

    unsigned long sec = millis() / 1000;
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", sec / 3600, (sec % 3600) / 60, sec % 60);
    lv_label_set_text(sys_uptime_label, buf);

    snprintf(buf, sizeof(buf), "%lu", rx_count);
    lv_label_set_text(sys_rx_label, buf);

    snprintf(buf, sizeof(buf), "%d", csv_record_count);
    lv_label_set_text(sys_csv_label, buf);
}

/*============================================================================
 *  切换界面 (5个界面循环)
 *============================================================================*/
void switchScreen(void) {
    current_screen = (current_screen + 1) % 5;
    switch (current_screen) {
        case 0: lv_scr_load(dashboard_scr); Serial.println("Switched to Dashboard"); break;
        case 1: lv_scr_load(temp_scr); Serial.println("Switched to Temp"); break;
        case 2: lv_scr_load(humi_scr); Serial.println("Switched to Humi"); break;
        case 3: lv_scr_load(baro_scr); Serial.println("Switched to Baro"); break;
        case 4: lv_scr_load(sysinfo_scr); updateSysinfo(); Serial.println("Switched to Sysinfo"); break;
    }
}

/*============================================================================
 *  旋转屏幕 (只支持 0° 和 90°)
 *============================================================================*/
void rotateScreen(void) {
    current_rotation = (current_rotation + 1) % 4;  // 0°→90°→180°→270°→0°
    LCD_SetRotation(current_rotation);
    
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
    
    Serial.printf("Screen rotated: %d°\n", current_rotation * 90);
}

/*============================================================================
 *  按键轮询 (检测短按和长按)
 *============================================================================*/
void checkButton(void) {
    bool btn_state = (digitalRead(BOOT_BTN) == LOW);  // 按下时为 LOW
    
    if (btn_state && !btn_pressed) {
        // 按下瞬间
        btn_pressed = true;
        btn_press_time = millis();
        long_press_triggered = false;
    } else if (btn_state && btn_pressed && !long_press_triggered) {
        // 按住中，检查是否超过 1.5 秒
        if (millis() - btn_press_time >= LONG_PRESS_MS) {
            long_press_triggered = true;
            rotateScreen();
        }
    } else if (!btn_state && btn_pressed) {
        // 释放瞬间，只有短按才切换界面
        unsigned long press_duration = millis() - btn_press_time;
        btn_pressed = false;
        
        if (!long_press_triggered && press_duration > 50 && press_duration < LONG_PRESS_MS) {
            switchScreen();
            switch_timer = millis();
        }
    }
}

/*============================================================================
 *  更新仪表盘界面
 *============================================================================*/
void updateDashboard(float temp, float humi, float baro, const char *timestamp) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f", temp);
    lv_label_set_text(temp_value_label, buf);
    snprintf(buf, sizeof(buf), "%.1f", humi);
    lv_label_set_text(humi_value_label, buf);
    snprintf(buf, sizeof(buf), "%.1f", baro);
    lv_label_set_text(baro_value_label, buf);

    const char *t = timestamp;
    const char *p = strrchr(t, '_');
    lv_label_set_text(time_value_label, p ? p + 1 : t);
}

/*============================================================================
 *  查找数据数组中的最小值和最大值
 *============================================================================*/
void findMinMax(float *data, int count, float *min_val, float *max_val) {
    if (count <= 0) {
        *min_val = 0;
        *max_val = 0;
        return;
    }

    int start = (data_count < CHART_POINT_NUM) ? 0 : data_index;
    int first = start % CHART_POINT_NUM;
    *min_val = data[first];
    *max_val = data[first];

    for (int i = 1; i < count; i++) {
        int idx = (start + i) % CHART_POINT_NUM;
        if (data[idx] < *min_val) *min_val = data[idx];
        if (data[idx] > *max_val) *max_val = data[idx];
    }
}

/*============================================================================
 *  更新折线图数据 (自适应 Y 轴范围)
 *  - 1个点: 显示单个点
 *  - 2个点以上: 显示折线
 *  - 未填充的点隐藏 (LV_CHART_POINT_NONE)
 *============================================================================*/
void updateChart(float temp, float humi, float baro) {
    char buf[32];

    temp_data[data_index] = temp;
    humi_data[data_index] = humi;
    baro_data[data_index] = baro;

    if (data_count < CHART_POINT_NUM) data_count++;

    /* 使用 set_next_value 从右侧添加新点 (放大10倍保留小数精度) */
    lv_chart_set_next_value(temp_chart, temp_series, (lv_coord_t)(temp * 10));
    lv_chart_set_next_value(humi_chart, humi_series, (lv_coord_t)(humi * 10));
    lv_chart_set_next_value(baro_chart, baro_series, (lv_coord_t)(baro * 10));

    lv_chart_refresh(temp_chart);
    lv_chart_refresh(humi_chart);
    lv_chart_refresh(baro_chart);

    data_index = (data_index + 1) % CHART_POINT_NUM;

    /* 动态调整 Temp Y 轴范围 (考虑今天+昨天) */
    if (data_count >= 1) {
        float tmin, tmax;
        findMinMax(temp_data, data_count, &tmin, &tmax);
        if (yesterday_count > 0) {
            for (int i = 0; i < yesterday_count; i++) {
                if (yesterday_data[i].temp < tmin) tmin = yesterday_data[i].temp;
                if (yesterday_data[i].temp > tmax) tmax = yesterday_data[i].temp;
            }
        }
        lv_coord_t y_min = (lv_coord_t)((tmin - 1) * 10);
        lv_coord_t y_max = (lv_coord_t)((tmax + 1) * 10);
        if (y_max - y_min < 10) y_max = y_min + 10;
        lv_chart_set_range(temp_chart, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", y_max / 10.0f);
        lv_label_set_text(temp_y_top, buf);
        snprintf(buf, sizeof(buf), "%.1f", y_min / 10.0f);
        lv_label_set_text(temp_y_bot, buf);
    }

    /* 动态调整 Humi Y 轴范围 (考虑今天+昨天) */
    if (data_count >= 1) {
        float hmin, hmax;
        findMinMax(humi_data, data_count, &hmin, &hmax);
        if (yesterday_count > 0) {
            for (int i = 0; i < yesterday_count; i++) {
                if (yesterday_data[i].humi < hmin) hmin = yesterday_data[i].humi;
                if (yesterday_data[i].humi > hmax) hmax = yesterday_data[i].humi;
            }
        }
        lv_coord_t y_min, y_max;
        float range = hmax - hmin;
        if (range < 2.0f) {
            y_min = (lv_coord_t)((hmin - 0.5f) * 10);
            y_max = (lv_coord_t)((hmax + 0.5f) * 10);
        } else if (range < 5.0f) {
            y_min = (lv_coord_t)((hmin - 1) * 10);
            y_max = (lv_coord_t)((hmax + 1) * 10);
        } else {
            y_min = (lv_coord_t)((hmin - 2) * 10);
            y_max = (lv_coord_t)((hmax + 2) * 10);
        }
        lv_chart_set_range(humi_chart, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", y_max / 10.0f);
        lv_label_set_text(humi_y_top, buf);
        snprintf(buf, sizeof(buf), "%.1f", y_min / 10.0f);
        lv_label_set_text(humi_y_bot, buf);
    }

    /* 动态调整 Baro Y 轴范围 (考虑今天+昨天) */
    if (data_count >= 1) {
        float bmin, bmax;
        findMinMax(baro_data, data_count, &bmin, &bmax);
        if (yesterday_count > 0) {
            for (int i = 0; i < yesterday_count; i++) {
                if (yesterday_data[i].baro < bmin) bmin = yesterday_data[i].baro;
                if (yesterday_data[i].baro > bmax) bmax = yesterday_data[i].baro;
            }
        }
        lv_coord_t y_min = (lv_coord_t)((bmin - 1) * 10);
        lv_coord_t y_max = (lv_coord_t)((bmax + 1) * 10);
        if (y_max - y_min < 20) y_max = y_min + 20;
        lv_chart_set_range(baro_chart, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", y_max / 10.0f);
        lv_label_set_text(baro_y_top, buf);
        snprintf(buf, sizeof(buf), "%.1f", y_min / 10.0f);
        lv_label_set_text(baro_y_bot, buf);
    }

    lv_chart_refresh(temp_chart);
    lv_chart_refresh(humi_chart);
    lv_chart_refresh(baro_chart);

    /* 更新标签: 当前值, 最大值, 最小值 */
    snprintf(buf, sizeof(buf), "Current: %.1f", temp);
    lv_label_set_text(temp_chart_label, buf);
    float tmin, tmax;
    findMinMax(temp_data, data_count, &tmin, &tmax);
    snprintf(buf, sizeof(buf), "Max: %.1f", tmax);
    lv_label_set_text(temp_max_label, buf);
    snprintf(buf, sizeof(buf), "Min: %.1f", tmin);
    lv_label_set_text(temp_min_label, buf);

    snprintf(buf, sizeof(buf), "Current: %.1f", humi);
    lv_label_set_text(humi_chart_label, buf);
    float hmin, hmax;
    findMinMax(humi_data, data_count, &hmin, &hmax);
    snprintf(buf, sizeof(buf), "Max: %.1f", hmax);
    lv_label_set_text(humi_max_label, buf);
    snprintf(buf, sizeof(buf), "Min: %.1f", hmin);
    lv_label_set_text(humi_min_label, buf);

    snprintf(buf, sizeof(buf), "Current: %.1f", baro);
    lv_label_set_text(baro_chart_label, buf);
    float bmin, bmax;
    findMinMax(baro_data, data_count, &bmin, &bmax);
    snprintf(buf, sizeof(buf), "Max: %.1f", bmax);
    lv_label_set_text(baro_max_label, buf);
    snprintf(buf, sizeof(buf), "Min: %.1f", bmin);
    lv_label_set_text(baro_min_label, buf);
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
    rx_count++;
    char msg[512];
    unsigned int copyLen = length < sizeof(msg) - 1 ? length : sizeof(msg) - 1;
    memcpy(msg, payload, copyLen);
    msg[copyLen] = '\0';
    Serial.printf("MQTT [%s]: %s\n", topic, msg);

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, msg);
    if (err) { Serial.printf("JSON parse error: %s\n", err.c_str()); return; }

    float temp = doc["temp"] | 0.0f;
    float humi = doc["humi"] | 0.0f;
    float baro = doc["baro"] | 0.0f;
    const char *ts = doc["time_stamp"] | "";

    updateDashboard(temp, humi, baro, ts);
    updateChart(temp, humi, baro);
    saveToSD(temp, humi, baro, ts);
    csv_record_count++;

    // 首次收到数据时，加载昨天历史数据
    if (!history_loaded && strlen(ts) > 0) {
        history_loaded = true;
        char today[12], yesterday[12];
        extractDate(ts, today, sizeof(today));
        dateMinusOneDay(today, yesterday, sizeof(yesterday));

        yesterday_count = loadEntriesByDateRange(yesterday, today,
                                                 yesterday_data, CHART_POINT_NUM);
        if (yesterday_count > 0) {
            // 绘制灰色折线 (分钟数映射到0-49索引)
            for (int i = 0; i < yesterday_count; i++) {
                int idx = yesterday_data[i].minute_of_day * CHART_POINT_NUM / 1440;
                if (idx >= CHART_POINT_NUM) idx = CHART_POINT_NUM - 1;
                lv_chart_set_value_by_id(temp_chart, temp_series_yesterday, idx,
                                         (lv_coord_t)(yesterday_data[i].temp * 10));
                lv_chart_set_value_by_id(humi_chart, humi_series_yesterday, idx,
                                         (lv_coord_t)(yesterday_data[i].humi * 10));
                lv_chart_set_value_by_id(baro_chart, baro_series_yesterday, idx,
                                         (lv_coord_t)(yesterday_data[i].baro * 10));
            }
            lv_chart_refresh(temp_chart);
            lv_chart_refresh(humi_chart);
            lv_chart_refresh(baro_chart);

            // Y轴取两天共同范围
            float tmin, tmax, hmin, hmax, bmin, bmax;
            findMinMax(temp_data, data_count, &tmin, &tmax);
            findMinMax(humi_data, data_count, &hmin, &hmax);
            findMinMax(baro_data, data_count, &bmin, &bmax);
            for (int i = 0; i < yesterday_count; i++) {
                if (yesterday_data[i].temp < tmin) tmin = yesterday_data[i].temp;
                if (yesterday_data[i].temp > tmax) tmax = yesterday_data[i].temp;
                if (yesterday_data[i].humi < hmin) hmin = yesterday_data[i].humi;
                if (yesterday_data[i].humi > hmax) hmax = yesterday_data[i].humi;
                if (yesterday_data[i].baro < bmin) bmin = yesterday_data[i].baro;
                if (yesterday_data[i].baro > bmax) bmax = yesterday_data[i].baro;
            }
            lv_chart_set_range(temp_chart, LV_CHART_AXIS_PRIMARY_Y,
                               (lv_coord_t)((tmin - 1) * 10), (lv_coord_t)((tmax + 1) * 10));
            lv_chart_set_range(humi_chart, LV_CHART_AXIS_PRIMARY_Y,
                               (lv_coord_t)((hmin - 1) * 10), (lv_coord_t)((hmax + 1) * 10));
            lv_chart_set_range(baro_chart, LV_CHART_AXIS_PRIMARY_Y,
                               (lv_coord_t)((bmin - 1) * 10), (lv_coord_t)((bmax + 1) * 10));

            Serial.printf("Yesterday history loaded: %d points\n", yesterday_count);
        }
    }
}

bool connectWiFi(bool force = false) {
    if (WiFi.status() == WL_CONNECTED) return true;

    unsigned long now = millis();
    if (!wifi_scan_running) {
        if (!force && now - last_wifi_attempt < WIFI_RETRY_MS) return false;
        last_wifi_attempt = now;
        setTitleConnected(false);
        Serial.println("Scanning WiFi networks...");
        WiFi.scanNetworks(true);
        wifi_scan_running = true;
        return false;
    }

    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return false;
    wifi_scan_running = false;

    if (n < 0) {
        Serial.println("WiFi scan failed");
        WiFi.scanDelete();
        return false;
    }

    Serial.printf("Found %d networks\n", n);
    for (int i = 0; i < WIFI_COUNT; i++) {
        for (int j = 0; j < n; j++) {
            if (WiFi.SSID(j) == WIFI_LIST[i].ssid) {
                Serial.printf("Connecting to: %s\n", WIFI_LIST[i].ssid);
                WiFi.begin(WIFI_LIST[i].ssid, WIFI_LIST[i].password);
                unsigned long start = millis();
                while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
                    lv_timer_handler();
                    checkButton();
                    delay(20);
                }
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
                    WiFi.scanDelete();
                    return true;
                }
                Serial.println("Failed, trying next...");
                WiFi.disconnect();
            }
        }
    }

    WiFi.scanDelete();
    Serial.println("No available WiFi found");
    return false;
}

bool connectMQTT(bool force = false) {
    if (mqttClient.connected()) return true;
    if (WiFi.status() != WL_CONNECTED) return false;

    unsigned long now = millis();
    if (!force && now - last_mqtt_attempt < MQTT_RETRY_MS) return false;
    last_mqtt_attempt = now;

    if (!mqtt_configured) {
        espClient.setInsecure();
        mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
        mqttClient.setCallback(mqttCallback);
        mqttClient.setBufferSize(1024);
        mqtt_configured = true;
    }

    Serial.print("Connecting to MQTT...");
    setTitleConnected(false);
    String clientId = "esp32c6-" + String(random(10000));
    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
        Serial.println("connected");
        mqttClient.subscribe(MQTT_TOPIC);
        mqttClient.publish(MQTT_STATUS, "online");
        setTitleConnected(true);
        return true;
    }

    Serial.printf("failed (rc=%d)\n", mqttClient.state());
    return false;
}

void setup() {
    Serial.begin(115200);
    randomSeed(analogRead(0));

    LCD_Init();
    Set_Backlight(80);
    Lvgl_Init();

    createDashboard();
    createSensorScreen(&temp_scr, "Temperature (\xC2\xB0" "C)",
                        lv_color_hex(0xFF6B6B), 0, 50,
                        "Current: --.-",
                        &temp_series, &temp_chart, &temp_chart_label,
                        &temp_max_label, &temp_min_label,
                        &temp_y_top, &temp_y_mid, &temp_y_bot,
                        &temp_series_yesterday);
    createSensorScreen(&humi_scr, "Humidity (%)",
                        lv_color_hex(0x6BCB77), 0, 100,
                        "Current: --.-",
                        &humi_series, &humi_chart, &humi_chart_label,
                        &humi_max_label, &humi_min_label,
                        &humi_y_top, &humi_y_mid, &humi_y_bot,
                        &humi_series_yesterday);
    createSensorScreen(&baro_scr, "Pressure (hPa)",
                        lv_color_hex(0x4ECDC4), 950, 1050,
                        "Current: ----.-",
                        &baro_series, &baro_chart, &baro_chart_label,
                        &baro_max_label, &baro_min_label,
                        &baro_y_top, &baro_y_mid, &baro_y_bot,
                        &baro_series_yesterday);
    createSysinfoScreen();
    lv_scr_load(dashboard_scr);
    lv_timer_handler();

    // BOOT 按键配置 (轮询模式，不用中断)
    pinMode(BOOT_BTN, INPUT_PULLUP);

    // SD 卡初始化
    if (SD_Init()) {
        ensureCSVHeader();
        csv_record_count = countCSVRecords();
        Serial.printf("SD card ready, CSV records: %d\n", csv_record_count);

        // 加载历史数据填充折线图
        float t[HISTORY_NUM], h[HISTORY_NUM], b[HISTORY_NUM];
        int loaded = 0;
        loadLastEntriesV2(t, h, b, &loaded, HISTORY_NUM);

        if (loaded > 0) {
            Serial.printf("Loading %d historical entries into charts\n", loaded);
            for (int i = 0; i < loaded; i++) {
                temp_data[data_index] = t[i];
                humi_data[data_index] = h[i];
                baro_data[data_index] = b[i];

                lv_chart_set_next_value(temp_chart, temp_series, (lv_coord_t)(t[i] * 10));
                lv_chart_set_next_value(humi_chart, humi_series, (lv_coord_t)(h[i] * 10));
                lv_chart_set_next_value(baro_chart, baro_series, (lv_coord_t)(b[i] * 10));

                data_index = (data_index + 1) % CHART_POINT_NUM;
                if (data_count < CHART_POINT_NUM) data_count++;
            }

            // 刷新图表
            lv_chart_refresh(temp_chart);
            lv_chart_refresh(humi_chart);
            lv_chart_refresh(baro_chart);

            // 更新 Y 轴和标签
            if (data_count >= 2) {
                float tmin, tmax, hmin, hmax, bmin, bmax;
                findMinMax(temp_data, data_count, &tmin, &tmax);
                findMinMax(humi_data, data_count, &hmin, &hmax);
                findMinMax(baro_data, data_count, &bmin, &bmax);

                // 温度 Y 轴
                lv_coord_t ty_min = (lv_coord_t)((tmin - 1) * 10);
                lv_coord_t ty_max = (lv_coord_t)((tmax + 1) * 10);
                lv_chart_set_range(temp_chart, LV_CHART_AXIS_PRIMARY_Y, ty_min, ty_max);
                char buf[16];
                snprintf(buf, sizeof(buf), "%.1f", ty_max / 10.0f);
                lv_label_set_text(temp_y_top, buf);
                snprintf(buf, sizeof(buf), "%.1f", ty_min / 10.0f);
                lv_label_set_text(temp_y_bot, buf);
                snprintf(buf, sizeof(buf), "Current: %.1f", temp_data[(data_index - 1 + CHART_POINT_NUM) % CHART_POINT_NUM]);
                lv_label_set_text(temp_chart_label, buf);
                snprintf(buf, sizeof(buf), "Max: %.1f", tmax);
                lv_label_set_text(temp_max_label, buf);
                snprintf(buf, sizeof(buf), "Min: %.1f", tmin);
                lv_label_set_text(temp_min_label, buf);

                // 湿度 Y 轴
                lv_coord_t hy_min = (lv_coord_t)((hmin - 1) * 10);
                lv_coord_t hy_max = (lv_coord_t)((hmax + 1) * 10);
                lv_chart_set_range(humi_chart, LV_CHART_AXIS_PRIMARY_Y, hy_min, hy_max);
                snprintf(buf, sizeof(buf), "%.1f", hy_max / 10.0f);
                lv_label_set_text(humi_y_top, buf);
                snprintf(buf, sizeof(buf), "%.1f", hy_min / 10.0f);
                lv_label_set_text(humi_y_bot, buf);
                snprintf(buf, sizeof(buf), "Current: %.1f", humi_data[(data_index - 1 + CHART_POINT_NUM) % CHART_POINT_NUM]);
                lv_label_set_text(humi_chart_label, buf);
                snprintf(buf, sizeof(buf), "Max: %.1f", hmax);
                lv_label_set_text(humi_max_label, buf);
                snprintf(buf, sizeof(buf), "Min: %.1f", hmin);
                lv_label_set_text(humi_min_label, buf);

                // 气压 Y 轴
                lv_coord_t by_min = (lv_coord_t)((bmin - 1) * 10);
                lv_coord_t by_max = (lv_coord_t)((bmax + 1) * 10);
                lv_chart_set_range(baro_chart, LV_CHART_AXIS_PRIMARY_Y, by_min, by_max);
                snprintf(buf, sizeof(buf), "%.1f", by_max / 10.0f);
                lv_label_set_text(baro_y_top, buf);
                snprintf(buf, sizeof(buf), "%.1f", by_min / 10.0f);
                lv_label_set_text(baro_y_bot, buf);
                snprintf(buf, sizeof(buf), "Current: %.1f", baro_data[(data_index - 1 + CHART_POINT_NUM) % CHART_POINT_NUM]);
                lv_label_set_text(baro_chart_label, buf);
                snprintf(buf, sizeof(buf), "Max: %.1f", bmax);
                lv_label_set_text(baro_max_label, buf);
                snprintf(buf, sizeof(buf), "Min: %.1f", bmin);
                lv_label_set_text(baro_min_label, buf);
            }
        }
    }

    connectWiFi(true);
    connectMQTT(true);
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        setTitleConnected(false);
        connectWiFi();
    } else {
        connectMQTT();
        mqttClient.loop();
    }
    checkButton();       // 按键轮询
    // 自动切换
    if (millis() - switch_timer >= AUTO_SWITCH_MS) {
        switch_timer = millis();
        switchScreen();
    }
    if (current_screen == 4) updateSysinfo();  // 系统信息界面实时更新
    lv_timer_handler();
    delay(5);
}
