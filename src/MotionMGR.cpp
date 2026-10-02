#include "MotionMGR.h"

XY2_100* _galvo;
LaserController* _laser;
GCode* currentGcode;

MotionMGR::MotionMGR(CircularBuffer<GCode, BUFFERSIZE> *buf)
{
  bufRef = buf;
}

void MotionMGR::begin(XY2_100* galvo, LaserController* laser)
{
  _galvo = galvo;
  _laser = laser;
  _status = IDLE;
}

MotionStatus MotionMGR::getStatus()
{
  return _status;
}

void MotionMGR::tic()
{
  _NOW = nanos();
  switch (_status) {
    case IDLE: processGcodes(); break;
    case INTERPOLATING: interpolateMove(); break;
    case SETTLING: if (_NOW >= SETTLE_ENDNANOS) _status = IDLE; break;
    default: break;
  }

  // Tier-1 serial dead-man: cut the beam if the laser is on but OPAL is IDLE,
  // the command buffer is empty, AND no serial line has arrived recently.
  //
  // The IDLE + empty-buffer requirement is critical: it prevents false-tripping
  // while OPAL is legitimately executing a layer's buffered moves (during the
  // host's post-stream M400 wait the buffer is non-empty or status is
  // INTERPOLATING).  This only fires in the "host died leaving the beam on"
  // case — nothing left to do, laser still enabled, host silent.
  //
  // Timeout is LASER_SERIAL_TIMEOUT_MS (defined in SerialCMDReader.h, default
  // 1000 ms — tunable).
  if (CURRENT_LASERENABLED
      && _status == IDLE
      && bufRef->isEmpty()
      && (millis() - lastSerialMillis) > LASER_SERIAL_TIMEOUT_MS)
  {
    CURRENT_LASERENABLED = false;           // updateBeam() darkens the pin below
  }

  setGalvoPosition(CURRENT_CMD_X, CURRENT_CMD_Y);
  updateBeam();
}

// The beam fires only while the mirror scans. M3 arms it; it lights when a G1
// is moving (after LASER_ON_DELAY_US) and goes dark when the G1 ends (after
// LASER_OFF_DELAY_US), unless the next queued command is another G1, as at
// the corners of a contour. Thus a host that is slow to send the next line
// leaves the beam dark, not lit on one spot: OPAL answers "ok" when it reads
// a line, so the queue is often empty between lines.
void MotionMGR::updateBeam()
{
  bool scanMove = _status == INTERPOLATING && CURRENT_CODE == 1;
  if (!CURRENT_LASERENABLED)
    BEAM_ON = false;
  else if (scanMove && !isMoveFirstInterpolation)
    BEAM_ON = BEAM_ON || _NOW >= CURRENT_STARTNANOS + LASER_ON_DELAY_US * NS_PER_US;
  else if (scanMove)
    ;                                       // a G1 starts on the next pass: hold
  else if (BEAM_ON && !nextIsScan())
    BEAM_ON = _NOW < LAST_SCAN_ENDNANOS + LASER_OFF_DELAY_US * NS_PER_US;

  double power = BEAM_ON ? CURRENT_S : 0;
  if (power != LAST_POWER)
  {
    setLaserPower(power);
    LAST_POWER = power;
  }
}

// The command after this one is a G1. The queue pops from its back.
bool MotionMGR::nextIsScan()
{
  if (bufRef->isEmpty()) return false;
  const GCode &next = bufRef->last();
  return next.codeprefix == 'G' && next.code == 1;
}


