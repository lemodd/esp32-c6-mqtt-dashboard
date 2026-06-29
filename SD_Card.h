#pragma once
#include <Arduino.h>
#include <SD.h>
#include <FS.h>

#define SD_CS 4
#define CHART_POINT_NUM 50

bool sd_ready = false;
static char last_saved_ts[32] = "";  // 上次保存的时间戳

/*============================================================================
 *  SD 卡初始化
 *============================================================================*/
bool SD_Init() {
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    
    if (!SD.begin(SD_CS)) {
        Serial.println("SD card init failed!");
        sd_ready = false;
        return false;
    }
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        sd_ready = false;
        return false;
    }
    
    uint64_t totalBytes = SD.totalBytes();
    uint64_t usedBytes = SD.usedBytes();
    Serial.printf("SD Card: %llu MB total, %llu MB used\n", 
                  totalBytes / (1024 * 1024), usedBytes / (1024 * 1024));
    sd_ready = true;
    return true;
}

/*============================================================================
 *  确保 CSV 文件有表头
 *============================================================================*/
void ensureCSVHeader() {
    if (!sd_ready) return;
    
    File file = SD.open("/sensor_data.csv", FILE_READ);
    if (!file || file.size() == 0) {
        if (file) file.close();
        file = SD.open("/sensor_data.csv", FILE_WRITE);
        if (file) {
            file.println("timestamp,temp,humi,baro");
            file.close();
            Serial.println("CSV header written");
        }
    } else {
        file.close();
    }
}

/*============================================================================
 *  保存数据到 SD 卡 (CSV 格式追加，去重)
 *============================================================================*/
void saveToSD(float temp, float humi, float baro, const char *timestamp) {
    if (!sd_ready) return;
    
    // 时间戳去重: 相同则跳过
    if (strlen(last_saved_ts) > 0 && strcmp(last_saved_ts, timestamp) == 0) {
        return;
    }
    strncpy(last_saved_ts, timestamp, sizeof(last_saved_ts) - 1);
    
    File file = SD.open("/sensor_data.csv", FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open CSV file");
        return;
    }
    
    file.printf("%s,%.2f,%.1f,%.2f\n", timestamp, temp, humi, baro);
    file.close();
}

/*============================================================================
 *  从 CSV 文件末尾读取最近 N 条数据
 *  
 *  参数:
 *    temps, humis, baros - 输出数组
 *    count               - 输出实际读取的条数
 *    max_count           - 最多读取条数
 *============================================================================*/
void loadLastEntries(float *temps, float *humis, float *baros, int *count, int max_count) {
    *count = 0;
    if (!sd_ready) return;
    
    File file = SD.open("/sensor_data.csv", FILE_READ);
    if (!file) {
        Serial.println("Failed to open CSV for reading");
        return;
    }
    
    // 跳过表头
    if (file.available()) {
        file.readStringUntil('\n');
    }
    
    // 读取所有行到临时缓冲区，保留最后 max_count 条
    int total = 0;
    char lines[max_count][128];
    
    while (file.available() && total < max_count) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            strncpy(lines[total], line.c_str(), 127);
            lines[total][127] = '\0';
            total++;
        }
    }
    
    // 如果文件行数超过 max_count，需要只保留最后 max_count 条
    // 但上面的逻辑已经限制了 total <= max_count
    
    file.close();
    
    // 解析并填充数组 (从旧到新)
    *count = total;
    for (int i = 0; i < total; i++) {
        // CSV: timestamp,temp,humi,baro
        char *p = strchr(lines[i], ',');
        if (!p) continue;
        p++;  // 跳过 timestamp 后的逗号
        temps[i] = atof(p);
        
        p = strchr(p, ',');
        if (!p) continue;
        p++;
        humis[i] = atof(p);
        
        p = strchr(p, ',');
        if (!p) continue;
        p++;
        baros[i] = atof(p);
    }
    
    Serial.printf("Loaded %d entries from SD card\n", *count);
}

/*============================================================================
 *  从 CSV 文件末尾读取最近 N 条数据 (改进版，支持超长文件)
 *  
 *  从文件末尾向前读取，只保留最后 max_count 条
 *============================================================================*/
