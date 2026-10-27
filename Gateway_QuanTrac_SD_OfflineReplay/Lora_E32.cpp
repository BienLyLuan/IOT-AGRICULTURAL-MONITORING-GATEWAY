#include "Lora_E32.h"
#include "Config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
// ham set che do, co the blocking o day vi o day rat it khi set mode lai
void LoRa::Lora_Setmode(uint8_t mode) {
  // PCB does not route M0/M1 (PIN_M0/PIN_M1 == 0xFF); leave the module in
  // its hardware-strapped mode instead of writing to invalid GPIO 255.
  uint32_t start = millis();
  while (digitalRead(PIN_AUX) == LOW) {
    if (millis() - start > 1000) {
      LORA_PRINTLN("[LORA] AUX timeout khi doi mode.");
      return;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  if (PIN_M0 != 0xFF) digitalWrite(PIN_M0, (mode == LORA_MODE_WAKEUP || mode == LORA_MODE_CONFIG) ? HIGH : LOW);
  if (PIN_M1 != 0xFF) digitalWrite(PIN_M1, (mode == LORA_MODE_SLEEP || mode == LORA_MODE_CONFIG) ? HIGH : LOW);
  vTaskDelay(pdMS_TO_TICKS(40));

  start = millis();
  while (digitalRead(PIN_AUX) == LOW) {
    if (millis() - start > 1000) {
      LORA_PRINTLN("[LORA] AUX timeout sau khi doi mode.");
      return;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
// ham config LoRa
void LoRa::Lora_Config(uint8_t addrH, uint8_t addrL, uint8_t chan, uint8_t mode) {
  if (PIN_M0 == 0xFF || PIN_M1 == 0xFF) {
    LORA_PRINTLN("[LORA] Khong the vao CONFIG: PCB khong ket noi M0/M1.");
    return;
  }
  LORA_PRINTLN("LoRa Config: START!");
  Lora_Setmode(LORA_MODE_CONFIG);
  byte command[6];
  command[0] = 0xC0;          // Ghi và lưu vĩnh viễn
  command[1] = addrH;         // Địa chỉ cao
  command[2] = addrL;         // Địa chỉ thấp
  command[3] = 0x1A;          // 9600bps, 8N1, Air 2.4k
  command[4] = chan;          // Tần số = 410 + chan
  command[5] = mode;          // fix mode
  Serial1.write(command, 6);  // Transmit
  vTaskDelay(pdMS_TO_TICKS(100));
  uint32_t auxStart = millis();

  while (digitalRead(PIN_AUX) == LOW && millis() - auxStart < 1000) { vTaskDelay(pdMS_TO_TICKS(1)); }
  if (digitalRead(PIN_AUX) == LOW) LORA_PRINTLN("[LORA] AUX timeout sau lenh config.");
  Lora_Setmode(LORA_MODE_NORMAL);
  LORA_PRINTLN("[LORA] CONFIG:LoRa SUCCESS!");
}

// ham begin ( Dung de cau hinh UART va cac chan lien quan)
void LoRa::Lora_Begin() {
  if (PIN_M0 != 0xFF) pinMode(PIN_M0, OUTPUT);
  if (PIN_M1 != 0xFF) pinMode(PIN_M1, OUTPUT);
  pinMode(PIN_AUX, INPUT);
  // Begin UART2
  Serial1.begin(LORA_BAUDRATE, SERIAL_8N1, LORA_RX, LORA_TX);
  LORA_PRINTLN("[LORA] CONFIG:Đang Khởi tạo LoRa E32...");
  Lora_Setmode(LORA_MODE_NORMAL);
  if (digitalRead(PIN_AUX) == HIGH) {
    LORA_PRINTLN("[LORA] CONFIG:LoRa E32 Ready!");
  } else {
    LORA_PRINTLN("[LORA] ERROR: LoRa E32 Error/Busy!");
  }
}
// Ham tinh CRC dung cho LoRa
uint16_t LoRa::Calculate_CRC16(uint8_t *buffer, uint16_t length) {
  uint16_t crc = 0xFFFF;
  for (uint16_t pos = 0; pos < length; pos++) {
    crc ^= (uint16_t)buffer[pos];
    for (int i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}
// ham gui data
//bool LoRa::Lora_SendData(uint8_t targetH, uint8_t targetL, uint8_t targetChan, NodeOut_t *data) {
//  data->header = 0xAA;
//  data->nodeID = MY_LORA_ID;
//  data->len = sizeof(NodeOut_t) - 5;
//  data->msgType = 0x02;
//  data->crc = Calculate_CRC16((uint8_t *)data, sizeof(NodeOut_t) - 2);
//  // Wait AUX
//  uint32_t startTime = millis();
//  while (digitalRead(PIN_AUX) == LOW) {
//    if (millis() - startTime > 1000) {
//      LORA_PRINTLN("[LORA] ERROR:Timeout LoRa Send");
//      return false;
//    }
//    vTaskDelay(pdMS_TO_TICKS(1));
//  }
//  // Pack Data
//  uint8_t buffer[3 + sizeof(NodeOut_t)];
//  buffer[0] = targetH;
//  buffer[1] = targetL;
//  buffer[2] = targetChan;
//  memcpy(&buffer[3], data, sizeof(NodeOut_t));
//  Serial1.write(buffer, sizeof(buffer));  // Send
//  // Wait AUX
//  vTaskDelay(pdMS_TO_TICKS(2));  // Doi mot chut
// startTime = millis();
// while (digitalRead(PIN_AUX) == LOW) {
//    if (millis() - startTime > 1000) {
//      LORA_PRINTLN("[LORA] ERROR:Timeout LoRa Send");
//      return false;
//   }
//    vTaskDelay(pdMS_TO_TICKS(1));
//  }
//  // In debug
//  logRawData("LORA TX", data->nodeID, (uint8_t *)data, sizeof(NodeOut_t));
//  return true;
//}

// Ham doc Data Raw tu cac node lưu vào rxBuffer
uint8_t LoRa::LoRa_Read_DataRaw(HardwareSerial &serial, uint8_t *rxbuf) {
  if (rxbuf == nullptr) return 0;
  // Resynchronize through a bounded number of junk bytes per call. This is
  // faster than discarding one byte every poll, without monopolizing the task.
  uint8_t discarded = 0;
  while (serial.available() > 0 && serial.peek() != 0xAA && discarded < 64) {
    serial.read();
    ++discarded;
  }
  if (serial.available() < 5) return 0;
  if (serial.peek() != 0xAA) return 0;

  uint8_t headerInfo[3];
  serial.readBytes(headerInfo, 3);

  const uint16_t byteCount = headerInfo[2];
  const uint16_t totalLen = byteCount + 5U;
  const uint16_t maxSize = sizeof(MyData);
  if (totalLen > maxSize) {
    // Discard the declared remainder with a bounded wait to restore frame alignment.
    uint16_t remaining = totalLen - 3U;
    const uint32_t discardStart = millis();
    while (remaining > 0 && millis() - discardStart < 500U) {
      if (serial.available() > 0) { serial.read(); --remaining; }
      else vTaskDelay(pdMS_TO_TICKS(1));
    }
    LORA_PRINTLN("[LORA] Frame co length vuot gioi han; da bo frame.");
    return 0;
  }

  memcpy(rxbuf, headerInfo, 3);
  size_t received = serial.readBytes(&rxbuf[3], totalLen - 3U);
  if (received != (size_t)(totalLen - 3U)) return 0;
  // In debug
  logRawData("LORA RX", rxbuf[1], rxbuf, totalLen);
  return totalLen;
}
// Ham Parse data khi nhan tu cac node
LoRa_Parse_Response_t LoRa::LoRa_ParseData(uint8_t *rxbuf, uint8_t rxlen, MyData *dataIn, NodeOut_t *dataOut) {
  if (rxbuf == nullptr || rxlen < 5 || rxbuf[0] != 0xAA) return LORA_PARSE_ERROR;
  uint16_t calCRC = Calculate_CRC16(rxbuf, rxlen - 2);
  uint16_t recCRC = rxbuf[rxlen - 2] | (rxbuf[rxlen - 1] << 8);
  if (calCRC != recCRC) {
    return LORA_PARSE_CRC_ERR;
  }
  if (rxlen == sizeof(MyData)) {
    memcpy(dataIn, rxbuf, sizeof(MyData));
    return LORA_PARSE_NODE_IN;
  }
  return LORA_PARSE_ERROR;
}
// Ham xoa buffer
void LoRa::clearUART1Buffer() {
  while (Serial1.available() > 0) {
    Serial1.read();
  }
}
// Ham in log de debug
void LoRa::logRawData(const char *prefix, uint8_t id, uint8_t *data, uint8_t len) {
  char logBuf[150];
  int p = 0;
  p += sprintf(logBuf + p, "[%s] ID:%02d | Raw: ", prefix, id);
  for (int i = 0; i < len; i++) {
    if (p < 145) {
      p += sprintf(logBuf + p, "%02X ", data[i]);
    }
  }
  LORA_PRINTLN(logBuf);
}