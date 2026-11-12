# Cultiva Tech — simulación 3D, Wokwi, Ubidots y documentación

Visor React + Three.js con tres pestañas:

1. **Simulación 3D**: un solo botón «▶ Iniciar simulación» arranca el circuito de Wokwi, el monitor serial y el campo 3D.
2. **Dashboard Ubidots**: muestra el dashboard público (gauge de temperatura, gráfica histórica, interruptores de bomba y luz).
3. **Documentación**: PDF de las semanas 10, 12, 13 y 14 (`public/docs/`).

## Puesta en marcha

```bash
npm ci
npm run sync:wokwi     # copia wokwi/final → public/wokwi
npm run dev            # http://127.0.0.1:5173
npm test
```

### Paso obligatorio: `firmware.bin`
El embed de Wokwi ejecuta un firmware **ya compilado**; este proyecto no compila Arduino.

1. En wokwi.com crea un proyecto ESP32 y pega `wokwi/final/sketch.ino`, `diagram.json` y `libraries.txt` (librerías: PubSubClient, DHT sensor library).
2. Pulsa Play y comprueba que compila.
3. F1 → **Download Compiled Firmware** y guarda el archivo como `wokwi/final/firmware.bin`.
4. `npm run sync:wokwi` y recarga el visor.

Si falta el archivo, el visor muestra un mensaje en español explicando estos pasos.

## Enlazar con Ubidots
1. En Ubidots STEM crea dos dispositivos: `cultivatech` (variables `temperatura`, `humedad_suelo`, `luminosidad`, `nivel_agua`, `latencia_ms`, `alarmas_activas`, `sensores_ok`) y `cultivatech-ctrl` (`bomba`, `luz`, `auto_estado` y las de orden `bomba_cmd`, `luz_cmd`, `modo_auto`, `rearme`).
2. Construye el dashboard (gauge, gráfica de 10 min, switches) y compártelo con enlace público.
3. En la pestaña **Dashboard Ubidots** pega ese enlace (o define `VITE_UBIDOTS_DASHBOARD_URL`). Se guarda solo en tu navegador.
4. En la pestaña 1, con la simulación en marcha, escribe el **token** de Ubidots y pulsa «Enviar token». Va por el serial virtual a la RAM del ESP32; no se muestra ni se guarda.
5. Telegram (opcional): por el campo serial envía `WEBHOOK <url>` y `TELEGRAM_CHAT <chat_id>`.

Puertos: HiveMQ público `broker.hivemq.com:1883` (MQTT Explorer) y Ubidots `stem.ubidots.com:1883` (8883 con TLS si cambias `UBIDOTS_TLS` a `true`).

Aviso: un enlace público con interruptores permite a cualquiera operar la bomba y la luz.

## Comandos por serial
`AUTO`, `REARME`, `RED_OFF`, `RED_ON`, `TEST_ALARMA`, `UBIDOTS_TOKEN …`, `WEBHOOK …`, `TELEGRAM_CHAT …`. Los secretos se ocultan en el monitor.

## Estructura
- `wokwi/final/` firmware y circuito vigentes (`wokwi/integrado/` es la versión anterior, solo de referencia).
- `src/` visor; `tests/firmware/` pruebas del firmware en PC (`bash tests/firmware/run.sh`).
- `package.json` aún lista `mqtt` y `@noble/hashes` (ya sin uso); puedes quitarlas y regenerar `package-lock.json` con `npm install`.

## Versión 2: simulador de condiciones y alertas en el 3D
Pestaña **Simulación**: barra de control → «Simulación de Wokwi» y «Campo tridimensional» lado a lado → monitor serial → botones del simulador → lecturas.

Los botones (Vaciar/Llenar tanque, Aumentar temperatura, Bajar iluminación, Suelo seco/húmedo, Falla DHT22, Falla HC-SR04, Normalizar todo) envían por el serial virtual comandos `SIM_*` que el firmware entiende:
`SIM_TANQUE_VACIO`, `SIM_TANQUE_LLENO`, `SIM_TEMP_ALTA`, `SIM_TEMP_NORMAL`, `SIM_LUZ_BAJA`, `SIM_LUZ_ALTA`, `SIM_SUELO_SECO`, `SIM_SUELO_HUMEDO`, `SIM_FALLA_DHT`, `SIM_FALLA_ULTRASONICO`, `SIM_NORMAL`.
Cada línea `CT_DATA` incluye `alarmMask` (bit0 temperatura, 1 suelo, 2 tanque, 3 sensor, 4 riego máximo, 5 latencia), `sim` (sensores forzados) y `latency`. La escena 3D usa esos datos para mostrar ⚠ (alerta) o ✖ (falla) sobre cada sensor, gotas de riego, cono de luz de la lámpara y nivel del tanque.
En «Otras fuentes de datos → Demostración local» los mismos botones funcionan sin Wokwi.

Correcciones al sketch de la v1: el host de Ubidots (`stem.ubidots.com`) y el chat inicial se dejan EXACTAMENTE como los verificó Gero; el DHT22 del diagrama arranca en 24 °C para no disparar una alarma al iniciar.
