/*
 4-28-2011
 Spark Fun Electronics 2011
 Nathan Seidle
 
 This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).
 
 This example shows how to play a given track (track001.mp3 for example) from the SD card when a GPIO pin goes low. 
 This code monitors A0 through A5. If A0 is pulled low then track 1 plays. If A5 is pulled low then the track # simply advances.
 
 This code is heavily based on the "MP3 PlayTrack" example code. Please be sure to review it for more comments and gotchas.
 
 While track is playing, Arduino scans to see if pins change. If pins change, stop playing track, check pins again to get 
 trigger # and then decide what new track to play.
 
 */

#include <SPI.h>

#define TRUE  0
#define FALSE  1

//Add the SdFat Libraries
#include <SdFat.h>
#include <SdFatUtil.h> 

//Create the variables to be used by SdFat Library
Sd2Card card;
SdVolume volume;
SdFile root;
SdFile track;

//This is the name of the file on the microSD card you would like to play
//Stick with normal 8.3 nomeclature. All lower-case works well.
//Note: you must name the tracks on the SD card with 001, 002, 003, etc. 
//For example, the code is expecting to play 'track002.mp3', not track2.mp3.
char trackName[] = "track001.mp3";
int trackNumber = 1;
int previousTrigger = 1; //This indicates that we've already triggered on 1

long lastCheck; //This stores the last millisecond since we had a trigger

char errorMsg[100]; //This is a generic array used for sprintf of error messages

int trigger0 = A0;
int trigger1 = A1;
int trigger2 = A2;
int trigger3 = A3;
int trigger4 = A4;
int trigger5 = A5;

//MP3 Player Shield pin mapping. See the schematic
#define MP3_XCS 6 //Control Chip Select Pin (for accessing SPI Control/Status registers)
#define MP3_XDCS 7 //Data Chip Select / BSYNC Pin
#define MP3_DREQ 2 //Data Request Pin: Player asks for more data
#define MP3_RESET 8 //Reset is active low
//Remember you have to edit the Sd2PinMap.h of the sdfatlib library to correct control the SD card.

//VS10xx SCI Registers
#define SCI_MODE 0x00
#define SCI_STATUS 0x01
#define SCI_BASS 0x02
#define SCI_CLOCKF 0x03
#define SCI_DECODE_TIME 0x04
#define SCI_AUDATA 0x05
#define SCI_WRAM 0x06
#define SCI_WRAMADDR 0x07
#define SCI_HDAT0 0x08
#define SCI_HDAT1 0x09
#define SCI_AIADDR 0x0A
#define SCI_VOL 0x0B
#define SCI_AICTRL0 0x0C
#define SCI_AICTRL1 0x0D
#define SCI_AICTRL2 0x0E
#define SCI_AICTRL3 0x0F

void setup() {
  //Setup the triggers as inputs
  pinMode(trigger0, INPUT);
  pinMode(trigger1, INPUT);
  pinMode(trigger2, INPUT);
  pinMode(trigger3, INPUT);
  pinMode(trigger4, INPUT);
  pinMode(trigger5, INPUT);

  //Enable pullups on triggers
  digitalWrite(trigger0, HIGH);
  digitalWrite(trigger1, HIGH);
  digitalWrite(trigger2, HIGH);
  digitalWrite(trigger3, HIGH);
  digitalWrite(trigger4, HIGH);
  digitalWrite(trigger5, HIGH);

  pinMode(MP3_DREQ, INPUT);
  pinMode(MP3_XCS, OUTPUT);
  pinMode(MP3_XDCS, OUTPUT);
  pinMode(MP3_RESET, OUTPUT);

  digitalWrite(MP3_XCS, HIGH); //Deselect Control
  digitalWrite(MP3_XDCS, HIGH); //Deselect Data
  digitalWrite(MP3_RESET, LOW); //Put VS1053 into hardware reset

  Serial.begin(57600); //Use serial for debugging 
  Serial.println("MP3 Player Example using Control");

  //Setup SD card interface
  pinMode(10, OUTPUT);       //Pin 10 must be set as an output for the SD communication to work.
  if (!card.init(SPI_FULL_SPEED, 9))  Serial.println("Error: Card init"); //Initialize the SD card, shield uses pin 9 for SD CS
  if (!volume.init(&card)) Serial.println("Error: Volume ini"); //Initialize a volume on the SD card.
  if (!root.openRoot(&volume)) Serial.println("Error: Opening root"); //Open the root directory in the volume. 

  //We have no need to setup SPI for VS1053 because this has already been done by the SDfatlib

  //From page 12 of datasheet, max SCI reads are CLKI/7. Input clock is 12.288MHz. 
  //Internal clock multiplier is 1.0x after power up. 
  //Therefore, max SPI speed is 1.75MHz. We will use 1MHz to be safe.
  SPI.setClockDivider(SPI_CLOCK_DIV16); //Set SPI bus speed to 1MHz (16MHz / 16 = 1MHz)
  SPI.transfer(0xFF); //Throw a dummy byte at the bus

  //Initialize VS1053 chip 
  delay(10);
  digitalWrite(MP3_RESET, HIGH); //Bring up VS1053

  //Mp3SetVolume(20, 20); //Set initial volume (20 = -10dB) LOUD
  Mp3SetVolume(40, 40); //Set initial volume (20 = -10dB) Manageable
  //Mp3SetVolume(80, 80); //Set initial volume (20 = -10dB) More quiet

  //Now that we have the VS1053 up and running, increase the internal clock multiplier and up our SPI rate
  Mp3WriteRegister(SCI_CLOCKF, 0x60, 0x00); //Set multiplier to 3.0x

  //From page 12 of datasheet, max SCI reads are CLKI/7. Input clock is 12.288MHz. 
  //Internal clock multiplier is now 3x.
  //Therefore, max SPI speed is 5MHz. 4MHz will be safe.
  SPI.setClockDivider(SPI_CLOCK_DIV4); //Set SPI bus speed to 4MHz (16MHz / 4 = 4MHz)

  //MP3 IC setup complete

  trackNumber = 1; //Setup to play track001 by default

  previousTrigger = 255; //Setup the new vs old trigger
}

