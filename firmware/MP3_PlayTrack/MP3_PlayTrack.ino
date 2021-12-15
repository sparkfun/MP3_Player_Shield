/*
  4-28-2011
  Spark Fun Electronics 2011
  Nathan Seidle

  This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  This example code works with the MP3 Shield (https://www.sparkfun.com/products/12660) and plays a MP3 from the
  SD card called 'track001.mp3'. The theory is that you can load a microSD card up with a bunch of MP3s and
  then play a given 'track' depending on some sort of input such as which pin is pulled low.

  Attach the shield to an Arduino. Load code (after editing Sd2PinMap.h) then open the terminal at 57600bps. This
  example shows that it takes ~30ms to load up the VS1053 buffer. We can then do whatever we want for ~100ms
  before we need to return to filling the buffer (for another 30ms).

  This code is heavily based on the example code I wrote to control the MP3 shield found here:
  http://www.sparkfun.com/products/9736
  This example code extends the previous example by reading the MP3 from an SD card and file rather than from internal
  memory of the ATmega. Because the current MP3 shield does not have a microSD socket, you will need to add the microSD
  shield to your Arduino stack.

  The main gotcha from all of this is that you have to make sure your CS pins for each device on an SPI bus is carefully
  declared. For the CS on the SD card, you need to correctly set it with pin_microSD_CS. 

  Four pins are needed to control the VS1503:
  DREQ
  CS
  DCS
  Reset (optional but good to have access to)
  Plus the SPI bus

  Only the SPI bus pins and another CS pin are needed to control the microSD card.

  What surprised me is the fact that with a normal MP3 we can do other things for up to 100ms while the MP3 IC crunches
  through its fairly large buffer of 2048 bytes. As long as you keep your sensor checks or serial reporting to under
  100ms and leave ~30ms to then replenish the MP3 buffer, you can do quite a lot while the MP3 is playing glitch free.
*/

//microSD Interface
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#include <SPI.h>
#include "SdFat.h" //http://librarymanager/All#SdFat_exFat by Bill Greiman. We currently use v2.1.2

const int pin_microSD_CS = 9; //Default pin for the SparkFun MP3 Shield
SdFat sd;
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//MP3 Shield Interface
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#include "vs1053_SdFat.h" //http://librarymanager/All#vs1053_sdFat by mpflaga

vs1053 MP3player; 
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

void setup()
{
  Serial.begin(115200); //Use serial for debugging
  Serial.println("MP3 Testing");

  if (sd.begin(pin_microSD_CS) == false)
  {
    Serial.println(F("SD init failed. Is card present? Formatted? Freezing..."));
    while (1);
  }

  MP3player.begin();
  MP3player.setVolume(40, 40); //(Left, Right) 40 is pretty good for ear buds

  //Let's play a track of a given number
  //This is the name of the file on the microSD card you would like to play
  //Stick with normal 8.3 nomeclature. All lower-case works well.
  //Note: you must name the tracks on the SD card with 001, 002, 003, etc.
  //For example, the code is expecting to play 'track002.mp3', not track2.mp3.
  
  char trackName[] = "track001.mp3";
  int trackNumber = 1;

  sprintf(trackName, "track%03d.mp3", trackNumber); //Splice the new file number into this file name

  MP3player.playMP3(trackName);
}

void loop()
{
  Serial.println("All done!");
  delay(1000);
}
