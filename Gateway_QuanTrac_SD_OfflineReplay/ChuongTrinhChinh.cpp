#define TINY_GSM_MODEM_SIM7600
#include "ChuongTrinhChinh.h"
#include "Config.h"
#include "ConfigFireBase.h"
#include "Lora_E32.h"
#include "SIM7600E.h"
#include "RTC.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static const uint32_t LORA_POLL_MS = 5;
static const uint32_t NETWORK_RETRY_MS = 30000;
static const uint32_t STATUS_PERIOD_MS = 45000;
static const uint32_t HTTP_MUTEX_TIMEOUT_MS = 15000;
static const uint32_t PERMANENT_HTTP_RETRY_MS = 60000;
static const uint32_t CHART_PERIOD_MS = 15UL * 60UL * 1000UL;

struct NetworkState {
  volatile bool connected = false;
  volatile uint8_t consecutiveHttpErrors = 0;
  volatile int rssi = 0;
};

LoRa lora;
SIM7600 modem;
RTC rtc;

SemaphoreHandle_t httpMutex = nullptr;
SemaphoreHandle_t rtcMutex = nullptr;
SemaphoreHandle_t sensorMutex = nullptr;

NetworkState network;

volatile bool haveSensorData = false;
MyData latestSensorData{};

uint32_t lastReconnectAttempt = 0;
uint32_t lastStatusPost = 0;
uint32_t lastChartPost = 0;

String latestTimestamp;

uint8_t latestNodeID = 0;
volatile bool permanentHttpError = false;
uint32_t lastTelemetryMillis = 0;
uint32_t sensorGeneration = 0;
uint8_t consecutiveNetworkConnectFailures = 0;
uint32_t lastLoRaStackReport = 0;

bool httpRequest(bool isPost, const char *url, const String &payload, String &response) {
  if (!network.connected) return false;
  if (httpMutex == nullptr || xSemaphoreTake(httpMutex, pdMS_TO_TICKS(HTTP_MUTEX_TIMEOUT_MS)) != pdTRUE) {
    MAIN_PRINTLN("[HTTP] Khong lay duoc mutex modem.");
    return false;
  }

  response = isPost ? modem.httpsPost(String(url), payload) : modem.httpsGet(String(url));
  const int status = modem.lastHttpStatusCode;
  xSemaphoreGive(httpMutex);

  MAIN_PRINTF("[HTTP] %s status=%d\n", isPost ? "POST" : "GET", status);
  if (status >= 200 && status < 300) {
    network.consecutiveHttpErrors = 0;
    return true;
  }
  // 4xx usually indicates a Firebase URL, permission, or payload problem.
  if (status >= 400 && status < 500) {
    if (strstr(url, "/History/") != nullptr || strstr(url, "/Sensors.json") != nullptr) {
      permanentHttpError = (status == 400 || status == 401 || status == 403 || status == 404);
    }
    MAIN_PRINTF("[HTTP] Loi HTTP %d; kiem tra Firebase URL/quyen/payload.\n", status);
    return false;
  }
  if (++network.consecutiveHttpErrors >= MAX_NETWORK_ERRORS) {
    network.connected = false;
    MAIN_PRINTLN("[NET] Loi mang/HTTP lien tiep; se thu ket noi lai.");
  }
  return false;
}

String sensorValueString(const MyData &d) {
  char value[128];
  snprintf(value, sizeof(value), "%d;%d;%u;%u;%u;%u;%u;%u;%u;%u",
           d.temperature, d.humidity,
           d.sensor2[0], d.sensor2[1], d.sensor2[2], d.sensor2[3],
           d.sensor2[4], d.sensor2[5], d.sensor2[6], d.Vbat);
  return String(value);
}

bool validCalendarDate(int year, int month, int day) {
  if (year < 2024 || year > 2099 || month < 1 || month > 12 || day < 1) return false;
  const int daysPerMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int maxDay = daysPerMonth[month - 1];
  const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
  if (month == 2 && leap) maxDay = 29;
  return day <= maxDay;
}

