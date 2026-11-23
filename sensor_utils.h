// sensor_utils.h
// Conversión y validación de la lectura del sensor capacitivo de humedad del suelo (RF1, RNF3).
// Sin dependencias de hardware: se prueba en PC.
#pragma once
#include <stdint.h>

// Calibración de dos puntos. Un sensor capacitivo suele dar valor ADC ALTO en seco y BAJO en húmedo.
// Estos valores son DE PARTIDA: deben reemplazarse con los medidos en el suelo de prueba (ver docs/GUIA_CALIBRACION.md).
struct Calibracion {
  int rawSeco = 3000;    // lectura ADC con el sensor en suelo seco / al aire (CALIBRAR)
  int rawHumedo = 1300;  // lectura ADC con el sensor en suelo saturado (CALIBRAR)
  int rawMin = 100;      // por debajo de esto se considera sensor desconectado/en corto
  int rawMax = 4000;     // por encima de esto se considera sensor desconectado/flotante
};

// true si la lectura cruda está dentro del rango físico esperado del sensor.
inline bool lecturaValida(int raw, const Calibracion& c) {
  return raw >= c.rawMin && raw <= c.rawMax;
}

// Convierte la lectura cruda a % de humedad (0-100) con interpolación lineal entre los dos puntos de calibración.
inline float rawAPorcentaje(int raw, const Calibracion& c) {
  if (c.rawSeco == c.rawHumedo) return 0.0f;
  float pct = 100.0f * (float)(c.rawSeco - raw) / (float)(c.rawSeco - c.rawHumedo);
  if (pct < 0.0f) pct = 0.0f;
  if (pct > 100.0f) pct = 100.0f;
  return pct;
}

// Promedio de varias lecturas para reducir ruido (diseño preliminar del documento maestro).
// Descarta las lecturas inválidas; devuelve false si no queda ninguna válida.
inline bool promedioValido(const int* lecturas, int n, const Calibracion& c, int& promedio) {
  long suma = 0;
  int validas = 0;
  for (int i = 0; i < n; i++) {
    if (lecturaValida(lecturas[i], c)) {
      suma += lecturas[i];
      validas++;
    }
  }
  if (validas == 0 || validas < (n + 1) / 2) return false;  // exige al menos la mitad de lecturas válidas
  promedio = (int)(suma / validas);
  return true;
}
