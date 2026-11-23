// registro.h — Registro local en CSV (LittleFS) de lecturas y eventos (RF5).
// El registro funciona aunque no haya Wi-Fi: la evaluación del proyecto depende de estos datos.
#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>
#include "config.h"

inline bool registroIniciar() { return LittleFS.begin(true); }  // true = formatea si es la primera vez

// Fecha y hora local si el NTP ya sincronizó; si no, tiempo desde el arranque (para no perder el orden).
inline void marcaTiempo(char* buf, size_t n) {
  time_t t = time(nullptr);
  if (t < 1700000000) {
    snprintf(buf, n, "arranque+%lus", (unsigned long)(millis() / 1000UL));
  } else {
    struct tm tmLocal;
    localtime_r(&t, &tmLocal);
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tmLocal);
  }
}

inline void registroRotarSiHaceFalta() {
  File f = LittleFS.open(ARCHIVO_REGISTRO, "r");
  if (!f) return;
  size_t tam = f.size();
  f.close();
  if (tam > TAM_MAX_REGISTRO) {
    LittleFS.remove(ARCHIVO_REGISTRO_ANT);
    LittleFS.rename(ARCHIVO_REGISTRO, ARCHIVO_REGISTRO_ANT);
  }
}

// tipo: "LECTURA" (periódica) o "EVENTO" (cambio de estado, comando, arranque).
inline void registroEscribir(const char* tipo, int raw, float humedadPct, const char* estado, bool rele,
                             const char* modo, const char* detalle) {
  registroRotarSiHaceFalta();
  bool nuevo = !LittleFS.exists(ARCHIVO_REGISTRO);
  File f = LittleFS.open(ARCHIVO_REGISTRO, "a");
  if (!f) return;  // el control NO depende del registro: si falla, se sigue operando
  if (nuevo) f.println("marca_tiempo,tipo,raw_adc,humedad_pct,estado,rele,modo,detalle");
  char marca[24];
  marcaTiempo(marca, sizeof(marca));
  f.printf("%s,%s,%d,%.1f,%s,%d,%s,%s\n", marca, tipo, raw, humedadPct, estado, rele ? 1 : 0, modo, detalle);
  f.close();  // se cierra en cada escritura para no perder datos si se corta la energía
}

// Vuelca el archivo por el puerto serie (comando "volcar") para copiarlo y analizarlo en una hoja de cálculo.
inline void registroVolcarSerie() {
  File f = LittleFS.open(ARCHIVO_REGISTRO, "r");
  if (!f) { Serial.println("(sin registro)"); return; }
  while (f.available()) Serial.write(f.read());
  f.close();
}
