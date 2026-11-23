/*
  Sistema de riego automatizado con IoT — vereda Juan Bosco, Villavicencio
  Proyecto de grado 202016907 (UNAD) — prototipo funcional

  Incremento 1: lectura y registro de humedad      (RF1, RF5)
  Incremento 2: control automático por umbrales    (RF2, RF3, RF4, RF7, RNF3)
  Incremento 3: comunicación y monitoreo MQTT      (RF6, RNF4)

  Módulos: sensor_utils.h (lectura) · control_riego.h (decisión) · registro.h (registro) · comunicacion.h (MQTT)
  Placa: ESP32 DevKit. Librerías: PubSubClient (Nick O'Leary). El resto viene con el núcleo ESP32 de Arduino.
*/
#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

#include "secrets.h"   // copiar de secrets.example.h (no se sube a GitHub)
#include "config.h"
#include "control_riego.h"
#include "sensor_utils.h"
#include "registro.h"
#include "comunicacion.h"

ControlRiego control;
Calibracion calib;
Comunicacion com;
Preferences prefs;

float humedadActual = 0.0f;
int rawActual = 0;
bool lecturaOk = false;
uint32_t intervaloMedMs = INTERVALO_MEDICION_MS;
uint32_t tUltMed = 0, tUltReg = 0, tUltPub = 0;
bool primeraMedicion = true;
bool forzarCiclo = false;
char bufSerie[48];
uint8_t nSerie = 0;

// ---------- Actuador ----------
void aplicarRele(bool encendido) {
  bool nivelAlto = RELE_ACTIVO_BAJO ? !encendido : encendido;
  digitalWrite(PIN_RELE, nivelAlto ? HIGH : LOW);
}

// ---------- Parámetros persistentes (sobreviven a un reinicio) ----------
void guardarParametros() {
  const ParamControl& p = control.parametros();
  prefs.putFloat("inf", p.umbralInf);
  prefs.putFloat("sup", p.umbralSup);
  prefs.putULong("tmax", p.tMaxBombeoMs);
  prefs.putULong("espera", p.tEsperaMs);
  prefs.putInt("seco", calib.rawSeco);
  prefs.putInt("humedo", calib.rawHumedo);
  prefs.putULong("interv", intervaloMedMs);
}

void cargarParametros() {
  ParamControl p;
  p.umbralInf = prefs.getFloat("inf", p.umbralInf);
  p.umbralSup = prefs.getFloat("sup", p.umbralSup);
  p.tMaxBombeoMs = prefs.getULong("tmax", p.tMaxBombeoMs);
  p.tEsperaMs = prefs.getULong("espera", p.tEsperaMs);
  if (!control.configurar(p)) control.configurar(ParamControl());  // valores guardados inválidos: usa los de fábrica
  calib.rawSeco = prefs.getInt("seco", calib.rawSeco);
  calib.rawHumedo = prefs.getInt("humedo", calib.rawHumedo);
  intervaloMedMs = prefs.getULong("interv", intervaloMedMs);
}

// ---------- Incremento 1: medición ----------
void medirHumedad() {
  int muestras[N_MUESTRAS];
  for (int i = 0; i < N_MUESTRAS; i++) {
    muestras[i] = analogRead(PIN_SENSOR);
    delay(5);
  }
  int prom = 0;
  lecturaOk = promedioValido(muestras, N_MUESTRAS, calib, prom);
  rawActual = lecturaOk ? prom : muestras[0];
  humedadActual = lecturaOk ? rawAPorcentaje(rawActual, calib) : 0.0f;
}

// ---------- Publicación ----------
void publicarEstado(uint32_t /*ahoraMs*/) {
  char json[256];
  const ParamControl& p = control.parametros();
  snprintf(json, sizeof(json),
           "{\"raw\":%d,\"hum\":%.1f,\"valida\":%d,\"estado\":\"%s\",\"rele\":%d,\"modo\":\"%s\","
           "\"inf\":%.1f,\"sup\":%.1f,\"tmax_min\":%lu,\"rssi\":%d}",
           rawActual, humedadActual, lecturaOk ? 1 : 0, nombreEstado(control.estado()),
           control.estado() == Estado::REGANDO ? 1 : 0, nombreModo(control.modo()), p.umbralInf, p.umbralSup,
           (unsigned long)(p.tMaxBombeoMs / 60000UL), com.rssi());
  com.publicarEstado(json);
}

void registrarEvento(const char* detalle) {
  registroEscribir("EVENTO", rawActual, humedadActual, nombreEstado(control.estado()),
                   control.estado() == Estado::REGANDO, nombreModo(control.modo()), detalle);
}

