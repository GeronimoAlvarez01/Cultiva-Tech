#pragma once
#include <stdint.h>
#include <math.h>

// Umbrales didacticos; no son una prescripcion agronomica para cafe.
struct Inputs {
  float soil = 50, tank = 70, light = 70;
  bool sensorOk = false, networkRequired = false, networkUp = false;
};
struct ControlLogic {
  bool automatic = true, requestWater = false, requestLight = false;
  bool pump = false, lamp = false;
  bool timeoutLatch = false, lowTankLatch = false;
  uint32_t waterStarted = 0, manualUntil = 0;
  const char* reason = "STARTUP";
  static constexpr uint32_t MAX_WATER_MS = 60000;
  static constexpr uint32_t MANUAL_LEASE_MS = 30000;
  void setMode(bool autoMode) {
    automatic = autoMode; requestWater = false; requestLight = false;
    pump = lamp = false;
  }
  void manualWater(bool on, uint32_t now) {
    if (!automatic) { requestWater = on; manualUntil = now + MANUAL_LEASE_MS; }
  }
  void acknowledge() { timeoutLatch = false; requestWater = false; }
  void tick(const Inputs& in, uint32_t now) {
    bool valid = in.sensorOk && isfinite(in.soil) && isfinite(in.tank) &&
      isfinite(in.light) && in.soil >= 0 && in.soil <= 100 &&
      in.tank >= 0 && in.tank <= 100 && in.light >= 0 && in.light <= 100;
    if (valid && in.tank < 15) lowTankLatch = true;
    if (valid && in.tank >= 20) lowTankLatch = false;
    if (pump && uint32_t(now - waterStarted) >= MAX_WATER_MS) timeoutLatch = true;
    bool netFault = in.networkRequired && !in.networkUp;
    reason = !valid ? "SENSOR_FAULT" : lowTankLatch ? "LOW_TANK" :
      timeoutLatch ? "MAX_RUNTIME" : netFault ? "NETWORK_LOSS" : "OK";
    if (!valid || lowTankLatch || timeoutLatch || netFault) {
      pump = false; requestWater = false;
    } else {
      bool desired;
      if (automatic) desired = pump ? in.soil < 60 : in.soil < 40;
      else {
        if (int32_t(now - manualUntil) >= 0) requestWater = false;
        desired = requestWater;
      }
      if (desired && !pump) waterStarted = now;
      pump = desired;
    }
    if (!valid) lamp = false;
    else if (automatic) lamp = lamp ? in.light < 40 : in.light < 25;
    else lamp = requestLight;
  }
};
