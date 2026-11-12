// Sustitutos mínimos de Arduino-ESP32, FreeRTOS, PubSubClient y DHT.
// Solo sirven para compilar y probar la lógica de sketch.ino en un PC.
// NO simulan red real, tiempos reales ni hardware.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <cmath>
#include <deque>
#include <string>
#include <vector>
#include <functional>
using std::isfinite;

typedef uint8_t byte;
typedef bool boolean;
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define DHT22 22
#define BIT0 1u
#define WIFI_STA 1
#define WL_CONNECTED 3
#define WL_DISCONNECTED 6
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

// ---------- Estado simulado, editable desde la prueba ----------
struct Mock {
  uint32_t ms = 0;
  int adcSuelo = 2048, adcLuz = 2048;
  unsigned long pulso = 1166;              // ≈ 20 cm
  float temp = 24.0f, hum = 65.0f;
  int wifi = WL_CONNECTED;
  int pinBomba = 1, pinLuz = 1;
  std::string serial;                      // todo lo que el sketch imprime
  std::string serialEntrada;               // lo que "escribe" el usuario
  int tareasCreadas = 0;
  int limiteDelays = -1;                   // si >=0, vTaskDelay lanza StopTask al agotarse
  int delays = 0;
};
extern Mock mock;
struct StopTask {};
struct ColaVacia {};

inline unsigned long millis() { return mock.ms; }
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned int) {}
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t pin, uint8_t v) {
  if (pin == 26) mock.pinBomba = v;
  if (pin == 27) mock.pinLuz = v;
}
inline int analogRead(uint8_t pin) { return pin == 34 ? mock.adcSuelo : mock.adcLuz; }
inline void analogReadResolution(int) {}
inline unsigned long pulseIn(uint8_t, uint8_t, unsigned long) { return mock.pulso; }
inline uint32_t esp_random() { static uint32_t x = 0x1234abcd; x = x * 1664525u + 1013904223u; return x; }
struct EspClass { unsigned long long getEfuseMac() { return 0x24a1b2c3d4ULL; } };
extern EspClass ESP;

struct HardwareSerial {
  void begin(unsigned long) {}
  void println(const char* s) { mock.serial += s; mock.serial += "\n"; }
  void printf(const char* f, ...) __attribute__((format(printf, 2, 3))) {
    char b[1024]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a); mock.serial += b;
  }
  int available() { return mock.serialEntrada.empty() ? 0 : 1; }
  int read() { if (mock.serialEntrada.empty()) return -1; char c = mock.serialEntrada[0]; mock.serialEntrada.erase(0, 1); return c; }
};
extern HardwareSerial Serial;

// ---------- FreeRTOS ----------
typedef int BaseType_t;
typedef uint32_t TickType_t;
typedef uint32_t EventBits_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY 0xFFFFFFFFu
#define pdMS_TO_TICKS(x) (x)
struct ColaImpl { size_t item, cap; std::deque<std::vector<uint8_t>> q; };
typedef ColaImpl* QueueHandle_t;
struct GrupoImpl { EventBits_t bits = 0; };
typedef GrupoImpl* EventGroupHandle_t;
typedef void* SemaphoreHandle_t;
typedef void* TaskHandle_t;
inline QueueHandle_t xQueueCreate(size_t cap, size_t item) { return new ColaImpl{item, cap, {}}; }
inline BaseType_t xQueueSend(QueueHandle_t q, const void* p, TickType_t) {
  if (q->q.size() >= q->cap) return pdFALSE;
  q->q.emplace_back((const uint8_t*)p, (const uint8_t*)p + q->item); return pdTRUE;
}
inline BaseType_t xQueueOverwrite(QueueHandle_t q, const void* p) { q->q.clear(); return xQueueSend(q, p, 0); }
inline BaseType_t xQueueReceive(QueueHandle_t q, void* p, TickType_t t) {
  if (q->q.empty()) { if (t == portMAX_DELAY) throw ColaVacia{}; return pdFALSE; }
  memcpy(p, q->q.front().data(), q->item); q->q.pop_front(); return pdTRUE;
}
inline BaseType_t xQueuePeek(QueueHandle_t q, void* p, TickType_t) {
  if (q->q.empty()) return pdFALSE;
  memcpy(p, q->q.front().data(), q->item); return pdTRUE;
}
inline EventGroupHandle_t xEventGroupCreate() { return new GrupoImpl(); }
inline EventBits_t xEventGroupGetBits(EventGroupHandle_t g) { return g->bits; }
inline EventBits_t xEventGroupSetBits(EventGroupHandle_t g, EventBits_t b) { g->bits |= b; return g->bits; }
inline EventBits_t xEventGroupClearBits(EventGroupHandle_t g, EventBits_t b) { g->bits &= ~b; return g->bits; }
inline SemaphoreHandle_t xSemaphoreCreateMutex() { static int x; return &x; }
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return pdTRUE; }
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t) { return pdTRUE; }
inline void vTaskDelay(TickType_t t) {
  mock.ms += t; mock.delays++;
  if (mock.limiteDelays >= 0 && mock.delays > mock.limiteDelays) throw StopTask{};
}
inline BaseType_t xTaskCreatePinnedToCore(void (*)(void*), const char*, uint32_t, void*, int, TaskHandle_t*, int) {
  mock.tareasCreadas++; return pdPASS;
}

