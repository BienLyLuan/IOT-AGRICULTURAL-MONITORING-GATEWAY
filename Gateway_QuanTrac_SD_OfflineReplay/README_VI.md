# Gateway Quan Trắc — ESP32-S3 + LoRa + 4G + SD Offline Replay

Bản firmware quan trắc-only được chỉnh trực tiếp từ source `Node_Gateway` và đối chiếu pin với các file KiCad được cung cấp. Giữ lại LoRa E32, SIM7600E, DS3231, Firebase REST và FreeRTOS; loại bỏ DWIN, Relay, AutoMode và luồng lấy lệnh điều khiển. Gateway chỉ nhận telemetry `MyData` từ node, lưu vào microSD trước, rồi đồng bộ lịch sử lên Firebase khi có mạng.

> **Trạng thái xác minh:** đã rà soát/chỉnh source và kiểm tra tĩnh cấu trúc, nhưng môi trường hiện tại không có Arduino-ESP32 toolchain cùng bộ thư viện/board profile để xác nhận build firmware hoàn chỉnh hoặc chạy trên PCB thật. Vì vậy không tuyên bố đã compile end-to-end hay đã thử mạng/SD thực tế.

## 1. Phần cứng và pin theo PCB

PCB sử dụng **ESP32-S3-WROOM-1-N4**. Không chọn board ESP32-WROOM-32 thông thường trong Arduino IDE.

| Chức năng | GPIO trên PCB | Khai báo firmware |
|---|---:|---|
| microSD CS | 10 | `SD_CS_PIN` |
| microSD MOSI | 11 | `SD_MOSI_PIN` |
| microSD SCK | 12 | `SD_SCK_PIN` |
| microSD MISO | 13 | `SD_MISO_PIN` |
| microSD DET | 14 | `SD_DET_PIN` (chưa dùng làm điều kiện bắt buộc) |
| DS3231 SDA | 8 | `MY_SDA` |
| DS3231 SCL | 9 | `MY_SCL` |
| SIM7600 UART TX phía MCU | 17 | `SIM_TX` |
| SIM7600 UART RX phía MCU | 18 | `SIM_RX` |
| SIM7600 PWRKEY | 1 | `SIM_PWRKEY` |
| SIM7600 RESET | 2 | hiện chưa được điều khiển trong luồng chính |
| LoRa UART TX phía MCU | 15 | `LORA_TX` |
| LoRa UART RX phía MCU | 16 | `LORA_RX` |
| LoRa AUX | 21 | `PIN_AUX` |
| ESP32-S3 UART0 | GPIO43/44 | không dùng GPIO1 làm UART0 TX trên thiết kế S3 này |

M0/M1 của E32 không được nối vào MCU trong sơ đồ hiện tại. Firmware không còn cấu hình GPIO `0xFF` (GPIO không hợp lệ) và không gửi lệnh cấu hình E32 qua UART khi không thể đưa module vào mode CONFIG. E32 phải được cấu hình sẵn theo đúng baud/channel/address của node.

## 2. Cấu trúc source

- `Gateway_QuanTrac_SD_OfflineReplay.ino`: entry point Arduino, tên file khớp tên thư mục sketch.
- `ChuongTrinhChinh.cpp/.h`: nhận LoRa, lưu SD, replay Firebase, status/chart, reconnect 4G.
- `(đã loại bỏ chức năng SD)/.h`: spool CSV trên SD, cursor xác nhận, sửa dòng cuối chưa hoàn tất, bỏ qua dòng rỗng/hỏng để không kẹt queue.
- `Lora_E32.cpp/.h`: giao thức `MyData`, CRC16 và UART LoRa.
- `SIM7600E.cpp/.h`: driver AT/TinyGSM/HTTP cũ của dự án.
- `RTC.cpp/.h`: DS3231 trên GPIO8/9; đồng bộ thời gian hợp lệ từ `AT+CCLK?` qua SIM7600E; không còn phụ thuộc Wi-Fi/NTP.
- `ConfigFireBase.h`: URL/schema và placeholder cho thông tin xác thực Firebase.
- `Config.h`: công tắc log.

Không còn file `DWIN.*`, `Relay.*`, `AutoMode.h` trong project build này.

## 3. Cài đặt và build