String currentTimestamp() {
  if (rtcMutex == nullptr || xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(1000)) != pdTRUE) return String();
  rtc.LayRTCTuDS3231();
  const bool valid = validCalendarDate(rtc.dt.Nam, rtc.dt.Thang, rtc.dt.Ngay) &&
    rtc.dt.Gio >= 0 && rtc.dt.Gio <= 23 && rtc.dt.Phut >= 0 && rtc.dt.Phut <= 59 &&
    rtc.dt.Giay >= 0 && rtc.dt.Giay <= 59;
  char value[24] = {0};
  if (valid) snprintf(value, sizeof(value), "%04d-%02d-%02dT%02d:%02d:%02d",
                      rtc.dt.Nam, rtc.dt.Thang, rtc.dt.Ngay,
                      rtc.dt.Gio, rtc.dt.Phut, rtc.dt.Giay);
  xSemaphoreGive(rtcMutex);
  return valid ? String(value) : String();
}

String makeHistoryKey(const MyData &d, const String &timestamp) {
  // Firebase key is deterministic so a reboot/retry overwrites the same record,
  // instead of creating duplicates. Samples are expected at most once/second/node.
  String key = String("n") + String((unsigned)d.nodeID) + "_" + timestamp;
  key.replace("-", "");
  key.replace(":", "");
  key.replace("T", "_");
  return key;
}

bool uploadLiveRecord(const MyData &d, const String &timestamp) {
  const String value = sensorValueString(d);
  const bool timeValid = timestamp.length() == 19 && !timestamp.startsWith("UNSYNCED_");
  char historyPayload[256];

  if (timeValid) {
    snprintf(historyPayload, sizeof(historyPayload),
      "{\"timestamp\":\"%s\",\"timeValid\":true,\"nodeID\":%u,\"R\":\"%s\"}",
      timestamp.c_str(), (unsigned)d.nodeID, value.c_str());
  } else {
    snprintf(historyPayload, sizeof(historyPayload),
      "{\"timestamp\":null,\"timeValid\":false,\"nodeID\":%u,\"R\":\"%s\"}",
      (unsigned)d.nodeID, value.c_str());
  }

  String historyKey = timeValid ? makeHistoryKey(d, timestamp)
      : String("n") + String((unsigned)d.nodeID) + "_u" + String(millis());
      
  String historyUrl = String(BASE_URL) + "/History/" + historyKey + ".json";
  String response;

  // Write the latest sensor snapshot first so the dashboard updates promptly.
  const String latestPayload = String("{\"R\":\"") + value + "\"}";
  if (!httpRequest(true, URL_SENSORS_PUT, latestPayload, response)) {
    MAIN_PRINTLN("[UPLOAD] Ghi /Sensors that bai; se thu lai mau hien tai.");
    return false;
  }

  if (!httpRequest(true, historyUrl.c_str(), String(historyPayload), response)) {
    MAIN_PRINTLN("[UPLOAD] Ghi /History that bai; se thu lai mau hien tai.");
    return false;
  }

  MAIN_PRINTF("[UPLOAD] Firebase OK: node=%u timestamp=%s key=%s\n",
              (unsigned)d.nodeID, timestamp.c_str(), historyKey.c_str());
  permanentHttpError = false;
  return true;
}

