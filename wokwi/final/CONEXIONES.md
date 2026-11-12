# Conexiones (wokwi/final/diagram.json)
| Elemento | Pin ESP32 | Nota |
|---|---|---|
| DHT22 (datos) | GPIO4 | 3V3 y GND |
| Potenciómetro (humedad de suelo) | GPIO34 | solo entrada |
| LDR (luz) | GPIO35 | solo entrada |
| HC-SR04 TRIG / ECHO | GPIO5 / GPIO18 | ECHO a 5 V requiere divisor en hardware real |
| Relé bomba | GPIO26 | activo en BAJO |
| Relé luz | GPIO27 | activo en BAJO |

Red: HiveMQ `broker.hivemq.com:1883` (MQTT Explorer, sin TLS) · Ubidots `stem.ubidots.com:1883` (TLS opcional 8883, usuario = token, contraseña vacía).

## v2: comandos de simulación (serial virtual de Wokwi)
SIM_TANQUE_VACIO · SIM_TANQUE_LLENO · SIM_TEMP_ALTA · SIM_TEMP_NORMAL · SIM_LUZ_BAJA · SIM_LUZ_ALTA · SIM_SUELO_SECO · SIM_SUELO_HUMEDO · SIM_FALLA_DHT · SIM_FALLA_ULTRASONICO · SIM_NORMAL.
Puentes de riel añadidos en la protoboard (tp/tn ↔ bp/bn) y DHT22 inicial a 24 °C.
