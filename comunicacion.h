// comunicacion.h — Wi-Fi + MQTT (RF6, RNF4). No bloquea el control: si no hay red, el sistema sigue regando por umbrales.
//
// Temas (base = riego/<ID_DISPOSITIVO>/):
//   base+"estado"        (publica) JSON con lectura, estado, modo y umbrales
//   base+"disponibilidad"(publica) "online" / "offline" (mensaje de última voluntad)
//   base+"cmd/<clave>"   (suscrito) comandos: modo, umbral_inf, umbral_sup, tmax_min, intervalo_s, cal_seco, cal_humedo
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"
#include "config.h"

struct Comando {
  char clave[20];
  char valor[24];
};

class Comunicacion;
static Comunicacion* g_com = nullptr;

class Comunicacion {
 public:
  Comunicacion() : mqtt_(wifi_) {}

  void iniciar() {
    g_com = this;
    snprintf(base_, sizeof(base_), "riego/%s/", ID_DISPOSITIVO);
    snprintf(temaEstado_, sizeof(temaEstado_), "%sestado", base_);
    snprintf(temaDisp_, sizeof(temaDisp_), "%sdisponibilidad", base_);
    snprintf(temaCmd_, sizeof(temaCmd_), "%scmd/#", base_);
    WiFi.mode(WIFI_STA);
    mqtt_.setServer(MQTT_SERVIDOR, MQTT_PUERTO);
    mqtt_.setCallback(&Comunicacion::alRecibir);
    mqtt_.setSocketTimeout(2);  // limita lo que puede tardar un intento de conexión
    mqtt_.setBufferSize(384);
  }

  // Llamar en cada vuelta del loop(). Reintenta con pausas largas para no frenar el control.
  void atender(uint32_t ahoraMs) {
    if (WiFi.status() != WL_CONNECTED) {
      if (ahoraMs - tUltWifi_ >= WIFI_REINTENTO_MS || tUltWifi_ == 0) {
        tUltWifi_ = ahoraMs ? ahoraMs : 1;
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_CLAVE);
      }
      return;
    }
    if (!mqtt_.connected()) {
      if (ahoraMs - tUltMqtt_ >= MQTT_REINTENTO_MS || tUltMqtt_ == 0) {
        tUltMqtt_ = ahoraMs ? ahoraMs : 1;
        conectarMqtt();
      }
      return;
    }
    mqtt_.loop();
  }

  bool conectado() { return mqtt_.connected(); }
  bool wifiConectado() { return WiFi.status() == WL_CONNECTED; }
  int rssi() { return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0; }

  bool publicarEstado(const char* json) {
    if (!mqtt_.connected()) return false;
    return mqtt_.publish(temaEstado_, json, false);
  }

  // Devuelve true si hay un comando pendiente y lo copia en 'c'.
  bool tomarComando(Comando& c) {
    if (!hayCmd_) return false;
    c = cmd_;
    hayCmd_ = false;
    return true;
  }

 private:
  WiFiClient wifi_;
  PubSubClient mqtt_;
  char base_[32], temaEstado_[48], temaDisp_[48], temaCmd_[48];
  uint32_t tUltWifi_ = 0, tUltMqtt_ = 0;
  Comando cmd_;
  volatile bool hayCmd_ = false;

  void conectarMqtt() {
    char id[40];
    snprintf(id, sizeof(id), "esp32-%s", ID_DISPOSITIVO);
    // Mensaje de última voluntad: si el dispositivo se cae, el tablero lo muestra como "offline".
    if (mqtt_.connect(id, MQTT_USUARIO, MQTT_CLAVE, temaDisp_, 0, true, "offline")) {
      mqtt_.publish(temaDisp_, "online", true);
      mqtt_.subscribe(temaCmd_);
    }
  }

  static void alRecibir(char* tema, byte* carga, unsigned int largo) {
    if (!g_com) return;
    const char* marcador = strstr(tema, "/cmd/");
    if (!marcador) return;
    marcador += 5;
    Comando c;
    strncpy(c.clave, marcador, sizeof(c.clave) - 1);
    c.clave[sizeof(c.clave) - 1] = 0;
    unsigned int n = largo < sizeof(c.valor) - 1 ? largo : sizeof(c.valor) - 1;
    memcpy(c.valor, carga, n);
    c.valor[n] = 0;
    g_com->cmd_ = c;
    g_com->hayCmd_ = true;
  }
};
