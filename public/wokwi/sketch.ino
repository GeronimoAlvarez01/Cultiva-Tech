// =====================================================================
// Cultiva Tech — Prototipo final (Semanas 13 y 14) · versión revisada
// ESP32 + DHT22 + humedad de suelo + LDR + HC-SR04 + 2 relés
//
//  Canal 1 · HiveMQ público  → MQTT Explorer (Semana 13)
//  Canal 2 · Ubidots STEM    → dashboard con gauge, gráfica y switches (Semana 14)
//  Además: telemetría JSON, latencia (RTT), alarmas con histéresis,
//  webhook (Telegram o genérico) y líneas CT_READY / CT_DATA para el visor 3D.
//
// Secretos: el token de Ubidots, la URL del webhook y el chat_id de Telegram
// NO se escriben en este archivo. Se envían por el monitor serial y viven
// solo en RAM (ver "COMANDOS LOCALES POR MONITOR SERIAL").
// =====================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>

// =====================================================================
// CONFIGURACIÓN: editar solo esta sección
// =====================================================================
const char* NOMBRE_UNICO = "geronimo-melanny-s13-a7c92";   // prefijo propio (sin espacios ni tildes)

// ---------- Canal 1: HiveMQ (MQTT Explorer) ----------
const char* BROKER = "broker.hivemq.com";
constexpr uint16_t MQTT_PORT = 1883;                         // sin TLS (solo simulación)

// ---------- Canal 2: Ubidots STEM ----------
const char* UBIDOTS_HOST = "stem.ubidots.com";
constexpr bool UBIDOTS_TLS = false;                          // false: puerto 1883 (guía S14). true: 8883
constexpr uint16_t UBIDOTS_PORT = UBIDOTS_TLS ? 8883 : 1883;
const char* UBIDOTS_DISPOSITIVO = "cultivatech";             // telemetría
const char* UBIDOTS_CONTROL = "cultivatech-ctrl";            // comandos y estados aplicados
// Déjalo vacío: el token se envía por serial con  UBIDOTS_TOKEN <token>
const char* UBIDOTS_TOKEN_INICIAL = "";

// ---------- Webhook (opcional) ----------
// Déjalos con PEGA-AQUI: se configuran por serial con  WEBHOOK <url>  y  TELEGRAM_CHAT <id>
const char* WEBHOOK_URL_INICIAL = "https://webhook.site/PEGA-AQUI-TU-UUID";
const char* TELEGRAM_CHAT_INICIAL = "t.me/CultivaTech_bot";

// ---------- Pines: no modificar ----------
constexpr uint8_t PIN_DHT = 4;
constexpr uint8_t PIN_SUELO = 34;
constexpr uint8_t PIN_LDR = 35;
constexpr uint8_t PIN_TRIG = 5;
constexpr uint8_t PIN_ECHO = 18;
constexpr uint8_t PIN_BOMBA = 26;
constexpr uint8_t PIN_LUZ = 27;

// Polaridad de los relés (verificar que el LED de carga coincida con ON/OFF).
constexpr uint8_t RELAY_ON = LOW;
constexpr uint8_t RELAY_OFF = (RELAY_ON == LOW) ? HIGH : LOW;

// ---------- Umbrales de control (con histéresis) ----------
constexpr float SUELO_REGAR = 40;        // inicia riego si suelo < 40 %
constexpr float SUELO_PARAR = 60;        // detiene riego al llegar a 60 %
constexpr float LUZ_ENCENDER = 25;       // luces ON si índice de luz < 25
constexpr float LUZ_APAGAR = 40;         // luces OFF al superar 40
constexpr float TANQUE_BLOQUEO = 15;     // bloquea bomba si tanque < 15 %
constexpr float TANQUE_LIBERA = 20;      // libera bomba si tanque >= 20 %
constexpr float TANQUE_VACIO_CM = 35;    // distancia con tanque vacío
constexpr float TANQUE_RANGO_CM = 30;    // recorrido útil del nivel

// ---------- Umbrales de alarma (con histéresis) ----------
constexpr float TEMP_ALTA_ON = 30;
constexpr float TEMP_ALTA_OFF = 28;
constexpr float SUELO_CRITICO_ON = 20;
constexpr float SUELO_CRITICO_OFF = 25;
constexpr float LATENCIA_ALTA_ON = 1000; // ms
constexpr float LATENCIA_ALTA_OFF = 800; // ms

// ---------- Tiempos ----------
constexpr uint32_t LECTURA_MS = 2000;
constexpr uint32_t PUBLICACION_MS = 5000;            // HiveMQ
constexpr uint32_t MAX_RIEGO_MS = 60000;
constexpr uint32_t MANUAL_MS = 30000;
constexpr uint32_t DATO_VIEJO_MS = 5000;
constexpr uint32_t COOLDOWN_ALARMA_MS = 30000;       // entre eventos de una misma alarma
// Ubidots STEM: 7 variables de telemetría + 3 de estado cada 30 s = 20 puntos/min (límite documentado: 30/min).
constexpr uint32_t UBIDOTS_PUBLICACION_MS = 30000;
constexpr uint32_t UBIDOTS_MIN_EVENTO_MS = 2000;     // separación mínima entre envíos por evento
constexpr uint32_t UBIDOTS_IGNORAR_MS = 3000;        // descarta los "últimos valores" tras suscribirse

// =====================================================================
// ALARMAS
// =====================================================================
enum Alarma : uint8_t {
  AL_TEMP, AL_SUELO, AL_TANQUE, AL_SENSOR, AL_RIEGO, AL_LATENCIA, NUM_ALARMAS
};
const char* const NOMBRE_ALARMA[NUM_ALARMAS + 1] = {
  "TEMPERATURA_ALTA", "SUELO_CRITICO", "TANQUE_CRITICO",
  "FALLO_SENSOR", "RIEGO_TIEMPO_MAXIMO", "LATENCIA_ALTA",
  "PRUEBA_WEBHOOK"
};

// =====================================================================
// MQTT (HiveMQ)
// =====================================================================
char prefijo[100];
char topicoComando[130];
char topicoEco[130];
char topicoTelemetria[130];
char topicoConexion[130];
char clienteId[60];
char bootId[17];                       // identificador de arranque para el visor 3D

DHT dht(PIN_DHT, DHT22);

// Solo la tarea de red utiliza estos objetos.
WiFiClient transporte;
PubSubClient mqtt(transporte);

// Medición de latencia (solo tareaRed y su callback).
float redLatencia = NAN;
uint32_t redLatenciaTs = 0;
uint32_t ecoSecuencia = 0;

// =====================================================================
// INTERCAMBIO SEGURO ENTRE TAREAS
// =====================================================================
struct Estado {
  float temperatura, humedadAmbiente;
  float suelo, luz, distancia, tanque, latencia;
  bool sensoresOK, bomba, luces, automatico, alerta;
  bool alarma[NUM_ALARMAS];
  uint8_t nAlarmas;
  uint32_t muestra, ordenesRx;
  char motivo[32];
  char ultimaOrden[24];
};

struct Orden {
  char texto[24];
  uint32_t recibida;
};

struct EstadoRed {
  bool conectado;
  uint32_t actualizado;
  float latenciaMs;
};

struct EventoAlarma {
  uint8_t id;
  bool activa;
  float valor;
  uint32_t uptime;
};

// Debe estar aquí arriba: el IDE de Arduino genera prototipos al inicio y necesita ver el tipo antes.
struct UbiPrevio {
  bool valido;
  bool bomba, luz, aut, sensores;
  uint8_t alarmas;
};