void postGatewayStatus() {

  if (!network.connected) return;
  bool gotNetworkTime = false;

  if (xSemaphoreTake(httpMutex, pdMS_TO_TICKS(HTTP_MUTEX_TIMEOUT_MS)) == pdTRUE) {
    gotNetworkTime = modem.getNetworkTime(modem.Time);
    network.rssi = modem.getSignalQuality();
    xSemaphoreGive(httpMutex);
  }
  
  if (rtcMutex == nullptr || xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(1000)) != pdTRUE) return;

  if (gotNetworkTime && modem.Time.Nam >= 24 && modem.Time.Nam <= 99 &&
      modem.Time.Thang >= 1 && modem.Time.Thang <= 12 && modem.Time.Ngay >= 1 && modem.Time.Ngay <= 31 &&
      modem.Time.Gio >= 0 && modem.Time.Gio <= 23 && modem.Time.Phut >= 0 && modem.Time.Phut <= 59 &&
      modem.Time.Giay >= 0 && modem.Time.Giay <= 59) {

    rtc.SoSanhSaiLechRTCDeCapNhatDS3231Tu4G(
      modem.Time.Thu, modem.Time.Ngay, modem.Time.Thang, modem.Time.Nam,
      modem.Time.Gio, modem.Time.Phut, modem.Time.Giay);
  } else if (gotNetworkTime) {
    MAIN_PRINTLN("[RTC] Thoi gian SIM khong hop le; bo qua dong bo RTC.");
  }

  rtc.LayRTCTuDS3231();

  const bool rtcTimeValid = validCalendarDate(rtc.dt.Nam, rtc.dt.Thang, rtc.dt.Ngay) &&
      rtc.dt.Gio >= 0 && rtc.dt.Gio <= 23 && rtc.dt.Phut >= 0 && rtc.dt.Phut <= 59 &&
      rtc.dt.Giay >= 0 && rtc.dt.Giay <= 59;
  String timestamp = rtcTimeValid ? rtc.ChuanHoaChuoiRTCDeGuiVeServer() : String();
  xSemaphoreGive(rtcMutex);

  if (!rtcTimeValid) {
    MAIN_PRINTLN("[RTC] Bo qua /Status vi DS3231 chua co gio hop le.");
    return;
  }
  char payload[100];
  snprintf(payload, sizeof(payload), "{\"S\":\"%s;%d\"}", timestamp.c_str(), network.rssi);
  String response;
  httpRequest(true, URL_STATUS_PUT, String(payload), response);
}


void postChartPoint() {
  if (rtcMutex == nullptr || xSemaphoreTake(rtcMutex, pdMS_TO_TICKS(1000)) != pdTRUE) return;
  rtc.LayRTCTuDS3231();
  const int chartDay = rtc.dt.Ngay, chartMonth = rtc.dt.Thang, chartYear = rtc.dt.Nam;
  const int chartHour = rtc.dt.Gio, chartMinute = rtc.dt.Phut;
  const bool chartTimeValid = validCalendarDate(chartYear, chartMonth, chartDay) &&
      chartHour >= 0 && chartHour <= 23 && chartMinute >= 0 && chartMinute <= 59;
  xSemaphoreGive(rtcMutex);
  if (!chartTimeValid) {
    MAIN_PRINTLN("[RTC] Bo qua /Chart vi DS3231 chua co gio hop le.");
    return;
  }
  
  char url[320];
  snprintf(url, sizeof(url),
    BASE_URL "/Chart/%02d-%02d-%02d/%02d-%02d.json",
    chartDay, chartMonth, chartYear, chartHour, chartMinute);
  char payload[140];
  snprintf(payload, sizeof(payload), "\"%d;%d;%u;%u;%u;%u;%u;%u;%u\"",
    chartData.temperature, chartData.humidity,
    chartData.sensor2[0], chartData.sensor2[1], chartData.sensor2[2],
    chartData.sensor2[3], chartData.sensor2[4], chartData.sensor2[5],
    chartData.sensor2[6]);
  String response;
  httpRequest(true, url, String(payload), response);
}