void loop(){
  //Let's look for a trigger activation
  int triggerNumber = 255;

  while(triggerNumber == 255) triggerNumber = checkTriggers(); //Wait for a trigger to be activated

  if(triggerNumber == 5) trackNumber++; //Simply advance the track
  if(trackNumber > 10) trackNumber = 1; //Loop tracks

  if(triggerNumber == 0) trackNumber = 1; //Play a certain track
  if(triggerNumber == 1) trackNumber = 2;
  if(triggerNumber == 2) trackNumber = 3;
  if(triggerNumber == 3) trackNumber = 4;
  if(triggerNumber == 4) trackNumber = 5;

  //Let's play a track of a given number
  sprintf(trackName, "track%03d.mp3", trackNumber); //Splice the new file number into this file name
  playMP3(trackName); //Go play trackXXX.mp3
}

//checkTriggers checks the level of the various inputs
//It returns 255 if no trigger is activated
//Otherwise, it returns the number of the first trigger it sees
int checkTriggers(void) {

#define DEBOUNCE  100

  int foundTrigger = 255;

  //Once a trigger is activated, we don't want to trigger on it perpetually
  //But after 3 seconds, reset the previous trigger number
  if( (previousTrigger != 255) && (millis() - lastCheck) > 3000) {
    lastCheck = millis();
    previousTrigger = 255;
    Serial.println("Previous trigger reset");
  }

  //Debounce button, then check again
  if(digitalRead(trigger0) == LOW){ 
    delay(DEBOUNCE); 
    if(digitalRead(trigger0) == LOW) foundTrigger = 0;
  }
  else if(digitalRead(trigger1) == LOW ){ 
    delay(DEBOUNCE); 
    if(digitalRead(trigger1) == LOW) foundTrigger = 1;
  }
  else if(digitalRead(trigger2) == LOW){ 
    delay(DEBOUNCE); 
    if(digitalRead(trigger2) == LOW) foundTrigger = 2;
  }
  else if(digitalRead(trigger3) == LOW){ 
    delay(DEBOUNCE); 
    if(digitalRead(trigger3) == LOW) foundTrigger = 3;
  }
  else if(digitalRead(trigger4) == LOW){ 
    delay(DEBOUNCE); 
    if(digitalRead(trigger4) == LOW) foundTrigger = 4;
  }
  else if(digitalRead(trigger5) == LOW){ 
    delay(DEBOUNCE); 
    if(digitalRead(trigger5) == LOW) foundTrigger = 5;
  }

  if(foundTrigger != previousTrigger){ //We've got a new trigger!
    previousTrigger = foundTrigger; 

    Serial.print("T");
    Serial.println(foundTrigger, DEC);

    return(foundTrigger);
  }
  else
    return(255); //No triggers pulled low (activated)
}

