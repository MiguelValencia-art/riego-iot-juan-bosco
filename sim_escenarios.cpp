// Simulación de escenarios del prototipo (modelo de suelo SIMPLE y ficticio).
// Sirve para ejercitar la lógica de control de punta a punta y generar un registro CSV de ejemplo.
// IMPORTANTE: son datos simulados, NO mediciones de campo. No se usan para afirmar ahorro de agua.
// Los resultados reales se obtienen con el prototipo físico (ver docs/PLAN_PRUEBAS.md).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../firmware/riego_iot/control_riego.h"
#include "../firmware/riego_iot/sensor_utils.h"

struct Escenario {
  const char* nombre;
  float humInicial;      // % humedad inicial
  float secadoPorMin;    // pérdida de humedad por minuto (evapotranspiración simulada)
  float aportePorMin;    // ganancia por minuto con la bomba encendida (0 simula bomba/tubería averiada)
  int falloSensorDesdeMin;  // -1 = sin fallo
  int falloSensorHastaMin;
  int duracionMin;
};

static int pctARaw(float pct, const Calibracion& k) {
  return (int)(k.rawSeco - (pct / 100.0f) * (k.rawSeco - k.rawHumedo));
}

static void correr(const Escenario& e, FILE* csv) {
  Calibracion k;
  ControlRiego c;
  ParamControl p;
  p.umbralInf = 30; p.umbralSup = 60;
  p.tMaxBombeoMs = 10 * 60000UL; p.tEsperaMs = 15 * 60000UL;
  c.configurar(p);

  float hum = e.humInicial;
  int activaciones = 0, minBombeo = 0, bloqueos = 0, fallasSensor = 0;
  float hMin = 100, hMax = 0;
  bool releOn = false;

  for (int min = 0; min < e.duracionMin; min++) {
    // El "sensor" mide la humedad real (con una pequeña variación fija para simular ruido).
    int ruido = (min % 3) * 4 - 4;
    int raw = pctARaw(hum, k) + ruido;
    bool enFalla = e.falloSensorDesdeMin >= 0 && min >= e.falloSensorDesdeMin && min < e.falloSensorHastaMin;
    if (enFalla) raw = 0;  // sensor desconectado

    bool valida = lecturaValida(raw, k);
    float medida = valida ? rawAPorcentaje(raw, k) : 0.0f;
    Salida s = c.actualizar(medida, valida, (uint32_t)min * 60000UL);

    if (s.cambioEstado) {
      if (s.estado == Estado::REGANDO) activaciones++;
      if (s.estado == Estado::BLOQUEO_TIEMPO) bloqueos++;
      if (s.estado == Estado::FALLA_SENSOR) fallasSensor++;
    }
    releOn = s.releOn;
    if (releOn) minBombeo++;

    fprintf(csv, "%s,%d,%d,%.1f,%.1f,%s,%d\n", e.nombre, min, raw, medida, hum, nombreEstado(s.estado), releOn ? 1 : 0);

    // Dinámica del suelo simulada.
    hum -= e.secadoPorMin;
    if (releOn) hum += e.aportePorMin;
    if (hum < 0) hum = 0;
    if (hum > 100) hum = 100;
    if (hum < hMin) hMin = hum;
    if (hum > hMax) hMax = hum;
  }
  printf("%-34s activaciones=%d  min_bombeo=%d  bloqueos_tiempo_max=%d  fallas_sensor=%d  humedad_real[min..max]=%.1f..%.1f%%\n",
         e.nombre, activaciones, minBombeo, bloqueos, fallasSensor, hMin, hMax);
}

int main(int argc, char** argv) {
  const char* ruta = argc > 1 ? argv[1] : "simulacion_registro.csv";
  FILE* csv = fopen(ruta, "w");
  if (!csv) { perror("csv"); return 1; }
  fprintf(csv, "escenario,minuto,raw_adc,humedad_medida_pct,humedad_real_sim_pct,estado,rele\n");

  Escenario esc[] = {
    {"E1_suelo_seco_arranque",        15, 0.30f, 6.0f, -1, -1, 480},
    {"E2_suelo_humedo_sin_riego",     70, 0.30f, 6.0f, -1, -1, 240},
    {"E3_falla_de_sensor",            50, 0.30f, 6.0f, 100, 160, 300},
    {"E4_bomba_sin_efecto_bloqueo",   20, 0.30f, 0.0f, -1, -1, 120},
  };
  printf("Simulación (datos ficticios, no son mediciones de campo)\n");
  for (auto& e : esc) correr(e, csv);
  fclose(csv);
  printf("\nRegistro CSV generado: %s\n", ruta);
  return 0;
}
