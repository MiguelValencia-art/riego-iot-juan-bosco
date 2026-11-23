// config.h — parámetros de hardware y operación del prototipo.
// Los umbrales y la calibración definitivos NO se escriben aquí: se ajustan en campo por comandos
// (Monitor Serie o MQTT) y quedan guardados en la memoria no volátil del ESP32.
#pragma once

#define ID_DISPOSITIVO "riego01"

// --- Pines (ESP32 DevKit de 30/38 pines) ---
#define PIN_SENSOR 34        // ADC1_CH6, solo entrada. Se usa ADC1 porque ADC2 no funciona con Wi-Fi activo.
#define PIN_RELE 26          // Salida digital hacia el módulo relé (IN)
#define RELE_ACTIVO_BAJO 1   // 1 = el relé se activa con nivel LOW (típico de módulos de 1/2 canales). 0 = activo en alto.

// --- Medición (RF1) ---
#define N_MUESTRAS 8                   // lecturas promediadas por ciclo (reduce ruido)
#define INTERVALO_MEDICION_MS 10000UL  // valor inicial; se puede cambiar con el comando "intervalo_s"

// --- Registro y publicación (RF5, RF6) ---
#define INTERVALO_REGISTRO_MS 60000UL      // una fila de lectura por minuto en el archivo local
#define INTERVALO_PUBLICACION_MS 30000UL   // estado por MQTT cada 30 s (y en cada cambio de estado)
#define ARCHIVO_REGISTRO "/registro.csv"
#define ARCHIVO_REGISTRO_ANT "/registro_ant.csv"
#define TAM_MAX_REGISTRO 200000UL          // al superar ~200 KB se rota el archivo

// --- Hora (NTP, Colombia UTC-5 sin horario de verano) ---
#define SERVIDOR_NTP "pool.ntp.org"
#define DESFASE_GMT_S (-5L * 3600L)

// --- MQTT (RF6) ---
#define MQTT_PUERTO 1883
#define MQTT_REINTENTO_MS 10000UL
#define WIFI_REINTENTO_MS 15000UL
