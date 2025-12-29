#include "blynk.h"                // Library utama untuk Blynk
#include <WiFiManager.h>          // Library untuk auto connect WiFi
#include <WiFi.h>                 // Library WiFi bawaan ESP32
#include "secrets.h"              // File rahasia berisi API key dan token
#include "ThingSpeak.h"           // Library untuk kirim data ke ThingSpeak
#include <Wire.h>                 // Library komunikasi I2C
#include "Adafruit_HTU21DF.h"     // Library sensor suhu & kelembapan HTU21
#include <BlynkSimpleEsp32.h>     // Library koneksi Blynk untuk ESP32
#include "RTClib.h"               // Library untuk modul RTC DS3231
#include <FS.h>                   // Library sistem file (digunakan untuk SD)
#include <SD.h>                   // Library SD Card
#include "esp_sleep.h"            // Library untuk fitur deep sleep ESP32

WiFiClient client;                // Objek client untuk koneksi ThingSpeak


unsigned long myChannelNumber = SECRET_CH_ID; // Nomor channel ThingSpeak
const char *myWriteAPIKey = SECRET_WRITE_APIKEY; // API Key ThingSpeak


Adafruit_HTU21DF htu = Adafruit_HTU21DF();     // Inisialisasi sensor HTU21
RTC_DS3231 rtc;                                // Inisialisasi RTC DS3231
DateTime now;                                  // Variabel untuk menyimpan waktu RTC
char timeBuffer[20];                           // Buffer untuk jam (HH:MM:SS)
char dateBuffer[20];                           // Buffer untuk tanggal (DD/MM/YYYY)


float suhu = 0.0;                              // Variabel suhu
float kelembaban = 0.0;                        // Variabel kelembapan
String myStatus = "";                          // Status untuk ThingSpeak


int ledError = 14;                              // Pin LED biru (indikasi error)
int ledOK = 12;                                // Pin LED putih (indikasi normal)

File myFile;                                   // Objek file untuk SD Card

void setup() {
  Serial.begin(115200);                        // Mulai komunikasi serial
  pinMode(ledError, OUTPUT);                   // Set pin LED error sebagai output
  pinMode(ledOK, OUTPUT);                      // Set pin LED OK sebagai output
  digitalWrite(ledOK, HIGH);                          // dulu LED OK di awal


  WiFiManager wm;                              // Objek WiFiManager
  bool res = wm.autoConnect("Nhunt", "12345678"); // Buat hotspot WiFi dan koneksi otomatis

  if (!res) {                                  // Jika WiFi gagal terkoneksi
    Serial.println("Gagal koneksi WiFi!");
    digitalWrite(ledError, HIGH);              // Nyalakan LED error
    digitalWrite(ledOK, LOW);                  // Matikan LED OK
    delay(200);
  } else {                                     // Jika berhasil koneksi
    Serial.println("WiFi tersambung...");
    digitalWrite(ledOK, HIGH);                 // Nyalakan LED OK
    Blynk.begin(BLYNK_AUTH_TOKEN, WiFi.SSID().c_str(), WiFi.psk().c_str()); // Koneksi ke Blynk
    Blynk.virtualWrite(V8, "WiFi tersambung...");
  }

  ThingSpeak.begin(client);                    // Mulai koneksi ke ThingSpeak
  
  if (!htu.begin()) {                          // Cek koneksi sensor HTU21
    Serial.println("Sensor HTU21 tidak terdeteksi!");
    Blynk.virtualWrite(V8, "Sensor HTU21 tidak terdeteksi!");
    for (int i = 0; i < 2; i++) {              // Kedipkan LED error 2 kali
      digitalWrite(ledError, HIGH); delay(300);
      digitalWrite(ledError, LOW); delay(300);
    }
    delay(2000);
    while (1);                                 // Hentikan program
  } else {
    Serial.println("Sensor HTU21 terhubung."); // Sensor HTU21 aktif
    digitalWrite(ledOK, HIGH);                 // Nyalakan LED OK
    Blynk.virtualWrite(V8, "Sensor HTU21 terhubung.");
  }

  if (!rtc.begin()) {                          // Cek koneksi RTC DS3231
    Serial.println("RTC tidak terdeteksi!");
    Blynk.virtualWrite(V8, "RTC tidak terdeteksi!");
    for (int i = 0; i < 3; i++) {              // Kedipkan LED error 3 kali
      digitalWrite(ledError, HIGH); delay(400);
      digitalWrite(ledError, LOW); delay(400);
    }
    delay(2000);
    while (1);                                 // Hentikan program
  } else {
    Serial.println("RTC terdeteksi!");
    Blynk.virtualWrite(V8, "RTC terdeteksi");
  }

  if (rtc.lostPower()) {                       // Jika RTC kehilangan daya
    Serial.println("RTC kehilangan daya, waktu perlu diatur ulang!");
    // rtc.adjus(DateTime(F(__DATE__), F(__TIME__))); // Atur waktu otomatis (opsional)
  }

  if (!SD.begin()) {                           // Inisialisasi SD Card
    Serial.println("Gagal inisialisasi SD Card!");
    Blynk.virtualWrite(V8, "Gagal inisialisasi SD Card!");
    for (int i = 0; i < 4; i++) {              // Kedipkan LED error 4 kali
      digitalWrite(ledError, HIGH); delay(500);
      digitalWrite(ledError, LOW); delay(500);
    }delay(2000);
  } else {
    Serial.println("SD Card berhasil diinisialisasi."); // SD Card siap
    Blynk.virtualWrite(V8, "SD Card berhasil diinisialisasi.");
  }
    // Mode hemat daya WiFi tanpa memutus koneksi
  WiFi.setSleep(true);               // Aktifkan WiFi modem sleep
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM); // Gunakan mode hemat daya paling rendah

}

