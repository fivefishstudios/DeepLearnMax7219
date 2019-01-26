#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define LED_HEARTBEAT 1

#if LED_HEARTBEAT
  #define HB_LED D2
  #define HB_LED_TIME 50 // in milliseconds
#endif


// Define the number of devices we have in the chain and the hardware interface
#define MAX_DEVICES 4

#define CLK_PIN   D5  // or SCK
#define DATA_PIN  D7  // or MOSI
#define CS_PIN    D8  // or SS

// SPI hardware interface
//MD_MAX72XX mx = MD_MAX72XX(CS_PIN, MAX_DEVICES);

//edit this as per your LED matrix hardware type
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW   
//    MD_MAX72XX::PAROLA_HW,    Use the Parola style hardware modules.
//    MD_MAX72XX::GENERIC_HW    Vertical with word wrap
//    MD_MAX72XX::ICSTATION_HW, Use ICStation style hardware module.
//    MD_MAX72XX::FC16_HW       This is our hardware type! Bougght from eBay

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Arbitrary pins
// MD_MAX72XX mx = MD_MAX72XX(DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// Global message buffers shared by Wifi and Scrolling functions
const uint8_t MESG_SIZE = 255;
const uint8_t CHAR_SPACING = 3;
const uint8_t SCROLL_DELAY = 40;

char curMessage[MESG_SIZE];
char newMessage[MESG_SIZE];
bool newMessageAvailable = false;

int prevtime;



bool direction = 0; // 0=shift left, 1=shift right


void scrollDataSink(uint8_t dev, MD_MAX72XX::transformType_t t, uint8_t col)
// Callback function for data that is being scrolled off the display
{
}

// Set the Shift Data In callback function.
// The callback function is called from the library when a transform shift left or shift right operation is executed 
// and the library needs to obtain data for the end element of the shift (ie, conceptually this is the new data that is shifted 'into' the display).
uint8_t scrollDataSource(uint8_t dev, MD_MAX72XX::transformType_t t)
// Callback function for data that is required for scrolling into the display
{
  static enum { S_IDLE,
                S_NEXT_CHAR,
                S_SHOW_CHAR,
                S_SHOW_SPACE } state;  // = S_IDLE;
  static char *p;
  static uint16_t curLen, showLen;
  static uint8_t cBuf[8];
  uint8_t colData = 0;

  // finite state machine to control what we do on the callback
  switch (state)
  {
  case S_IDLE: // reset the message pointer and check for new message to load
    p = curMessage;          // reset the pointer to start of message
    if (newMessageAvailable) // there is a new message waiting
    {
      strcpy(curMessage, newMessage); // copy it in
      newMessageAvailable = false;
    }
    state = S_NEXT_CHAR;
    break;

  case S_NEXT_CHAR: // Load the next character from the font table
    if (*p == '\0')
      state = S_IDLE;
    else
    {
      showLen = mx.getChar(*p++, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);   // returns width (in columns) of the character
      curLen = 0;
      state = S_SHOW_CHAR;
    }
    break;

  case S_SHOW_CHAR: // display the next part of the character
    colData = cBuf[curLen++];
    if (curLen < showLen)
      break;

    // set up the inter character spacing
    showLen = (*p != '\0' ? CHAR_SPACING : (MAX_DEVICES * COL_SIZE) / 2);
    curLen = 0;
    state = S_SHOW_SPACE;
    // fall through

  case S_SHOW_SPACE: // display inter-character spacing (blank column)
    curLen++;
    if (curLen == showLen)
      state = S_NEXT_CHAR;
    break;

  default:
    state = S_IDLE;
  }

  return (colData);
}

void scrollText(void)
{
  static uint32_t prevTime = 0;

  // Is it time to scroll the text?
  if (millis() - prevTime >= SCROLL_DELAY)
  {
    // TSL,  ///< Transform Shift Left one pixel element
    // TSR,  ///< Transform Shift Right one pixel element   i.e. MIRROR IMAGE
    // TSU,  ///< Transform Shift Up one pixel element
    // TSD,  ///< Transform Shift Down one pixel element
    // TFLR, ///< Transform Flip Left to Right
    // TFUD, ///< Transform Flip Up to Down
    // TRC,  ///< Transform Rotate Clockwise 90 degrees
    // TINV  ///< Transform INVert (pixels inverted) 
    // mx.transform(MD_MAX72XX::TSL); // scroll along - the callback will load all the data

    mx.transform(MD_MAX72XX::TSL); // scroll along - the callback will load all the data
    prevTime = millis();           // starting point for next time
  }
}

// setup callback after shifting left (or doing transforms)
// The callback function takes 2 parameters:
//   the device number requesting the data [0..getDeviceCount()-1]
//   one of the transformation types in transformType_t) that tells the callback function what shift is being performed 
// The return value is the data for the column to be shifted into the display.
// uint8_t myCallback(uint8_t dev, MD_MAX72XX::transformType_t t) {
//   // colData = 0b10000000; // MSB=bottom LED; LSB=top LED
//   return colData;
// }


