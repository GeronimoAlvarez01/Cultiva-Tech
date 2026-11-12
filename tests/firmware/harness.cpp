// Prueba en PC de la lógica de wokwi/final/sketch.ino con sustitutos de Arduino/FreeRTOS.
// No valida el hardware, el emulador Wokwi, ni los brokers reales.
#include "Arduino.h"

Mock mock;
EspClass ESP;
HardwareSerial Serial;
WiFiClass WiFi;
std::vector<Publicacion> publicaciones;
std::vector<std::string> suscripciones;
std::vector<HttpPost> posts;
std::string ultimoUsuario, ultimaClave, ultimoHost;
int ultimoPuerto = 0;
std::function<void()> alCicloMqtt;
PubSubClient* ultimoClienteUbidots = nullptr;

#include "../../wokwi/final/sketch.ino"

static int fallos = 0, pruebas = 0;
#define CHECK(cond, msg) do { pruebas++; if (!(cond)) { fallos++; printf("  FALLO  %s   [%s:%d]\n", msg, __FILE__, __LINE__); } else { printf("  ok     %s\n", msg); } } while (0)

static bool contiene(const std::string& s, const char* t) { return s.find(t) != std::string::npos; }
static std::string ultimaLineaCt() {
  size_t p = mock.serial.rfind("CT_DATA ");
  if (p == std::string::npos) return "";
  return mock.serial.substr(p, mock.serial.find('\n', p) - p);
}
static void avanzar(int muestras) {          // cada vuelta = una muestra de 2 s
  for (int i = 0; i < muestras; i++) { mock.ms += 2000; loop(); }
}
static void publicarRed(QueueHandle_t q, bool conectado, int32_t desfase) {
  EstadoRed r; r.conectado = conectado; r.actualizado = mock.ms + desfase; r.latenciaMs = NAN;
  xQueueOverwrite(q, &r);
}
static int contar(const char* topic, const char* fragmento) {
  int n = 0;
  for (auto& p : publicaciones) if (p.topic == topic && p.payload.find(fragmento) != std::string::npos) n++;
  return n;
}

