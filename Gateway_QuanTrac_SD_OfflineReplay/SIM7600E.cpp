#include "SIM7600E.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Config.h"
#define PRINT_GET_RAW_DATA
#define PRINT_POST_RAW_DATA
// Constructor: Khởi tạo con trỏ bằng NULL
SIM7600::SIM7600() : _baudRate(0), _txPin(-1), _rxPin(-1), _pwrKeyPin(-1), _resetPin(-1),
                     _serial(nullptr), _modem(nullptr) {}
// Destructor: Giải phóng bộ nhớ khi không dùng đến
SIM7600::~SIM7600() {
  if (_modem) delete _modem;
}
////////////////////////////////////////////////////////////////////////// KHỐI CẤU HÌNH ////////////////////////////
// Hàm Khởi tạo UART và đối tượng TinyGSM
bool SIM7600::begin(HardwareSerial* serialPort, unsigned long baud, int tx, int rx) {
  if (serialPort == nullptr) return false;
  _serial = serialPort;
  _baudRate = baud;
  _txPin = tx;
  _rxPin = rx;
  _serial->setRxBufferSize(8192);
  _serial->begin(_baudRate, SERIAL_8N1, _rxPin, _txPin);
  vTaskDelay(pdMS_TO_TICKS(100));
  clearUART2Buffer();
  
  if (_modem) delete _modem;
  _modem = new TinyGsm(*_serial);
  return true;
}
// Hàm kiểm tra sự sống của Modem
bool SIM7600::isAlive() {
  if (!_modem) return false;
  return _modem->testAT(2000L);  // Chờ tối đa 2000 miligiây
}
// Hàm bật nguồn phần cứng
bool SIM7600::powerOn(int pwrkey) {
  if (_serial == nullptr || _modem == nullptr) return false;
  if (isAlive()) {
    SIM_PRINTLN("[SIM] CONFIG:Module đã bật sẵn, bỏ qua bước bật nguồn.");
    return true;
  }

  SIM_PRINTLN("[SIM] CONFIG: Module đang tắt, tiến hành bật nguồn...");
  _pwrKeyPin = pwrkey;
  pinMode(_pwrKeyPin, OUTPUT);
  digitalWrite(_pwrKeyPin, HIGH);
  vTaskDelay(pdMS_TO_TICKS(1500));

  digitalWrite(_pwrKeyPin, LOW);
  vTaskDelay(pdMS_TO_TICKS(1500));

  digitalWrite(_pwrKeyPin, HIGH);
  vTaskDelay(pdMS_TO_TICKS(5000));  // Chờ module khởi động ổn định firmware bên trong

  if (!_modem->restart()) {
    SIM_PRINTLN("[SIM] CONFIG:Lỗi TinyGSM không thể cấu hình Modem sau khi bật nguồn!");
    return false;
  }
  return true;
}
// Hàm Reset phần cứng
bool SIM7600::reset(int resetPin) {
  
  if (_serial == nullptr || _modem == nullptr || resetPin < 0) return false;
    _resetPin = resetPin;
    pinMode(_resetPin, OUTPUT);
    digitalWrite(_resetPin, HIGH);
    digitalWrite(_resetPin, LOW);
    vTaskDelay(pdMS_TO_TICKS(200));
    digitalWrite(_resetPin, HIGH);
    delay(200);

  if (!_modem->init()) {
    SIM_PRINTLN("[SIM] CONFIG:Lỗi Không thể init modem sau khi Reset!");
    return false;
  }
  return true;
}
////////////////////////////////////////////////////////////////////////////////KHỐI LẤY THÔNG TIN MODEM ///////////////////////////////////
// Hàm lấy thông tin SIM
String SIM7600::getIMEI() {
  if (!_modem) return "";
  return _modem->getIMEI(); 
}
// Hàm lấy nhà mạng
String SIM7600::getOperator() {
  if (!_modem) return "";
  return _modem->getOperator();
}
// Hàm lấy ID
String SIM7600::getCCID() {
  if (!_modem) return "";
  return _modem->getSimCCID();
}
// Hàm check
bool SIM7600::isSIMReady() {
  if (!_modem) return false;
  return (_modem->getSimStatus() == SIM_READY);
}
// Hàm lấy tín hiệu
//int SIM7600::getSignalQuality() {
//  if (!_modem) return -1;
//  return _modem->getSignalQuality();  // Trả về trực tiếp chỉ số RSSI
// }
// Hàm lấy tín hiệu mạng quy đổi ra phần trăm (0 - 100%)
 int SIM7600::getSignalQuality() {
   if (!_modem) return -1;
   int rssi = _modem->getSignalQuality();
   if (rssi == 99 || rssi <= 1) {
     return 0;
   }
   if (rssi > 31) {
     rssi = 31;
   }
   int percent = ((rssi - 2) * 100) / 29;
   return percent;
 }
