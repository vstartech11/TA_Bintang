#include "esp_camera.h"
#include <WiFi.h>
#include "soc/soc.h"           // Disable brownout detector
#include "soc/rtc_cntl_reg.h"   // Disable brownout detector

// Ganti dengan SSID dan password WiFi Anda
const char* ssid = "ESP32HUB";
const char* password = "12345678";

// Web server di port 80
WiFiServer server(80);

TaskHandle_t TaskHandleSendFrame;
camera_fb_t * fb = NULL; // Buffer untuk frame

// Fungsi untuk inisialisasi kamera
void startCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;   // Y2_GPIO_NUM
  config.pin_d1 = 18;  // Y3_GPIO_NUM
  config.pin_d2 = 19;  // Y4_GPIO_NUM
  config.pin_d3 = 21;  // Y5_GPIO_NUM
  config.pin_d4 = 36;  // Y6_GPIO_NUM
  config.pin_d5 = 39;  // Y7_GPIO_NUM
  config.pin_d6 = 34;  // Y8_GPIO_NUM
  config.pin_d7 = 35;  // Y9_GPIO_NUM
  config.pin_xclk = 0; // XCLK_GPIO_NUM
  config.pin_pclk = 22; // PCLK_GPIO_NUM
  config.pin_vsync = 25; // VSYNC_GPIO_NUM
  config.pin_href = 23;  // HREF_GPIO_NUM
  config.pin_sscb_sda = 26; // SIOD_GPIO_NUM
  config.pin_sscb_scl = 27; // SIOC_GPIO_NUM
  config.pin_pwdn = 32; // PWDN_GPIO_NUM
  config.pin_reset = -1; // RESET_GPIO_NUM
  config.xclk_freq_hz = 20000000;         // XCLK Frequency
  config.pixel_format = PIXFORMAT_JPEG;   // Set JPEG format

  // Mengatur resolusi dan menambah buffer frame
  config.frame_size = FRAMESIZE_QVGA;      // Resolusi QVGA (320x240)
  config.jpeg_quality = 40;               // Kualitas JPEG (40 = lebih cepat)
  config.fb_count = 2;                    // Menambah buffer frame untuk stabilitas

  // Inisialisasi kamera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Gagal menginisialisasi kamera: 0x%x", err);
    return;
  }
}

// Fungsi untuk pengambilan frame dari kamera
void captureFrame(void * parameter) {
  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Gagal menangkap gambar"); // Tetap memunculkan error ini untuk debug
      delay(100);  // Delay sebelum mencoba lagi
    } else {
      // Jika frame berhasil diambil, tunggu sebentar sebelum mengambil lagi
      delay(10);  // Pengurangan delay untuk meningkatkan FPS
    }
  }
}

// Fungsi untuk mengirimkan frame ke client
void sendFrame(void * parameter) {
  while (true) {
    WiFiClient client = server.available();
    
    if (!client) {
      continue; // Jika tidak ada client, tidak ada yang dilakukan
    }

    // Menunggu permintaan dari client
    String request = client.readStringUntil('\r');
    client.flush();

    // Membangun header MJPEG
    if (request.indexOf("GET / ") >= 0) {
      String response = "HTTP/1.1 200 OK\r\n";
      response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
      client.print(response);

      // Menghilangkan timeout agar streaming terus berjalan
      while (client.connected()) {
        if (fb) {
          // Membuat boundary frame untuk MJPEG
          client.printf("--frame\r\n");
          client.printf("Content-Type: image/jpeg\r\n");
          client.printf("Content-Length: %u\r\n\r\n", fb->len);
          client.write(fb->buf, fb->len);
          client.printf("\r\n");

          // Membebaskan buffer setelah pengiriman
          esp_camera_fb_return(fb);
          fb = NULL;
        }

        // Mengurangi delay untuk menjaga frame rate
        delay(15);
      }
    } else {
      // Jika request tidak ditemukan
      client.println("HTTP/1.1 404 Not Found\r\n\r\n");
    }
  }
}

// Fungsi untuk mencoba menghubungkan kembali ke WiFi
void reconnectWiFi() {
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Mencoba menyambung ulang ke WiFi...");
    WiFi.begin(ssid, password);
    delay(5000); // Tunggu 5 detik sebelum mencoba lagi
  }
  Serial.println("WiFi Terhubung Kembali");
}

void setup() {
  Serial.begin(115200);
  // Disable brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Mematikan brownout detector
  // Menghubungkan ke jaringan WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi Terhubung");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Memulai kamera
  startCamera();

  // Memulai task FreeRTOS untuk pengambilan dan pengiriman frame
  xTaskCreate(captureFrame, "TaskCaptureFrame", 8192, NULL, 1, NULL); // Memperbesar stack size
  xTaskCreate(sendFrame, "TaskSendFrame", 8192, NULL, 1, &TaskHandleSendFrame); // Memperbesar stack size

  // Memulai web server
  server.begin();
}

void loop() {
  // Periksa apakah WiFi terputus, jika ya, coba sambungkan ulang
  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi();
  }

  // Task sendFrame ditangani oleh FreeRTOS, sehingga tidak perlu di loop utama
}
