# Estado de verificación (honesto)

## Verificado
- Firmware `wokwi/final/sketch.ino` compilado con g++ en PC usando stubs de Arduino/FreeRTOS/PubSubClient: 60/60 comprobaciones (umbrales, bloqueo de tanque, 60 s de riego máximo, órdenes desde Ubidots, valores obsoletos ignorados, omisión de valores inválidos, cuerpo del webhook, formato CT_DATA).
- Pruebas JS (`npm test`): serial, validación de tramas, enlaces de Ubidots, y las líneas CT_DATA reales del firmware pasan el validador del visor.
- Visor compilado con esbuild y abierto en Chromium: pestañas, navegación con flechas, validación del enlace de Ubidots, selector de documentos, modo demostración, sin errores de consola y sin desbordamiento horizontal en móvil.

## NO verificado
- Compilación para ESP32 real (no hay toolchain ni red de paquetes): no existe `firmware.bin`; hay que generarlo en Wokwi (ver README).
- Conexión viva a HiveMQ y Ubidots, TLS 8883 y suscripción `/lv` reales.
- Que Ubidots permita incrustar tu dashboard en un iframe (si no, usa «Abrir en pestaña nueva»).
- El embed experimental de Wokwi y el arranque simultáneo con el firmware real; sin internet aquí solo se vio el estado «Cargando».
- `npm ci` / `vite build` (registro npm bloqueado en este entorno).
- El PDF de la semana 14 se regeneró para el sketch nuevo; las demás semanas se dejaron como las entregó el docente/ChatGPT.

## v2 — verificado
- Firmware (arnés g++ con stubs): 80/80 (SIM_*, alarmMask, validación de chat_id).
- Pruebas JS: 6/6 (`node --test tests/*.test.js`).
- 3D en modo demostración con Chromium: tanque vacío/lleno, luz baja/alta, suelo seco/húmedo, alertas ⚠ y fallas ✖ por sensor, tres pestañas y móvil sin desbordamiento.
- NO verificado aquí: compilación para ESP32, `firmware.bin`, el embed real de Wokwi, HiveMQ, Ubidots ni Telegram en vivo (el entorno no tiene acceso).

## v3.1 — parpadeo de pantalla al iniciar
Causas corregidas: (1) el lienzo 3D fijaba su tamaño en píxeles y podía realimentar el redimensionado en la rejilla de dos columnas → ahora el lienzo se ajusta por CSS, el redimensionado se agrupa por fotograma y solo ocurre si cambió el tamaño; (2) el arranque del ESP32 manda cientos de líneas seguidas y cada una repintaba toda la página → ahora se agrupan (máx. 4 repintados por segundo); (3) sombras más ligeras (1024 px) y menor resolución máxima.
Medido en Chromium: 1 redimensionado en 4 s (antes sin límite).

## v3.2 — lo que reportó Gero con su zip
- Botones «sin efecto» y lecturas «estáticas»: el `firmware.bin` incluido en su zip era anterior al sketch (no contiene los comandos `SIM_*`). El visor ahora lo detecta (las muestras no traen el campo `sim`) y muestra un aviso con los pasos para recompilar; los botones quedan deshabilitados mientras tanto. Se retiró el `firmware.bin` viejo del paquete: hay que generarlo de nuevo.
- Cada lectura muestra ahora «Muestra #N · ESP32 encendido hace X s» para comprobar que llegan datos aunque los sensores no cambien.
- Dashboard de Ubidots: Ubidots no permite incrustar su página (X-Frame-Options), por eso «rechazó la conexión». Se reemplazó por un botón «Abrir en ventana nueva» y un panel en vivo propio (gráficas de temperatura, suelo, tanque y luz).
- Se conserva el logo de Gero (`src/assets/Logo_CultivaTech.png`) y el título de la página.

## v3.3 — la simulación de Wokwi se «congelaba» al bajar la página o cambiar de pestaña
Causa probable (visto en las capturas de Gero: el monitor no recibe nada tras el arranque y las lecturas quedan «Sin confirmar»): el navegador limita los iframes que no se ven en pantalla y Wokwi deja de avanzar. Corrección: cuando el recuadro de Wokwi sale de la vista (scroll o cambio de pestaña) y la simulación corre, pasa a una ventanita flotante (abajo a la derecha, «sigue corriendo») que mantiene el iframe visible. No se pudo comprobar con Wokwi real; sí el aspecto y la ausencia de errores.

## v3.4 — ventanita flotante de Wokwi minimizable y movible
Botones «—» (minimizar a una miniatura que sigue visible y por tanto sigue corriendo) y «⇄» (cambiar de lado). No se puede ocultar del todo: un iframe oculto hace que Wokwi se detenga.