// Hàm in Thông tin tổng
void SIM7600::printModemInfo() {
  SIM_PRINTLN("=================================START_INFO_MODEM======================================");
  // In IMEI
  SIM_PRINT("[SIM] INFO:IMEI: ");
  SIM_PRINTLN(getIMEI());
  // Kiểm tra thẻ SIM
  if (isSIMReady()) {
    SIM_PRINTLN("[SIM] INFO: Thẻ SIM đã sẵn sàng");
  } else {
    SIM_PRINTLN("[SIM] INFO: Thẻ SIM chưa nhận hoặc bị lỗi.");
  }
  // In tên nhà mạng
  SIM_PRINT("[SIM] INFO:Nhà mạng đang kết nối: ");
  SIM_PRINTLN(getOperator());
  // In cường độ sóng
  SIM_PRINT("[SIM] INFO:Cường độ sóng (CSQ RSSI) = ");
  SIM_PRINTLN(getSignalQuality());
  SIM_PRINTLN("=================================END_INFO_MODEM=======================================");
}
/////////////////////////////////////////////////////////////////////////KHỐI KẾT NỐI MẠNG ///////////////////////////////////////////////////////////////
// Hàm kết nối mạng DATA (GPRS/LTE)
bool SIM7600::connectNetwork(const char* apn) {
  if (!_modem) return false;
  SIM_PRINTLN("==============================START_CONNECT_NETWORK=====================================");
  SIM_PRINTLN("[SIM] STATUS:Đang chờ đăng ký mạng...");
  
  if (!_modem->waitForNetwork(60000L)) {
    SIM_PRINTLN("[SIM] CONFIG:Lỗi Không thể đăng ký mạng với trạm BTS!");
    return false;
  }
  SIM_PRINT("[SIM] STATUS:Đang kết nối vào APN: ");
  SIM_PRINTLN(apn);
  if (!_modem->gprsConnect(apn, "", "")) {
    SIM_PRINTLN("[SIM] STATUS:Lỗi gprsConnect thất bại!");
    return false;
  }
  SIM_PRINTLN("[SIM] STATUS:KẾT NỐI MẠNG THÀNH CÔNG!");
  SIM_PRINTLN("===============================END_CONNECT_NETWORK=====================================");
  return true;
}

