#ifndef _Lora_E32_h
#define _Lora_E32_h
#include <Arduino.h>
// Define PIN LoRa UART
#define LORA_TX 15
#define LORA_RX 16
// Define LoRa Mode
#define LORA_MODE_NORMAL 0
#define LORA_MODE_WAKEUP 1
#define LORA_MODE_SLEEP 2
#define LORA_MODE_CONFIG 3
// Define config LoRa
#define LORA_BAUDRATE         9600
#define LORA_FREQ_433MHZ      0x17
#define LORA_MODE_FIXED       0xC4
#define MY_LORA_ID            0x00
#define LORA_NODE_OUTPUT_ADDR 0x0002
// define cau hinh cua Struct 
// Khai báo các chân cấu hình LoRa, ở board này không có chân M0 và M1 nên sẽ là 0xFF cho cả 2, chỉ có chân AUX
const int PIN_M0 = 0xFF;
const int PIN_M1 = 0xFF;
const int PIN_AUX = 21;
// Struc data Node Input
typedef struct {
  uint8_t header;
  uint8_t nodeID;
  uint8_t len;
  int16_t temperature;
  int16_t humidity;
  uint8_t Vbat;
  uint16_t sensor2[7];
  uint8_t status;
  uint16_t crc;
} __attribute__((packed)) MyData;

// Struc data Node Output
//typedef struct {
//  uint8_t header;      // 1 byte: 0xAA
//  uint8_t nodeID;      // 1 byte: ID của Node
//  uint8_t len;         // 1 byte:uint8_t  msgType;
//  uint8_t msgType;     // cmd,ack,
//  uint8_t systemMode;  // 1 byte: 0:Auto, 1:Manual, dùng số
//  uint8_t relay;       // 1 byte: Trạng thái 7 Relay, dùng bit
//  uint8_t curtain;     // 1 byte: Trạng thái rèm, dùng số . 3 mo 2 dong 1 stop 
//  uint8_t inputData;   // 1 byte: 5 ngõ vào, dùng bit
//  uint8_t errorCode;   // 1 byte: Mã lỗi, dùng số
//  uint16_t crc;        // 2 bytes: CRC16 Modbus
//} __attribute__((packed)) NodeOut_t;
// Enum Kieu du lieu cua Ham Parse Data LoRa
typedef enum {
  LORA_PARSE_ERROR = 0,
  LORA_PARSE_NODE_IN,
  LORA_PARSE_NODE_OUT,
  LORA_PARSE_CRC_ERR
} LoRa_Parse_Response_t;
// Class Handle Function LoRa
class LoRa {
private:
  void Lora_Setmode(uint8_t mode);
  uint16_t Calculate_CRC16(uint8_t *buffer, uint16_t length);
  void logRawData(const char *prefix, uint8_t id, uint8_t *data, uint8_t len);
public:
  MyData DataNodeIn;        // Chua Data NodeIn
  NodeOut_t DataNodeOut;    // Chua Data ACK NodeOut
  NodeOut_t Controlpacket;  // Nhan Data Tu Sever gui ve NodeOut
  void Lora_Config(uint8_t addrH, uint8_t addrL, uint8_t chan, uint8_t mode);
  void Lora_Begin();
  bool Lora_SendData(uint8_t targetH, uint8_t targetL, uint8_t targetChan, NodeOut_t *data);
  uint8_t LoRa_Read_DataRaw(HardwareSerial &serial, uint8_t *rxbuf);
  LoRa_Parse_Response_t LoRa_ParseData(uint8_t *rxbuf, uint8_t rxlen, MyData *dataIn, NodeOut_t *dataOut);
  void clearUART1Buffer();  // UART1 dung cho LoRa
};
#endif