void taskReceiveLoRa(void *parameter) {
  (void)parameter;
  uint8_t rxBuffer[128];
  for (;;) {
    if (millis() - lastLoRaStackReport >= 30000UL) {
      lastLoRaStackReport = millis();
      const UBaseType_t highWaterWords = uxTaskGetStackHighWaterMark(nullptr);
      MAIN_PRINTF("[STACK] LoRa_RX minimum free stack=%u bytes (ESP-IDF FreeRTOS units).\n",
                    (unsigned)highWaterWords);
    }
    
    const uint8_t length = lora.LoRa_Read_DataRaw(Serial1, rxBuffer);
    if (length > 0) {
      MyData incoming{};
      NodeOut_t ignoredOutput{};
      const LoRa_Parse_Response_t parsed = lora.LoRa_ParseData(rxBuffer, length, &incoming, &ignoredOutput);
      if (parsed == LORA_PARSE_NODE_IN) {
        const String timestamp = currentTimestamp();
        String recordTimestamp = timestamp;
        if (recordTimestamp.length() != 19) {
          // Keep the sample in RAM even if RTC has lost time; mark timestamp invalid.
          recordTimestamp = String("UNSYNCED_") + String(millis());
          MAIN_PRINTLN("[RTC] Thoi gian chua hop le; mau RAM se co timestamp UNSYNCED.");
        }
        {
          if (sensorMutex && xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            latestSensorData = incoming;
            haveSensorData = true;
            latestTimestamp = recordTimestamp;
            latestNodeID = incoming.nodeID;
            lastTelemetryMillis = millis();
            ++sensorGeneration;
            xSemaphoreGive(sensorMutex);
          }
          MAIN_PRINTF("[LORA RX] Da cap nhat mau trong RAM: Node %u, generation=%lu\n",
                      (unsigned)incoming.nodeID, (unsigned long)sensorGeneration);
        }
      } else if (parsed == LORA_PARSE_CRC_ERR) {
        MAIN_PRINTLN("[LORA] CRC sai; bo goi tin hien tai, khong xoa toan bo UART de tranh mat frame tiep theo.");
      } else if (parsed == LORA_PARSE_NODE_OUT) {
        // This gateway is receive-only for sensor telemetry. Output/control
        // packets are deliberately ignored because this project has no actuators.
      }
    }
    vTaskDelay(pdMS_TO_TICKS(LORA_POLL_MS));
  }
}

void taskFirebaseUpload(void *parameter) {
  (void)parameter;
  uint32_t uploadedGeneration = 0;
  for (;;) {
    if (!network.connected) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    if (permanentHttpError) {
      MAIN_PRINTLN("[UPLOAD] HTTP 4xx: kiem tra Firebase Rules, URL va payload; thu lai sau 60 giay.");
      vTaskDelay(pdMS_TO_TICKS(PERMANENT_HTTP_RETRY_MS));
      permanentHttpError = false;
      continue;
    }

    MyData data{};
    String timestamp;
    uint32_t generation = 0;
    bool hasNewSample = false;
    if (sensorMutex && xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      generation = sensorGeneration;
      hasNewSample = haveSensorData && generation != uploadedGeneration;
      if (hasNewSample) {
        data = latestSensorData;
        timestamp = latestTimestamp;
      }
      xSemaphoreGive(sensorMutex);
    }

    if (!hasNewSample) {
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }

    if (uploadLiveRecord(data, timestamp)) {
      uploadedGeneration = generation;
    } else {
      // No SD/offline queue: retain only the latest sample in RAM and retry it.
      vTaskDelay(pdMS_TO_TICKS(3000));
    }
  }
}