void loop() {
  Blynk.run();                                 // Jalankan fungsi Blynk

  suhu = htu.readTemperature();                // Baca suhu dari sensor
  kelembaban = htu.readHumidity();             // Baca kelembapan dari sensor

  if (isnan(suhu) || isnan(kelembaban)) {  // Jika pembacaan dari HTU21 invalid
    Blynk.virtualWrite(V8, "Gagal membaca HTU21!");
    for (int i = 0; i < 2; i++) {              // Kedipkan LED error 2 kali
      digitalWrite(ledError, HIGH); 
      delay(300);
      digitalWrite(ledError, LOW); 
      delay(300);
  }
  delay(2000);
  }

  now = rtc.now();                             // Baca waktu dari RTC
  sprintf(timeBuffer, "%02d:%02d:%02d", now.hour(), now.minute(), now.second()); // Format jam
  sprintf(dateBuffer, "%02d/%02d/%04d", now.day(), now.month(), now.year());     // Format tanggal

  Serial.println("=========================="); // Pemisah di Serial Monitor
  Serial.print("Suhu: "); Serial.print(suhu); Serial.println(" °C");            // Tampilkan suhu
  Serial.print("Kelembaban: "); Serial.print(kelembaban); Serial.println(" %"); // Tampilkan kelembapan
  Serial.print("Waktu: "); Serial.println(timeBuffer);                          // Tampilkan waktu
  Serial.print("Tanggal: "); Serial.println(dateBuffer);                        // Tampilkan tanggal

  ThingSpeak.setField(1, suhu);               // Isi field 1 dengan suhu
  ThingSpeak.setField(2, kelembaban);         // Isi field 2 dengan kelembapan
  ThingSpeak.setStatus("Update dari HTU21 + RTC"); // Status kiriman

  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey); // Kirim ke ThingSpeak
  if (x == 200) {                            // 200 = sukses
    Serial.println("ThingSpeak update berhasil.");
    Blynk.virtualWrite(V9, "ThingSpeak update berhasil.");
  } else {                                   // Selain itu = gagal
    Serial.println("Gagal kirim ke ThingSpeak! Kode error: " + String(x));
    Blynk.virtualWrite(V9, "Gagal kirim ke ThingSpeak! Kode error: " + String(x));
  }

  myFile = SD.open("/data_logTest.txt", FILE_APPEND); // Buka file log di SD Card (mode append)
  if (myFile) {                             // Jika file berhasil dibuka
    myFile.print(dateBuffer);               // Tulis tanggal
    myFile.print(" ");                      // Spasi
    myFile.print(timeBuffer);               // Tulis waktu
    myFile.print(", Suhu: ");               // Label suhu
    myFile.print(suhu);                     // Nilai suhu
    myFile.print(, Kelembapan: ");       // Label kelembapan
    myFile.print(kelembaban);               // Nilai kelembapan
    myFile.print(, HTTP: ");
    myFile.print(x);
    myFile.println(" ");                   // Akhiri baris
    myFile.close();                         // Tutup file

    Serial.println("Data tersimpan ke SD Card."); // Konfirmasi di Serial
    Blynk.virtualWrite(V9, "Data tersimpan ke SD Card"); // Kirim notifikasi ke Blynk
  } else {                                  // Jika file gagal dibuka
    Serial.println("Gagal menyimpan data ke SD Card!");
    for (int i = 0; i < 5; i++) {           // LED error berkedip 5 kali
      digitalWrite(ledError, HIGH); delay(500);
      digitalWrite(ledError, LOW); delay(500);
    }
    delay(2000);
    Blynk.virtualWrite(V9, "Gagal menyimpan data ke SD Card!"); // Notifikasi ke Blynk
  }

  Blynk.virtualWrite(V2, suhu);              // Kirim suhu ke Blynk (Virtual Pin V2)
  Blynk.virtualWrite(V3, kelembaban);        // Kirim kelembapan ke Blynk (Virtual Pin V3)
  Blynk.virtualWrite(V4, timeBuffer);        // Kirim waktu ke Blynk (Virtual Pin V4)
  Blynk.virtualWrite(V5, dateBuffer);        // Kirim tanggal ke Blynk (Virtual Pin V5)

    // ---- Masuk light sleep tapi WiFi tetap aktif ----
  Serial.println("Masuk mode sleep ringan (WiFi tetap aktif)...");
  Blynk.virtualWrite(V8, "Sleep ringan 20 detik...");

  // Pastikan semua data sudah terkirim dulu
  WiFi.setSleep(true);

  // Gunakan delay non-blocking agar tetap bisa jaga koneksi Blynk
  unsigned long startSleep = millis();
  while (millis() - startSleep < 20000) {
    Blynk.run();  // Blynk tetap aktif selama tidur ringan
    delay(100);
  }

  Serial.println("Bangun dari sleep ringan!");
  Blynk.virtualWrite(V8, "Bangun dari sleep ringan!");

}
