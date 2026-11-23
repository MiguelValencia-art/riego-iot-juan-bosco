// control_riego.h
// Lógica de decisión del riego por umbrales con histéresis (RF2, RF3, RF4, RF7, RNF3, RNF5).
// Módulo SIN dependencias de hardware: se compila en el ESP32 y también en un PC
// para ejecutar las pruebas automáticas (carpeta /pruebas).
#pragma once
#include <stdint.h>

enum class Modo : uint8_t {
  AUTO = 0,       // decide según humedad y umbrales
  MANUAL_ON = 1,  // riego forzado (sigue limitado por el tiempo máximo de bombeo)
  PARADA = 2      // riego apagado siempre (RF7)
};

enum class Estado : uint8_t {
  REPOSO = 0,          // relé apagado, esperando
  REGANDO = 1,         // relé encendido
  BLOQUEO_TIEMPO = 2,  // se superó el tiempo máximo de bombeo: relé apagado y en espera (RNF3)
  FALLA_SENSOR = 3     // lectura inválida: relé apagado (RNF3)
};

struct ParamControl {
  float umbralInf = 30.0f;               // % humedad: por debajo se activa el riego (provisional, se fija con diagnóstico)
  float umbralSup = 60.0f;               // % humedad: al alcanzarlo se desactiva el riego (provisional)
  uint32_t tMaxBombeoMs = 10UL * 60000;  // tiempo continuo máximo de bombeo (10 min, provisional)
  uint32_t tEsperaMs = 15UL * 60000;     // espera obligatoria tras un bloqueo por tiempo máximo
};

struct Salida {
  bool releOn;        // orden para el relé
  bool cambioEstado;  // true si en esta llamada cambió el estado (para registrar el evento)
  Estado estado;
};

class ControlRiego {
 public:
  ControlRiego() {}

  // Valida: umbralInf < umbralSup, ambos entre 0 y 100. Devuelve false y no aplica si es inválido.
  bool configurar(const ParamControl& p) {
    if (!(p.umbralInf >= 0.0f && p.umbralSup <= 100.0f && p.umbralInf < p.umbralSup)) return false;
    if (p.tMaxBombeoMs == 0) return false;
    param_ = p;
    return true;
  }

  const ParamControl& parametros() const { return param_; }
  Modo modo() const { return modo_; }
  Estado estado() const { return estado_; }

  void setModo(Modo m, uint32_t ahoraMs) {
    if (m == modo_) return;
    modo_ = m;
    if (m == Modo::PARADA) cambiarEstado(Estado::REPOSO, ahoraMs);
  }

  // Se llama en cada ciclo de medición.
  // humedadPct: humedad ya convertida a porcentaje (0-100). lecturaValida: resultado de validar el sensor.
  // ahoraMs: millis(); la aritmética sin signo tolera el desbordamiento a los ~49 días.
  Salida actualizar(float humedadPct, bool lecturaValida, uint32_t ahoraMs) {
    bool cambio = false;
    Estado previo = estado_;

    if (modo_ == Modo::PARADA) {
      cambio = forzar(Estado::REPOSO, ahoraMs);
      return armar(cambio, previo);
    }

    // Falla de sensor: prioridad máxima, apaga el riego en cualquier modo automático/manual (RNF3).
    if (!lecturaValida) {
      cambio = forzar(Estado::FALLA_SENSOR, ahoraMs);
      return armar(cambio, previo);
    }
    if (estado_ == Estado::FALLA_SENSOR) {  // el sensor se recuperó
      cambiarEstado(Estado::REPOSO, ahoraMs);
    }

    // Bloqueo por tiempo máximo: espera obligatoria antes de volver a regar.
    if (estado_ == Estado::BLOQUEO_TIEMPO) {
      if ((uint32_t)(ahoraMs - tEstado_) >= param_.tEsperaMs) cambiarEstado(Estado::REPOSO, ahoraMs);
      return armar(estado_ != previo, previo);
    }

    if (modo_ == Modo::MANUAL_ON) {
      if (estado_ != Estado::REGANDO) cambiarEstado(Estado::REGANDO, ahoraMs);
    } else {  // AUTO
      if (estado_ == Estado::REPOSO && humedadPct < param_.umbralInf) {
        cambiarEstado(Estado::REGANDO, ahoraMs);
      } else if (estado_ == Estado::REGANDO && humedadPct >= param_.umbralSup) {
        cambiarEstado(Estado::REPOSO, ahoraMs);
      }
    }

    // Límite de bombeo continuo (aplica en AUTO y MANUAL_ON).
    if (estado_ == Estado::REGANDO && (uint32_t)(ahoraMs - tEstado_) >= param_.tMaxBombeoMs) {
      cambiarEstado(Estado::BLOQUEO_TIEMPO, ahoraMs);
    }

    return armar(estado_ != previo, previo);
  }

 private:
  ParamControl param_;
  Modo modo_ = Modo::AUTO;
  Estado estado_ = Estado::REPOSO;
  uint32_t tEstado_ = 0;  // instante en que se entró al estado actual

  void cambiarEstado(Estado e, uint32_t ahoraMs) {
    estado_ = e;
    tEstado_ = ahoraMs;
  }
  bool forzar(Estado e, uint32_t ahoraMs) {
    if (estado_ == e) return false;
    cambiarEstado(e, ahoraMs);
    return true;
  }
  Salida armar(bool cambio, Estado previo) const {
    Salida s;
    s.estado = estado_;
    s.releOn = (estado_ == Estado::REGANDO);
    s.cambioEstado = cambio || (estado_ != previo);
    return s;
  }
};

inline const char* nombreEstado(Estado e) {
  switch (e) {
    case Estado::REPOSO: return "REPOSO";
    case Estado::REGANDO: return "REGANDO";
    case Estado::BLOQUEO_TIEMPO: return "BLOQUEO_TIEMPO";
    case Estado::FALLA_SENSOR: return "FALLA_SENSOR";
  }
  return "?";
}

inline const char* nombreModo(Modo m) {
  switch (m) {
    case Modo::AUTO: return "AUTO";
    case Modo::MANUAL_ON: return "MANUAL_ON";
    case Modo::PARADA: return "PARADA";
  }
  return "?";
}