1. Mở thư mục `Gateway_QuanTrac_SD_OfflineReplay` bằng Arduino IDE; chọn board ESP32-S3 phù hợp với module/board thực tế.
2. Cài Arduino core Espressif ESP32 tương thích với `SPIClass(FSPI)` và thư viện mà source SIM7600 hiện dùng (`TinyGsmClient.h`).
3. Cài đúng thư viện `DS3231.h` mà source ban đầu của nhóm sử dụng. Các thư viện có cùng tên nhưng API khác nhau có thể gây lỗi compile.
4. Định dạng thẻ microSD thành FAT32, lắp thẻ trước khi cấp nguồn.
5. Điền credential hợp lệ vào `DATABASE_SECRET` trong `ConfigFireBase.h` trên máy local. Không commit credential vào GitHub. Nếu Firebase project không còn hỗ trợ Database Secret, cần thay lớp xác thực theo cơ chế token phù hợp của project.
6. Kiểm tra APN SIM, đường dẫn RTDB, quyền ghi, schema dữ liệu và cấu hình E32 ở cả gateway lẫn node.
7. Compile trước khi nạp. Sau khi nạp, kiểm tra Serial Monitor 115200 baud.

## 4. Luồng lưu offline và replay

1. Khi nhận được gói LoRa có CRC hợp lệ, gateway tạo timestamp từ DS3231.
2. Firmware chỉ ghi mẫu khi thời gian RTC có vẻ hợp lệ (năm 2024–2099 và các trường ngày/giờ nằm trong giới hạn cơ bản). Nếu RTC chưa có thời gian hợp lệ, mẫu bị bỏ qua và có log cảnh báo; nên bảo đảm pin RTC còn tốt hoặc SIM đã đồng bộ giờ.
3. Mẫu được append vào `/pending.csv` trước khi uploader xử lý. `/pending.pos` lưu byte offset của bản ghi cũ nhất chưa xác nhận.
4. Khi có 4G, uploader ghi `/History/<key>.json` bằng PUT với key xác định từ nodeID + timestamp. Nếu HTTP timeout/reboot xảy ra sau khi Firebase nhận dữ liệu nhưng trước khi cursor được cập nhật, retry sẽ ghi đè cùng key thay vì tạo một bản ghi History mới.
5. Các bản ghi cũ được replay chỉ cập nhật `/History`; không làm `/Sensors` lùi về giá trị cũ. Bản ghi đang là telemetry mới nhất mới được phép cập nhật `/Sensors`.
6. Khi cursor đã tới cuối file, spool được làm rỗng để giới hạn dung lượng. Nếu phát hiện dòng cuối bị cắt do mất nguồn, firmware chép lại chỉ các dòng đã kết thúc bằng newline.
7. Dòng rỗng hoặc sai định dạng được bỏ qua có xác nhận để không chặn vĩnh viễn các dòng sau. Lỗi SD làm store chuyển trạng thái not-ready và task upload thử mount lại định kỳ.
8. HTTP 4xx không bị coi là lỗi sóng 4G. Bản ghi được giữ trên SD và retry chậm; kiểm tra credential, URL, quyền Firebase và payload nếu lỗi lặp lại.

### Giới hạn về độ tin cậy

- Nếu SD không có, đầy, hỏng hoặc bị rút, firmware không thể bảo đảm lưu bền vững mẫu mới. Nó phát log cảnh báo; không có bộ nhớ dự phòng vô hạn.
- `pending.csv` và `pending.pos` là định dạng đơn giản. Dù có sửa dòng cuối và cursor, vẫn cần kiểm thử mất nguồn thực tế; thẻ chất lượng tốt và nguồn ổn định rất quan trọng.
- History key dựa trên timestamp đến giây. Thiết kế giả định mỗi node không phát nhiều mẫu khác nhau trong cùng một giây; nếu giao thức node có thể làm vậy, nên thêm sequence number từ node vào `MyData` và dùng nó trong key.
- HTTP của driver SIM hiện dùng phương pháp HTTP method override để thực hiện PUT. Cần xác minh SIM7600 firmware, TLS, HTTP status và Firebase thật.
- `DATABASE_SECRET` trong file chỉ là placeholder. Không có credential hợp lệ thì upload sẽ không thành công.

## 5. Kiểm thử bắt buộc trên bàn