QueueHandle_t colaEstado;
QueueHandle_t colaOrden;
QueueHandle_t colaRed;        // HiveMQ
QueueHandle_t colaRedUbi;     // Ubidots
QueueHandle_t colaAlarmas;
EventGroupHandle_t eventos;
SemaphoreHandle_t mtxConfig;
constexpr EventBits_t RED_DESHABILITADA = BIT0;

// Configuración sensible en RAM (se escribe desde loop(), se lee desde otras tareas).
char cfgToken[80] = "";
char cfgWebhook[200] = "";
char cfgChat[32] = "";

// Variables modificadas únicamente desde loop().
Estado estado = {};
EstadoRed red = {};
EstadoRed redUbi = {};
bool automatico = true;
bool pedirBomba = false;
bool pedirLuz = false;
bool bomba = false;
bool luces = false;
bool tanqueBloqueado = true;
bool tiempoBloqueado = false;
bool alertaTemperatura = false;
bool sueloCritico = false;
bool latenciaAlta = false;
bool tieneLectura = false;
bool redDisponible = false;      // al menos un canal (HiveMQ o Ubidots) operativo
bool redHive = false;
bool redUbidots = false;

bool alarmaPrev[NUM_ALARMAS] = {};
uint32_t ultimoEvento[NUM_ALARMAS] = {};

uint32_t ultimaLectura = 0;
uint32_t inicioRiego = 0;
uint32_t inicioManual = 0;
uint32_t ultimoSerial = 0;
uint32_t ultimaMuestraSerial = 0;

// Estado compartido solo entre tareaUbidots y su callback.
volatile uint32_t ubiIgnorarHasta = 0;
volatile bool ubiRearmeAtendido = false;

// Promedio de las últimas tres lecturas de suelo.
float historialSuelo[3] = {};
uint8_t posicionSuelo = 0;
uint8_t cantidadSuelo = 0;

// ---------- Banco de pruebas: los botones del visor 3D envían SIM_* por serial ----------
// Mientras un bit esté activo, ese sensor usa el valor simulado en lugar del de Wokwi.
// NAN = el sensor "falla". SIM_NORMAL devuelve todo a los sensores del circuito.
enum : uint8_t { SIM_TEMP = 1, SIM_SUELO = 2, SIM_LUZ = 4, SIM_DIST = 8 };
uint8_t simMask = 0;
float simTemp = NAN, simSuelo = 50, simLuz = 50, simDist = NAN;

// ---------- Utilidad ----------
void formatear(char* destino, size_t n, float valor, uint8_t decimales) {
  if (isfinite(valor)) snprintf(destino, n, "%.*f", decimales, valor);
  else snprintf(destino, n, "null");
}

// Copia una cadena protegida por mutex (la configuración sensible la escribe loop()).
void copiarProtegido(char* destino, size_t n, const char* origen) {
  xSemaphoreTake(mtxConfig, portMAX_DELAY);
  snprintf(destino, n, "%s", origen);
  xSemaphoreGive(mtxConfig);
}

bool configurado(const char* valor) {
  return valor[0] != '\0' && strstr(valor, "PEGA-AQUI") == nullptr;
}

// chat_id de Telegram: número (con "-" para grupos) o @canal. Un enlace t.me NO es válido.
bool chatValido(const char* v) {
  if (v[0] == '@') {
    if (strlen(v) < 6) return false;
    for (const char* c = v + 1; *c; c++)
      if (!isalnum((unsigned char)*c) && *c != '_') return false;
    return true;
  }
  const char* c = v;
  if (*c == '-') c++;
  if (*c == '\0') return false;
  for (; *c; c++) if (*c < '0' || *c > '9') return false;
  return true;
}

// =====================================================================
// SENSORES
// =====================================================================
float leerDistancia() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  // Espera acotada: no permanecer bloqueado sin eco.
  unsigned long pulso = pulseIn(PIN_ECHO, HIGH, 30000UL);
  if (pulso == 0) return NAN;          // sin eco = lectura inválida, nunca "tanque lleno"

  float cm = pulso * 0.0343f / 2.0f;
  return (cm >= 2 && cm <= 400) ? cm : NAN;
}

void leerSensores() {
  estado.temperatura = dht.readTemperature();
  estado.humedadAmbiente = dht.readHumidity();
  if (simMask & SIM_TEMP) {
    estado.temperatura = simTemp;
    if (!isfinite(simTemp)) estado.humedadAmbiente = NAN;   // falla del DHT22: ambos valores se pierden
  }

  int adcSuelo = analogRead(PIN_SUELO);
  int adcLuz = analogRead(PIN_LDR);

  float sueloActual = adcSuelo * 100.0f / 4095.0f;
  historialSuelo[posicionSuelo] = sueloActual;
  posicionSuelo = (posicionSuelo + 1) % 3;
  if (cantidadSuelo < 3) cantidadSuelo++;

  float suma = 0;
  for (uint8_t i = 0; i < cantidadSuelo; i++) suma += historialSuelo[i];
  estado.suelo = suma / cantidadSuelo;
  if (simMask & SIM_SUELO) estado.suelo = simSuelo;

  // Índice relativo: NO representa lux.
  estado.luz = 100.0f - adcLuz * 100.0f / 4095.0f;
  if (simMask & SIM_LUZ) estado.luz = simLuz;

  estado.distancia = leerDistancia();
  if (simMask & SIM_DIST) estado.distancia = simDist;
  estado.tanque = isfinite(estado.distancia)
    ? constrain((TANQUE_VACIO_CM - estado.distancia) * 100.0f / TANQUE_RANGO_CM,
                0.0f, 100.0f)
    : NAN;

  estado.sensoresOK =
    isfinite(estado.temperatura) &&
    estado.temperatura >= -40 && estado.temperatura <= 80 &&
    isfinite(estado.humedadAmbiente) &&
    estado.humedadAmbiente >= 0 && estado.humedadAmbiente <= 100 &&
    isfinite(estado.distancia) &&
    adcSuelo >= 0 && adcSuelo <= 4095 &&
    adcLuz >= 0 && adcLuz <= 4095;

  if (isfinite(estado.temperatura)) {
    if (estado.temperatura > TEMP_ALTA_ON) alertaTemperatura = true;
    if (estado.temperatura < TEMP_ALTA_OFF) alertaTemperatura = false;
  }

  tieneLectura = true;
  ultimaLectura = millis();
  estado.muestra++;
}

// =====================================================================
// CONTROL LOCAL
// =====================================================================
void cambiarModo(bool nuevoAutomatico) {
  automatico = nuevoAutomatico;
  pedirBomba = pedirLuz = false;
  bomba = luces = false;
}

void aplicarOrden(const char* texto) {
  uint32_t ahora = millis();

  if (strcmp(texto, "AUTO") == 0) {
    cambiarModo(true);
  } else if (strcmp(texto, "MANUAL") == 0) {
    // Pasa a manual sin solicitar ninguna carga; las solicitudes caducan a los 30 s.
    if (automatico) cambiarModo(false);
    inicioManual = ahora;
  } else if (strcmp(texto, "REARME") == 0) {
    tiempoBloqueado = false;
    pedirBomba = false;
    bomba = false;
  } else {
    bool reconocida =
      strcmp(texto, "BOMBA_ON") == 0 ||
      strcmp(texto, "BOMBA_OFF") == 0 ||
      strcmp(texto, "LUZ_ON") == 0 ||
      strcmp(texto, "LUZ_OFF") == 0;

    if (!reconocida) {
      Serial.println("Orden no reconocida.");
      return;
    }

    if (automatico) cambiarModo(false);
    inicioManual = ahora;

    if (strcmp(texto, "BOMBA_ON") == 0) pedirBomba = true;
    if (strcmp(texto, "BOMBA_OFF") == 0) pedirBomba = false;
    if (strcmp(texto, "LUZ_ON") == 0) pedirLuz = true;
    if (strcmp(texto, "LUZ_OFF") == 0) pedirLuz = false;
  }

  // Registro de la última orden válida (se publica en la telemetría).
  estado.ordenesRx++;
  snprintf(estado.ultimaOrden, sizeof(estado.ultimaOrden), "%s", texto);

  Serial.printf("Orden procesada: %s; verificar estado y motivo.\n", texto);
}

