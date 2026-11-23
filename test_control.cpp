// Pruebas automáticas de la lógica de control y de la conversión del sensor.
// Compilar y ejecutar:  make -C pruebas test
// Cada prueba referencia el requerimiento del documento maestro que verifica (RF/RNF).
#include <cstdio>
#include <cmath>
#include "../firmware/riego_iot/control_riego.h"
#include "../firmware/riego_iot/sensor_utils.h"

static int total = 0, fallidas = 0;
#define CHECK(id, cond)                                         \
  do {                                                          \
    total++;                                                    \
    if (!(cond)) { fallidas++; printf("  FALLA  [%s] %s\n", id, #cond); } \
    else { printf("  ok     [%s] %s\n", id, #cond); }           \
  } while (0)

static ParamControl params() {
  ParamControl p;
  p.umbralInf = 30; p.umbralSup = 60;
  p.tMaxBombeoMs = 10 * 60000UL;
  p.tEsperaMs = 15 * 60000UL;
  return p;
}

static void prueba_histeresis() {
  printf("Histéresis (RF2, RF3, RF4)\n");
  ControlRiego c; c.configurar(params());
  uint32_t t = 0;
  Salida s = c.actualizar(45, true, t += 1000);
  CHECK("RF3", !s.releOn);                    // 45% está entre umbrales: no riega
  s = c.actualizar(29.9f, true, t += 1000);
  CHECK("RF3", s.releOn && s.estado == Estado::REGANDO);  // bajo el umbral inferior: riega
  s = c.actualizar(45, true, t += 1000);
  CHECK("RF2", s.releOn);                     // zona de histéresis: sigue regando
  s = c.actualizar(59.9f, true, t += 1000);
  CHECK("RF4", s.releOn);                     // aún no llega al superior
  s = c.actualizar(60.0f, true, t += 1000);
  CHECK("RF4", !s.releOn && s.estado == Estado::REPOSO);  // alcanzó el superior: apaga
  s = c.actualizar(45, true, t += 1000);
  CHECK("RF2", !s.releOn);                    // no vuelve a encender en la zona intermedia
}

static void prueba_sin_rebote() {
  printf("Sin cambios repetidos cerca de un umbral (histéresis)\n");
  ControlRiego c; c.configurar(params());
  uint32_t t = 0; int cambios = 0;
  // La lectura oscila alrededor de 30%: con histéresis solo debe encender una vez.
  float lect[] = {31, 29.5f, 30.5f, 29.8f, 30.2f, 29.9f, 31, 30.1f};
  for (float h : lect) { Salida s = c.actualizar(h, true, t += 1000); if (s.cambioEstado) cambios++; }
  CHECK("RF2", cambios == 1);
}

static void prueba_tiempo_maximo() {
  printf("Tiempo máximo de bombeo y espera (RNF3)\n");
  ControlRiego c; c.configurar(params());
  uint32_t t = 0;
  Salida s = c.actualizar(10, true, t);          // inicia riego
  CHECK("RNF3", s.releOn);
  s = c.actualizar(10, true, t += 9 * 60000UL);   // 9 min: aún dentro del límite
  CHECK("RNF3", s.releOn);
  s = c.actualizar(10, true, t += 60000UL);       // 10 min: límite alcanzado
  CHECK("RNF3", !s.releOn && s.estado == Estado::BLOQUEO_TIEMPO);
  s = c.actualizar(10, true, t += 14 * 60000UL);  // 14 min de espera: sigue bloqueado aunque el suelo esté seco
  CHECK("RNF3", !s.releOn && s.estado == Estado::BLOQUEO_TIEMPO);
  s = c.actualizar(10, true, t += 60000UL);       // 15 min: termina la espera, vuelve a reposo
  CHECK("RNF3", s.estado == Estado::REPOSO && !s.releOn);
  s = c.actualizar(10, true, t += 1000);          // suelo sigue seco: reanuda
  CHECK("RF3", s.releOn);
}

static void prueba_falla_sensor() {
  printf("Falla de sensor (RNF3)\n");
  ControlRiego c; c.configurar(params());
  uint32_t t = 0;
  c.actualizar(10, true, t += 1000);              // regando
  Salida s = c.actualizar(0, false, t += 1000);   // lectura inválida
  CHECK("RNF3", !s.releOn && s.estado == Estado::FALLA_SENSOR);
  s = c.actualizar(10, true, t += 1000);          // el sensor se recupera, suelo seco
  CHECK("RNF3", s.releOn && s.estado == Estado::REGANDO);
  c.setModo(Modo::MANUAL_ON, t);
  s = c.actualizar(0, false, t += 1000);          // falla también apaga en modo manual
  CHECK("RNF3", !s.releOn);
}

static void prueba_modos() {
  printf("Modo manual y parada (RF7)\n");
  ControlRiego c; c.configurar(params());
  uint32_t t = 0;
  c.actualizar(10, true, t += 1000);              // regando en AUTO
  c.setModo(Modo::PARADA, t);
  Salida s = c.actualizar(10, true, t += 1000);
  CHECK("RF7", !s.releOn && s.estado == Estado::REPOSO);
  s = c.actualizar(5, true, t += 1000);           // seco, pero en PARADA no riega
  CHECK("RF7", !s.releOn);
  c.setModo(Modo::MANUAL_ON, t);
  s = c.actualizar(80, true, t += 1000);          // húmedo, pero manual fuerza riego
  CHECK("RF7", s.releOn);
  s = c.actualizar(80, true, t += 10 * 60000UL);  // manual también respeta el tiempo máximo
  CHECK("RNF3", !s.releOn && s.estado == Estado::BLOQUEO_TIEMPO);
  c.setModo(Modo::AUTO, t);
}

static void prueba_configuracion() {
  printf("Configuración de umbrales (RF2)\n");
  ControlRiego c;
  ParamControl p = params();
  CHECK("RF2", c.configurar(p));
  p.umbralInf = 70; p.umbralSup = 60;
  CHECK("RF2", !c.configurar(p));                 // inferior >= superior: rechazado
  p.umbralInf = -5; p.umbralSup = 60;
  CHECK("RF2", !c.configurar(p));
  p.umbralInf = 30; p.umbralSup = 120;
  CHECK("RF2", !c.configurar(p));
  CHECK("RF2", c.parametros().umbralInf == 30 && c.parametros().umbralSup == 60);  // conserva la config válida
}

static void prueba_desbordamiento_millis() {
  printf("Desbordamiento de millis() a los ~49 días\n");
  ControlRiego c; c.configurar(params());
  uint32_t t = 0xFFFFFFFFu - 5 * 60000UL;         // 5 min antes del desbordamiento
  c.actualizar(10, true, t);                      // inicia riego
  Salida s = c.actualizar(10, true, t + 4 * 60000UL);
  CHECK("RNF3", s.releOn);
  s = c.actualizar(10, true, t + 10 * 60000UL);   // cruza el cero del contador
  CHECK("RNF3", !s.releOn && s.estado == Estado::BLOQUEO_TIEMPO);
}

static void prueba_sensor() {
  printf("Conversión y validación del sensor (RF1, RNF3)\n");
  Calibracion k;  // seco=3000, húmedo=1300
  CHECK("RF1", std::fabs(rawAPorcentaje(3000, k) - 0.0f) < 0.01f);
  CHECK("RF1", std::fabs(rawAPorcentaje(1300, k) - 100.0f) < 0.01f);
  CHECK("RF1", std::fabs(rawAPorcentaje(2150, k) - 50.0f) < 0.01f);
  CHECK("RF1", rawAPorcentaje(3500, k) == 0.0f);      // fuera de rango: se satura
  CHECK("RF1", rawAPorcentaje(900, k) == 100.0f);
  CHECK("RNF3", !lecturaValida(0, k));                // sensor desconectado / en corto
  CHECK("RNF3", !lecturaValida(4095, k));             // pin flotante
  CHECK("RNF3", lecturaValida(2000, k));
  int prom = 0;
  int ok[] = {2000, 2010, 1990, 2005, 1995};
  CHECK("RF1", promedioValido(ok, 5, k, prom) && prom == 2000);
  int mixto[] = {2000, 0, 2000, 4095, 2000};          // 3 de 5 válidas: se acepta descartando las malas
  CHECK("RNF3", promedioValido(mixto, 5, k, prom) && prom == 2000);
  int malo[] = {0, 0, 4095, 4095, 2000};              // solo 1 de 5 válida: se rechaza
  CHECK("RNF3", !promedioValido(malo, 5, k, prom));
}

int main() {
  prueba_histeresis();
  prueba_sin_rebote();
  prueba_tiempo_maximo();
  prueba_falla_sensor();
  prueba_modos();
  prueba_configuracion();
  prueba_desbordamiento_millis();
  prueba_sensor();
  printf("\nResultado: %d pruebas, %d fallidas\n", total, fallidas);
  return fallidas == 0 ? 0 : 1;
}
