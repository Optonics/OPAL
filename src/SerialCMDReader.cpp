/*
  SerialCMDReader.cpp - driver code to interpret GCode over Serial on PJRC Teensy 4.x board

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

#include <Arduino.h>
#include <CircularBuffer.h>
#include "Helpers.h"
#include "SerialCMDReader.h"

// Timestamp of the last successfully parsed command line (millis()).
// Initialised to 0; MotionMGR dead-man uses this with LASER_SERIAL_TIMEOUT_MS.
unsigned long lastSerialMillis = 0;

SerialCMDReader::SerialCMDReader(CircularBuffer<GCode, BUFFERSIZE> *buf)
{
  bufRef = buf;
}

void SerialCMDReader::begin(){
  
  }

void SerialCMDReader::stop(){}

// Reads what has arrived, up to SERIAL_BYTES_PER_PASS bytes. One byte per
// pass made a 30-byte line take 30 passes of loop(); the queue then ran dry
// while a short scan line was still being read. A full queue stops the
// reading, and USB flow control holds the rest.
void SerialCMDReader::handleSerial()
{
  for (int i = 0; i < SERIAL_BYTES_PER_PASS; i++) {
    if (SerialCMDReader::bufRef->isFull() || !Serial.available()) return;
    handleByte();
  }
}

void SerialCMDReader::handleByte()
{
    {
      static char worda[COMMAND_SIZE], *pSdata=worda;
      byte ch;

      ch = Serial.read();
      // -1 for null terminator space
      if ((pSdata - worda)>=COMMAND_SIZE-1) {
         pSdata--;
         Serial.print("BUFFER OVERRUN\n");
      }

      *pSdata++ = (char)ch;
      // The parser scans cnt characters. It counts what the buffer holds, not
      // what arrived. Thus a cut line never makes the scan leave the buffer.
      cnt = pSdata - worda;

      if (ch=='\n')// Command received and ready.
      {
        lastSerialMillis = millis(); // update dead-man timestamp on every received line
        pSdata = worda;

        /* the character / means delete block... used for comments and stuff.*/
        if (worda[0] == '/' || worda[0] == '(' || worda[0] == ';')
        {
          Serial.println("ok");
          // A comment is cleared like a command. Otherwise its characters
          // stay in the buffer and in cnt, and the next line reads them as
          // its own words.
          init_process_string(worda);
          return;
        }
        else
        {
          GCode* tmp = SerialCMDReader::process_string(worda);
          SerialCMDReader::bufRef->unshift(*tmp);
          delete tmp;
        }
        init_process_string(worda);
      }
   }
}

GCode* SerialCMDReader::process_string(char instruction[])
{
  //process our command!
  
  GCode* newCode = new GCode();
  //TODO: determine if delete newcode is needed to keep memmory clean...
  
  if(has_command('M', instruction, cnt))        {
    int startpos = has_command_at('M', instruction, cnt); //Line numbering disrupts startposition
    
    newCode->codeprefix = 'M';
    newCode->code = (double)search_string('M', instruction, cnt);
    if(newCode->code == 9)
    {
      if(startpos != -1)
        strcat((*newCode).FWD_CMD,instruction+startpos);
      else
        strcat((*newCode).FWD_CMD,instruction); //TODO: remove: Desperate recovery attempt
      return newCode;
    }
      
    newCode->s = getVal('S', instruction, cnt);
    
    Serial.println("ok");  
    return newCode;
  } // END of Mcode
  if(has_command('G', instruction, cnt))        {
    newCode->codeprefix = 'G';
    newCode->code = (double)search_string('G', instruction, cnt);

    newCode->x = getVal('X', instruction, cnt);
    newCode->y = getVal('Y', instruction, cnt);
    newCode->z = getVal('Z', instruction, cnt);
    newCode->e = getVal('E', instruction, cnt);
    newCode->a = getVal('A', instruction, cnt);
    newCode->b = getVal('B', instruction, cnt);
    newCode->c = getVal('C', instruction, cnt);
    newCode->f = getVal('F', instruction, cnt);
    newCode->i = getVal('I', instruction, cnt);
    newCode->j = getVal('J', instruction, cnt);
    newCode->p = getVal('P', instruction, cnt);
    newCode->r = getVal('R', instruction, cnt);
    newCode->s = getVal('S', instruction, cnt);
    newCode->t = getVal('T', instruction, cnt);

    Serial.println("ok"); 
  #ifdef REPORT_TEMP_EACH_G
    Serial.println(tempmonGetTemp());
  #endif
    return newCode;
  } //END of Gcode
  else 
  {
    newCode->x = getVal('X', instruction, cnt);
    newCode->y = getVal('Y', instruction, cnt);
    newCode->z = getVal('Z', instruction, cnt);
    newCode->e = getVal('E', instruction, cnt);
    newCode->a = getVal('A', instruction, cnt);
    newCode->b = getVal('B', instruction, cnt);
    newCode->c = getVal('C', instruction, cnt);
    newCode->f = getVal('F', instruction, cnt);
    newCode->i = getVal('I', instruction, cnt);
    newCode->j = getVal('J', instruction, cnt);
    newCode->p = getVal('P', instruction, cnt);
    newCode->r = getVal('R', instruction, cnt);
    newCode->s = getVal('S', instruction, cnt);
    newCode->t = getVal('T', instruction, cnt);
    
    Serial.println("ok"); 
    return newCode;
  }
}
