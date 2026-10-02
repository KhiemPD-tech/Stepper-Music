# 🎵 STM32 Musical Stepper Motors (Hệ thống 3 Động cơ bước phát nhạc)

Dự án điều khiển **3 động cơ bước (NEMA 17)** phát nhạc đa âm (3 kênh / 3-voice polyphony) thông qua vi điều khiển **STM32F103C8T6 (Blue Pill)** và driver **A4988**.

---

## 🛠 1. Sơ đồ tín hiệu điều khiển (STM32 $\rightarrow$ Driver A4988)

Các kênh PWM / Timer hoặc GPIO phát xung tần số âm thanh từ STM32 được đấu nối đến các driver tương ứng:

| Động cơ / Kênh | Chân STM32 | Chân Driver A4988 | Chức năng |
| :--- | :--- | :--- | :--- |
| **Motor 1** (Kênh 1) | `PA0` | **STEP** | Xung tạo tần số nốt nhạc |
| | `PA1` | **DIR** | Hướng quay động cơ |
| **Motor 2** (Kênh 2) | `PA6` | **STEP** | Xung tạo tần số nốt nhạc |
| | `PA7` | **DIR** | Hướng quay động cơ |
| **Motor 3** (Kênh 3) | `PB6` | **STEP** | Xung tạo tần số nốt nhạc |
| | `PB7` | **DIR** | Hướng quay động cơ |

> **Lưu ý:** Nếu cấu hình trong STM32CubeMX theo cặp chân liền kề khác (ví dụ `PA0-PA1`, `PA2-PA3`, `PA4-PA5`), hãy đấu nối theo đúng nhãn chân Timer đã sinh mã.

---

## ⚙️ 2. Thiết lập trên từng Driver A4988

* **RESET & SLEEP:** Trên mỗi driver A4988, **nối tắt (jumper/hàn chập) chân `RESET` và `SLEEP`** với nhau để kích hoạt driver hoạt động liên tục.
* **ENABLE:** 
  * Để trống (mặc định kéo xuống LOW = kích hoạt liên tục).
  * *Hoặc* nối chung cả 3 chân `ENABLE` về 1 chân GPIO của STM32 để bật/tắt toàn bộ hệ thống bằng phần mềm.
* **Cấu hình vi bước (MS1, MS2, MS3):** 
  * Để ngỏ hoặc nối cả 3 chân xuống `GND` để chạy ở chế độ **Full Step**. Chế độ Full Step mang lại âm lượng to, đanh và rõ nhất khi phát nhạc.

---

## ⚡ 3. Sơ đồ cấp nguồn (Power Delivery)

### Nguồn động lực (Nguồn ngoài 12V / 24V cho động cơ)
* **+12V / +24V (Cực dương):** Nối vào chân **`VMOT`** của cả 3 driver A4988.
* **GND nguồn ngoài (Cực âm):** Nối vào chân **`GND`** (chân GND nằm ngay cạnh chân `VMOT`) của cả 3 driver.
* **Tụ lọc bảo vệ (Bắt buộc):** Mắc 1 tụ hóa (**$100\mu\text{F} - 1000\mu\text{F}$**, điện áp chịu đựng $\ge 25\text{V}$) song song ngay sát cặp chân `VMOT` và `GND` của mỗi module driver để dập xung điện áp ngược khi động cơ đảo chiều/ngắt nốt.

### Nguồn Logic
* **3.3V (hoặc 5V) từ STM32:** Nối vào chân **`VDD`** của cả 3 driver.
* **GND STM32:** Nối vào chân **`GND`** (chân GND nằm cạnh `VDD`) của cả 3 driver.

> ⚠️ **Quy tắc Chung Mass (Common GND):** Bắt buộc phải nối thông toàn bộ **GND nguồn 12V**, **GND của 3 driver**, và **GND của STM32** lại với nhau để đồng bộ mức tín hiệu logic.

---

## 🔄 4. Đấu nối Driver A4988 $\rightarrow$ Động cơ bước (4 dây Bipolar)

Mỗi động cơ bước hai pha (như NEMA 17) gồm 2 cuộn dây độc lập:

* **Cuộn dây 1:** Nối vào chân **`1A`** và **`1B`** của A4988.
* **Cuộn dây 2:** Nối vào chân **`2A`** và **`2B`** của A4988.

### 💡 Mẹo xác định cặp dây cuộn:
1. Chập thử 2 dây bất kỳ của động cơ lại với nhau và dùng tay vặn trục.
2. Nếu trục bị bó cứng, khó quay hơn bình thường $\rightarrow$ Hai dây đó thuộc cùng 1 cuộn dây.
3. Cặp còn lại sẽ thuộc cuộn dây thứ hai.