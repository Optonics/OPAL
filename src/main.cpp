/*
  main.cpp - Main projectfile to run OPAL FW on PJRC Teensy 4.x board

  Part of OpenGalvo - OPAL Firmware

  Copyright (c) 2020-2021 Daniel Olsson

  OPAL Firmware is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPAL Firmware is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPAL Firmware.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "main.h"


static CircularBuffer<GCode, BUFFERSIZE> commandBuffer;
static CircularBuffer<String, BUFFERSIZE> FWDBuffer;

MotionMGR* motion;

XY2_100* galvo;

SerialCMDReader *serialReciever;

#ifdef LASER_IS_SYNRAD
//Synrad48Ctrl syn;
#endif
LaserController *laser;
void setup() {
  serialReciever = new SerialCMDReader(&commandBuffer);
  serialReciever->begin();

  // Init pins
  pinMode(LASER_SSR_OUT_PIN, OUTPUT);
  digitalWrite(LASER_SSR_OUT_PIN,0);

  // Drive the PWM modulation pin LOW explicitly before laser->begin() so pin 6
  // cannot float at power-on. DiodeLaserCtrl::begin() will configure it as a
  // PWM output and write 0 duty, but this GPIO LOW ensures it is safe even
  // during the brief window between power-on and begin().
  pinMode(LASER_PWM_OUT_PIN, OUTPUT);
  digitalWrite(LASER_PWM_OUT_PIN, LOW);

  pinMode(GALVO_SSR_OUT_PIN, OUTPUT);
  digitalWrite(GALVO_SSR_OUT_PIN,0);

  #ifdef LASER_IS_DIODE
  laser = new DiodeLaserCtrl();
  #elif defined(LASER_IS_SYNRAD)
  laser = new Synrad48Ctrl();
  #endif
  laser->begin(LASER_PWM_OUT_PIN, LASER_SSR_OUT_PIN);
  //init Galvo Protocol
  galvo = new XY2_100();
  galvo->begin(); //TODO:ADD define "Galvo has SSR" for galvo PSU

  motion = new MotionMGR(&commandBuffer);
  motion->begin(galvo, laser);
  printWelcome();
  Serial5.begin(115200);

  Serial5.print("G28\n");
}
char* nextFWDMSG[150];
void loop() {  
  if(Serial5.available())
  {
    ReadSerial5();
  }
  else
  {
    if(!FWDBuffer.isEmpty())
    {
      String data = FWDBuffer.pop();
      Serial5.print(data);
      Serial5.print("\n");
    }
  }
  serialReciever->handleSerial();
  motion->tic();

}

void setGalvoPosition(double x, double y)
{
  // Float scaling in GalvoMap.h preserves sub-mm resolution; AXIS_INVERSE_*
  // (from main.h) keep their current meaning -- both false unless defined.
  uint16_t tmp_x = mmToGalvoCount(x, X_MAX_POS_MM, AXIS_INVERSE_X);
  uint16_t tmp_y = mmToGalvoCount(y, Y_MAX_POS_MM, AXIS_INVERSE_Y);
  galvo->setPos(tmp_x, tmp_y);
}

void setLaserPower(double PWM)
{
  // duty = power, 0 = off (no Synrad floor); mapping is unit-tested in
  // test/native/test_laser_map.cpp.
  laser->update(laserDutyFromS(PWM, LASER_MAX, LASER_RESOLUTION));
}

void setNextFWDMSG(char MSG[150])
{
  String str = String(MSG);
  FWDBuffer.unshift(str);
}

bool ReadSerial5()
{
  bool retval = false;
  if (Serial5.available()) {
    retval = true;
    static char worda[COMMAND_SIZE], *pSdata=worda;
    byte ch;

    ch = Serial5.read();
    //mcnt++;
    // -1 for null terminator space
    if ((pSdata - worda)>=COMMAND_SIZE-1) {
        pSdata--;
        Serial.print("Serial5: BUFFER OVERRUN\n");
    }

    *pSdata++ = (char)ch;
    if (ch=='\n' || ((pSdata - worda)>=COMMAND_SIZE-3))// Command received and ready.
    {
      
      pSdata = worda;
      Serial.print("\nECHO Serial5: ");Serial.println(worda);
      xinit_process_string(worda);
    }
  }
  return retval;
}

/*
Used by Serial5 to clear input string array.
*/
void xinit_process_string(char instruction[])  {
  //init our command
  for (byte i=0; i<COMMAND_SIZE; i++)
    instruction[i] = 0;
  //mcnt = 0;
}