1. **SD cơ bản:** boot không thẻ → LoRa vẫn khởi động; gắn thẻ/khởi động lại → log `[SD] San sang`.
2. **Mất mạng:** cho Firebase/4G không khả dụng, phát ít nhất 5 mẫu → kiểm tra `pending.csv` có các dòng hoàn chỉnh và cursor chưa tiến.
3. **Replay:** bật lại mạng → kiểm tra từng key trong `/History`, cursor tăng và file spool được làm rỗng sau khi gửi hết.
4. **Idempotency:** ngắt nguồn ngay sau khi Firebase nhận History nhưng trước cursor ack → sau reboot, cùng key được PUT lại, không tạo key mới.
5. **Tail repair:** tạo bản sao test của `pending.csv` có dòng cuối không có newline → boot phải bỏ phần cuối chưa hoàn tất và vẫn đọc được các dòng hoàn chỉnh trước đó.
6. **HTTP 4xx:** dùng credential sai trên môi trường test → dữ liệu vẫn còn trên SD; sửa credential rồi reset/cho retry và xác nhận upload.
7. **RTC:** tháo pin RTC hoặc làm mất giờ, kiểm tra firmware không ghi timestamp giả; sau khi 4G có giờ hợp lệ, kiểm tra DS3231 được đồng bộ.
8. **Tải LoRa:** gửi liên tục ở tốc độ cao, theo dõi CRC, UART overflow, task stack high-water mark và số mẫu thiếu.

## 6. Schema Firebase giữ tương thích

- `/Sensors`: `{"R":"temperature;humidity;sensor2[0];...;sensor2[6];Vbat"}`.
- `/History/<deterministic-key>`: `{"timestamp":"YYYY-MM-DDTHH:MM:SS","nodeID":N,"R":"..."}`.
- `/Status` và `/Chart` vẫn theo schema cũ ở mức source; cần đối chiếu dashboard hiện tại trước khi triển khai.

## 7. Các lỗi đã xử lý trong source

- Đổi pin SD về GPIO10/11/12/13 đúng PCB; I2C về GPIO8/9.
- Bỏ phụ thuộc Wi-Fi/NTP khỏi `RTC.cpp`; kiểm tra cơ bản thời gian SIM trước khi cập nhật RTC.
- Không ghi dữ liệu cũ replay vào `/Sensors`.
- Đổi History từ POST tạo key ngẫu nhiên sang PUT key xác định để retry idempotent.
- Dòng trống/sai định dạng không làm uploader kẹt; dòng cuối chưa hoàn tất được loại bỏ khi mount.
- Retry mount SD nếu lỗi; không chặn việc khởi tạo task LoRa chỉ vì 4G đang mất mạng.
- HTTP 4xx không kích hoạt reconnect 4G như lỗi radio; queue giữ nguyên dữ liệu và retry chậm.
- Đồng bộ truy cập RTC/modem/telemetry giữa task bằng mutex.
- Không xóa toàn bộ UART LoRa sau một frame CRC lỗi; AUX có timeout; không thao tác GPIO255 cho M0/M1 không nối.
- Chart không post nếu đã quá 20 phút không có telemetry mới.


## Bổ sung: phục hồi RTC, modem và chẩn đoán LoRa

- Nếu DS3231 chưa có ngày giờ hợp lệ, mẫu vẫn được ghi vào SD với timestamp `UNSYNCED_<millis>`; khi upload, Firebase nhận `timestamp: null` và `timeValid: false`. Firmware không tự suy đoán thời điểm lịch sử. Các mẫu này cần được xử lý riêng nếu yêu cầu dashboard bắt buộc có timestamp chuẩn.
- Đồng bộ DS3231 từ SIM chỉ khi RTC không hợp lệ hoặc sai lệch không quá 12 giờ. Nếu lệch lớn hơn 12 giờ, firmware từ chối ghi đè để tránh phá giờ đúng do SIM/timezone cấu hình sai. Cần kiểm thử AT+CCLK/CTZU và múi giờ trên SIM thực tế.
- Sau 3 lần kết nối 4G thất bại liên tiếp, firmware có nhánh phục hồi SIM7600E qua GPIO2 nhưng `ENABLE_SIM_HARD_RESET` mặc định bằng `0`. Chỉ bật thành `1` sau khi xác nhận cực tính RESET trên PCB/module và thử bằng nguồn an toàn.
- LoRa_RX stack tăng lên 6144 bytes và in mức stack trống thấp nhất mỗi 30 giây. Đây là số liệu chẩn đoán; sau test tải dài, điều chỉnh lại nếu cần.
- LoRa resync bỏ tối đa 64 byte rác mỗi vòng, tránh vừa quá chậm vừa chiếm CPU không giới hạn.
- `ENABLE_ALL_DEBUG=0` tắt log qua các macro SIM, LoRa, Main và SD; Serial.begin vẫn được giữ để có thể bật debug lại.