// Evalúa alarmas, actualiza el estado y encola eventos para el webhook.
void evaluarAlarmas(bool validos) {
  uint32_t ahora = millis();

  if (validos) {
    if (estado.suelo < SUELO_CRITICO_ON) sueloCritico = true;
    if (estado.suelo >= SUELO_CRITICO_OFF) sueloCritico = false;
  }
  if (isfinite(red.latenciaMs)) {
    if (red.latenciaMs > LATENCIA_ALTA_ON) latenciaAlta = true;
    if (red.latenciaMs < LATENCIA_ALTA_OFF) latenciaAlta = false;
  }

  bool nuevas[NUM_ALARMAS] = {
    alertaTemperatura && isfinite(estado.temperatura),  // AL_TEMP
    validos && sueloCritico,                            // AL_SUELO
    validos && tanqueBloqueado,                         // AL_TANQUE
    tieneLectura && !validos,                           // AL_SENSOR
    tiempoBloqueado,                                    // AL_RIEGO
    latenciaAlta                                        // AL_LATENCIA
  };
  float valores[NUM_ALARMAS] = {
    estado.temperatura, estado.suelo, estado.tanque,
    NAN, MAX_RIEGO_MS / 1000.0f, red.latenciaMs
  };

  uint8_t activas = 0;
  for (uint8_t i = 0; i < NUM_ALARMAS; i++) {
    estado.alarma[i] = nuevas[i];
    if (nuevas[i]) activas++;

    // Evento solo en cambio de estado y respetando el enfriamiento.
    if (nuevas[i] != alarmaPrev[i] &&
        (ultimoEvento[i] == 0 ||
         uint32_t(ahora - ultimoEvento[i]) >= COOLDOWN_ALARMA_MS)) {
      EventoAlarma ev = {i, nuevas[i], valores[i], ahora / 1000};
      xQueueSend(colaAlarmas, &ev, 0);
      alarmaPrev[i] = nuevas[i];
      ultimoEvento[i] = ahora;
      Serial.printf("ALARMA %s: %s\n", NOMBRE_ALARMA[i],
                    nuevas[i] ? "ACTIVA" : "RESUELTA");
    }
  }
  estado.nAlarmas = activas;
}

void controlar() {
  uint32_t ahora = millis();
  bool validos = tieneLectura && estado.sensoresOK &&
                 uint32_t(ahora - ultimaLectura) <= DATO_VIEJO_MS;

  if (validos) {
    if (estado.tanque < TANQUE_BLOQUEO) tanqueBloqueado = true;
    if (estado.tanque >= TANQUE_LIBERA) tanqueBloqueado = false;
  }

  if (bomba && uint32_t(ahora - inicioRiego) >= MAX_RIEGO_MS) {
    tiempoBloqueado = true;
  }

  bool manualVencido =
    !automatico && uint32_t(ahora - inicioManual) >= MANUAL_MS;
  bool falloRedManual = !automatico && !redDisponible;

  if (manualVencido || falloRedManual) {
    pedirBomba = pedirLuz = false;
  }

  const char* motivo = "OK";

  if (!validos) motivo = "FALLO_SENSOR";
  else if (tanqueBloqueado) motivo = "TANQUE_CRITICO";
  else if (tiempoBloqueado) motivo = "TIEMPO_MAXIMO";
  else if (falloRedManual) motivo = "SIN_RED_MANUAL";
  else if (manualVencido) motivo = "MANUAL_VENCIDO";

  bool nuevaBomba = false;

  if (validos && !tanqueBloqueado &&
      !tiempoBloqueado && !falloRedManual) {
    nuevaBomba = automatico
      ? (bomba ? estado.suelo < SUELO_PARAR : estado.suelo < SUELO_REGAR)
      : pedirBomba;
  } else {
    // Un bloqueo cancela la solicitud manual pendiente.
    pedirBomba = false;
  }

  if (nuevaBomba && !bomba) inicioRiego = ahora;
  bomba = nuevaBomba;

  if (!validos) {
    luces = false;
    pedirLuz = false;
  } else if (automatico) {
    luces = luces ? estado.luz < LUZ_APAGAR : estado.luz < LUZ_ENCENDER;
  } else {
    luces = pedirLuz && !falloRedManual && !manualVencido;
  }

  digitalWrite(PIN_BOMBA, bomba ? RELAY_ON : RELAY_OFF);
  digitalWrite(PIN_LUZ, luces ? RELAY_ON : RELAY_OFF);

  estado.bomba = bomba;
  estado.luces = luces;
  estado.automatico = automatico;
  estado.alerta = alertaTemperatura;
  estado.sensoresOK = validos;
  estado.latencia = red.latenciaMs;
  snprintf(estado.motivo, sizeof(estado.motivo), "%s", motivo);

  evaluarAlarmas(validos);

  // Cola de longitud 1: siempre conserva el estado más reciente.
  xQueueOverwrite(colaEstado, &estado);
}

// =====================================================================
// MQTT · HiveMQ: se ejecuta exclusivamente en tareaRed
// =====================================================================
void recibirMQTT(char* topic, byte* payload, unsigned int longitud) {
  // Eco propio: sirve para medir la latencia de ida y vuelta (RTT).
  if (strcmp(topic, topicoEco) == 0) {
    char tmp[32];
    if (longitud == 0 || longitud >= sizeof(tmp)) return;
    memcpy(tmp, payload, longitud);
    tmp[longitud] = '\0';

    unsigned long seq = 0, ts = 0;
    if (sscanf(tmp, "%lu,%lu", &seq, &ts) == 2 && seq == ecoSecuencia) {
      redLatencia = (float)(uint32_t)(millis() - (uint32_t)ts);
      redLatenciaTs = millis();
    }
    return;
  }

  if (strcmp(topic, topicoComando) != 0 ||
      longitud == 0 || longitud >= sizeof(Orden::texto)) return;

  Orden orden = {};
  memcpy(orden.texto, payload, longitud);
  orden.texto[longitud] = '\0';
  orden.recibida = millis();

  // Callback breve: no acciona GPIO ni espera.
  if (xQueueSend(colaOrden, &orden, 0) != pdTRUE) {
    Serial.println("Cola de comandos llena: orden descartada.");
  }
}

bool publicarTexto(const char* sufijo, const char* valor) {
  char topic[150];
  snprintf(topic, sizeof(topic), "%s/%s", prefijo, sufijo);
  return mqtt.publish(topic, valor, false); // QoS 0, no retenido
}

bool publicarNumero(const char* sufijo, float valor) {
  char texto[24];
  formatear(texto, sizeof(texto), valor, 1);
  return publicarTexto(sufijo, texto);
}

void publicarEco() {
  char msg[32];
  ecoSecuencia++;
  snprintf(msg, sizeof(msg), "%lu,%lu",
           (unsigned long)ecoSecuencia, (unsigned long)millis());
  mqtt.publish(topicoEco, msg, false);
}