void loadLastEntriesV2(float *temps, float *humis, float *baros, int *count, int max_count) {
    *count = 0;
    if (!sd_ready) return;
    
    File file = SD.open("/sensor_data.csv", FILE_READ);
    if (!file) {
        Serial.println("Failed to open CSV for reading");
        return;
    }
    
    // 跳过表头 (第一行)
    if (file.available()) {
        file.readStringUntil('\n');
    }
    
    // 第一遍: 统计总行数
    int total_lines = 0;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        if (line.length() > 2) total_lines++;  // 行头"ts,"至少3字符
    }
    
    // 计算需要跳过的行数
    int skip = total_lines - max_count;
    if (skip < 0) skip = 0;
    
    // 第二遍: 从头开始跳过，读取最后 max_count 条
    file.seek(0);
    file.readStringUntil('\n');  // 跳过表头
    
    int idx = 0;
    int read_count = 0;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() < 3) continue;
        
        if (read_count >= skip && idx < max_count) {
            // CSV: timestamp,temp,humi,baro
            char buf[128];
            strncpy(buf, line.c_str(), 127);
            buf[127] = '\0';
            
            char *p = strchr(buf, ',');
            if (!p) continue;
            p++;
            temps[idx] = atof(p);
            
            p = strchr(p, ',');
            if (!p) continue;
            p++;
            humis[idx] = atof(p);
            
            p = strchr(p, ',');
            if (!p) continue;
            p++;
            baros[idx] = atof(p);
            
            idx++;
        }
        read_count++;
    }
    
    file.close();
    *count = idx;
    
    Serial.printf("Loaded %d entries from SD card (total: %d)\n", idx, total_lines);
}

/*============================================================================
 *  统计 CSV 文件中的数据行数 (不含表头)
 *============================================================================*/
int countCSVRecords() {
    if (!sd_ready) return 0;

    File file = SD.open("/sensor_data.csv", FILE_READ);
    if (!file) return 0;

    // 跳过表头
    if (file.available()) {
        file.readStringUntil('\n');
    }

    int count = 0;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        if (line.length() > 2) count++;
    }
    file.close();
    return count;
}

/*============================================================================
 *  日期/时间辅助函数
 *============================================================================*/
typedef struct {
    float temp, humi, baro;
    int minute_of_day;  // 0-1439
} SensorEntry;

void extractDate(const char *ts, char *date_out, int size) {
    // "2026-06-26_14:30:00" → "2026-06-26"
    const char *p = strchr(ts, '_');
    if (p) {
        int len = p - ts;
        if (len >= size) len = size - 1;
        strncpy(date_out, ts, len);
        date_out[len] = '\0';
    } else {
        strncpy(date_out, ts, size - 1);
        date_out[size - 1] = '\0';
    }
}

int extractMinuteOfDay(const char *ts) {
    // "2026-06-26_14:30:00" → 14*60+30 = 870
    const char *p = strchr(ts, '_');
    if (!p) return 0;
    p++;
    int h = atoi(p);
    const char *m = strchr(p, ':');
    if (m) m++;
    int min = atoi(m);
    return h * 60 + min;
}

void dateMinusOneDay(const char *date, char *result, int size) {
    // "2026-06-26" → "2026-06-25"
    int y, mo, d;
    sscanf(date, "%d-%d-%d", &y, &mo, &d);
    d--;
    if (d < 1) {
        mo--;
        if (mo < 1) { mo = 12; y--; }
        // 每月天数 (简化，忽略闰年2月)
        static const int days_in_month[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
        d = days_in_month[mo];
        if (mo == 2 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) d = 29;
    }
    snprintf(result, size, "%04d-%02d-%02d", y, mo, d);
}

/*============================================================================
 *  按日期范围加载数据 (用于历史对比)
 *  date_start: "2026-06-25", date_end: "2026-06-26"
 *  筛选 timestamp >= date_start 且 < date_end
 *============================================================================*/
int loadEntriesByDateRange(const char *date_start, const char *date_end,
                           SensorEntry *entries, int max_count) {
    int count = 0;
    if (!sd_ready) return 0;

    File file = SD.open("/sensor_data.csv", FILE_READ);
    if (!file) return 0;

    // 跳过表头
    if (file.available()) {
        file.readStringUntil('\n');
    }

    int start_len = strlen(date_start);
    int end_len = strlen(date_end);

    while (file.available() && count < max_count) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() < 10) continue;

        char buf[128];
        strncpy(buf, line.c_str(), 127);
        buf[127] = '\0';

        // 提取时间戳 (第一个逗号前的部分)
        char *comma = strchr(buf, ',');
        if (!comma) continue;
        *comma = '\0';
        char *ts = buf;

        // 日期范围比较 (字符串比较即可，格式为 YYYY-MM-DD)
        if (strncmp(ts, date_start, start_len) < 0) continue;
        if (strncmp(ts, date_end, end_len) >= 0) continue;

        // 解析数据
        char *p = comma + 1;
        entries[count].temp = atof(p);

        p = strchr(p, ',');
        if (!p) continue;
        p++;
        entries[count].humi = atof(p);

        p = strchr(p, ',');
        if (!p) continue;
        p++;
        entries[count].baro = atof(p);

        entries[count].minute_of_day = extractMinuteOfDay(ts);
        count++;
    }

    file.close();
    Serial.printf("Loaded %d entries for date range [%s, %s)\n", count, date_start, date_end);
    return count;
}
