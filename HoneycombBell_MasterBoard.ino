/* ========================================
 *
 *  HoneycombBell_Proto6
 *    description: Main Loop
 *    for Arduino Leonardo
 *
 *  Copyright(c)2020- Masahiko Hasebe at Kigakudoh
 *  This software is released under the MIT License, see LICENSE.txt
 *
 * ========================================
 */
#include  <MsTimer2.h>
#include  <Adafruit_NeoPixel.h>
#include  <MIDI.h>
#include  <MIDIUSB.h>

#include  "configuration.h"
#include  "TouchMIDI_AVR_if.h"

#include  "i2cdevice.h"
#include  "honeycombbell.h"

#ifdef __AVR__
  #include <avr/power.h>
#endif

/*----------------------------------------------------------------------------*/
//
//     Global Variables
//
/*----------------------------------------------------------------------------*/
#define J23PIN 6

#define PIN 4

#define MODEPIN1  8
#define MODEPIN2  9
#define MODEPIN3  10
#define MODEPIN4  11

Adafruit_NeoPixel led = Adafruit_NeoPixel(MAX_LED, PIN, NEO_GRB + NEO_KHZ800);
MIDI_CREATE_DEFAULT_INSTANCE();

/*----------------------------------------------------------------------------*/
int touchSensor1Err;
int touchSensor2Err;
//bool isMasterBoard;

GlobalTimer gt;
static HoneycombBell hcb;