void setup()
{
  #if LED_HEARTBEAT
    pinMode(HB_LED, OUTPUT);
    digitalWrite(HB_LED, LOW);
  #endif

  // Initialize the object. 
  // The LED hardware is initialized to the middle intensity value, all rows showing, and all LEDs cleared (off). 
  // Test, shutdown and decode modes are off. Display updates are on and wraparound is off. 
  mx.begin();   

  // Set the Shift Data In callback function.
  // The callback function is called from the library when a transform shift left or shift right operation is executed and 
  // the library needs to obtain data for the end element of the shift (ie, conceptually this is the new data that is shifted 'into' the display).
  // The callback function is invoked when 
  // WRAPAROUND is not active, as the data would automatically supplied within the library.
  // the call to transform() is global (ie, not for an individual buffer).
  mx.setShiftDataInCallback(scrollDataSource);
  // mx.setShiftDataOutCallback(scrollDataSink);   not needed




  curMessage[0] = newMessage[0] = '\0';

  // Set up message
  // sprintf(curMessage, "Hello World!");
  // char displayString[] = {'o', 'w', 'e','l', '.', 'c', 'o', 'd', 'e', 's', ' ', 3, 2, 1, 3, 4, 5, 6, 16, 17, 18, 19, 20, '\0'};    // '\0' end of string
  char displayString[] = "owel.codes \x01\x02\x3\x0F";  // '\0' end of string      
  sprintf(curMessage, displayString);  





  // set a specific X,Y pixel (LED)
  // mx.setPoint(4, 1, true);  // Row, Col, True=1=Lit    // row and col are 0 based
  // mx.setPoint(4, 2, true);  // Row, Col, True=1=Lit    // row and col are 0 based
  // mx.setPoint(4, 3, true);  // Row, Col, True=1=Lit    // row and col are 0 based

  // our code... to see if we understand this stuff
  // ----------------------------------------------------------------
  // Set all LEDs in a row to a new state on all devices.
  // This method operates on all devices, setting the value of the LEDs in the row to the specified value bit field. The method is useful for drawing patterns and lines horizontally across on the entire display. The least significant bit of the value is the lowest column number.
  // mx.setRow(0, 0b10101010);  // row (0=top, 7=bottom), binary equivalent of LEDs to turn ON (LSB=right, MSB=left)
  // mx.setRow(1, 0b01010101);  // row 1, i.e. 2nd row from top
  // mx.setRow(2, 0b10101010); 
  // mx.setRow(3, 0b01010101);
  // mx.setRow(4, 0b10101010);
  // mx.setRow(5, 0b01010101);
  // mx.setRow(6, 0b10101010);
  // mx.setRow(7, 0b01010101);  
  // NOTE: setRow affects ALL devices/modules. So if we have a 4 device display, then the above pattern are duplicated on all 4 devices
  
  // setup callback afffter shifting left (or doing transforms)
  // The callback function takes 2 parameters:
  //   the device number requesting the data [0..getDeviceCount()-1]
  //   one of the transformation types in transformType_t) that tells the callback function what shift is being performed 
  // The return value is the data for the column to be shifted into the display.
  // mx.setShiftDataInCallback(myCallback);    // or now, we just output a pre-set column data


  // shift left x times
  // for (int i=0; i<4; i++){
  //   // startDev, endDev, transformType
  //   colData = 255-pow(2, i);
  //   mx.transform(1, 1, MD_MAX72XX::TSL);   // when we shift left, the callback function will be called. Return value of cb() will be the column data for the end column
  // }
  // // shift left x times
  // for (int i=1; i<5; i++){
  //   // startDev, endDev, transformType
  //   colData = i;
  //   mx.transform(2, 2, MD_MAX72XX::TSL);   // when we shift left, the callback function will be called. Return value of cb() will be the column data for the end column
  // }
  // NOTE: If we have a callback function for setShiftDataInCallback(), then instead of 0s, it will be whatever data supplied by the callback function
  // setShiftDataInCallback() return value is the data for the column that is being shifted in.... // MSB=bottom LED; LSB=top LED
  // NOTE: You can't shift more than the number of columns in a device.. so max column is 8 for each device


  // clear/turn off the whole display, or just specific devices
  // mx.clear(2,3);

  // set LED intensity 0..15 
  // mx.control(MD_MAX72XX::INTENSITY, 1);       // 1=dimmest, 15=brightest
  // mx.control(1, MD_MAX72XX::INTENSITY, 4);   // set brightness level of 2nd device, 1=dimmest, 15=brightest

  // turn off all middle row
  // mx.setRow(4, 0); // off row 4

  // mx.setColumn(20, 0b11111111);  // col# (absolute number, counting number of devices, first col=0), new value for column (0-255)

  // mx.setColumn(1, 0b01111110);
  // mx.setColumn(2, 0b11111111);
  // mx.setColumn(3, 0b01111110);

}

void loop()
{
  // put your main code here, to run repeatedly:
  #if LED_HEARTBEAT
    static uint32_t timeLast = 0;

    if (millis() - timeLast >= HB_LED_TIME)
    {
      digitalWrite(HB_LED, digitalRead(HB_LED) == LOW ? HIGH : LOW);
      timeLast = millis();
    }
  #endif


  // scanning ... knight rider effect
  // if (direction){  // 0 = SL
  //   mx.transform(MD_MAX72XX::TSL);  
  //   if (mx.getPoint(3, 31)==true) {
  //     direction = 0;
  //   }
  // } else {
  //   mx.transform(MD_MAX72XX::TSR);
  //   if (mx.getPoint(3, 0)==true) {
  //     direction = 1;
  //   }
  // }
  // delay(30);  // ms

  scrollText();
}