void MotionMGR::processMcode(GCode* code)
{
  #ifdef DEBUG_GCODES
    Serial.print("Processing Mcode from Queue: \n");
    Serial.print("code"); Serial.println(code->code);
  #endif
  if(code->codeprefix == 'M') {
    //Handle MCodes
    switch (code->code)
    {
    case 3:
    case 4:
    //M3 / M4
      
      setVal(&CURRENT_S, code->s);
      CURRENT_LASERENABLED = true;
      break;
    case 5:
    //M5
      CURRENT_LASERENABLED = false;
      break;
    case 9:
      setNextFWDMSG(code->FWD_CMD);
      //TODO: Should set a 'WaitForM400Sync' Flag.
      return;
      
    case 17:
    //M17
      digitalWrite(GALVO_SSR_OUT_PIN,1);
      return;
    case 18:
    //M18
      digitalWrite(GALVO_SSR_OUT_PIN,0);
      return;

    case 80:
    //M80
    // A whole line, and only when debugging: text without a line end ran
    // into the next "ok" ("Set 1 highok"), which the host waits for.
    #ifdef DEBUG_GCODES
      Serial.println("Set 1 high");
    #endif
      digitalWrite(LASER_SSR_OUT_PIN,1);
      if(_laser->isHalted())
      {
        delay(250);
        _laser->begin(LASER_PWM_OUT_PIN,LASER_SSR_OUT_PIN);
        LAST_POWER = 0;                     // begin() leaves the pin dark
      }
      return;
    case 81:
    //M81
    #ifdef DEBUG_GCODES
      Serial.println("Set 1 low");
    #endif
      digitalWrite(LASER_SSR_OUT_PIN,0);
      if(!_laser->isHalted())
      {
        _laser->stop();
        LAST_POWER = 0;                     // stop() leaves the pin dark
      }
      return;

    case 400:
    // M400 — "scan done" synchronisation token.
    //
    // Contract: the host appends "M400\n" after the last G-code of a layer and
    // then reads replies from OPAL until it sees exactly the line "done".
    // Because OPAL processes commands STRICTLY sequentially (processGcodes()
    // pops one command per tic() only while _status==IDLE, and every move
    // completes — returning to IDLE — before the next command is popped),
    // M400 cannot be reached until every preceding move has physically
    // finished.  "done" is therefore an accurate motion-complete signal.
      Serial.println("done");
      return;

    default:
      break;
    }
  }
  else  {
    //Handle GCodes
    switch (code->code)
    {
    case 90:
    //G90
      CURRENT_ABSOLUTE = true;
      break;
    case 91:
    //G91
      CURRENT_ABSOLUTE = false;
      break;    
    default:
      break;
    }
  }
}
bool MotionMGR::processGcodes()
{
  bool gcodeFound = false;
  if(!bufRef->isEmpty())
  {
    currentGcode = new GCode((bufRef->pop()));
    if(!(
      (*currentGcode).codeprefix == 'G' && (
            (*currentGcode).code == 0 ||
            (*currentGcode).code == 1 ||
            (*currentGcode).code == 2 ||
            (*currentGcode).code == 3 ||
            (*currentGcode).code == 28 )))
    {
      processMcode(currentGcode);
    }
    else
    {
      processGcode(currentGcode);
    }
    gcodeFound = true;     
    delete currentGcode;
  }
  return gcodeFound;
}
void MotionMGR::processGcode(GCode* code)
{

  // Serial.print("\n CURRENT_ABSOLUTE: ");Serial.print(CURRENT_ABSOLUTE);
  // Serial.print("\n CURRENT_S: ");Serial.print(CURRENT_S);
  // Serial.print("\n CURRENT_F: ");Serial.print(CURRENT_F);
  // Serial.print("\n CURRENT_J: ");Serial.print(CURRENT_J);
  // Serial.print("\n CURRENT_I: ");Serial.print(CURRENT_I);
  // Serial.print("\n CURRENT_TO_X: ");Serial.print(CURRENT_TO_X);
  // Serial.print("\n CURRENT_TO_Y: ");Serial.print(CURRENT_TO_Y);
  // Serial.print("\n CURRENT_TO_Z: ");Serial.print(CURRENT_TO_Z);
  // Serial.print("\n CURRENT_CMD_X: ");Serial.print(CURRENT_CMD_X);
  // Serial.print("\n CURRENT_CMD_Y: ");Serial.print(CURRENT_CMD_Y);
  // Serial.print("\n CURRENT_CMD_Z: ");Serial.print(CURRENT_CMD_Z);
  // Serial.print("\n CURRENT_FROM_X: ");Serial.print(CURRENT_FROM_X);
  // Serial.print("\n CURRENT_FROM_Y: ");Serial.print(CURRENT_FROM_Y);
  // Serial.print("\n CURRENT_FROM_Z: ");Serial.print(CURRENT_FROM_Z);
  // Serial.print("\n CURRENT_CODE: ");Serial.print(CURRENT_CODE);

  CURRENT_CODE = 0;
  switch (code->code) {
    case 0:
      CURRENT_CODE = 0;
      CURRENT_LASERENABLED = false;         // a jump is never lit, M5 or not
      setXY(code);
      break;
   case 1:
      CURRENT_CODE = 1;
      setVal(&CURRENT_F, code->f);
      setVal(&CURRENT_S, code->s);
      setXY(code);
      break;
      //TODO: Add G2 / G3 Implementation
    /* case 2:
      CURRENT_CODE = 2;
      setVal(&CURRENT_F, code->f);
      setVal(&CURRENT_S, code->s);
      setVal(&CURRENT_I, code->i);
      setVal(&CURRENT_J, code->j);
      setXYZ(code);
      //NOT IMPLEMENTED
      return;
      break;
    case 3:
      CURRENT_CODE = 3;
      setVal(&CURRENT_F, code->f);
      setVal(&CURRENT_S, code->s);
      setVal(&CURRENT_I, code->i);
      setVal(&CURRENT_J, code->j);
      setXYZ(code);
      //NOT IMPLEMENTED
      return;
      break;
    case 4:
      CURRENT_CODE = 4;
      //NOT IMPLEMENTED
      return;
      break; */
    case 28:
      CURRENT_CODE = 28;
      CURRENT_LASERENABLED = false;
      CURRENT_TO_X = 0;
      CURRENT_TO_Y = 0;
      CURRENT_TO_Z = 0;
      break;
    default:
      CURRENT_CODE = 9999;
      break;
  }
  _status = INTERPOLATING;
}

// v clamped to 0..max; NaN gives 0.
double MotionMGR::inField(double v, double max)
{
  return fmin(fmax(v, 0.0), max);
}