/*----------------------------------------------------------------------------*/
void flash()
{
  gt.incGlobalTime();
}
/*----------------------------------------------------------------------------*/
void setup()
{
  int err;

  //  Initialize Hardware
  wireBegin();
//  Serial.begin(31250);
  MIDI.setHandleNoteOff( handlerNoteOff );
  MIDI.setHandleNoteOn( handlerNoteOn );
  MIDI.setHandleControlChange( handlerCC );
  MIDI.begin();
  MIDI.turnThruOff();

  pinMode(J23PIN, OUTPUT);   // LED
  digitalWrite(J23PIN, LOW);

#ifdef USE_ADA88
  ada88_init();
  ada88_write(1);
#endif

  //  Read Jumper Pin Setting
  pinMode(MODEPIN1, INPUT); 
  pinMode(MODEPIN2, INPUT);
  pinMode(MODEPIN3, INPUT); 
  pinMode(MODEPIN4, INPUT);
  const uint8_t setupJmpr = digitalRead(MODEPIN1);
  const uint8_t whichMbrJmpr = digitalRead(MODEPIN2);

  //  CY8CMBR3110
  if ( setupJmpr == LOW ){  // connect
    //  Setting Mode
    if ( whichMbrJmpr == HIGH ){ // open
      touchSensor1Err = err = MBR3110_setup(0);
      touchSensor2Err = 1;
    }
    else {  //  connect
      touchSensor2Err = err = MBR3110_setup(1);
      touchSensor1Err = 1;     
    }
    if ( err == 0 ){
      //  succeeded!
      digitalWrite(J23PIN, HIGH);
    }
    while(1){
      ada88_write(2);
      //  Turn off the power!
    }
  }

  else {
    //  Normal Mode
    touchSensor1Err = MBR3110_init(0);
    touchSensor2Err = MBR3110_init(1);
    err = touchSensor1Err + touchSensor2Err;
    if ( err ){
      //  if err, stop 5sec.
      setAda88_Number(err);
      delay(5000);
    }
  }


  //  Check who I am
  int setNumber;
  const uint8_t dip1 = digitalRead(MODEPIN2);
  const uint8_t dip2 = digitalRead(MODEPIN3);
  const uint8_t dip3 = digitalRead(MODEPIN4);
  //isMasterBoard = ( digitalRead(MASTER_NORMAL) == LOW )? true:false;

  if      (( dip1 == HIGH ) && ( dip2 == HIGH ) && ( dip3 == HIGH )){ setNumber = 1;}
  else if (( dip1 == HIGH ) && ( dip2 == HIGH ) && ( dip3 == LOW  )){ setNumber = 2;}
  else if (( dip1 == HIGH ) && ( dip2 == LOW  ) && ( dip3 == LOW  )){ setNumber = 3;}
  else if (( dip1 == HIGH ) && ( dip2 == LOW  ) && ( dip3 == HIGH )){ setNumber = 4;}
  else if (( dip1 == LOW  ) && ( dip2 == HIGH ) && ( dip3 == HIGH )){ setNumber = 5;}
  else if (( dip1 == LOW  ) && ( dip2 == HIGH ) && ( dip3 == LOW  )){ setNumber = 6;}
  else { setNumber = 1;}
  hcb.setSetNumber(setNumber);
  hcb.decideOctave();

  //  Set NeoPixel Library 
  led.begin();
  led.show(); // Initialize all pixels to 'off'

  //  Set Interrupt
  MsTimer2::set(10, flash);     // 10ms Interval Timer Interrupt
  MsTimer2::start();
}
/*----------------------------------------------------------------------------*/
void loop()
{
  //  Global Timer 
  generateTimer();

  //  HoneycombBell
  hcb.mainLoop();

  //  MIDI Receive
  receiveMidi();

  if ( gt.timer10msecEvent() ){
    //  Touch Sensor
    if ( touchSensor1Err == 0 ){
      hcb.checkTwelveTouch(0);
    }
    if ( touchSensor2Err == 0 ){
      hcb.checkTwelveTouch(1);
    }
  }
}
/*----------------------------------------------------------------------------*/
//
//     Global Timer
//
/*----------------------------------------------------------------------------*/
void generateTimer( void )
{
  uint32_t diff = gt.readGlobalTimeAndClear();

  gt.clearAllTimerEvent();
  gt.updateTimer(diff);
  //setAda88_Number(diff);

  if ( gt.timer100msecEvent() == true ){
    //  for Debug
    // blink LED
    (gt.timer100ms() & 0x0002)? digitalWrite(J23PIN, HIGH):digitalWrite(J23PIN, LOW);
    //setAda88_Number(gt.timer100ms());
  }
}
/*----------------------------------------------------------------------------*/
//
//     MIDI Command & UI
//
/*----------------------------------------------------------------------------*/
void receiveMidi( void ){ MIDI.read();}
/*----------------------------------------------------------------------------*/
void setMidiNoteOn( uint8_t dt0, uint8_t dt1 )
{
//  uint8_t dt[3] = { 0x90, dt0, dt1 };
//  Serial.write(dt,3);
  MIDI.sendNoteOn( dt0, dt1, 1 );

  midiEventPacket_t event = {0x09, 0x90, dt0, dt1};
  MidiUSB.sendMIDI(event);
  MidiUSB.flush();
}
/*----------------------------------------------------------------------------*/
void setMidiNoteOff( uint8_t dt0, uint8_t dt1 )
{
//  uint8_t dt[3] = { 0x80, dt0, dt1 };
//  Serial.write(dt,3);
  MIDI.sendNoteOff( dt0, dt1, 1 );

  midiEventPacket_t event = {0x09, 0x90, dt0, 0};
  MidiUSB.sendMIDI(event);
  MidiUSB.flush();
}
/*----------------------------------------------------------------------------*/
void handlerNoteOn( byte channel , byte number , byte value ){ setMidiNoteOn( number, value );}
/*----------------------------------------------------------------------------*/
void handlerNoteOff( byte channel , byte number , byte value ){ setMidiNoteOff( number, value );}
/*----------------------------------------------------------------------------*/
void handlerCC( byte channel , byte number , byte value )
{
  if ( number == /*0x10*/ midi::GeneralPurposeController1 ){
    hcb.rcvClock( value );
  }
}
/*----------------------------------------------------------------------------*/
void midiClock( uint8_t msg )
{
//  uint8_t dt[3] = { 0xb0, 0x10, msg };

//  if ( isMasterBoard == false ){
//  Serial.write(dt,3);
//    MIDI.sendControlChange( midi::GeneralPurposeController1, msg, 1 );
//  }
}
/*----------------------------------------------------------------------------*/
//
//     Hardware Access Functions
//
/*----------------------------------------------------------------------------*/
void setAda88_Number( int number )
{
#ifdef USE_ADA88
  ada88_writeNumber(number);  // -1999 - 1999
#endif
}
/*----------------------------------------------------------------------------*/
//
//     Blink LED by NeoPixel Library
//
/*----------------------------------------------------------------------------*/
const uint8_t colorTable[16][3] = {
  { 200,   0,   0 },//  C
  { 175,  30,   0 },
  { 155,  50,   0 },//  D
  { 135,  70,   0 },
  { 110,  90,   0 },//  E
  {   0, 160,   0 },//  F
  {   0, 100, 100 },
  {   0,   0, 250 },//  G
  {  30,   0, 230 },
  {  60,   0, 190 },//  A
  { 100,   0, 140 },
  { 140,   0,  80 },//  B

  { 100, 100, 100 },
  { 100, 100, 100 },
  { 100, 100, 100 },
  { 100, 100, 100 }
 };
/*----------------------------------------------------------------------------*/
uint8_t colorTbl( uint8_t doremi, uint8_t rgb ){ return colorTable[doremi][rgb];}
void setLed( int ledNum, uint8_t red, uint8_t green, uint8_t blue )
{
  led.setPixelColor(ledNum,led.Color(red, green, blue));
}
void lightLed( void )
{
  led.show();
}