// ---------- Red ----------
class Client { public: virtual ~Client() {} };
class WiFiClient : public Client {};
class WiFiClientSecure : public WiFiClient { public: void setInsecure() {} };
struct WiFiClass {
  void mode(int) {}
  void setAutoReconnect(bool) {}
  void begin(const char*, const char*, int) {}
  int status() { return mock.wifi; }
  void disconnect() {}
  int RSSI() { return -55; }
};
extern WiFiClass WiFi;

struct Publicacion { std::string topic, payload; bool retenido; uint32_t ms; };
struct HttpPost { std::string url, cuerpo; };
extern std::vector<Publicacion> publicaciones;
extern std::vector<std::string> suscripciones;
extern std::vector<HttpPost> posts;
extern std::string ultimoUsuario, ultimaClave, ultimoHost;
extern int ultimoPuerto;
extern std::function<void()> alCicloMqtt;     // la prueba inyecta mensajes aquí

class PubSubClient {
 public:
  typedef void (*Callback)(char*, uint8_t*, unsigned int);
  PubSubClient() {}
  PubSubClient(Client&) {}
  PubSubClient& setClient(Client&) { return *this; }
  PubSubClient& setServer(const char* h, uint16_t p) { servidor = h; ultimoHost = h; ultimoPuerto = p; return *this; }
  PubSubClient& setCallback(Callback c) { cb = c; return *this; }
  bool setBufferSize(uint16_t) { return true; }
  PubSubClient& setKeepAlive(uint16_t) { return *this; }
  PubSubClient& setSocketTimeout(uint16_t) { return *this; }
  bool connected() { return conectado; }
  bool connect(const char*) { conectado = true; return true; }
  bool connect(const char*, const char* u, const char* p) {
    ultimoUsuario = u ? u : "(null)"; ultimaClave = p ? p : "(null)"; conectado = true; return true;
  }
  bool connect(const char*, const char*, const char*, const char*, uint8_t, bool, const char*) { conectado = true; return true; }
  void disconnect() { conectado = false; }
  bool subscribe(const char* t, uint8_t) { suscripciones.push_back(t); return true; }
  bool publish(const char* t, const char* p, bool r) { publicaciones.push_back({t, p, r, mock.ms}); return true; }
  bool publish(const char* t, const char* p) { return publish(t, p, false); }
  bool loop() { if (alCicloMqtt) alCicloMqtt(); return conectado; }
  int state() { return conectado ? 0 : -2; }
  void entregar(const char* topic, const char* payload) {
    if (!cb) return;
    std::string t = topic; std::string p = payload;
    cb(&t[0], (uint8_t*)&p[0], (unsigned)p.size());
  }
  Callback cb = nullptr;
 private:
  bool conectado = false;
  std::string servidor;
};
extern PubSubClient* ultimoClienteUbidots;

class HTTPClient {
 public:
  void setTimeout(int) {}
  bool begin(WiFiClient&, const char* url) { u = url; return true; }
  void addHeader(const char*, const char*) {}
  int POST(uint8_t* c, size_t n) { posts.push_back({u, std::string((char*)c, n)}); return 200; }
  void end() {}
 private:
  std::string u;
};

class DHT {
 public:
  DHT(uint8_t, uint8_t) {}
  void begin() {}
  float readTemperature() { return mock.temp; }
  float readHumidity() { return mock.hum; }
};