## Checklist bắt buộc trên phần cứng

1. Khởi động với DS3231 đúng giờ và kiểm tra firmware không ghi đè giờ nếu SIM báo lệch bất thường.
2. Ngắt 4G, tiếp tục gửi LoRa; kiểm tra pending.csv có bản ghi bình thường hoặc `UNSYNCED_...`, không bị bỏ mẫu do RTC chưa hợp lệ.
3. Khôi phục 4G; kiểm tra History nhận `timeValid` đúng với từng loại timestamp và `/Sensors` chỉ phản ánh mẫu hiện tại.
4. Thử SIM không có sóng trong ít nhất 3 chu kỳ retry; quan sát reset GPIO2 và khả năng đăng ký lại mạng. Xác nhận reset pin có đúng cực tính theo sơ đồ module trước khi thử.
5. Tạo nhiễu UART LoRa có kiểm soát, kiểm tra frame hợp lệ tiếp theo vẫn được nhận.
6. Chạy ít nhất vài giờ với tải thực tế, ghi nhận stack watermark và backlog SD.

Các thay đổi trên là cập nhật source tĩnh; chưa được compile bằng toolchain Arduino-ESP32 hoặc kiểm thử trên PCB trong môi trường này.

---

## Firebase trực tiếp không Authentication (CHỈ THỬ NGHIỆM)

Firmware hiện được cấu hình cho Realtime Database của dự án `lora-ad871` và nhánh:

`Devices_7/GW001`

Cấu trúc dự kiến:

- `Sensors/R`: dữ liệu cảm biến mới nhất
- `History/<record-key>`: từng bản ghi lịch sử
- `Status/S`: thời gian và trạng thái mạng Gateway
- `Chart/<dd-mm-yy>/<hh-mm>`: điểm biểu đồ

### Rules thử nghiệm

Chỉ dùng khi kiểm tra trên dữ liệu không nhạy cảm. Trong Firebase Console → Realtime Database → Rules, có thể tạm đặt:

```json
{
  "rules": {
    "Devices_7": {
      "GW001": {
        ".read": true,
        ".write": true
      }
    }
  }
}
```

Nhấn Publish. Quy tắc này cho phép bất kỳ ai biết URL ghi/đọc/xóa dữ liệu trong nhánh `Devices_7/GW001`; không dùng cho triển khai thực tế. Sau khi kiểm thử, hãy đóng quyền công khai và chuyển sang xác thực hoặc máy chủ trung gian.

### Kiểm tra sau khi nạp firmware

1. Mở Serial Monitor theo baud rate được cấu hình trong `setup`.
2. Đảm bảo SIM7600E đã đăng ký mạng và có PDP/Internet.
3. Khi Gateway nhận gói LoRa, tìm log `[UPLOAD]` và `[HTTP]`.
4. Trong Realtime Database, mở `Devices_7/GW001` và kiểm tra `Sensors/R`, `History`, `Status/S`, `Chart`.
5. HTTP 200/2xx chỉ xác nhận request được chấp nhận; hãy kiểm tra nội dung thực tế trong Database.

### Ghi chú kiểm thử

Bản này cập nhật URL Firebase và chuyển yêu cầu ghi có đường dẫn cố định sang header `X-HTTP-Method-Override: PUT` trong request POST của SIM7600. Cần xác minh trên đúng firmware SIM7600E rằng lệnh `AT+HTTPPARA="USERDATA"` được hỗ trợ và header được gửi đi. Chưa thể khẳng định hoạt động trên phần cứng thực chỉ bằng sửa mã nguồn; nếu Serial Monitor báo không thể đặt header hoặc Firebase trả lỗi HTTP, cần kiểm tra phản hồi AT và phiên bản firmware modem.


## Phiên bản không dùng thẻ SD

Phiên bản này không mount/đọc/ghi thẻ SD và không có hàng đợi offline. Mẫu LoRa mới nhất được giữ trong RAM, sau đó gửi trực tiếp lên Firebase qua SIM7600E. Nếu mạng hoặc Firebase lỗi, firmware thử gửi lại mẫu hiện tại; khi có mẫu mới hơn, mẫu cũ chưa gửi có thể bị thay thế. Dữ liệu RAM mất khi Gateway khởi động lại hoặc mất nguồn.
