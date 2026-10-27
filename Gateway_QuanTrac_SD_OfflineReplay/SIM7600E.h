#ifndef SIM7600_H
#define SIM7600_H
#include <Arduino.h>
#include <string.h>  // Bắt buộc phải có để dùng các hàm xử lý chuỗi C
#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>
// define các thông số cấu hình của SIM
#define SIM_BAUDRATE 115200
#define SIM_TX 17
#define SIM_RX 18
#define SIM_PWRKEY 1
#define SIM_RESET 2
#define APN_MOBIFONE "v-internet"
//#define APN_MOBIFONE "m-wap"
// Khai báo STruct chứa ngày tháng năm lấy từ nhà mạng
typedef struct {
  int Thu;
  int Ngay;
  int Thang;
  int Nam;
  int Gio;
  int Phut;
  int Giay;
} SIMTime;
// Khởi tạo Class
class SIM7600 {
private:
  unsigned long _baudRate;
  int _txPin;
  int _rxPin;
  int _pwrKeyPin;
  int _resetPin;
  HardwareSerial* _serial = nullptr;
  TinyGsm* _modem = nullptr;  // Khởi tạo đối tượng của TinyGSM
  // Các hàm bổ trợ
  bool SendATCommand(const char* command, const char* expected, uint32_t timeout_ms, char* responseBuffer, size_t bufferSize);
  void clearUART2Buffer();
public:
  int lastHttpStatusCode = -1;
  int lastHttpBodyLength = 0;
  SIM7600();
  ~SIM7600();
  SIMTime Time;  // Struct trung gian chứa giờ lấy được từ nhà mạng
  // Các hàm hệ thống
  bool begin(HardwareSerial* serialPort, unsigned long baud, int tx = 17, int rx = 18);
  bool powerOn(int pwrkey);  // Hàm bật nguồn module SIM
  bool reset(int resetPin);  // Hàm RESET module SIM
  bool isAlive();            // Hàm check AT
  bool isSIMReady();         // Check SIM có đang hoạt động hay không
  // Các hàm thông tin Trạm 
  String getIMEI();        // Lấy số IMEI Module SIM
  String getOperator();    // Lấy tên nhà mạng
  String getCCID();        // Lấy số Seri SIM
  int getSignalQuality();  // Lấy tín hiệu mạng
  void printModemInfo();   // Hàm in các thông tin ở trên
  // Các hàm kết nối mạng
  bool connectNetwork(const char* apn);  // Hàm kết nối mạng chính
  bool isNetworkConnected();             // Hàm kiểm tra kết nối mạng
  // Các hàm xử lý POSTGET
  String httpsPost(const String& url, const String& dataToPost);  // Hàm POST Data
  String httpsGet(const String& url);                             // Hàm GET Data
  int extractJsonInt(const String& jsonString, const String& key);
  String extractJsonString(const String& jsonString, const String& key);
  bool CheckHttpResponse(const String& res, const char* Key);
  bool getNetworkTime(SIMTime& timeData);  // Hàm lấy giờ từ nhà mạng
};
#endif