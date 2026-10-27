#ifndef DEBUG_CONFIG_H
#define DEBUG_CONFIG_H
#include <Arduino.h>

// =================================================================
// 1. MASTER SWITCH (CÔNG TẮC TỔNG)
// Để số 1 là cho phép in log toàn hệ thống.
// Khi nào đem mạch đi lắp thực tế, đổi thành số 0 là tắt sạch sành sanh.
// =================================================================
#define ENABLE_ALL_DEBUG 1

// =================================================================
// 2. MODULE SWITCHES (CÔNG TẮC TỪNG FILE)
// 1 = Bật in log cho module đó | 0 = Tắt in log module đó
// =================================================================
#define DEBUG_SIM7600 1
#define DEBUG_LORA 1
#define DEBUG_MAIN 1

// Enable only after confirming SIM7600E RESET polarity on the assembled PCB.
#define ENABLE_SIM_HARD_RESET 0
// =================================================================
// 3. ĐỊNH NGHĨA MACRO (PHÉP MÀU NẰM Ở ĐÂY)
// Sử dụng __VA_ARGS__ để hỗ trợ truyền nhiều tham số (VD: in số HEX, format)
// =================================================================

// --- MACRO CHO SIM7600 ---
#if (ENABLE_ALL_DEBUG && DEBUG_SIM7600)
#define SIM_PRINT(...) Serial.print(__VA_ARGS__)
#define SIM_PRINTLN(...) Serial.println(__VA_ARGS__)
#define SIM_PRINTF(...) Serial.printf(__VA_ARGS__)
#define SIM_WRITE(...) Serial.write(__VA_ARGS__)
#else
#define SIM_PRINT(...)
#define SIM_PRINTLN(...)
#define SIM_PRINTF(...)
#define SIM_WRITE(...)
#endif

// --- MACRO CHO LORA ---
#if (ENABLE_ALL_DEBUG && DEBUG_LORA)
#define LORA_PRINT(...) Serial.print(__VA_ARGS__)
#define LORA_PRINTLN(...) Serial.println(__VA_ARGS__)
#define LORA_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define LORA_PRINT(...)
#define LORA_PRINTLN(...)
#define LORA_PRINTF(...)
#endif
// --- MACRO CHO CHUONGTRINHCHINH ---
#if (ENABLE_ALL_DEBUG && DEBUG_MAIN)
#define MAIN_PRINT(...) Serial.print(__VA_ARGS__)
#define MAIN_PRINTLN(...) Serial.println(__VA_ARGS__)
#define MAIN_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define MAIN_PRINT(...)
#define MAIN_PRINTLN(...)
#define MAIN_PRINTF(...)
#endif
#endif  // DEBUG_CONFIG_H