// ---------- Comandos (los mismos por MQTT y por el Monitor Serie) ----------
void procesarComando(const char* clave, const char* valor) {
  char detalle[64];
  uint32_t ahora = millis();
  ParamControl p = control.parametros();
  bool ok = true;

  if (strcmp(clave, "modo") == 0) {
    if (strcasecmp(valor, "AUTO") == 0) control.setModo(Modo::AUTO, ahora);
    else if (strcasecmp(valor, "MANUAL") == 0) control.setModo(Modo::MANUAL_ON, ahora);
    else if (strcasecmp(valor, "PARADA") == 0) control.setModo(Modo::PARADA, ahora);
    else ok = false;
  } else if (strcmp(clave, "umbral_inf") == 0) {
    p.umbralInf = atof(valor);
    ok = control.configurar(p);
  } else if (strcmp(clave, "umbral_sup") == 0) {
    p.umbralSup = atof(valor);
    ok = control.configurar(p);
  } else if (strcmp(clave, "tmax_min") == 0) {
    long m = atol(valor);
    ok = m >= 1 && m <= 120;
    if (ok) { p.tMaxBombeoMs = (uint32_t)m * 60000UL; ok = control.configurar(p); }
  } else if (strcmp(clave, "intervalo_s") == 0) {
    long s = atol(valor);
    ok = s >= 2 && s <= 3600;
    if (ok) intervaloMedMs = (uint32_t)s * 1000UL;
  } else if (strcmp(clave, "cal_seco") == 0) {   // dejar el sensor en suelo seco/aire y enviar este comando
    medirHumedad();
    calib.rawSeco = rawActual;
  } else if (strcmp(clave, "cal_humedo") == 0) { // dejar el sensor en suelo saturado y enviar este comando
    medirHumedad();
    calib.rawHumedo = rawActual;
  } else if (strcmp(clave, "estado") == 0) {
    publicarEstado(ahora);
    Serial.printf("raw=%d hum=%.1f%% valida=%d estado=%s modo=%s inf=%.1f sup=%.1f seco=%d humedo=%d\n", rawActual,
                  humedadActual, lecturaOk, nombreEstado(control.estado()), nombreModo(control.modo()),
                  control.parametros().umbralInf, control.parametros().umbralSup, calib.rawSeco, calib.rawHumedo);
    return;
  } else if (strcmp(clave, "volcar") == 0) {
    registroVolcarSerie();
    return;
  } else {
    ok = false;
  }

  snprintf(detalle, sizeof(detalle), "cmd %s=%s %s", clave, valor, ok ? "aplicado" : "RECHAZADO");
  registrarEvento(detalle);
  Serial.println(detalle);
  if (ok) guardarParametros();
  forzarCiclo = true;  // aplica el cambio de inmediato (p. ej. PARADA debe apagar el riego ya)
}

void atenderSerie() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (nSerie > 0) {
        bufSerie[nSerie] = 0;
        char* espacio = strchr(bufSerie, ' ');
        const char* valor = "";
        if (espacio) { *espacio = 0; valor = espacio + 1; }
        procesarComando(bufSerie, valor);
        nSerie = 0;
      }
    } else if (nSerie < sizeof(bufSerie) - 1) {
      bufSerie[nSerie++] = c;
    }
  }
}

// ---------- Arranque ----------
void setup() {
  pinMode(PIN_RELE, OUTPUT);
  aplicarRele(false);  // PRIMERO: el riego arranca siempre apagado (seguridad ante reinicios)

  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_SENSOR, ADC_11db);

  prefs.begin("riego", false);
  cargarParametros();
  registroIniciar();
  com.iniciar();
  configTime(DESFASE_GMT_S, 0, SERVIDOR_NTP);  // se sincroniza solo cuando hay Wi-Fi

  Serial.println("Riego IoT listo. Comandos: modo AUTO|MANUAL|PARADA, umbral_inf N, umbral_sup N, tmax_min N,");
  Serial.println("intervalo_s N, cal_seco, cal_humedo, estado, volcar");
  registrarEvento("arranque");
}

// ---------- Ciclo principal (sin delay largos: no bloquea el control) ----------
void loop() {
  uint32_t ahora = millis();

  com.atender(ahora);
  atenderSerie();

  Comando c;
  if (com.tomarComando(c)) procesarComando(c.clave, c.valor);

  if (primeraMedicion || forzarCiclo || (uint32_t)(ahora - tUltMed) >= intervaloMedMs) {
    primeraMedicion = false;
    forzarCiclo = false;
    tUltMed = ahora;

    medirHumedad();                                                   // Incremento 1
    Salida s = control.actualizar(humedadActual, lecturaOk, ahora);   // Incremento 2
    aplicarRele(s.releOn);

    if (s.cambioEstado) {
      registrarEvento("cambio_de_estado");
      publicarEstado(ahora);
      tUltPub = ahora;
    }
  }

  if ((uint32_t)(ahora - tUltReg) >= INTERVALO_REGISTRO_MS) {
    tUltReg = ahora;
    registroEscribir("LECTURA", rawActual, humedadActual, nombreEstado(control.estado()),
                     control.estado() == Estado::REGANDO, nombreModo(control.modo()), "");
  }

  if ((uint32_t)(ahora - tUltPub) >= INTERVALO_PUBLICACION_MS) {   // Incremento 3
    tUltPub = ahora;
    publicarEstado(ahora);
  }
}