void taskStatusAndChart(void *parameter) {
  (void)parameter;
  uint32_t previousStatus = millis() - STATUS_PERIOD_MS; // sync RTC as soon as 4G is up
  uint32_t previousChart = millis();
  for (;;) {
    const uint32_t now = millis();
    if (network.connected && now - previousStatus >= STATUS_PERIOD_MS) {
      previousStatus = now;
      postGatewayStatus();
    }
    if (now - previousChart >= CHART_PERIOD_MS) {
      previousChart = now;
      postChartPoint();
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void reconnectNetworkIfNeeded() {

  if (network.connected || millis() - lastReconnectAttempt < NETWORK_RETRY_MS) return;

  lastReconnectAttempt = millis();
  MAIN_PRINTLN("[NET] Thu ket noi lai mang 4G...");

  if (httpMutex == nullptr || xSemaphoreTake(httpMutex, pdMS_TO_TICKS(HTTP_MUTEX_TIMEOUT_MS)) != pdTRUE) return;

  const bool connected = modem.connectNetwork(APN_MOBIFONE);
  xSemaphoreGive(httpMutex);

  if (connected) {

    network.connected = true;
    network.consecutiveHttpErrors = 0;
    consecutiveNetworkConnectFailures = 0;
    MAIN_PRINTLN("[NET] Ket noi 4G thanh cong.");

  } else {

    network.connected = false;
    if (consecutiveNetworkConnectFailures < 255) ++consecutiveNetworkConnectFailures;
    MAIN_PRINTLN("[NET] Ket noi 4G that bai; se thu lai sau.");

    if (consecutiveNetworkConnectFailures >= 3) {
      
#if ENABLE_SIM_HARD_RESET
      MAIN_PRINTLN("[SIM] Da that bai ket noi 3 lan; thu reset phan cung SIM7600E GPIO2.");
      bool resetOk = false;
      if (httpMutex != nullptr && xSemaphoreTake(httpMutex, pdMS_TO_TICKS(HTTP_MUTEX_TIMEOUT_MS)) == pdTRUE) {
        resetOk = modem.reset(SIM_RESET);
        xSemaphoreGive(httpMutex);
      }
      if (resetOk) MAIN_PRINTLN("[SIM] Reset modem xong; se thu dang ky mang lai o chu ky tiep theo.");
      else MAIN_PRINTLN("[SIM] Reset modem khong thanh cong; kiem tra mutex, mach RESET va nguon.");

#else

      MAIN_PRINTLN("[SIM] Da that bai 3 lan; hardware reset dang DISABLED. Xac nhan mach GPIO2 truoc khi bat ENABLE_SIM_HARD_RESET.");

#endif
      consecutiveNetworkConnectFailures = 0;
    }
  }
}
} // namespace
//========================================================================================================================================================================
//========================================================================================================================================================================
//========================================================================================================================================================================
//========================================================================================================================================================================
void khoiTao() {
  Serial.begin(115200);
  delay(300);
  MAIN_PRINTLN("\n=== GATEWAY QUAN TRAC: LORA -> SIM7600E -> FIREBASE ===");

  httpMutex = xSemaphoreCreateMutex();
  rtcMutex = xSemaphoreCreateMutex();
  sensorMutex = xSemaphoreCreateMutex();

  if (httpMutex == nullptr || rtcMutex == nullptr || sensorMutex == nullptr) {
    MAIN_PRINTLN("[FATAL] Khong tao duoc mutex; dung he thong.");
    while (true) delay(1000);
  }

  rtc.beginRTC();
  rtc.LayRTCTuDS3231();
  lora.Lora_Begin();

  // Start receiving before the SIM7600 boot sequence so early LoRa frames
  // are not lost while the modem initializes.
  xTaskCreatePinnedToCore(taskReceiveLoRa, "LoRa_RX", 6144, nullptr, 3, nullptr, 0);
  xTaskCreatePinnedToCore(taskFirebaseUpload, "Firebase_TX", 8192, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(taskStatusAndChart, "StatusChart", 6144, nullptr, 1, nullptr, 1);

  // Preserve the PCB's SIM7600 UART/pin configuration.
  modem.begin(&Serial2, SIM_BAUDRATE, SIM_TX, SIM_RX);
  modem.powerOn(SIM_PWRKEY);
  modem.printModemInfo();
  network.connected = false;
  lastReconnectAttempt = millis() - NETWORK_RETRY_MS; // request first network attempt immediately in loop
}

void chayChuongTrinhChinh() {
  reconnectNetworkIfNeeded();
  delay(500);
}