void MotionMGR::setVal(double* varToSet, double valToSet)
{
  if(valToSet!=MAX_VAL)
    *varToSet = valToSet;
}
void MotionMGR::setValG91(double* varToSet, double valToSet, double base)
{
  if(valToSet!=MAX_VAL)
    *varToSet = base + valToSet;
  else
    *varToSet = base;
}
void MotionMGR::setXY(GCode* code)
{
  if(CURRENT_ABSOLUTE) {                               //G90 - ABSOLUTE
    setVal(&CURRENT_TO_X, code->x);
    setVal(&CURRENT_TO_Y, code->y);
    setVal(&CURRENT_TO_Z, code->z);
  }
  else{                                               //G91 - RELATIVE
    setValG91(&CURRENT_TO_X, code->x,CURRENT_FROM_X);
    setValG91(&CURRENT_TO_Y, code->y,CURRENT_FROM_Y);
    setValG91(&CURRENT_TO_Z, code->z,CURRENT_FROM_Z);
  }
}

void MotionMGR::interpolateMove()
{
  if(isMoveFirstInterpolation)
  {
    if(CURRENT_CODE == 0 || CURRENT_CODE == 28)
    {
      // A jump is one step. The next command waits until the mirror has
      // arrived: JUMP_DELAY_MIN_US plus JUMP_DELAY_PER_MM_US per mm.
      // On field coordinates: the mirror saturates at the field edge, and a
      // garbage target (1e400, nan) must not become an endless wait.
      double dx = inField(CURRENT_TO_X, X_MAX_POS_MM) - inField(CURRENT_FROM_X, X_MAX_POS_MM);
      double dy = inField(CURRENT_TO_Y, Y_MAX_POS_MM) - inField(CURRENT_FROM_Y, Y_MAX_POS_MM);
      double jump = sqrt(dx * dx + dy * dy);
      SETTLE_ENDNANOS = _NOW + (uint64_t)((JUMP_DELAY_MIN_US + jump * JUMP_DELAY_PER_MM_US) * NS_PER_US);
      CURRENT_FROM_X = CURRENT_TO_X;
      CURRENT_FROM_Y = CURRENT_TO_Y;
      CURRENT_FROM_Z = CURRENT_TO_Z;
      CURRENT_CMD_X = CURRENT_TO_X;
      CURRENT_CMD_Y = CURRENT_TO_Y;
      CURRENT_CMD_Z = CURRENT_TO_Z;
      _status = SETTLING;
      isMoveFirstInterpolation = true;
      return;
    }
    if(CURRENT_CODE == 1)
    {
      CURRENT_DISTANCE_X = CURRENT_TO_X-CURRENT_FROM_X;
      CURRENT_DISTANCE_Y = CURRENT_TO_Y-CURRENT_FROM_Y;
      calculateMoveLengthNanos(CURRENT_DISTANCE_X, CURRENT_DISTANCE_Y, CURRENT_F, &CURRENT_DURATION);
      CURRENT_STARTNANOS = _NOW;
      CURRENT_ENDNANOS = _NOW + CURRENT_DURATION;
      isMoveFirstInterpolation = false;
    }
  }

  //Actual interpolation
  if(_NOW >= CURRENT_ENDNANOS)
  {
    //done interpolating
    CURRENT_FROM_X = CURRENT_TO_X;
    CURRENT_FROM_Y = CURRENT_TO_Y;
    CURRENT_FROM_Z = CURRENT_TO_Z;
    CURRENT_CMD_X = CURRENT_TO_X;
    CURRENT_CMD_Y = CURRENT_TO_Y;
    CURRENT_CMD_Z = CURRENT_TO_Z;
    if (CURRENT_CODE == 1) LAST_SCAN_ENDNANOS = _NOW;
    _status = IDLE;
    isMoveFirstInterpolation = true;
    return;
  }
  else
  {
    double fraction_of_move = (double)(_NOW-CURRENT_STARTNANOS)/CURRENT_DURATION;
    CURRENT_CMD_X = (CURRENT_FROM_X + (CURRENT_DISTANCE_X*fraction_of_move));
    CURRENT_CMD_Y = (CURRENT_FROM_Y + (CURRENT_DISTANCE_Y*fraction_of_move));
    return;
  }
}

/* 
  MotionMGR::calculateMoveLengthNanos
    Velocity is presumed to be in mm/s
    lengthOfMove = calc hypotenuse a^2+b^2=c^2
    result <-- (mm)/(mm/s) = s   (movelength/moveVolocity) -> (example) 2units / (4units/second) = 0.5seconds *1000 = 500ms
 */
void MotionMGR::calculateMoveLengthNanos(double xdist, double ydist, double moveVelocity, double* result)  {  
  //TODO: Verify unit conversions
  double lengthOfMove = sqrt( (0.0 + xdist)*(0.0 + xdist)  + (0.0 + ydist)*(0.0 + ydist) ); 
  *result = ((lengthOfMove*1000*1000*1000/moveVelocity));  
  return;
}







