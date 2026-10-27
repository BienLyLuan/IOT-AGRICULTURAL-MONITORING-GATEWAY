#ifndef _RTC_h
#define _RTC_h
#include <Arduino.h>
//#include "IoTVision.h"  // Có define debug để bật/tắt các debug ra Serial.
#include <DS3231.h>      // Thư viện lấy thời gian thực (RTC)
#define MY_SDA 8
#define MY_SCL 9
typedef struct {
  int Thu;
  int Ngay;
  int Thang;
  int Nam;
  int Gio;
  int Phut;
  int Giay;
} ThoiGian;

class RTC {
private:
  DS3231 ds;
public:
  ThoiGian dt;
public:  //private:
  // Lấy RTC từ nguồn server NTP hay IoTVision
  bool LayRTCTuServerNTP(void);
  bool LayRTCTuServerIoTVision(void);
  void beginRTC();
  void LayRTCTuDS3231(void);
  void CaiDatRTCChoDS3231(void);
  void SoSanhSaiLechRTCDeCapNhatDS3231(void);
  void SoSanhSaiLechRTCDeCapNhatDS3231Tu4G(int thu, int ngay, int thang, int nam, int gio, int phut, int giay);
  String ChuanHoaChuoiRTCDeGuiVeServer(void);
  String ChuanHoaHienThiHMI(void);
};


#endif
