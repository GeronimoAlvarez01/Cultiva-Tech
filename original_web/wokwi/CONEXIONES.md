# Cultiva Tech: montaje final

Solo simulación. ECHO se conecta directamente según solicitud; en hardware real necesita adaptación a 3,3 V.

Riel superior positivo tp: 3V3. Riel inferior positivo bp: 5V. tn y bn: GND común. No unir tp con bp.

| Componente/pin | Destino | Color |
|---|---|---|
| esp:3V3 | bb:tp.1 | red |
| esp:VIN | bb:bp.1 | red |
| esp:GND.1 | bb:tn.1 | black |
| bb:tn.50 | bb:bn.50 | black |
| bb:tp.25 | bb:tp.26 | red |
| bb:bp.25 | bb:bp.26 | red |
| bb:tn.25 | bb:tn.26 | black |
| bb:bn.25 | bb:bn.26 | black |
| dht:VCC | bb:tp.6 | red |
| dht:GND | bb:tn.7 | black |
| soil:VCC | bb:tp.22 | red |
| soil:GND | bb:tn.23 | black |
| light:VCC | bb:tp.38 | red |
| light:GND | bb:tn.39 | black |
| dht:SDA | esp:D4 | green |
| dht:SDA | pull:1 | green |
| pull:2 | bb:tp.9 | red |
| soil:SIG | esp:D34 | orange |
| light:AO | esp:D35 | magenta |
| tank:VCC | bb:bp.25 | red |
| tank:GND | bb:bn.29 | black |
| tank:TRIG | esp:D5 | blue |
| tank:ECHO | esp:D18 | cyan |
| pumpRelay:VCC | bb:bp.42 | red |
| pumpRelay:GND | bb:bn.42 | black |
| pumpRelay:IN | esp:D26 | purple |
| pumpRelay:COM | bb:tp.42 | red |
| pumpRelay:NO | pumpR:1 | purple |
| pumpR:2 | pumpLED:A | purple |
| pumpLED:C | bb:bn.43 | black |
| lampRelay:VCC | bb:bp.47 | red |
| lampRelay:GND | bb:bn.47 | black |
| lampRelay:IN | esp:D27 | yellow |
| lampRelay:COM | bb:tp.47 | red |
| lampRelay:NO | lampR:1 | yellow |
| lampR:2 | lampLED:A | yellow |
| lampLED:C | bb:bn.48 | black |
| esp:TX0 | $serialMonitor:RX | Serial virtual |
| esp:RX0 | $serialMonitor:TX | Serial virtual |

## Importar en Wokwi
1. Crear un proyecto ESP32 o abrir el proyecto propio.
2. Reemplazar diagram.json y sketch.ino. Añadir libraries.txt.
3. Para versión integrada, copiar también ControlLogic.h y ca.h, o usar firmware expandido de wokwi-setup.html.
4. Iniciar. Potenciómetro: riego por debajo de 40 %, apagado a 60 %.
5. LDR: poca luz <25 % ON; >=40 % OFF. HC-SR04: lleno 5 cm, vacío 35 cm; bloqueo <15 %, recuperación >=20 %.
6. Las cargas son LED a 3,3 V con 220 ohm; los módulos de relé sí se alimentan a 5 V.
7. La vista 3D integrada carga este circuito y su firmware. Un Wokwi independiente no abre automáticamente otra web.
8. Sin sesión Wokwi no se pueden guardar cambios en tu perfil.

El firmware integrado conserva MQTT/TLS y firmas; minimo/sketch.ino solo controla sensores y emite serial. No cargar los dos .ino a la vez.

## Parámetros y límites
- 13 partes: ESP32, protoboard, cuatro sensores (uno emulado), dos relés, dos LED y tres resistencias. No hay botones, válvulas ni otros actuadores.
- El porcentaje de suelo es un índice 0–100 del potenciómetro; no es contenido volumétrico de agua calibrado.
- Luz relativa = 100 − ADC×100/4095; no son lux. Histéresis: encendido <25, apagado ≥40.
- Nivel (%) = limitar((35 − distancia_cm)×100/30, 0, 100). Bloqueo <15 % (distancia >30,5 cm); recuperación ≥20 % (distancia ≤29 cm).
- Cada relé representa un canal del SSR. Un relé mecánico simulado no reproduce las características eléctricas de un SSR real.
- Algunos componentes de Wokwi no admiten etiquetas personalizadas. Se mantienen sus identificadores y una leyenda en el visor, sin añadir partes de texto fuera de la lista permitida.
- La versión integrada mantiene el control automático local sin MQTT. En modo manual, la pérdida del enlace bloquea la bomba. La versión mínima funciona localmente sin Wi-Fi.
- ECHO del HC-SR04 es de 5 V: la conexión directa a GPIO18 es exclusivamente para simulación. Un montaje físico exige adaptar el nivel, fuera de los componentes autorizados.

## Prueba guiada
1. Tanque a 20 cm y suelo a 30 %: bomba ON.
2. Subir suelo a 50 %: conserva ON. Subir a 60 %: OFF.
3. Bajar suelo a 50 %: conserva OFF. Bajar a 39 %: ON.
4. Con suelo seco, aumentar distancia a 33 cm: bomba bloqueada. Bajar a 28 cm: se recupera el permiso.
5. Cambiar lux del LDR hasta obtener índice <25: iluminación ON; índice ≥40: OFF.
6. No confundir la velocidad de simulación con tiempo real: las lecturas se generan cada 2 segundos del ESP32 simulado.
