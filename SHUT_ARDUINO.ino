#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <FreeRTOS.h>
#include "LIS3DHTR.h"
#include <Wire.h>
LIS3DHTR<TwoWire> LIS; //IIC
#include <HTTPClient.h>
#define WIRE Wire
#include <ChainableLED.h>
// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <SD.h>
#include <Adafruit_VS1053.h>

// These are the pins used
#define VS1053_RESET   -1     // VS1053 reset pin (not used!)

#define VS1053_CS      32     // VS1053 chip select pin (output)
#define VS1053_DCS     33     // VS1053 Data/command select pin (output)
#define CARDCS         14     // Card chip select pin
#define VS1053_DREQ    15     // VS1053 Data request, ideally an Interrupt pin

Adafruit_VS1053_FilePlayer musicPlayer = 
  Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);
  
const char *SSID = "Basile's Galaxy A32";
const char *PWD = "vife9004";
#define NUM_LEDS  1

ChainableLED leds(A0, A1, NUM_LEDS);
const int pinAdc = A2;
const int buttonPin = 14; 
 
// Web server running on port 80
WebServer server(80);
 
// JSON data buffer
StaticJsonDocument<250> jsonDocument;
char buffer[250];

// env variable
float decibel;
int maxdecibel = 60;
float maxvibration = 2;
int musique = 2;
bool ledalert = 1;
bool sonalert = 0;
int volume = 5;
int userId;
float oldx, oldy, oldz;
float x;
float y;
float z;
float frequence;
bool depasser;


volatile int interruptCounter = 0;
int numberOfInterrupts = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void connectToWiFi() {
  Serial.print("Connecting to ");
  Serial.println(SSID);
  
  WiFi.begin(SSID, PWD);
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    // we can even make the ESP32 to sleep
  }
 
  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());
}

void getDecibel() {
  create_json("decibel", decibel, "dB");
  server.send(200, "application/json", buffer);
}

void setup_routing() {     
  server.on("/decibel", getDecibel);   
  server.on("/update", HTTP_POST, handlePost);  
  // start server    
  server.begin();    
}

void create_json(char *tag, float value, char *unit) {  
  jsonDocument.clear();  
//  jsonDocument["type"] = tag;
  jsonDocument["value"] = value;
//  jsonDocument["unit"] = unit;
  serializeJson(jsonDocument, buffer);
}
 
void add_json_object(char *tag, float value, char *unit) {
  JsonObject obj = jsonDocument.createNestedObject();
  obj["type"] = tag;
  obj["value"] = value;
  obj["unit"] = unit; 
}

void updateDecibel(){
  int value = analogRead(pinAdc);
  while(value == 0){
    value = analogRead(pinAdc);
  }
  decibel = 20*log10(value);
  if(decibel > maxdecibel){
    sendPick();
  }
}

void sendPick(){
  
}

void setDb(){
  int value = analogRead(pinAdc);
  while(value == 0){
    value = analogRead(pinAdc);
  }
  decibel = 20*log10(value);
}

void setLed(){
  if(ledalert == 0){
    leds.setColorRGB(0, 0, 0, 0);
  }
  else{
    leds.setColorRGB(0, 0, 255, 0);
  }
}

void setVibration(){
  oldx = LIS.getAccelerationX();
  oldy = LIS.getAccelerationY();
  oldz = LIS.getAccelerationZ();
}

void handlePost() {
  if (server.hasArg("plain") == false) {
    //handle error here
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);
  maxdecibel = jsonDocument["max_sound"];
  maxvibration = jsonDocument["max_vibration"];
  musique = jsonDocument["music"];
  sonalert = jsonDocument["sound_alert"];
  ledalert = jsonDocument["color_alert"];
  volume = jsonDocument["sound_control"];;
  userId = jsonDocument["user_id"];
  Serial.println(maxdecibel);
  Serial.println(maxvibration);
  Serial.println(musique);
  Serial.println(sonalert);
  Serial.println(ledalert);
  Serial.println(volume);
  Serial.println(userId);
  musicPlayer.setVolume(volume,volume);
  if(ledalert == 0){
    leds.setColorRGB(0, 0, 0, 0);
  } else{
    leds.setColorRGB(0, 0, 255, 0);
  }
  depasser = false;
  // Respond to the client
  server.send(200, "application/json", "{}");
}

void IRAM_ATTR handleInterrupt() {
  portENTER_CRITICAL_ISR(&mux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&mux);
}

void setup(void) {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPin), handleInterrupt, FALLING);
  LIS.begin(WIRE,0x19);
  delay(100);
  LIS.setFullScaleRange(LIS3DHTR_RANGE_2G);
  LIS.setOutputDataRate(LIS3DHTR_DATARATE_50HZ);
  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }
  musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }
  Serial.println("SD OK!");
  musicPlayer.setVolume(volume,volume);
  connectToWiFi();  
  setDb();
  setVibration();   
  setLed();
  setup_routing();
  depasser = false;
}

void loop(void) {
  if(interruptCounter>0){
      portENTER_CRITICAL(&mux);
      interruptCounter--;
      portEXIT_CRITICAL(&mux);
      musicPlayer.pausePlaying(true);
      Serial.print("An interrupt has occurred. Total: ");
      Serial.println(numberOfInterrupts);
  }
  server.handleClient();  
  decibel = 20*log10(analogRead(pinAdc));
//  Serial.println(decibel);
  x = LIS.getAccelerationX();
  y = LIS.getAccelerationY();
  z = LIS.getAccelerationZ();
  frequence = abs(oldx - x) + abs(oldy - y) + abs(oldz - z);
//  Serial.println(frequence);
  oldx = x;
  oldy = y;
  oldz = z;
  if(depasser || decibel > maxdecibel){
    depasser = true;
    if(ledalert == 1){
      leds.setColorRGB(0, 255, 0, 0);
    } else{
      leds.setColorRGB(0, 0, 0, 0);
    }
    if(sonalert == 1 && musicPlayer.stopped()){
      Serial.println(musique);
      if(musique == 2){
        musicPlayer.playFullFile("/music2.mp3");
      } else{
        if(musique == 3){
          musicPlayer.playFullFile("/music3.mp3");
        } else{
          if(musique == 4){
            musicPlayer.playFullFile("/music4.mp3");
          } else{
            musicPlayer.playFullFile("/music1.mp3");
          }
        }
      }
    }
  }
  else{
    if(ledalert == 1){
      int color = 255 / maxdecibel;
      leds.setColorRGB(0, floor(255 - (color * decibel)), 255, 0);
    } else{
      leds.setColorRGB(0, 0, 0, 0);
    }
  }
}