void publicarEstado() {
  Estado copia;
  if (xQueuePeek(colaEstado, &copia, 0) != pdTRUE) return;

  float lat = redLatencia;  // medición más reciente de esta tarea

  // ---- 1) Telemetría JSON única ----
  char t[12], hA[12], s[12], l[12], d[12], tk[12], lt[12];
  formatear(t, sizeof(t), copia.temperatura, 1);
  formatear(hA, sizeof(hA), copia.humedadAmbiente, 1);
  formatear(s, sizeof(s), copia.suelo, 1);
  formatear(l, sizeof(l), copia.luz, 1);
  formatear(d, sizeof(d), copia.distancia, 1);
  formatear(tk, sizeof(tk), copia.tanque, 1);
  formatear(lt, sizeof(lt), lat, 0);

  char json[700];
  snprintf(json, sizeof(json),
    "{\"temperatura\":%s,\"humedad_ambiente\":%s,\"humedad_suelo\":%s,"
    "\"luminosidad\":%s,\"distancia_cm\":%s,\"nivel_agua\":%s,"
    "\"latencia_ms\":%s,"
    "\"bomba\":%d,\"luz\":%d,\"auto\":%d,\"sensores_ok\":%d,"
    "\"al_temp\":%d,\"al_suelo\":%d,\"al_tanque\":%d,"
    "\"al_sensor\":%d,\"al_riego\":%d,\"al_latencia\":%d,"
    "\"alarmas_activas\":%u,\"rssi\":%d,\"uptime_s\":%lu,"
    "\"muestra\":%lu,\"ordenes_rx\":%lu,"
    "\"motivo\":\"%s\",\"ultima_orden\":\"%s\"}",
    t, hA, s, l, d, tk, lt,
    copia.bomba ? 1 : 0, copia.luces ? 1 : 0,
    copia.automatico ? 1 : 0, copia.sensoresOK ? 1 : 0,
    copia.alarma[AL_TEMP] ? 1 : 0, copia.alarma[AL_SUELO] ? 1 : 0,
    copia.alarma[AL_TANQUE] ? 1 : 0, copia.alarma[AL_SENSOR] ? 1 : 0,
    copia.alarma[AL_RIEGO] ? 1 : 0, copia.alarma[AL_LATENCIA] ? 1 : 0,
    (unsigned)copia.nAlarmas, WiFi.RSSI(),
    (unsigned long)(millis() / 1000),
    (unsigned long)copia.muestra, (unsigned long)copia.ordenesRx,
    copia.motivo, copia.ultimaOrden);

  bool ok = mqtt.publish(topicoTelemetria, json, false);

  // ---- 2) Tópicos individuales (se ven en MQTT Explorer, Semana 13) ----
  ok &= publicarNumero("temperatura", copia.temperatura);
  ok &= publicarNumero("humedad_ambiente", copia.humedadAmbiente);
  ok &= publicarNumero("humedad_suelo", copia.suelo);
  ok &= publicarNumero("luminosidad", copia.luz);
  ok &= publicarNumero("distancia_cm", copia.distancia);
  ok &= publicarNumero("nivel_agua", copia.tanque);
  ok &= publicarNumero("latencia_ms", lat);
  ok &= publicarTexto("estado_bomba", copia.bomba ? "ON" : "OFF");
  ok &= publicarTexto("estado_luz", copia.luces ? "ON" : "OFF");
  ok &= publicarTexto("modo", copia.automatico ? "AUTO" : "MANUAL");
  ok &= publicarTexto("sensores_ok", copia.sensoresOK ? "SI" : "NO");
  ok &= publicarTexto("motivo", copia.motivo);
  ok &= publicarTexto("alerta_temperatura",
    !isfinite(copia.temperatura) ? "SIN_DATO" :
    (copia.alerta ? "ALTA" : "NORMAL"));

  char num[16];
  snprintf(num, sizeof(num), "%u", (unsigned)copia.nAlarmas);
  ok &= publicarTexto("alarmas_activas", num);
  snprintf(num, sizeof(num), "%lu", (unsigned long)copia.muestra);
  ok &= publicarTexto("muestra", num);

  Serial.println(ok
    ? "MQTT (HiveMQ): publicaciones enviadas; comprobar recepción en Explorer."
    : "MQTT (HiveMQ): alguna publicación falló.");
}

