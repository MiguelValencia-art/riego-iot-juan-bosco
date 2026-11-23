Pagina Web Funcional = Esta en el archivo Riego IoT Juan Bosco en HTML localhost
# Sistema de riego automatizado con IoT — vereda Juan Bosco, Villavicencio

Prototipo funcional del proyecto de grado **202016907** (Ingeniería de Sistemas, UNAD).
Autores: Nelson Steven Duarte Traslaviña 
Miguel Ángel Valencia Gómez
Ingri Alvarez Gamboa
Tutor: Daniel Andrés Guzmán Arevalo.

El sistema mide la humedad del suelo con un sensor capacitivo, decide con dos umbrales (histéresis) y activa
una bomba o electroválvula mediante un relé. Registra lecturas y eventos en el propio ESP32 y, si hay Wi-Fi,
publica el estado por MQTT y desplegado en una pagina web funcional como metodo de prueba

## Estructura

```
firmware/riego_iot/      Código del ESP32 (Arduino)
  riego_iot.ino          Programa principal (incrementos 1, 2 y 3)
  control_riego.h        Lógica de decisión por umbrales (sin hardware, se prueba en PC)
  sensor_utils.h         Conversión ADC -> % y validación de la lectura (sin hardware)
  registro.h             Registro CSV en la memoria del ESP32 (LittleFS)
  comunicacion.h         Wi-Fi + MQTT
  config.h               Pines y parámetros de hardware
  secrets.example.h      Plantilla de credenciales (copiar como secrets.h)
pruebas/                 Pruebas automáticas y simulación (C++ en PC)
docs/                    Documentación para el documento maestro, calibración, plan de pruebas y guion del video
```

## Cómo ejecutar las pruebas de la lógica (en cualquier PC con g++)

```bash
cd pruebas
make test      # 38 pruebas de la lógica de control y del sensor
make sim       # simulación de 4 escenarios; genera un CSV de ejemplo (datos ficticios)
```

## Cómo cargar el firmware al ESP32

1. Instalar Arduino IDE, el soporte "esp32 by Espressif" y la librería **PubSubClient** (Nick O'Leary).
2. Copiar `firmware/riego_iot/secrets.example.h` como `secrets.h` y completar Wi-Fi y MQTT con credenciales propias.
3. Abrir `riego_iot.ino`, elegir la placa "ESP32 Dev Module" y cargar.
4. Abrir el Monitor Serie a 115200 baudios y seguir `docs/PLAN_PRUEBAS_Y_CALIBRACION.md` (calibrar antes de regar).

## Conexión (baja tensión, prototipo)

| Componente | Pin del componente | ESP32 / fuente |
|---|---|---|
| Sensor capacitivo de humedad | VCC / GND / AOUT | 3.3 V / GND / GPIO 34 |
| Módulo relé | VCC / GND / IN | 5 V (VIN) / GND / GPIO 26 |
| Bomba o electroválvula | por los contactos COM y NO del relé | fuente propia, con fusible |

Si el actuador es un motor DC o bobina, poner un diodo de rueda libre en paralelo. No conectar tensión de red
al prototipo. Separar control y potencia y proteger el montaje del agua (RETIE como referencia de buenas prácticas).

## Comandos (Monitor Serie o MQTT)

`modo AUTO|MANUAL|PARADA` · `umbral_inf N` · `umbral_sup N` · `tmax_min N` · `intervalo_s N` ·
`cal_seco` · `cal_humedo` · `estado` · `volcar` (solo Serie: imprime el CSV).
Por MQTT cada comando se publica en `riego/riego01/cmd/<comando>` y el estado sale en `riego/riego01/estado`.

## Seguridad y límites conocidos

- El relé arranca siempre apagado; se apaga si el sensor falla y tras el tiempo máximo de bombeo continuo.
- `secrets.h` no se sube a GitHub. La conexión MQTT del prototipo usa usuario y clave sobre el puerto 1883 (sin TLS);
  para un uso real habría que habilitar TLS.
- Umbrales (30 % y 60 %), tiempo máximo (10 min) y calibración vienen con valores de partida: deben fijarse con el
  diagnóstico y la calibración en el suelo de prueba.