//PlayMP3 plays a given file name
//It pulls 32 byte chunks from the SD card and throws them at the VS1053
//We monitor the DREQ (data request pin). If it goes low then we determine if
//we need new data or not. If yes, pull new from SD card. Then throw the data
//at the VS1053 until it is full.
void playMP3(char* fileName) {

  if (!track.open(&root, fileName, O_READ)) { //Open the file in read mode.
    sprintf(errorMsg, "Failed to open %s", fileName);
    Serial.println(errorMsg);
    return;
  }

  sprintf(errorMsg, "Playing track %s", fileName);
  Serial.println(errorMsg);

  uint8_t mp3DataBuffer[32]; //Buffer of 32 bytes. VS1053 can take 32 bytes at a go.
  int need_data = TRUE; 

  while(1) {
    while(!digitalRead(MP3_DREQ)) { 
      //DREQ is low while the receive buffer is full
      //You can do something else here, the buffer of the MP3 is full and happy.
      //Maybe set the volume or test to see how much we can delay before we hear audible glitches

      //If the MP3 IC is happy, but we need to read new data from the SD, now is a great time to do so
      if(need_data == TRUE) {
        if(!track.read(mp3DataBuffer, sizeof(mp3DataBuffer))) { //Try reading 32 new bytes of the song
          //Oh no! There is no data left to read!
          //Time to exit
          break;
        }
        need_data = FALSE;
      }

      //Check to see if we need to bail on this track
      if(checkTriggers() != 255) { 
        Serial.println("Exiting MP3!");
        track.close(); //Close this track!
        previousTrigger = 255; //Trick the next check into thinking we haven't seen a previous trigger
        return;
      }
    }

    if(need_data == TRUE){ //This is here in case we haven't had any free time to load new data
      if(!track.read(mp3DataBuffer, sizeof(mp3DataBuffer))) { //Go out to SD card and try reading 32 new bytes of the song
        //Oh no! There is no data left to read!
        //Time to exit
        break;
      }
      need_data = FALSE;
    }

    //Once DREQ is released (high) we now feed 32 bytes of data to the VS1053 from our SD read buffer
    digitalWrite(MP3_XDCS, LOW); //Select Data
    for(int y = 0 ; y < sizeof(mp3DataBuffer) ; y++)
      SPI.transfer(mp3DataBuffer[y]); // Send SPI byte

    digitalWrite(MP3_XDCS, HIGH); //Deselect Data
    need_data = TRUE; //We've just dumped 32 bytes into VS1053 so our SD read buffer is empty. Set flag so we go get more data
  }

  while(!digitalRead(MP3_DREQ)) ; //Wait for DREQ to go high indicating transfer is complete
  digitalWrite(MP3_XDCS, HIGH); //Deselect Data

  track.close(); //Close out this track

  sprintf(errorMsg, "Track %s done!", fileName);
  Serial.println(errorMsg);
}


//Write to VS10xx register
//SCI: Data transfers are always 16bit. When a new SCI operation comes in 
//DREQ goes low. We then have to wait for DREQ to go high again.
//XCS should be low for the full duration of operation.
void Mp3WriteRegister(unsigned char addressbyte, unsigned char highbyte, unsigned char lowbyte){
  while(!digitalRead(MP3_DREQ)) ; //Wait for DREQ to go high indicating IC is available
  digitalWrite(MP3_XCS, LOW); //Select control

  //SCI consists of instruction byte, address byte, and 16-bit data word.
  SPI.transfer(0x02); //Write instruction
  SPI.transfer(addressbyte);
  SPI.transfer(highbyte);
  SPI.transfer(lowbyte);
  while(!digitalRead(MP3_DREQ)) ; //Wait for DREQ to go high indicating command is complete
  digitalWrite(MP3_XCS, HIGH); //Deselect Control
}

//Read the 16-bit value of a VS10xx register
unsigned int Mp3ReadRegister (unsigned char addressbyte){
  while(!digitalRead(MP3_DREQ)) ; //Wait for DREQ to go high indicating IC is available
  digitalWrite(MP3_XCS, LOW); //Select control

  //SCI consists of instruction byte, address byte, and 16-bit data word.
  SPI.transfer(0x03);  //Read instruction
  SPI.transfer(addressbyte);

  char response1 = SPI.transfer(0xFF); //Read the first byte
  while(!digitalRead(MP3_DREQ)) ; //Wait for DREQ to go high indicating command is complete
  char response2 = SPI.transfer(0xFF); //Read the second byte
  while(!digitalRead(MP3_DREQ)) ; //Wait for DREQ to go high indicating command is complete

  digitalWrite(MP3_XCS, HIGH); //Deselect Control

  int resultvalue = response1 << 8;
  resultvalue |= response2;
  return resultvalue;
}

//Set VS10xx Volume Register
void Mp3SetVolume(unsigned char leftchannel, unsigned char rightchannel){
  Mp3WriteRegister(SCI_VOL, leftchannel, rightchannel);
}