void tareaRed(void*) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin("Wokwi-GUEST", "", 6);

  mqtt.setServer(BROKER, MQTT_PORT);
  mqtt.setCallback(recibirMQTT);
  mqtt.setBufferSize(1024);
  mqtt.setKeepAlive(15);
  mqtt.setSocketTimeout(1);

  uint32_t ultimoWifi = millis();
  uint32_t ultimoMqtt = millis() - 5000UL;
  uint32_t ultimaPublicacion = millis();
  bool pausaAnterior = false;

  for (;;) {
    uint32_t ahora = millis();
    bool pausa = xEventGroupGetBits(eventos) & RED_DESHABILITADA;

    if (pausa) {
      if (!pausaAnterior) {
        mqtt.disconnect();
        WiFi.disconnect();
        Serial.println("Prueba: WiFi y MQTT deshabilitados.");
      }
    } else {
      if (pausaAnterior) {
        WiFi.begin("Wokwi-GUEST", "", 6);
        ultimoWifi = ahora;
      }

      if (WiFi.status() != WL_CONNECTED) {
        if (mqtt.connected()) mqtt.disconnect();

        if (uint32_t(ahora - ultimoWifi) >= 10000) {
          ultimoWifi = ahora;
          WiFi.begin("Wokwi-GUEST", "", 6);
          Serial.println("WiFi: reintento.");
        }
      } else {
        if (!mqtt.connected() &&
            uint32_t(ahora - ultimoMqtt) >= 5000) {
          ultimoMqtt = ahora;
          Serial.println("MQTT (HiveMQ): intentando conexión...");

          // Última voluntad: si el ESP32 cae, el broker publica OFFLINE.
          if (mqtt.connect(clienteId, nullptr, nullptr,
                           topicoConexion, 1, true, "OFFLINE")) {
            bool subs = mqtt.subscribe(topicoComando, 1) &&
                        mqtt.subscribe(topicoEco, 0);
            if (subs) {
              mqtt.publish(topicoConexion, "ONLINE", true);
              Serial.printf("MQTT (HiveMQ) conectado. Comandos: %s\n", topicoComando);
            } else {
              mqtt.disconnect();
              Serial.println("MQTT (HiveMQ): falló la suscripción.");
            }
          } else {
            Serial.printf("MQTT (HiveMQ): error %d\n", mqtt.state());
          }
        }

        if (mqtt.connected()) {
          mqtt.loop();

          if (mqtt.connected() &&
              uint32_t(millis() - ultimaPublicacion) >= PUBLICACION_MS) {
            ultimaPublicacion = millis();
            publicarEstado();
            publicarEco();   // la respuesta llega en el siguiente mqtt.loop()
          }
        }
      }
    }

    pausaAnterior = pausa;

    // La latencia caduca si dejan de llegar ecos.
    if (isfinite(redLatencia) &&
        uint32_t(millis() - redLatenciaTs) > 3 * PUBLICACION_MS) {
      redLatencia = NAN;
    }

    EstadoRed reporte;
    reporte.conectado = !pausa &&
      WiFi.status() == WL_CONNECTED && mqtt.connected();
    reporte.actualizado = millis();
    reporte.latenciaMs = redLatencia;
    xQueueOverwrite(colaRed, &reporte);

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// =====================================================================
// UBIDOTS STEM: tarea independiente con su propio cliente MQTT
//   Publica:   /v1.6/devices/cultivatech        (7 variables de telemetría)
//              /v1.6/devices/cultivatech-ctrl   (bomba, luz, auto_estado)
//   Suscribe:  /v1.6/devices/cultivatech-ctrl/<variable>/lv
//              con variable = bomba_cmd | luz_cmd | modo_auto | rearme
//   Usuario MQTT = token, contraseña vacía.
// =====================================================================
const char* const UBIDOTS_VARIABLES_CMD[] = {"bomba_cmd", "luz_cmd", "modo_auto", "rearme"};
constexpr uint8_t UBIDOTS_NUM_CMD = 4;

// Añade "etiqueta":valor al JSON. Un valor no finito se OMITE (nunca se envía null ni 0).
void anexarNumero(char* buf, size_t n, bool& primero,
                  const char* etiqueta, float valor, uint8_t decimales) {
  if (!isfinite(valor)) return;
  size_t usados = strlen(buf);
  snprintf(buf + usados, n - usados, "%s\"%s\":%.*f",
           primero ? "" : ",", etiqueta, decimales, valor);
  primero = false;
}

// Telemetría: temperatura, humedad_suelo, luminosidad, nivel_agua,
//             latencia_ms, alarmas_activas, sensores_ok.
bool jsonTelemetriaUbidots(const Estado& e, char* salida, size_t n) {
  salida[0] = '{';
  salida[1] = '\0';
  bool primero = true;
  anexarNumero(salida, n, primero, "temperatura", e.temperatura, 1);
  anexarNumero(salida, n, primero, "humedad_suelo", e.suelo, 1);
  anexarNumero(salida, n, primero, "luminosidad", e.luz, 1);
  anexarNumero(salida, n, primero, "nivel_agua", e.tanque, 1);
  anexarNumero(salida, n, primero, "latencia_ms", e.latencia, 0);
  anexarNumero(salida, n, primero, "alarmas_activas", (float)e.nAlarmas, 0);
  anexarNumero(salida, n, primero, "sensores_ok", e.sensoresOK ? 1.0f : 0.0f, 0);
  size_t usados = strlen(salida);
  if (usados + 2 > n) return false;
  salida[usados] = '}';
  salida[usados + 1] = '\0';
  return !primero;
}

// Estados aplicados (salida real del firmware, no la solicitud del switch).
void jsonEstadoUbidots(const Estado& e, char* salida, size_t n) {
  snprintf(salida, n, "{\"bomba\":%d,\"luz\":%d,\"auto_estado\":%d}",
           e.bomba ? 1 : 0, e.luces ? 1 : 0, e.automatico ? 1 : 0);
}

// Convierte (tópico /lv, valor) de Ubidots en una orden de texto del firmware.
// Devuelve false si el tópico no es de comando o el valor no es numérico.
bool ordenDesdeUbidots(const char* topic, const char* texto, char* orden, size_t n) {
  char base[96];
  snprintf(base, sizeof(base), "/v1.6/devices/%s/", UBIDOTS_CONTROL);
  size_t lb = strlen(base);
  if (strncmp(topic, base, lb) != 0) return false;

  const char* variable = topic + lb;
  const char* fin = strstr(variable, "/lv");
  if (fin == nullptr || fin[3] != '\0') return false;
  size_t lv = (size_t)(fin - variable);

  char* final_ = nullptr;
  double valor = strtod(texto, &final_);
  if (final_ == texto) return false;
  bool encendido = valor >= 0.5;

  const char* resultado = nullptr;
  for (uint8_t i = 0; i < UBIDOTS_NUM_CMD; i++) {
    const char* nombre = UBIDOTS_VARIABLES_CMD[i];
    if (strlen(nombre) != lv || strncmp(variable, nombre, lv) != 0) continue;
    if (i == 0) resultado = encendido ? "BOMBA_ON" : "BOMBA_OFF";
    else if (i == 1) resultado = encendido ? "LUZ_ON" : "LUZ_OFF";
    else if (i == 2) resultado = encendido ? "AUTO" : "MANUAL";
    else resultado = encendido ? "REARME" : nullptr;   // rearme=0 es solo el reinicio del switch
    break;
  }
  if (resultado == nullptr) return false;
  snprintf(orden, n, "%s", resultado);
  return true;
}

void recibirUbidots(char* topic, byte* payload, unsigned int longitud) {
  if (longitud == 0 || longitud >= 24) return;
  char texto[24];
  memcpy(texto, payload, longitud);
  texto[longitud] = '\0';

  // Ubidots entrega el último valor de cada variable al suscribirse. Ese valor
  // es un comando antiguo: se descarta durante la ventana inicial.
  if (int32_t(millis() - ubiIgnorarHasta) < 0) {
    Serial.println("Ubidots: valor previo ignorado (no es una orden nueva).");
    return;
  }

  Orden orden = {};
  if (!ordenDesdeUbidots(topic, texto, orden.texto, sizeof(orden.texto))) return;
  orden.recibida = millis();

  if (xQueueSend(colaOrden, &orden, 0) != pdTRUE) {
    Serial.println("Cola de comandos llena: orden de Ubidots descartada.");
  } else if (strcmp(orden.texto, "REARME") == 0) {
    ubiRearmeAtendido = true;    // publicar rearme=0 fuera del callback
  }
}

bool ubidotsPublicar(PubSubClient& c, const char* dispositivo, const char* json) {
  char topic[96];
  snprintf(topic, sizeof(topic), "/v1.6/devices/%s", dispositivo);
  return c.publish(topic, json, false);
}

// Tras conectar: inicializa los comandos con el estado real y se suscribe.
bool ubidotsIniciarSesion(PubSubClient& c) {
  Estado e = {};
  e.automatico = true;
  xQueuePeek(colaEstado, &e, 0);     // si aún no hay muestra, conserva valores seguros

  char json[160];
  snprintf(json, sizeof(json),
           "{\"bomba_cmd\":%d,\"luz_cmd\":%d,\"modo_auto\":%d,\"rearme\":0}",
           e.bomba ? 1 : 0, e.luces ? 1 : 0, e.automatico ? 1 : 0);
  if (!ubidotsPublicar(c, UBIDOTS_CONTROL, json)) return false;

  for (uint8_t i = 0; i < UBIDOTS_NUM_CMD; i++) {
    char topic[128];
    snprintf(topic, sizeof(topic), "/v1.6/devices/%s/%s/lv",
             UBIDOTS_CONTROL, UBIDOTS_VARIABLES_CMD[i]);
    if (!c.subscribe(topic, 1)) return false;
  }
  ubiIgnorarHasta = millis() + UBIDOTS_IGNORAR_MS;
  return true;
}


// Publica la telemetría cada 30 s y los cambios relevantes de inmediato.
void ubidotsCiclo(PubSubClient& c, UbiPrevio& previo, bool& primera,
                  uint32_t& ultimaTelemetria, uint32_t& ultimoEnvio) {
  Estado e;
  if (xQueuePeek(colaEstado, &e, 0) != pdTRUE) return;

  uint32_t ahora = millis();
  bool vencida = primera || uint32_t(ahora - ultimaTelemetria) >= UBIDOTS_PUBLICACION_MS;
  bool libre = primera || uint32_t(ahora - ultimoEnvio) >= UBIDOTS_MIN_EVENTO_MS;
  bool cambioActuadores = previo.valido &&
    (e.bomba != previo.bomba || e.luces != previo.luz || e.automatico != previo.aut);
  bool cambioCritico = previo.valido &&
    (e.sensoresOK != previo.sensores || e.nAlarmas != previo.alarmas);

  bool enviarTel = vencida || (cambioCritico && libre);
  bool enviarEst = vencida || (cambioActuadores && libre);
  if (!enviarTel && !enviarEst) return;

  char json[320];
  bool ok = true;
  if (enviarTel && jsonTelemetriaUbidots(e, json, sizeof(json))) {
    ok &= ubidotsPublicar(c, UBIDOTS_DISPOSITIVO, json);
  }
  if (enviarEst) {
    jsonEstadoUbidots(e, json, sizeof(json));
    ok &= ubidotsPublicar(c, UBIDOTS_CONTROL, json);
  }

  ultimoEnvio = ahora;
  if (!ok) {
    Serial.println("Ubidots: publicación fallida; se reintentará.");
    return;
  }
  if (enviarTel) {
    previo.sensores = e.sensoresOK;
    previo.alarmas = e.nAlarmas;
    ultimaTelemetria = ahora;
  }
  if (enviarEst) {
    previo.bomba = e.bomba;
    previo.luz = e.luces;
    previo.aut = e.automatico;
  }
  if (primera) {
    previo.valido = true;
    primera = false;
  }
  if (vencida) Serial.println("Ubidots: telemetría y estados enviados.");
  else Serial.println("Ubidots: cambio enviado de inmediato.");
}

void tareaUbidots(void*) {
  WiFiClient plano;
  WiFiClientSecure seguro;
  // Con UBIDOTS_TLS = true NO se valida el certificado (solo prueba, igual que el webhook).
  if (UBIDOTS_TLS) seguro.setInsecure();
  PubSubClient cliente(UBIDOTS_TLS ? static_cast<Client&>(seguro)
                                   : static_cast<Client&>(plano));
  cliente.setServer(UBIDOTS_HOST, UBIDOTS_PORT);
  cliente.setCallback(recibirUbidots);
  cliente.setBufferSize(512);
  cliente.setKeepAlive(30);
  cliente.setSocketTimeout(2);

  char idUbidots[60];
  snprintf(idUbidots, sizeof(idUbidots), "ct-ubi-%llx-%08lx",
           (unsigned long long)ESP.getEfuseMac(), (unsigned long)esp_random());

  char token[80], tokenUsado[80] = "";
  uint32_t ultimoIntento = millis() - 5000UL;
  uint32_t ultimaTelemetria = 0, ultimoEnvio = 0;
  bool primera = true, avisoSinToken = false;
  UbiPrevio previo = {};

  for (;;) {
    uint32_t ahora = millis();
    bool pausa = xEventGroupGetBits(eventos) & RED_DESHABILITADA;
    copiarProtegido(token, sizeof(token), cfgToken);
    bool habilitado = configurado(token);

    // Si cambia el token, se reconecta con el nuevo.
    if (strcmp(token, tokenUsado) != 0) {
      if (cliente.connected()) cliente.disconnect();
      snprintf(tokenUsado, sizeof(tokenUsado), "%s", token);
      primera = true;
      previo = {};
    }

    if (!habilitado) {
      if (!avisoSinToken) {
        avisoSinToken = true;
        Serial.println("Ubidots desactivado: falta el token. Envía  UBIDOTS_TOKEN <token>  por serial.");
      }
    } else if (pausa || WiFi.status() != WL_CONNECTED) {
      if (cliente.connected()) cliente.disconnect();
      primera = true;
    } else if (!cliente.connected()) {
      primera = true;
      if (uint32_t(ahora - ultimoIntento) >= 5000) {
        ultimoIntento = ahora;
        Serial.println("Ubidots: intentando conexión...");
        if (cliente.connect(idUbidots, token, "")) {
          if (ubidotsIniciarSesion(cliente)) {
            Serial.println("Ubidots conectado: comandos suscritos.");
          } else {
            cliente.disconnect();
            Serial.println("Ubidots: falló la inicialización o la suscripción.");
          }
        } else {
          int codigo = cliente.state();
          Serial.printf("Ubidots: error %d%s\n", codigo,
                        (codigo == 4 || codigo == 5) ? " (token rechazado: revisa que sea un Token, no la API Key)" : "");
        }
      }
    } else {
      cliente.loop();
      if (cliente.connected()) {
        ubidotsCiclo(cliente, previo, primera, ultimaTelemetria, ultimoEnvio);
        if (ubiRearmeAtendido) {
          ubiRearmeAtendido = false;
          ubidotsPublicar(cliente, UBIDOTS_CONTROL, "{\"rearme\":0}");
        }
      }
    }

    EstadoRed reporte;
    reporte.conectado = habilitado && !pausa && WiFi.status() == WL_CONNECTED && cliente.connected();
    reporte.actualizado = millis();
    reporte.latenciaMs = NAN;
    xQueueOverwrite(colaRedUbi, &reporte);

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// =====================================================================
// WEBHOOK: tarea independiente (una petición HTTP lenta no frena MQTT)
//   Telegram:  WEBHOOK https://api.telegram.org/bot<TOKEN>/sendMessage
//              TELEGRAM_CHAT <chat_id>      → cuerpo {"chat_id":...,"text":...}
//   Otro:      webhook.site / Discord        → cuerpo {"content":...}
// =====================================================================
void tareaWebhook(void*) {
  WiFiClientSecure seguro;
  WiFiClient plano;
  seguro.setInsecure();  // simulación: no valida el certificado del servidor

  EventoAlarma ev;
  char url[200], chat[32];
  for (;;) {
    if (xQueueReceive(colaAlarmas, &ev, portMAX_DELAY) != pdTRUE) continue;

    copiarProtegido(url, sizeof(url), cfgWebhook);
    copiarProtegido(chat, sizeof(chat), cfgChat);

    if (!configurado(url)) {
      Serial.println("Webhook sin configurar: evento omitido.");
      continue;
    }
    if (WiFi.status() != WL_CONNECTED ||
        (xEventGroupGetBits(eventos) & RED_DESHABILITADA)) {
      Serial.println("Webhook omitido: sin red (modo local).");
      continue;
    }
    bool telegram = strstr(url, "api.telegram.org") != nullptr;
    if (telegram && !configurado(chat)) {
      Serial.println("Webhook de Telegram sin chat_id: evento omitido. Envía  TELEGRAM_CHAT <id>.");
      continue;
    }
    if (telegram && !chatValido(chat)) {
      Serial.println("Webhook de Telegram: el chat_id no es válido (debe ser un número, no un enlace t.me). Evento omitido.");
      continue;
    }

    char valorJson[16], valorTxt[16];
    formatear(valorJson, sizeof(valorJson), ev.valor, 1);
    snprintf(valorTxt, sizeof(valorTxt), "%s",
             isfinite(ev.valor) ? valorJson : "n/d");

    const char* estadoTxt = ev.activa ? "ACTIVA" : "RESUELTA";
    char cuerpo[360];
    if (telegram) {
      snprintf(cuerpo, sizeof(cuerpo),
        "{\"chat_id\":\"%s\",\"text\":\"[Cultiva Tech] %s %s (valor=%s, t=%lus)\"}",
        chat, NOMBRE_ALARMA[ev.id], estadoTxt, valorTxt, (unsigned long)ev.uptime);
    } else {
      snprintf(cuerpo, sizeof(cuerpo),
        "{\"content\":\"[Cultiva Tech] %s %s (valor=%s, t=%lus)\","
        "\"proyecto\":\"Cultiva Tech\",\"alarma\":\"%s\","
        "\"estado\":\"%s\",\"valor\":%s,\"uptime_s\":%lu}",
        NOMBRE_ALARMA[ev.id], estadoTxt, valorTxt, (unsigned long)ev.uptime,
        NOMBRE_ALARMA[ev.id], estadoTxt, valorJson, (unsigned long)ev.uptime);
    }

    HTTPClient http;
    http.setTimeout(5000);
    bool https = strncmp(url, "https://", 8) == 0;
    bool listo = https ? http.begin(seguro, url)
                       : http.begin(plano, url);
    if (!listo) {
      Serial.println("Webhook: URL inválida.");
      continue;
    }

    http.addHeader("Content-Type", "application/json");
    int codigo = http.POST((uint8_t*)cuerpo, strlen(cuerpo));
    Serial.printf("Webhook %s %s -> HTTP %d\n",
                  NOMBRE_ALARMA[ev.id], estadoTxt, codigo);
    http.end();
  }
}

// =====================================================================
// VISOR 3D: línea CT_DATA (una por muestra nueva). Un valor inválido va como null.
// =====================================================================
const char* verdadero(bool valor) { return valor ? "true" : "false"; }

void imprimirCtData() {
  char t[12], hA[12], s[12], l[12], d[12], tk[12], lat[12];
  uint8_t mascara = 0;
  for (uint8_t i = 0; i < NUM_ALARMAS; i++) if (estado.alarma[i]) mascara |= (uint8_t)(1u << i);
  formatear(lat, sizeof(lat), red.latenciaMs, 0);
  formatear(t, sizeof(t), estado.temperatura, 1);
  formatear(hA, sizeof(hA), estado.humedadAmbiente, 1);
  formatear(s, sizeof(s), estado.suelo, 1);
  formatear(l, sizeof(l), estado.luz, 1);
  formatear(d, sizeof(d), estado.distancia, 1);
  formatear(tk, sizeof(tk), estado.tanque, 1);

  Serial.printf(
    "CT_DATA {\"v\":1,\"source\":\"wokwi\",\"device\":\"cultivatech\",\"boot\":\"%s\","
    "\"seq\":%lu,\"uptime\":%lu,\"soil\":%s,\"light\":%s,\"tank\":%s,\"distance\":%s,"
    "\"temp\":%s,\"humidity\":%s,\"sensorOk\":%s,\"automatic\":%s,"
    "\"pump\":%s,\"lamp\":%s,\"reason\":\"%s\",\"alarms\":%u,"
    "\"alarmMask\":%u,\"sim\":%u,\"latency\":%s,"
    "\"hivemq\":%s,\"ubidots\":%s}\n",
    bootId, (unsigned long)estado.muestra, (unsigned long)millis(),
    s, l, tk, d, t, hA,
    verdadero(estado.sensoresOK), verdadero(automatico),
    verdadero(bomba), verdadero(luces), estado.motivo,
    (unsigned)estado.nAlarmas, (unsigned)mascara, (unsigned)simMask, lat,
    verdadero(redHive), verdadero(redUbidots));
}

// =====================================================================
// COMANDOS LOCALES POR MONITOR SERIAL (pruebas y secretos)
// =====================================================================
void mostrarAyuda() {
  Serial.println("Serial: AUTO, REARME, RED_OFF, RED_ON, TEST_ALARMA,");
  Serial.println("        UBIDOTS_TOKEN <token>, WEBHOOK <url>, TELEGRAM_CHAT <id>,");
  Serial.println("        SIM_NORMAL, SIM_TANQUE_VACIO/LLENO, SIM_TEMP_ALTA/NORMAL, SIM_LUZ_BAJA/ALTA,");
  Serial.println("        SIM_SUELO_SECO/HUMEDO, SIM_FALLA_DHT, SIM_FALLA_ULTRASONICO.");
}

void guardarSecreto(char* destino, size_t n, const char* valor, const char* nombre) {
  size_t largo = strlen(valor);
  if (largo < 3 || largo >= n) {
    Serial.printf("%s: valor inválido (longitud %u).\n", nombre, (unsigned)largo);
    return;
  }
  copiarProtegido(destino, n, valor);
  Serial.printf("%s recibido (%u caracteres); queda solo en RAM y no se muestra.\n",
                nombre, (unsigned)largo);
}

// Botones de simulación del visor 3D. Devuelve false si el comando no existe.
bool simular(const char* c) {
  const char* msg = nullptr;
  if (strcmp(c, "SIM_NORMAL") == 0) { simMask = 0; msg = "sensores reales del circuito de Wokwi"; }
  else if (strcmp(c, "SIM_TANQUE_VACIO") == 0) { simDist = TANQUE_VACIO_CM; simMask |= SIM_DIST; msg = "tanque vacío (35 cm)"; }
  else if (strcmp(c, "SIM_TANQUE_LLENO") == 0) { simDist = 5; simMask |= SIM_DIST; msg = "tanque lleno (5 cm)"; }
  else if (strcmp(c, "SIM_TEMP_ALTA") == 0) { simTemp = 35; simMask |= SIM_TEMP; msg = "temperatura alta (35 °C)"; }
  else if (strcmp(c, "SIM_TEMP_NORMAL") == 0) { simTemp = 24; simMask |= SIM_TEMP; msg = "temperatura normal (24 °C)"; }
  else if (strcmp(c, "SIM_LUZ_BAJA") == 0) { simLuz = 10; simMask |= SIM_LUZ; msg = "poca luz (índice 10)"; }
  else if (strcmp(c, "SIM_LUZ_ALTA") == 0) { simLuz = 85; simMask |= SIM_LUZ; msg = "mucha luz (índice 85)"; }
  else if (strcmp(c, "SIM_SUELO_SECO") == 0) { simSuelo = 15; simMask |= SIM_SUELO; msg = "suelo seco (15 %)"; }
  else if (strcmp(c, "SIM_SUELO_HUMEDO") == 0) { simSuelo = 75; simMask |= SIM_SUELO; msg = "suelo húmedo (75 %)"; }
  else if (strcmp(c, "SIM_FALLA_DHT") == 0) { simTemp = NAN; simMask |= SIM_TEMP; msg = "falla del DHT22"; }
  else if (strcmp(c, "SIM_FALLA_ULTRASONICO") == 0) { simDist = NAN; simMask |= SIM_DIST; msg = "falla del HC-SR04"; }
  if (msg == nullptr) {
    Serial.println("Simulación desconocida. Usa SIM_NORMAL, SIM_TANQUE_VACIO, SIM_TEMP_ALTA, SIM_LUZ_BAJA, SIM_SUELO_SECO...");
    return false;
  }
  ultimaLectura = millis() - LECTURA_MS;   // fuerza una lectura inmediata
  Serial.printf("Simulación: %s.\n", msg);
  return true;
}

void procesarLineaSerial(char* linea) {
  if (strcmp(linea, "RED_OFF") == 0) {
    xEventGroupSetBits(eventos, RED_DESHABILITADA);
    redDisponible = false;
  } else if (strcmp(linea, "RED_ON") == 0) {
    xEventGroupClearBits(eventos, RED_DESHABILITADA);
  } else if (strcmp(linea, "TEST_ALARMA") == 0) {
    EventoAlarma ev = {NUM_ALARMAS, true, NAN, (uint32_t)(millis() / 1000)};
    xQueueSend(colaAlarmas, &ev, 0);
    Serial.println("Prueba: evento de webhook encolado.");
  } else if (strcmp(linea, "AUTO") == 0 || strcmp(linea, "REARME") == 0) {
    aplicarOrden(linea);
  } else if (strncmp(linea, "UBIDOTS_TOKEN ", 14) == 0) {
    guardarSecreto(cfgToken, sizeof(cfgToken), linea + 14, "Token de Ubidots");
  } else if (strncmp(linea, "WEBHOOK ", 8) == 0) {
    guardarSecreto(cfgWebhook, sizeof(cfgWebhook), linea + 8, "URL del webhook");
  } else if (strncmp(linea, "SIM_", 4) == 0) {
    simular(linea);
  } else if (strncmp(linea, "TELEGRAM_CHAT ", 14) == 0) {
    if (!chatValido(linea + 14)) {
      Serial.println("TELEGRAM_CHAT: usa el número de chat_id (ej. 123456789 o -100123456), no un enlace t.me ni el nombre del bot.");
    } else {
      guardarSecreto(cfgChat, sizeof(cfgChat), linea + 14, "chat_id de Telegram");
    }
  } else {
    mostrarAyuda();
  }
}

void leerSerial() {
  static char linea[220];
  static uint16_t usados = 0;
  static bool exceso = false;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;

    if (c == '\n') {
      linea[usados] = '\0';
      if (!exceso && usados > 0) procesarLineaSerial(linea);
      usados = 0;
      exceso = false;
    } else if (usados < sizeof(linea) - 1) {
      linea[usados++] = c;
    } else {
      exceso = true;
    }
  }
}

// =====================================================================
// SETUP Y LOOP
// =====================================================================
void setup() {
  // Estado inicial seguro.
  digitalWrite(PIN_BOMBA, RELAY_OFF);
  digitalWrite(PIN_LUZ, RELAY_OFF);
  pinMode(PIN_BOMBA, OUTPUT);
  pinMode(PIN_LUZ, OUTPUT);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  pinMode(PIN_SUELO, INPUT);
  pinMode(PIN_LDR, INPUT);
  analogReadResolution(12);

  Serial.begin(115200);
  dht.begin(); // La biblioteca utiliza INPUT_PULLUP.

  colaEstado = xQueueCreate(1, sizeof(Estado));
  colaOrden = xQueueCreate(8, sizeof(Orden));
  colaRed = xQueueCreate(1, sizeof(EstadoRed));
  colaRedUbi = xQueueCreate(1, sizeof(EstadoRed));
  colaAlarmas = xQueueCreate(8, sizeof(EventoAlarma));
  eventos = xEventGroupCreate();
  mtxConfig = xSemaphoreCreateMutex();

  if (!colaEstado || !colaOrden || !colaRed || !colaRedUbi ||
      !colaAlarmas || !eventos || !mtxConfig) {
    Serial.println("Error de memoria: actuadores apagados.");
    while (true) delay(1000);
  }

  copiarProtegido(cfgToken, sizeof(cfgToken), UBIDOTS_TOKEN_INICIAL);
  copiarProtegido(cfgWebhook, sizeof(cfgWebhook), WEBHOOK_URL_INICIAL);
  copiarProtegido(cfgChat, sizeof(cfgChat), TELEGRAM_CHAT_INICIAL);

  red.latenciaMs = NAN;
  estado.latencia = NAN;
  snprintf(estado.ultimaOrden, sizeof(estado.ultimaOrden), "NINGUNA");

  snprintf(prefijo, sizeof(prefijo), "cultivatech/%s", NOMBRE_UNICO);
  snprintf(topicoComando, sizeof(topicoComando), "%s/comando", prefijo);
  snprintf(topicoEco, sizeof(topicoEco), "%s/eco", prefijo);
  snprintf(topicoTelemetria, sizeof(topicoTelemetria), "%s/telemetria", prefijo);
  snprintf(topicoConexion, sizeof(topicoConexion), "%s/conexion", prefijo);

  snprintf(clienteId, sizeof(clienteId), "ct-%llx-%08lx",
           (unsigned long long)ESP.getEfuseMac(),
           (unsigned long)esp_random());
  snprintf(bootId, sizeof(bootId), "%08lx%08lx",
           (unsigned long)esp_random(), (unsigned long)esp_random());

  Serial.printf("Cultiva Tech | Prefijo HiveMQ: %s\n", prefijo);
  Serial.println("Comandos MQTT (HiveMQ): BOMBA_ON, BOMBA_OFF, LUZ_ON, LUZ_OFF, AUTO, MANUAL, REARME.");
  Serial.printf("Ubidots: %s:%u · dispositivos %s y %s\n",
                UBIDOTS_HOST, (unsigned)UBIDOTS_PORT,
                UBIDOTS_DISPOSITIVO, UBIDOTS_CONTROL);
  mostrarAyuda();
  Serial.println("Inicio en AUTO. Esperando primera lectura.");

  BaseType_t creada = xTaskCreatePinnedToCore(
    tareaRed, "mqtt", 8192, nullptr, 1, nullptr, 0);
  if (creada != pdPASS) {
    Serial.println("No se creó la tarea MQTT; continúa el control local.");
  }

  creada = xTaskCreatePinnedToCore(
    tareaUbidots, "ubidots", 12288, nullptr, 1, nullptr, 0);
  if (creada != pdPASS) {
    Serial.println("No se creó la tarea de Ubidots; continúa el control local.");
  }

  creada = xTaskCreatePinnedToCore(
    tareaWebhook, "webhook", 12288, nullptr, 1, nullptr, 0);
  if (creada != pdPASS) {
    Serial.println("No se creó la tarea del webhook; alarmas solo por MQTT.");
  }

  Serial.println("CT_READY");
}

void loop() {
  uint32_t ahora = millis();

  // Un canal vale si reportó "conectado" en los últimos 2 s. La resta es CON SIGNO:
  // la otra tarea puede escribir su marca de tiempo unos ms después de leer 'ahora'.
  bool pausa = xEventGroupGetBits(eventos) & RED_DESHABILITADA;
  redHive = false;
  redUbidots = false;
  if (xQueuePeek(colaRed, &red, 0) == pdTRUE) {
    redHive = red.conectado && int32_t(ahora - red.actualizado) < 2000;
  }
  if (xQueuePeek(colaRedUbi, &redUbi, 0) == pdTRUE) {
    redUbidots = redUbi.conectado && int32_t(ahora - redUbi.actualizado) < 2000;
  }
  redHive = redHive && !pausa;
  redUbidots = redUbidots && !pausa;
  redDisponible = redHive || redUbidots;

  leerSerial();

  if (uint32_t(ahora - ultimaLectura) >= LECTURA_MS) {
    leerSensores();
  }

  Orden orden;
  while (xQueueReceive(colaOrden, &orden, 0) == pdTRUE) {
    if (redDisponible &&
        uint32_t(millis() - orden.recibida) < 2000) {
      aplicarOrden(orden.texto);
    } else {
      Serial.println("Orden descartada: sin red o vencida en cola.");
    }
  }

  controlar();

  if (estado.muestra != ultimaMuestraSerial) {
    ultimaMuestraSerial = estado.muestra;
    imprimirCtData();
  }

  if (uint32_t(millis() - ultimoSerial) >= 2000) {
    ultimoSerial = millis();
    Serial.printf(
      "T=%.1f C HR=%.1f%% suelo=%.1f%% luz=%.1f%% "
      "dist=%.1f cm tanque=%.1f%% | bomba=%s luz=%s "
      "modo=%s hive=%s ubidots=%s lat=%.0f ms alarmas=%u motivo=%s\n",
      estado.temperatura, estado.humedadAmbiente,
      estado.suelo, estado.luz, estado.distancia, estado.tanque,
      bomba ? "ON" : "OFF", luces ? "ON" : "OFF",
      automatico ? "AUTO" : "MANUAL",
      redHive ? "SI" : "NO", redUbidots ? "SI" : "NO", red.latenciaMs,
      (unsigned)estado.nAlarmas, estado.motivo);
  }

  delay(10); // Cede CPU; no espera reconexiones.
}