int main() {
  printf("== 1. Arranque ==\n");
  setup();
  CHECK(mock.tareasCreadas == 3, "se crean 3 tareas: HiveMQ, Ubidots y webhook");
  CHECK(contiene(mock.serial, "CT_READY"), "imprime CT_READY");
  CHECK(strlen(bootId) == 16, "bootId de 16 caracteres hexadecimales");
  CHECK(mock.pinBomba == HIGH && mock.pinLuz == HIGH, "salidas apagadas al iniciar (relé activo en bajo)");
  CHECK(!configurado(cfgToken), "Ubidots desactivado mientras no haya token");
  CHECK(!configurado(cfgWebhook), "webhook desactivado con la URL de ejemplo");

  printf("== 2. Lecturas normales y línea CT_DATA ==\n");
  avanzar(1);
  std::string ct = ultimaLineaCt();
  CHECK(contiene(ct, "\"soil\":50.0"), "suelo 50,0 % con ADC 2048");
  CHECK(contiene(ct, "\"tank\":50.0"), "tanque 50,0 % con 20 cm");
  CHECK(contiene(ct, "\"sensorOk\":true"), "sensores válidos");
  CHECK(contiene(ct, "\"seq\":1"), "primera muestra con seq 1");
  CHECK(contiene(ct, "\"hivemq\":false") && contiene(ct, "\"ubidots\":false"), "enlaces apagados sin tareas de red");
  { FILE* f = fopen("ct_data.log", "w"); fputs(ct.c_str(), f); fputc('\n', f); fclose(f); }

  printf("== 3. Riego con histéresis ==\n");
  mock.adcSuelo = 1000;                       // ≈ 24,4 %
  avanzar(3);                                 // el promedio de 3 lecturas baja de 40 %
  CHECK(mock.pinBomba == LOW, "bomba ON con suelo seco");
  mock.adcSuelo = 2400;                       // 58,6 %: entre 40 y 60 → sigue ON
  avanzar(3);
  CHECK(mock.pinBomba == LOW, "bomba sigue ON entre 40 % y 60 % (histéresis)");
  mock.adcSuelo = 2600;                       // 63,5 %
  avanzar(3);
  CHECK(mock.pinBomba == HIGH, "bomba OFF al llegar a 60 %");

  printf("== 4. Protecciones ==\n");
  mock.adcSuelo = 800;
  mock.pulso = 1924;                          // ≈ 33 cm → 6,7 %
  avanzar(3);
  CHECK(mock.pinBomba == HIGH && contiene(ultimaLineaCt(), "TANQUE_CRITICO"), "tanque crítico bloquea la bomba");
  mock.pulso = 0;                             // sin eco
  avanzar(2);
  ct = ultimaLineaCt();
  CHECK(contiene(ct, "\"tank\":null") && contiene(ct, "\"sensorOk\":false"), "sin eco: tank null y sensorOk falso (no es tanque lleno)");
  CHECK(contiene(ct, "FALLO_SENSOR") && mock.pinBomba == HIGH, "sensor inválido apaga la bomba");
  { FILE* f = fopen("ct_data.log", "a"); fputs(ct.c_str(), f); fputc('\n', f); fclose(f); }
  mock.pulso = 1166; mock.adcSuelo = 2048;
  avanzar(2);

  printf("== 5. JSON hacia Ubidots ==\n");
  Estado e = {};
  e.temperatura = 24.54f; e.suelo = 45.2f; e.luz = 70.05f; e.tanque = 50.0f;
  e.latencia = 123.4f; e.nAlarmas = 2; e.sensoresOK = true;
  char json[320];
  CHECK(jsonTelemetriaUbidots(e, json, sizeof json), "se genera la telemetría");
  CHECK(strcmp(json, "{\"temperatura\":24.5,\"humedad_suelo\":45.2,\"luminosidad\":70.1,\"nivel_agua\":50.0,"
                     "\"latencia_ms\":123,\"alarmas_activas\":2,\"sensores_ok\":1}") == 0, "7 variables con las etiquetas API exactas");
  e.temperatura = NAN; e.tanque = NAN; e.latencia = NAN; e.sensoresOK = false;
  jsonTelemetriaUbidots(e, json, sizeof json);
  CHECK(!contiene(json, "null") && !contiene(json, "temperatura") && !contiene(json, "nivel_agua") && !contiene(json, "latencia_ms"),
        "valores inválidos se omiten (ni null ni cero)");
  CHECK(contiene(json, "\"sensores_ok\":0"), "sensores_ok = 0 cuando hay fallo");
  e.bomba = true; e.luces = false; e.automatico = false;
  jsonEstadoUbidots(e, json, sizeof json);
  CHECK(strcmp(json, "{\"bomba\":1,\"luz\":0,\"auto_estado\":0}") == 0, "estados aplicados bomba/luz/auto_estado");

  printf("== 6. Órdenes desde Ubidots (/lv) ==\n");
  char orden[24];
  struct Caso { const char* topic; const char* valor; const char* esperado; };
  Caso casos[] = {
    {"/v1.6/devices/cultivatech-ctrl/bomba_cmd/lv", "1.0", "BOMBA_ON"},
    {"/v1.6/devices/cultivatech-ctrl/bomba_cmd/lv", "0", "BOMBA_OFF"},
    {"/v1.6/devices/cultivatech-ctrl/luz_cmd/lv", "1", "LUZ_ON"},
    {"/v1.6/devices/cultivatech-ctrl/luz_cmd/lv", "0.0", "LUZ_OFF"},
    {"/v1.6/devices/cultivatech-ctrl/modo_auto/lv", "1", "AUTO"},
    {"/v1.6/devices/cultivatech-ctrl/modo_auto/lv", "0", "MANUAL"},
    {"/v1.6/devices/cultivatech-ctrl/rearme/lv", "1", "REARME"},
    {"/v1.6/devices/cultivatech-ctrl/rearme/lv", "0", nullptr},
    {"/v1.6/devices/cultivatech-ctrl/bomba/lv", "1", nullptr},          // estado aplicado, no comando
    {"/v1.6/devices/cultivatech-ctrl/bomba_cmd_x/lv", "1", nullptr},
    {"/v1.6/devices/cultivatech/bomba_cmd/lv", "1", nullptr},           // dispositivo equivocado
    {"/v1.6/devices/cultivatech-ctrl/bomba_cmd/lv", "abc", nullptr},    // no numérico
    {"/v1.6/devices/cultivatech-ctrl/bomba_cmd", "1", nullptr},         // sin /lv
  };
  for (auto& c : casos) {
    orden[0] = 0;
    bool ok = ordenDesdeUbidots(c.topic, c.valor, orden, sizeof orden);
    char msg[160];
    snprintf(msg, sizeof msg, "%s = %s → %s", strrchr(c.topic, '/') - 0 ? c.topic + 30 : c.topic, c.valor, c.esperado ? c.esperado : "(ignorada)");
    CHECK(c.esperado ? (ok && strcmp(orden, c.esperado) == 0) : !ok, msg);
  }

  printf("== 7. Secretos por serial ==\n");
  mock.serial.clear();
  char l1[] = "UBIDOTS_TOKEN BBFF-secreto-de-prueba-1234567890";
  procesarLineaSerial(l1);
  CHECK(configurado(cfgToken), "token aceptado");
  CHECK(!contiene(mock.serial, "secreto-de-prueba"), "el token no se imprime");
  mock.serial.clear();
  mock.serialEntrada = "WEBHOOK https://api.telegram.org/bot123456:ABC-secreto-largo/sendMessage\nTELEGRAM_CHAT -100123456\n";
  while (!mock.serialEntrada.empty()) leerSerial();
  CHECK(contiene(cfgWebhook, "api.telegram.org") && strcmp(cfgChat, "-100123456") == 0, "URL larga (>24 caracteres) y chat_id aceptados por leerSerial");
  CHECK(!contiene(mock.serial, "ABC-secreto-largo") && !contiene(mock.serial, "100123456"), "ni la URL ni el chat_id se imprimen");
  mock.serial.clear();
  char l2[] = "UBIDOTS_TOKEN ab";
  procesarLineaSerial(l2);
  CHECK(contiene(mock.serial, "inválido"), "un token demasiado corto se rechaza");

  printf("== 8. Tarea de Ubidots (conexión, suscripción, órdenes y publicación) ==\n");
  publicaciones.clear(); suscripciones.clear(); mock.serial.clear();
  while (xQueueReceive(colaOrden, &orden, 0) == pdTRUE) {}
  Estado antes = {}; xQueuePeek(colaEstado, &antes, 0);
  char baseEsperada[160];
  snprintf(baseEsperada, sizeof baseEsperada, "{\"bomba_cmd\":%d,\"luz_cmd\":%d,\"modo_auto\":%d,\"rearme\":0}", antes.bomba ? 1 : 0, antes.luces ? 1 : 0, antes.automatico ? 1 : 0);
  mock.delays = 0; mock.limiteDelays = 1700;                  // ≈ 34 s de tiempo simulado
  uint32_t t0 = mock.ms;
  bool staleEnviado = false, cmdEnviado = false, cambioEnviado = false;
  alCicloMqtt = [&]() {
    if (suscripciones.size() < 4) return;
    uint32_t t = mock.ms - t0;
    if (!staleEnviado && t >= 500) {
      staleEnviado = true;
      recibirUbidots((char*)"/v1.6/devices/cultivatech-ctrl/bomba_cmd/lv", (byte*)"1", 1);   // valor viejo al suscribirse
    }
    if (!cmdEnviado && t >= 4000) {
      cmdEnviado = true;
      recibirUbidots((char*)"/v1.6/devices/cultivatech-ctrl/bomba_cmd/lv", (byte*)"1", 1);
      recibirUbidots((char*)"/v1.6/devices/cultivatech-ctrl/modo_auto/lv", (byte*)"0", 1);
      recibirUbidots((char*)"/v1.6/devices/cultivatech-ctrl/rearme/lv", (byte*)"1", 1);
    }
    if (!cambioEnviado && t >= 10000) {
      cambioEnviado = true;
      Estado s; xQueuePeek(colaEstado, &s, 0); s.bomba = !s.bomba; xQueueOverwrite(colaEstado, &s);   // cambio de actuador
    }
  };
  try { tareaUbidots(nullptr); } catch (StopTask&) {}
  alCicloMqtt = nullptr; mock.limiteDelays = -1;
  CHECK(ultimoHost == "stem.ubidots.com" && ultimoPuerto == 1883, "servidor stem.ubidots.com:1883");
  CHECK(ultimoUsuario == "BBFF-secreto-de-prueba-1234567890" && ultimaClave.empty(), "usuario MQTT = token y contraseña vacía");
  CHECK(suscripciones.size() == 4 &&
        suscripciones[0] == "/v1.6/devices/cultivatech-ctrl/bomba_cmd/lv" &&
        suscripciones[1] == "/v1.6/devices/cultivatech-ctrl/luz_cmd/lv" &&
        suscripciones[2] == "/v1.6/devices/cultivatech-ctrl/modo_auto/lv" &&
        suscripciones[3] == "/v1.6/devices/cultivatech-ctrl/rearme/lv", "suscrito a los 4 comandos /lv");
  CHECK(!publicaciones.empty() && publicaciones[0].topic == "/v1.6/devices/cultivatech-ctrl" &&
        publicaciones[0].payload == baseEsperada,
        "primero inicializa los comandos con el estado real y rearme en 0 (antes de suscribirse)");
  CHECK(contiene(mock.serial, "valor previo ignorado"), "el último valor guardado en Ubidots se ignora al reconectar");
  CHECK(colaOrden->q.size() == 3, "solo 3 órdenes nuevas llegan a la cola (la antigua no)");
  Orden o1, o2, o3;
  xQueueReceive(colaOrden, &o1, 0); xQueueReceive(colaOrden, &o2, 0); xQueueReceive(colaOrden, &o3, 0);
  CHECK(!strcmp(o1.texto, "BOMBA_ON") && !strcmp(o2.texto, "MANUAL") && !strcmp(o3.texto, "REARME"), "órdenes: BOMBA_ON, MANUAL, REARME");
  CHECK(contar("/v1.6/devices/cultivatech-ctrl", "{\"rearme\":0}") == 1, "tras REARME se devuelve rearme a 0");
  int tel = 0;
  for (auto& p : publicaciones) if (p.topic == "/v1.6/devices/cultivatech") { tel++;
    if (tel == 1) CHECK(contiene(p.payload, "humedad_suelo") && contiene(p.payload, "luminosidad") && contiene(p.payload, "sensores_ok"), "telemetría inicial con las variables del dispositivo"); }
  CHECK(tel == 2, "telemetría al conectar y una vez más a los 30 s (no más)");
  bool inmediato = false;
  for (auto& p : publicaciones) if (p.topic == "/v1.6/devices/cultivatech-ctrl" && contiene(p.payload, antes.bomba ? "\"bomba\":0" : "\"bomba\":1") && p.ms - t0 >= 10000 && p.ms - t0 < 14000) inmediato = true;
  CHECK(inmediato, "el cambio de la bomba se publica de inmediato (<4 s), sin esperar 30 s");
  CHECK(!contiene(mock.serial, "falló"), "sin errores de conexión");

  printf("== 9. Canal disponible, modo manual y falla de red ==\n");
  mock.pulso = 1166; mock.adcSuelo = 2048;
  while (xQueueReceive(colaOrden, &orden, 0) == pdTRUE) {}
  publicarRed(colaRed, false, 0); publicarRed(colaRedUbi, true, 0);
  avanzar(1);
  publicarRed(colaRedUbi, true, 0);
  Orden ob = {}; snprintf(ob.texto, sizeof ob.texto, "BOMBA_ON"); ob.recibida = mock.ms; xQueueSend(colaOrden, &ob, 0);
  loop();
  CHECK(redDisponible && !redHive && redUbidots, "solo Ubidots conectado cuenta como canal disponible");
  CHECK(mock.pinBomba == LOW && !automatico, "orden manual aplicada por Ubidots");
  publicarRed(colaRedUbi, true, 7);            // marca 7 ms en el futuro (otra tarea escribió justo después)
  loop();
  CHECK(redUbidots, "marca de tiempo ligeramente futura no se toma como canal caído (resta con signo)");
  publicarRed(colaRedUbi, false, 0);
  loop();
  CHECK(mock.pinBomba == HIGH && contiene(std::string(estado.motivo), "SIN_RED_MANUAL"), "sin ningún canal, el modo manual apaga la bomba");
  char aut[] = "AUTO"; procesarLineaSerial(aut);

  printf("== 10. Webhook (Telegram y genérico) ==\n");
  posts.clear(); mock.serial.clear();
  { EventoAlarma viejo; while (xQueueReceive(colaAlarmas, &viejo, 0) == pdTRUE) {} }   // eventos de pruebas anteriores
  EventoAlarma ev = {AL_TEMP, true, 31.0f, 120};
  xQueueSend(colaAlarmas, &ev, 0);
  try { tareaWebhook(nullptr); } catch (ColaVacia&) {}
  CHECK(posts.size() == 1 && posts[0].url.find("api.telegram.org") != std::string::npos, "alarma enviada a la URL de Telegram");
  CHECK(posts.size() == 1 && posts[0].cuerpo == "{\"chat_id\":\"-100123456\",\"text\":\"[Cultiva Tech] TEMPERATURA_ALTA ACTIVA (valor=31.0, t=120s)\"}",
        "cuerpo con chat_id y text (formato de Telegram)");
  posts.clear();
  char w[] = "WEBHOOK https://webhook.site/aaaa-bbbb"; procesarLineaSerial(w);
  EventoAlarma ev2 = {AL_SENSOR, true, NAN, 130};
  xQueueSend(colaAlarmas, &ev2, 0);
  try { tareaWebhook(nullptr); } catch (ColaVacia&) {}
  CHECK(posts.size() == 1 && contiene(posts[0].cuerpo, "\"content\":\"[Cultiva Tech] FALLO_SENSOR ACTIVA (valor=n/d") && contiene(posts[0].cuerpo, "\"valor\":null"),
        "webhook genérico conserva el campo content y envía valor null válido");
  posts.clear();
  copiarProtegido(cfgWebhook, sizeof cfgWebhook, "https://webhook.site/PEGA-AQUI-TU-UUID");
  xQueueSend(colaAlarmas, &ev, 0);
  try { tareaWebhook(nullptr); } catch (ColaVacia&) {}
  CHECK(posts.empty() && contiene(mock.serial, "Webhook sin configurar"), "con la URL de ejemplo no se envía nada");

  printf("== 11. Botones de simulación (SIM_*) y máscara de alarmas ==\n");
  {
    char a1[] = "AUTO"; procesarLineaSerial(a1);
    mock.adcSuelo = 2048; mock.adcLuz = 2048; mock.pulso = 1166; mock.temp = 24.0f;
    char n0[] = "SIM_NORMAL"; procesarLineaSerial(n0);
    avanzar(3);
    std::string c0 = ultimaLineaCt();
    CHECK(contiene(c0, "\"alarmMask\":0") && contiene(c0, "\"sim\":0"), "sin simulación: alarmMask 0 y sim 0");

    char b1[] = "SIM_TANQUE_VACIO"; procesarLineaSerial(b1); avanzar(2);
    std::string c1 = ultimaLineaCt();
    CHECK(contiene(c1, "\"tank\":0.0") && contiene(c1, "TANQUE_CRITICO"), "SIM_TANQUE_VACIO: tanque 0 % y bloqueo");
    CHECK(contiene(c1, "\"alarmMask\":4") && contiene(c1, "\"sim\":8"), "bit 2 de alarmMask (tanque) y bit 3 de sim (distancia)");
    char b2[] = "SIM_TANQUE_LLENO"; procesarLineaSerial(b2); avanzar(2);
    CHECK(contiene(ultimaLineaCt(), "\"tank\":100.0"), "SIM_TANQUE_LLENO: tanque 100 %");

    char t1[] = "SIM_TEMP_ALTA"; procesarLineaSerial(t1); avanzar(2);
    std::string c2 = ultimaLineaCt();
    CHECK(contiene(c2, "\"temp\":35.0") && (c2.find("\"alarmMask\":") != std::string::npos), "SIM_TEMP_ALTA: 35 °C");
    int mascara = atoi(c2.c_str() + c2.find("\"alarmMask\":") + 12);
    CHECK((mascara & 1) != 0, "bit 0 de alarmMask (temperatura alta)");

    char l1[] = "SIM_LUZ_BAJA"; procesarLineaSerial(l1); avanzar(2);
    CHECK(mock.pinLuz == LOW && contiene(ultimaLineaCt(), "\"lamp\":true"), "SIM_LUZ_BAJA enciende la iluminación");
    char l2[] = "SIM_LUZ_ALTA"; procesarLineaSerial(l2); avanzar(2);
    CHECK(mock.pinLuz == HIGH, "SIM_LUZ_ALTA la apaga");

    char n1[] = "SIM_NORMAL"; procesarLineaSerial(n1);
    mock.pulso = 1166; avanzar(3);
    char s1[] = "SIM_SUELO_SECO"; procesarLineaSerial(s1); avanzar(2);
    std::string c3 = ultimaLineaCt();
    CHECK(contiene(c3, "\"soil\":15.0") && mock.pinBomba == LOW && contiene(c3, "\"pump\":true"), "SIM_SUELO_SECO: suelo 15 % y la bomba riega");
    mascara = atoi(c3.c_str() + c3.find("\"alarmMask\":") + 12);
    CHECK((mascara & 2) != 0, "bit 1 de alarmMask (suelo crítico)");
    char s2[] = "SIM_SUELO_HUMEDO"; procesarLineaSerial(s2); avanzar(2);
    CHECK(mock.pinBomba == HIGH && contiene(ultimaLineaCt(), "\"soil\":75.0"), "SIM_SUELO_HUMEDO: suelo 75 % y la bomba se apaga");

    char f1[] = "SIM_FALLA_DHT"; procesarLineaSerial(f1); avanzar(2);
    std::string c4 = ultimaLineaCt();
    CHECK(contiene(c4, "\"temp\":null") && contiene(c4, "\"humidity\":null") && contiene(c4, "\"sensorOk\":false"), "SIM_FALLA_DHT: temp y humedad null, sensorOk falso");
    mascara = atoi(c4.c_str() + c4.find("\"alarmMask\":") + 12);
    CHECK((mascara & 8) != 0, "bit 3 de alarmMask (fallo de sensor)");
    char f2[] = "SIM_NORMAL"; procesarLineaSerial(f2);
    char f3[] = "SIM_FALLA_ULTRASONICO"; procesarLineaSerial(f3); avanzar(2);
    std::string c5 = ultimaLineaCt();
    CHECK(contiene(c5, "\"distance\":null") && contiene(c5, "\"tank\":null") && contiene(c5, "\"temp\":2"), "SIM_FALLA_ULTRASONICO: distance y tank null, la temperatura sigue");
    { FILE* f = fopen("ct_data.log", "a"); fputs(c1.c_str(), f); fputc('\n', f); fputs(c4.c_str(), f); fputc('\n', f); fputs(c5.c_str(), f); fputc('\n', f); fclose(f); }
    mock.serial.clear();
    char x1[] = "SIM_INVENTADO"; procesarLineaSerial(x1);
    CHECK(contiene(mock.serial, "Simulación desconocida"), "un SIM_ desconocido se rechaza");
    char n2[] = "SIM_NORMAL"; procesarLineaSerial(n2); avanzar(3);
    CHECK(contiene(ultimaLineaCt(), "\"sim\":0") && contiene(ultimaLineaCt(), "\"sensorOk\":true"), "SIM_NORMAL devuelve los sensores reales");
  }

  printf("== 12. Validación del chat_id de Telegram ==\n");
  CHECK(chatValido("123456789") && chatValido("-100123456") && chatValido("@cultiva_canal"), "números, grupos y @canal son válidos");
  CHECK(!chatValido("t.me/CultivaTech_bot") && !chatValido("") && !chatValido("-") && !chatValido("12 34") && !chatValido("@ab"), "enlaces t.me, vacío y basura se rechazan");
  mock.serial.clear();
  char tc[] = "TELEGRAM_CHAT t.me/CultivaTech_bot"; procesarLineaSerial(tc);
  CHECK(contiene(mock.serial, "usa el número de chat_id") && strcmp(cfgChat, "-100123456") == 0, "un enlace t.me no reemplaza el chat_id guardado");
  copiarProtegido(cfgChat, sizeof cfgChat, "t.me/CultivaTech_bot");
  copiarProtegido(cfgWebhook, sizeof cfgWebhook, "https://api.telegram.org/bot1:AA/sendMessage");
  posts.clear(); mock.serial.clear();
  xQueueSend(colaAlarmas, &ev, 0);
  try { tareaWebhook(nullptr); } catch (ColaVacia&) {}
  CHECK(posts.empty() && contiene(mock.serial, "chat_id no es válido"), "webhook de Telegram no envía con un chat_id inválido");

  printf("\n%d/%d comprobaciones correctas\n", pruebas - fallos, pruebas);
  return fallos ? 1 : 0;
}
