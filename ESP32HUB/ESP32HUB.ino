#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include "MPU9250.h"

#define MPU9250_ADDRESS 0x68  // Alamat I2C MPU9250
#define PCA9548A_ADDRESS 0x77  // Alamat PCA9548A
#define RCWL1601_DEFAULT_ADDRESS 0x57 // alamat RCWL-1601
#define WIFI_SSID "ESP32HUB"    // Nama Wi-Fi
#define WIFI_PASSWORD "12345678" // Password Wi-Fi
#define WIFI_PORT 80             // Port web server
#define SDA_PIN 18               // Pin SDA untuk I2C
#define SCL_PIN 19               // Pin SCL untuk I2C
#define GYRO_SCALE_2000DPS 16.4  // Sensitivitas giroskop untuk ±2000 dps, 16.4 LSB/dps
#define G_FORCE_CONVERSION 0.061 // Konversi dari dps ke G (1 G = 9.81 m/s²)

WebServer server(WIFI_PORT);
unsigned long previousMillis = 0;
const long interval = 500;  // Interval 500ms untuk mencetak data gyroskop

template <typename WireType = TwoWire>
uint8_t readByte(uint8_t address, uint8_t subAddress, WireType& wire = Wire) {
    uint8_t data = 0;
    wire.beginTransmission(address);
    wire.write(subAddress);
    wire.endTransmission(false);
    wire.requestFrom(address, (size_t)1);
    if (wire.available()) data = wire.read();
    return data;
}

// Fungsi untuk membaca data giroskop dan mengonversi ke satuan G
void readGyroData(float &gAccumulated) {
    int16_t gyroX = (readByte(MPU9250_ADDRESS, GYRO_XOUT_H) << 8) | readByte(MPU9250_ADDRESS, GYRO_XOUT_L);
    int16_t gyroY = (readByte(MPU9250_ADDRESS, GYRO_YOUT_H) << 8) | readByte(MPU9250_ADDRESS, GYRO_YOUT_L);
    int16_t gyroZ = (readByte(MPU9250_ADDRESS, GYRO_ZOUT_H) << 8) | readByte(MPU9250_ADDRESS, GYRO_ZOUT_L);

    float gx = gyroX / GYRO_SCALE_2000DPS; // Konversi ke dps
    float gy = gyroY / GYRO_SCALE_2000DPS; // Konversi ke dps
    float gz = gyroZ / GYRO_SCALE_2000DPS; // Konversi ke dps

    // Konversi dps ke G (1 G = 9.81 m/s²)
    gx *= G_FORCE_CONVERSION;
    gy *= G_FORCE_CONVERSION;
    gz *= G_FORCE_CONVERSION;

    // Menghitung akumulasi dari X, Y, dan Z
    gAccumulated = sqrt(gx * gx + gy * gy + gz * gz);
}

// Fungsi untuk mencetak akumulasi g-force dari giroskop ke Serial setiap 500ms
void printGyroData() {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        float gAccumulated;
        readGyroData(gAccumulated);
        Serial.print("Akumulasi G: "); Serial.print(gAccumulated); Serial.println(" G");
        Serial.println("---------------------------");
    }
}

void readSensorData(byte kanal, String& jsonResponse) {
    Wire.beginTransmission(PCA9548A_ADDRESS);
    Wire.write(1 << kanal); // Pilih kanal pada PCA9548A
    Wire.endTransmission();

    Wire.beginTransmission(RCWL1601_DEFAULT_ADDRESS);
    Wire.write(1); // Mengirim nilai '1' untuk memulai sesi pengukuran
    Wire.endTransmission();

    delay(60); // Tunggu sensor

    Wire.requestFrom(RCWL1601_DEFAULT_ADDRESS, 3);
    if (Wire.available() == 3) {
        uint8_t byte1 = Wire.read();
        uint8_t byte2 = Wire.read();
        uint8_t byte3 = Wire.read();
        unsigned long distance_um = (byte1 << 16) | (byte2 << 8) | byte3;
        jsonResponse += ", \"distance_cm\": " + String(distance_um / 10000.0); // Jarak dalam cm
    } else {
        jsonResponse += ", \"distance_cm\": -1"; // Indikator tidak terdeteksi
    }
}

// Fungsi untuk menangani root URL "/"
void handleRoot() {
    server.send(200, "text/html", "<h1>Selamat datang di ESP32 Web Server!</h1>");
}

// Fungsi untuk menangani endpoint "/sensor"
void handleSensor() {
    String jsonResponse = "{";
    const char* namaSensor[3] = {"Kiri", "Tengah", "Kanan"};
    byte kanal[3] = {6, 5, 1};

    for (byte urutanSensor = 0; urutanSensor < 3; urutanSensor++) {
        readSensorData(kanal[urutanSensor], jsonResponse);
    }

    jsonResponse += "}"; // Akhiri JSON string
    server.send(200, "application/json", jsonResponse);
}

// Fungsi untuk menangani endpoint "/sensorSamping"
void handleSensorSamping() {
    String jsonResponse = "{";
    const char* namaSensor[2] = {"Kiri", "Kanan"};
    byte kanal[2] = {7, 0};

    for (byte urutanSensor = 0; urutanSensor < 2; urutanSensor++) {
        readSensorData(kanal[urutanSensor], jsonResponse);
    }

    jsonResponse += "}"; // Akhiri JSON string
    server.send(200, "application/json", jsonResponse);
}

void setup() {
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Access Point IP: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", handleRoot);
    server.on("/sensor", handleSensor);
    server.on("/sensorSamping", handleSensorSamping);
    server.begin();
    Serial.println("Web server dimulai.");
}

void loop() {
    server.handleClient(); // Menangani permintaan HTTP
    printGyroData();       // Mencetak akumulasi g-force setiap 500ms
}
