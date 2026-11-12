#include "../wokwi/integrado/ControlLogic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(){
  Inputs in; in.sensorOk=true; in.soil=30; in.tank=70; in.light=80;
  ControlLogic c; c.tick(in,100); assert(c.pump&&!c.lamp);
  in.soil=45;c.tick(in,200);assert(c.pump);
  in.soil=60;c.tick(in,300);assert(!c.pump);
  in.soil=30;c.tick(in,400);in.tank=10;c.tick(in,500);assert(!c.pump);
  in.tank=18;c.tick(in,600);assert(!c.pump);
  in.tank=21;c.tick(in,700);assert(c.pump);
  in.sensorOk=false;c.tick(in,800);assert(!c.pump&&!c.lamp);
  in.sensorOk=true;in.networkRequired=true;in.networkUp=false;c.tick(in,900);assert(!c.pump);
  in.networkUp=true;c.tick(in,1000);assert(c.pump);
  c.tick(in,61000);assert(!c.pump&&c.timeoutLatch);
  c.tick(in,62000);assert(!c.pump);c.acknowledge();c.tick(in,63000);assert(c.pump);
  c.setMode(false);c.manualWater(true,64000);c.tick(in,64000);assert(c.pump);
  c.tick(in,94000);assert(!c.pump);
  c.manualWater(true,95000);in.tank=0;c.tick(in,95000);assert(!c.pump);
  in.tank=80;in.light=10;c.setMode(true);c.tick(in,96000);assert(c.lamp);
  in.light=30;c.tick(in,97000);assert(c.lamp);
  in.light=40;c.tick(in,98000);assert(!c.lamp);
  in.soil=NAN;c.tick(in,99000);assert(!c.pump);
  puts("PASS: hysteresis, tank lockout, sensor fault, network loss, hard timeout, manual lease, light, invalid input");
}