// Hàm check connect mạng
bool SIM7600::isNetworkConnected() {
  if (!_modem) return false;
  return _modem->isGprsConnected();
}
///////////////////////////////////////////////////////////////////////KHỐI POSTGET HTTPS/////////////////////////////////////////////////////////////////
// Hàm get data
String SIM7600::httpsGet(const String& url) {
  lastHttpStatusCode = -1;
  lastHttpBodyLength = 0;
  if (!_modem) { return ""; }
  // CLEAR UART
  clearUART2Buffer();
  SIM_PRINTLN("=================================START_GET_DATA=======================================");
  char response[128];
  // KẾT THÚC PHIÊN TRƯỚC
  bool isClosed = SendATCommand("AT+HTTPTERM", "OK", 2000, response, sizeof(response));
  if (isClosed) {
    SIM_PRINTLN("[SIM] GET:Đã đóng kết nối HTTP thành công.");
  } else {
    SIM_PRINTLN("[SIM] GET:Lỗi đóng kết nối HTTP hoặc chưa có kết nối nào được mở.");
  }
  // KHỞI TẠO PHIÊN MỚI
  bool isInit = SendATCommand("AT+HTTPINIT", "OK", 2000, response, sizeof(response));
  if (isInit) {
    SIM_PRINTLN("[SIM] GET:Khởi tạo HTTP thành công.");
  } else {
    SIM_PRINTLN("[SIM] GET:Lỗi khởi tạo HTTP");
    return "";
  }
  // GẮN URL
  char commandBuffer[512];
  const int urlCommandLength = snprintf(commandBuffer, sizeof(commandBuffer), "AT+HTTPPARA=\"URL\",\"%s\"", url.c_str());

  if (urlCommandLength <= 0 || (size_t)urlCommandLength >= sizeof(commandBuffer)) {
    SIM_PRINTLN("[SIM] URL vuot qua kich thuoc command buffer.");
    SendATCommand("AT+HTTPTERM", "OK", 2000, response, sizeof(response));
    return "";
  }


  bool isUrlSet = SendATCommand(commandBuffer, "OK", 2000, response, sizeof(response));
  if (isUrlSet) {
    SIM_PRINTLN("[SIM] GET:Cài đặt URL thành công");
  } else {
    SIM_PRINTLN("[SIM] GET:Lỗi không thể set URL");
    SendATCommand("AT+HTTPTERM", "OK", 2000, response, sizeof(response));
    return "";
  }

  // BẮT ĐẦU GET
  int statusCode = 0;
  int totalHttpLength = 0;
  bool isActionDone2 = SendATCommand("AT+HTTPACTION=0", "+HTTPACTION: 0", 10000, response, sizeof(response));
  if (isActionDone2) {
    char* actionStr = strstr(response, "+HTTPACTION: 0,");
    if (actionStr != NULL) {
      sscanf(actionStr, "+HTTPACTION: 0,%d,%d", &statusCode, &totalHttpLength);
      lastHttpStatusCode = statusCode;
      lastHttpBodyLength = totalHttpLength;
    }
  } else {
    SIM_PRINTLN("[SIM] GET: timeout, mat mang hoac server khong phan hoi.");
    SendATCommand("AT+HTTPTERM", "OK", 2000, response, sizeof(response));
    return "";
  }

  SIM_PRINT("[SIM] GET:HTTP Status Code = ");
  SIM_PRINTLN(statusCode);
  SIM_PRINT("[SIM] GET:HTTP Body Length = ");
  SIM_PRINTLN(totalHttpLength);
  // HTTP ERROR
  if (statusCode != 200 || totalHttpLength <= 0) {
    SendATCommand("AT+HTTPTERM", "OK", 2000, response, sizeof(response));
    return "";
  }
  // PRE-ALLOCATE STRING
  String resultResponseBody;
  resultResponseBody.reserve(totalHttpLength);
  // CHUNK CONFIG
  const int CHUNK_SIZE = 812;
  int currentOffset = 0;
  char uartBuf[256];

  // CHUNK LOOP
  while (currentOffset < totalHttpLength) {
    int bytesToReadThisChunk =
      totalHttpLength - currentOffset;

    if (bytesToReadThisChunk > CHUNK_SIZE) {
      bytesToReadThisChunk = CHUNK_SIZE;
    }
    // CLEAR UART BEFORE READ
    clearUART2Buffer();
    // SEND HTTPREAD VÀ WAIT HEADER
    char readCmd[64];
    char headerResponse[128];  // Mảng nhỏ để chứa phản hồi Header
    int actualBytesReturned = 0;
    // Lắp ráp lệnh HTTPREAD kèm theo Offset và Chunk Size
    snprintf(readCmd, sizeof(readCmd), "AT+HTTPREAD=%d,%d", currentOffset, bytesToReadThisChunk);
    // Gửi lệnh và chờ Header trả về (Timeout 5 giây)
    bool foundHeader = SendATCommand(readCmd, "+HTTPREAD: DATA,", 5000, headerResponse, sizeof(headerResponse));
    if (foundHeader) {
      // Nếu bắt được Header, tiến hành cắt lấy con số dung lượng (actualBytesReturned)
      char* dataStr = strstr(headerResponse, "+HTTPREAD: DATA,");
      if (dataStr != NULL) {
        sscanf(dataStr, "+HTTPREAD: DATA,%d", &actualBytesReturned);
      }
    } else {
      SIM_PRINTLN("[SIM] GET:ERROR: HTTPREAD header timeout hoặc bị lỗi");
      break;
    }

    // READ RAW DATA
    int countReceived = 0;
    _serial->setTimeout(200);
    while (countReceived < actualBytesReturned) {
      int remain = actualBytesReturned - countReceived;
      int toRead = remain;
      if (toRead > (int)sizeof(uartBuf)) {
        toRead = (int)sizeof(uartBuf);
      }
      int len = _serial->readBytes(
        uartBuf,
        toRead);
      if (len > 0) {
        resultResponseBody.concat(
          uartBuf,
          len);
        countReceived += len;
      } else {
        SIM_PRINTLN("[SIM] GET:ERROR UART timeout");
        break;
      }
    }

    currentOffset += countReceived;
    SIM_PRINT("[SIM] GET:Received bytes = ");
    SIM_PRINTLN(currentOffset);
    // WAIT FOR OK
    uint32_t start = millis();
    while (millis() - start < 300) {
      clearUART2Buffer();
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (countReceived == 0) {
      SIM_PRINTLN("[SIM] GET:ERROR UART stalled");
      break;
    }
  }
#ifdef PRINT_GET_RAW_DATA
  SIM_PRINT("[SIM] GET:RAW DATA FROM SERVER:");
  SIM_PRINTLN(resultResponseBody);
#endif
  // HTTP TERM
  SendATCommand("AT+HTTPTERM", "OK", 2000, response, sizeof(response));
  SIM_PRINTLN("==================================END_GET_DATA========================================");
  // CLEAR UART
  clearUART2Buffer();
  return resultResponseBody;
}

// Hàm POST Data
String SIM7600::httpsPost(const String& url, const String& dataToPost) {
  lastHttpStatusCode = -1;
  lastHttpBodyLength = 0;
  if (!_modem) { return ""; }
  char response[128];
  char commandBuffer[512];
  // CLEAR UART
  clearUART2Buffer();
  SIM_PRINTLN("=================================START_POST_DATA======================================");
  // HTTP TERM
  bool isClosed = SendATCommand("AT+HTTPTERM", "OK", 2000, response, sizeof(response));
  if (isClosed) {
    SIM_PRINTLN("[SIM] POST:Đã đóng kết nối HTTP thành công.");
  } else {
    SIM_PRINTLN("[SIM] POST:Lỗi đóng kết nối HTTP hoặc chưa có kết nối nào được mở.");
  }

  // HTTP INIT
  bool isInit = SendATCommand("AT+HTTPINIT", "OK", 2000, response, sizeof(response));
  if (isInit) {
    SIM_PRINTLN("[SIM] POST: Khởi tạo HTTP thành công.");
  } else {
    SIM_PRINTLN("[SIM] POST: Lỗi khởi tạo HTTP");
    return "";
  }

  // SET URL (Firebase URLs can exceed 256 bytes when auth and History key are included)
  const int urlCommandLength = snprintf(commandBuffer, sizeof(commandBuffer), "AT+HTTPPARA=\"URL\",\"%s\"", url.c_str());
  if (urlCommandLength <= 0 || (size_t)urlCommandLength >= sizeof(commandBuffer)) {
    SIM_PRINTLN("[SIM] URL vuot qua kich thuoc command buffer.");
    SendATCommand("AT+HTTPTERM", "OK", 2000, response, sizeof(response));
    return "";
  }

  bool isUrlSet = SendATCommand(commandBuffer, "OK", 2000, response, sizeof(response));
  if (!isUrlSet) {
    SIM_PRINTLN("[SIM] POST: Lỗi không thể set URL.");
    SendATCommand("AT+HTTPTERM", "OK", 2000, response, sizeof(response));
    return "";
  }

  // Firebase REST uses PUT for replacing a known path. SIM7600 HTTPACTION=1
  // performs POST, so request Firebase's documented method-override header.
  bool isOverrideSet = SendATCommand("AT+HTTPPARA=\"USERDATA\",\"X-HTTP-Method-Override: PUT\"", "OK", 2000, response, sizeof(response));
  if (!isOverrideSet) {
    SIM_PRINTLN("[SIM] Khong set duoc X-HTTP-Method-Override header.");
    SendATCommand("AT+HTTPTERM", "OK", 2000, response, sizeof(response));
    return "";
  }
  // SET CONTENT TYPE
  bool isContentSet = SendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", 2000, response, sizeof(response));
  if (!isContentSet) {
    SIM_PRINTLN("[SIM] POST: Lỗi set Content Type.");
    SendATCommand("AT+HTTPTERM", "OK", 2000, response, sizeof(response));
    return "";
  }

  // YÊU CẦU GỬI DATA VÀ CHỜ "DOWNLOAD
  snprintf(commandBuffer, sizeof(commandBuffer), "AT+HTTPDATA=%d,10000", (int)dataToPost.length());
  // Gửi lệnh HTTPDATA
  bool canDownload = SendATCommand(commandBuffer, "DOWNLOAD", 10000, response, sizeof(response));
  if (!canDownload) {
    SIM_PRINTLN("[SIM] POST:Lỗi Không nhận được lệnh DOWNLOAD từ SIM");
    SendATCommand("AT+HTTPTERM", "OK", 2000, response, sizeof(response));
    return "";
  }
  // ĐẨY PAYLOAD XUỐNG SIM
  _serial->print(dataToPost);
  vTaskDelay(pdMS_TO_TICKS(500));  // Cho SIM thời gian lưu data
  clearUART2Buffer();              // Xóa rác chữ "OK" do SIM nhả ra sau khi nhận đủ data
  // THỰC THI POST (HTTPACTION=1)
  int statusCode = 0;
  int totalHttpLength = 0;
  bool isActionDone = SendATCommand("AT+HTTPACTION=1", "+HTTPACTION: 1", 20000, response, sizeof(response));
  if (isActionDone) {
    char* actionStr = strstr(response, "+HTTPACTION: 1,");
    if (actionStr != NULL) {
      sscanf(actionStr, "+HTTPACTION: 1,%d,%d", &statusCode, &totalHttpLength);
      lastHttpStatusCode = statusCode;
      lastHttpBodyLength = totalHttpLength;
    }
  } else {
    SIM_PRINTLN("[SIM] POST: Lỗi HTTPACTION timeout hoặc lỗi Server");
    SendATCommand("AT+HTTPTERM", "OK", 2000, response, sizeof(response));
    return "";
  }
  //////////////////////////////////////////////////////////////////////
  SIM_PRINT("[SIM] POST:HTTP Status = ");
  SIM_PRINTLN(statusCode);
  SIM_PRINT("[SIM] POST:HTTP Length = ");
  SIM_PRINTLN(totalHttpLength);
  if (statusCode != 200 || totalHttpLength <= 0) {
    SendATCommand("AT+HTTPTERM", "OK", 2000, response, sizeof(response));
    return "";
  }
  // KÉO DỮ LIỆU PHẢN HỒI
  String resultResponseBody;
  resultResponseBody.reserve(totalHttpLength);
  const int CHUNK_SIZE = 812;
  int currentOffset = 0;
  char uartBuf[256];
  while (currentOffset < totalHttpLength) {
    int bytesToReadThisChunk = totalHttpLength - currentOffset;
    if (bytesToReadThisChunk > CHUNK_SIZE) {
      bytesToReadThisChunk = CHUNK_SIZE;
    }
    clearUART2Buffer();
    char readCmd[64];
    char headerResponse[128];
    int actualBytesReturned = 0;
    snprintf(readCmd, sizeof(readCmd), "AT+HTTPREAD=%d,%d", currentOffset, bytesToReadThisChunk);
    bool foundHeader = SendATCommand(readCmd, "+HTTPREAD: DATA,", 5000, headerResponse, sizeof(headerResponse));
    
    if (foundHeader) {
      char* dataStr = strstr(headerResponse, "+HTTPREAD: DATA,");
      if (dataStr != NULL) {
        sscanf(dataStr, "+HTTPREAD: DATA,%d", &actualBytesReturned);
      }
    } else {
      SIM_PRINTLN("[SIM] POST: Lỗi HTTPREAD header timeout");
      break;
    }
    // Đọc dữ liệu thô
    int countReceived = 0;
    _serial->setTimeout(300);
    while (countReceived < actualBytesReturned) {
      int remain = actualBytesReturned - countReceived;
      int toRead = remain;
      if (toRead > (int)sizeof(uartBuf)) {
        toRead = (int)sizeof(uartBuf);
      }
      int len = _serial->readBytes(uartBuf, toRead);
      if (len > 0) {
        resultResponseBody.concat(uartBuf, len);
        countReceived += len;
      } else {
        SIM_PRINTLN("[SIM] POST:Lỗi UART timeout");
        break;
      }
    }
    currentOffset += countReceived;
    SIM_PRINT("[SIM] POST:Received = ");
    SIM_PRINTLN(currentOffset);
    // Vòng lặp Flush (Dọn chữ OK cuối Chunk)
    uint32_t waitOkStart = millis();
    while (millis() - waitOkStart < 300) {
      clearUART2Buffer();
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (countReceived == 0) {
      SIM_PRINTLN("[SIM] POST:Lỗi UART stalled");
      break;
    }
  }
#ifdef PRINT_POST_RAW_DATA
  SIM_PRINT("[SIM] POST:RAW DATA FROM SERVER:");
  SIM_PRINTLN(resultResponseBody);
#endif
  // HTTP TERM
  SendATCommand("AT+HTTPTERM", "OK", 2000, response, sizeof(response));
  SIM_PRINTLN("==================================END_POST_DATA=======================================");
  // CLEAR UART
  clearUART2Buffer();
  return resultResponseBody;
}

// Hàm tách JSON Int
int SIM7600::extractJsonInt(const String& jsonString, const String& key) {
  String searchKey = "\"" + key + "\"";
  // TÌM TỪ KHÓA
  int idx = jsonString.indexOf(searchKey);
  if (idx == -1) return -1;  // Trả về -1 nếu không tìm thấy từ khóa
  // XÁC ĐỊNH RANH GIỚI
  int colonIdx = jsonString.indexOf(':', idx);
  int commaIdx = jsonString.indexOf(',', colonIdx);

  if (commaIdx == -1) {
    commaIdx = jsonString.indexOf('}', colonIdx);
  }
  if (colonIdx == -1 || commaIdx == -1) return -1;  // Trả về -1 nếu cấu trúc chuỗi bị sai
  // CẮT LẤY CHUỖI CON
  String value = jsonString.substring(colonIdx + 1, commaIdx);
  // LÀM SẠCH CHUỖI
  value.replace("\"", "");
  value.trim();
  // CHUYỂN ĐỔI VÀ TRẢ VỀ KIỂU INT
  return value.toInt();
}
// Hàm tách JSON String 
String SIM7600::extractJsonString(const String& jsonString, const String& key) {
  //TẠO TỪ KHÓA TÌM KIẾM
  String searchKey;
  // Đặt trước một vùng nhớ nhỏ để nối chuỗi (độ dài key + 2 dấu nháy kép)
  searchKey.reserve(key.length() + 3); 
  searchKey = "\"" + key + "\"";
  // TÌM TỪ KHÓA
  int idx = jsonString.indexOf(searchKey);
  if (idx == -1) return "-1";  // Trả về "-1" nếu không tìm thấy
  // XÁC ĐỊNH RANH GIỚI
  int colonIdx = jsonString.indexOf(':', idx);
  int commaIdx = jsonString.indexOf(',', colonIdx);
  if (commaIdx == -1) {
    commaIdx = jsonString.indexOf('}', colonIdx); // Nếu là phần tử cuối cùng của JSON
  }
  if (colonIdx == -1 || commaIdx == -1) return "-1"; 
  
  // CẮT VÀ LƯU CHUỖI VỚI RESERVE
  String value;
  // Đặt trước 50 byte 
  value.reserve(50); 
  value = jsonString.substring(colonIdx + 1, commaIdx);
  // LÀM SẠCH CHUỖI
  value.replace("\"", ""); // Xóa dấu ngoặc kép thừa
  value.trim();            // Xóa khoảng trắng 2 đầu
  // TRẢ VỀ KẾT QUẢ
  return value;
}
// Hàm SendAT Command
bool SIM7600::SendATCommand(const char* command, const char* expected, uint32_t timeout_ms, char* responseBuffer, size_t bufferSize) {
  if (bufferSize == 0 || responseBuffer == nullptr) {
    return false;
  }
  // 2. Khởi tạo bộ đệm tổng
  responseBuffer[0] = '\0';
  size_t total_idx = 0;
  // Khởi tạo bộ đệm cục bộ (dùng để check OK/ERROR)
  char lineBuffer[128];
  size_t line_idx = 0;
  lineBuffer[0] = '\0';
  // Xóa dữ liệu rác
  clearUART2Buffer();
  // Gửi lệnh AT và ký tự kết thúc dòng xuống SIM qua Serial2
  _serial->print(command);
  _serial->print("\r\n");
  // Bắt đầu tính giờ chờ phản hồi
  uint32_t start_time = millis();
  while ((millis() - start_time) < timeout_ms) {
    while (_serial->available()) {
      char c = _serial->read();
      //  Ghi vào mảng tổng (có bảo vệ chống tràn)
      if (total_idx < (bufferSize - 1)) {
        responseBuffer[total_idx++] = c;
        responseBuffer[total_idx] = '\0';
      }
      // Ghi vào mảng cục bộ
      if (line_idx < (sizeof(lineBuffer) - 1)) {
        lineBuffer[line_idx++] = c;
        lineBuffer[line_idx] = '\0';
      }
      // Phân tích dữ liệu khi hết 1 dòng
      if (c == '\n') {
        // Quét xem dòng hiện tại có chứa chuỗi mong đợi (vd: "OK") không
        if (strstr(lineBuffer, expected) != NULL) {
          return true;
        }
        // Quét xem dòng hiện tại có báo lỗi không
        if (strstr(lineBuffer, "ERROR") != NULL) {
          return false;
        }
        // Nếu chưa thấy OK/ERROR, reset mảng dòng để hứng dòng tiếp theo của SIM
        line_idx = 0;
        lineBuffer[0] = '\0';
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  // Nếu thoát khỏi vòng lặp while lớn nghĩa là đã quá thời gian timeout
  return false;
}
// Hàm Clear UART2
void SIM7600::clearUART2Buffer() {
  if (_serial == nullptr) return;
  while (_serial->available()) {
    _serial->read();
  }
}
// Hàm lấy thời gian từ nhà mạng
bool SIM7600::getNetworkTime(SIMTime& timeData) {
  SIM_PRINTLN("====================================START_GET_TIME=====================================");
  char response[128];
  // Gửi lệnh AT lấy giờ mạng
  if (!SendATCommand("AT+CCLK?", "OK", 2000, response, sizeof(response))) {
    return false;
  }
  SIM_PRINT("[SIM] STATUS: RAW DATA TỪ AT+CCLK: ");

  SIM_PRINTLN(response);
  
  char* ptr = strstr(response, "+CCLK: \"");
  if (ptr == NULL) return false;
  int y, m, d, h, min, s;
  // Bóc tách dữ liệu
  int parsedItems = sscanf(ptr, "+CCLK: \"%2d/%2d/%2d,%2d:%2d:%2d", &y, &m, &d, &h, &min, &s);
  if (parsedItems == 6) {
    const int fullYearForValidation = 2000 + y;
    const int daysPerMonth[] = {31, ((fullYearForValidation % 4 == 0 && fullYearForValidation % 100 != 0) 
    || (fullYearForValidation % 400 == 0)) ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (y < 24 || y > 99 || m < 1 || m > 12 || d < 1 ||
        (m >= 1 && m <= 12 && d > daysPerMonth[m - 1]) ||
        h < 0 || h > 23 || min < 0 || min > 59 || s < 0 || s > 59) {
      SIM_PRINTLN("[SIM] Bo qua CCLK vi ngay/gio ngoai pham vi.");
      return false;
    }
    timeData.Nam = y;
    timeData.Thang = m;
    timeData.Ngay = d;
    timeData.Gio = h;
    timeData.Phut = min;
    timeData.Giay = s;
    
 // 1. Đổi năm thành 4 số (vd: 26 -> 2026)
    int fullYear = y + 2000;
    int n = fullYear - 2000;   // số năm kể từ 2000

    // 2. Số ngày tích lũy trước mỗi tháng (năm thường)
    const int daysBeforeMonth[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

    // 3. Tổng số ngày từ mốc 01/01/2000 (d tính từ 1)
    //    (n + 3) / 4 = số năm nhuận đã qua trong khoảng 2000..fullYear-1 (đúng cho 2000-2099)
    long totalDays = (long)n * 365 + (n + 3) / 4;
    totalDays += daysBeforeMonth[m - 1] + d;

  // Năm nhuận hiện tại và đã qua tháng 2 thì cộng thêm 1 ngày
  bool isLeap = (fullYear % 4 == 0 && fullYear % 100 != 0) || (fullYear % 400 == 0);
  if (isLeap && m > 2) {
    totalDays += 1;
  }

// 4. Mốc 01/01/2000 là thứ Bảy
int dow = (totalDays + 5) % 7;

// 1 = Chủ nhật, 2 = Thứ 2, ... 7 = Thứ 7
timeData.Thu = dow + 1;

    SIM_PRINTLN("=====================================END_GET_TIME=====================================");
    return true;
  }
  SIM_PRINTLN("=====================================END_GET_TIME=====================================");
  return false;
}
//
bool SIM7600::CheckHttpResponse(const String& res, const char* errKey) {
  if (res.length() == 0) {
    return false;
  }
  if (res.indexOf(errKey) != -1) {
    return false;
  }
  return true;
}