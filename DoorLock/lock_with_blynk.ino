#include <Blynk.h>

//#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
char ssid[] = "wifi_name";
char pass[] = "wifi_password";

#include<Servo.h>
Servo Lockservo;
#include "EEPROM.h"
byte eepromValid = 35;// If the first byte in eeprom is this then the data is valid.
#define EEPROM_SIZE 64
/*Pin definitions*/
const int programButton = 26;   // Record A New Knock button.
const int led= 25;          // The built in LED
const int knockSensor = 34;     // (Analog 1) for using the piezo as an input device. (aka knock sensor)
const int lock=33;
const int unlockswitch=32;
const int block = 2;
const int bunlock = 4;
/*Tuning constants. Changing the values below changes the behavior of the device.*/
int pos = 35;
int threshold = 3000;                 // Minimum signal from the piezo to register as a knock. Higher = less sensitive. Typical values 1 - 10
const int rejectValue = 25;        // If an individual knock is off by this percentage of a knock we don't unlock. Typical values 10-30
const int averageRejectValue = 15; // If the average timing of all the knocks is off by this percent we don't unlock. Typical values 5-20
const int knockFadeTime = 150;     // Milliseconds we allow a knock to fade before we listen for another one. (Debounce timer.)
const int lockOperateTime = 2500;  // Milliseconds that we operate the lock solenoid latch before releasing it.
const int maximumKnocks = 20;      // Maximum number of knocks to listen for.
const int knockComplete = 1200;    // Longest time to wait for a knock before we assume that it's finished. (milliseconds)

byte secretCode[maximumKnocks] = {50, 25, 25, 50, 100, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};  // Initial setup: "Shave and a Hair Cut, two bits."
int knockReadings[maximumKnocks];    // When someone knocks this array fills with the delays between knocks.
int knockSensorValue = 0;            // Last reading of the knock sensor.
boolean programModeActive = false;   // True if we're trying to program a new knock.

void setup() {
  EEPROM.begin(EEPROM_SIZE);
  pinMode(led, OUTPUT); 
  pinMode(lock, INPUT);
  pinMode(unlockswitch, INPUT);
  pinMode(knockSensor, INPUT);
  pinMode(programButton, INPUT);
  pinMode(block, OUTPUT);
  pinMode(bunlock, OUTPUT);
  Lockservo.attach(13);https://dl.espressif.com/dl/package_esp32_index.json
  doorUnlock(20);     // Unlock the door for a bit when we power up. For system check and to allow a way in if the key is forgotten.
  readSecretKnock();
  WiFi.begin(ssid, pass);
  int wifi_ctr = 0;
  while (WiFi.status() != WL_CONNECTED)
  { delay(500);
    }
  Blynk.begin("given by blynk app", ssid, pass); //auth by blynk app 3435466765438765 like that
   }

void loop() {
    Blynk.run();

    if(digitalRead(block)==HIGH)
  {pos=90;
  Lockservo.attach(13);
    Lockservo.write(pos);
   delay(100);
   Lockservo.detach();
   
    }
   if(digitalRead(bunlock)==HIGH)
  { pos=35;
  Lockservo.attach(13);
    Lockservo.write(pos);
   delay(100);
  Lockservo.detach();
  
   }


  if(digitalRead(lock)==HIGH)
  {pos=90;
  Lockservo.attach(13);
    Lockservo.write(pos);
   delay(100);
   Lockservo.detach();
   
    }
   if(digitalRead(unlockswitch)==HIGH)
  { pos=35;
  Lockservo.attach(13);
    Lockservo.write(pos);
   delay(100);
   Lockservo.detach();
   
    }
  // Listen for any knock at all.
  knockSensorValue = analogRead(knockSensor);
    
      
  if (digitalRead(programButton) == HIGH){  // is the program button pressed?
    delay(100);   // Cheap debounce.
    if (digitalRead(programButton) == HIGH){ 
      if (programModeActive == false){     // If we're not in programming mode, turn it on.
        programModeActive=true;          // Remember we're in programming mode.
        digitalWrite(led, HIGH);        // Turn on the red light too so the user knows we're programming.
        } else {                             // If we are in programing mode, turn it off.
        programModeActive = false;
        digitalWrite(led, LOW);
     
        delay(500);
      }
      while (digitalRead(programButton) == HIGH){
        delay(50);                         // Hang around until the button is released.
      } 
    }
    delay(50);   // Another cheap debounce. Longer because releasing the button can sometimes be sensed as a knock.
  }
  
  
  if (knockSensorValue >= threshold){
     if (programModeActive == true){  // Blink the LED when we sense a knock.
       digitalWrite(led, LOW);
     } else {
       digitalWrite(led, HIGH);
     }
     knockDelay();
     if (programModeActive == true){  // Un-blink the LED.
       digitalWrite(led, HIGH);
     } else {
       digitalWrite(led, LOW);
     }
     listenToSecretKnock();           // We have our first knock. Go and see what other knocks are in store...
  }
 
} 

// Records the timing of knocks.
void listenToSecretKnock(){
  int i = 0;
  // First reset the listening array.
  for (i=0; i < maximumKnocks; i++){
    knockReadings[i] = 0;
  }
  
  int currentKnockNumber = 0;               // Position counter for the array.
  int startTime = millis();                 // Reference for when this knock started.
  int now = millis();   

  do {                                      // Listen for the next knock or wait for it to timeout. 
    knockSensorValue = analogRead(knockSensor);
    if (knockSensorValue >= threshold){                   // Here's another knock. Save the time between knocks.
      now=millis();
      knockReadings[currentKnockNumber] = now - startTime;
      currentKnockNumber ++;                             
      startTime = now;          

       if (programModeActive==true){     // Blink the LED when we sense a knock.
         digitalWrite(led, LOW);
       } else {
         digitalWrite(led, HIGH);
       } 
       knockDelay();
       if (programModeActive == true){  // Un-blink the LED.
         digitalWrite(led, HIGH);
       } else {
         digitalWrite(led, LOW);
       }
    }

    now = millis();
  
    // Stop listening if there are too many knocks or there is too much time between knocks.
  } while ((now-startTime < knockComplete) && (currentKnockNumber < maximumKnocks));
  
  //we've got our knock recorded, lets see if it's valid
  if (programModeActive == false){           // Only do this if we're not recording a new knock.
    if (validateKnock() == true){
      doorUnlock(lockOperateTime); 
    } else {
      // knock is invalid. Blink the LED as a warning to others.
      for (i=0; i < 4; i++){          
        digitalWrite(led, HIGH);
        delay(50);
        digitalWrite(led, LOW);
        delay(50);
      }
    }
  } else { // If we're in programming mode we still validate the lock because it makes some numbers we need, we just don't do anything with the return.
    validateKnock();
  }
}


// Unlocks the door.
void doorUnlock(int delayTime){
  digitalWrite(led, HIGH);
  Lockservo.attach(13);
  pos=35;
  Lockservo.write(pos);
  delay(delayTime);
  digitalWrite(led, LOW);  
  delay(100);   // This delay is here because releasing the latch can cause a vibration that will be sensed as a knock.
  Lockservo.detach();
  
}

// Checks to see if our knock matches the secret.
// Returns true if it's a good knock, false if it's not.
boolean validateKnock(){
  int i = 0;
 
  int currentKnockCount = 0;
  int secretKnockCount = 0;
  int maxKnockInterval = 0;               // We use this later to normalize the times.
  
  for (i=0;i<maximumKnocks;i++){
    if (knockReadings[i] > 0){
      currentKnockCount++;
    }
    if (secretCode[i] > 0){         
      secretKnockCount++;
    }
    
    if (knockReadings[i] > maxKnockInterval){   // Collect normalization data while we're looping.
      maxKnockInterval = knockReadings[i];
    }
  }
  
  // If we're recording a new knock, save the info and get out of here.
  if (programModeActive == true){
      for (i=0; i < maximumKnocks; i++){ // Normalize the time between knocks. (the longest time = 100)
        secretCode[i] = map(knockReadings[i], 0, maxKnockInterval, 0, 100); 
      }
      saveSecretKnock();                // save the result to EEPROM
      programModeActive = false;
      playbackKnock(maxKnockInterval);
      return false;
  }
  
  if (currentKnockCount != secretKnockCount){  // Easiest check first. If the number of knocks is wrong, don't unlock.
    return false;
  }
  
  /*  Now we compare the relative intervals of our knocks, not the absolute time between them.
      (ie: if you do the same pattern slow or fast it should still open the door.)
      This makes it less picky, which while making it less secure can also make it
      less of a pain to use if you're tempo is a little slow or fast. 
  */
  int totaltimeDifferences = 0;
  int timeDiff = 0;
  for (i=0; i < maximumKnocks; i++){    // Normalize the times
    knockReadings[i]= map(knockReadings[i], 0, maxKnockInterval, 0, 100);      
    timeDiff = abs(knockReadings[i] - secretCode[i]);
    if (timeDiff > rejectValue){        // Individual value too far out of whack. No access for this knock!
      return false;
    }
    totaltimeDifferences += timeDiff;
  }
  // It can also fail if the whole thing is too inaccurate.
  if (totaltimeDifferences / secretKnockCount > averageRejectValue){
    return false; 
  }
  
  return true;
}


// reads the secret knock from EEPROM. (if any.)
void readSecretKnock(){
  byte reading;
  int i;
  reading = EEPROM.read(0);
 
  if (reading==eepromValid){    // only read EEPROM if the signature byte is correct.
    for (int i=0; i < maximumKnocks ;i++){
      secretCode[i] =  EEPROM.read(i+1);
      
    }
  }
}


//saves a new pattern too eeprom
void saveSecretKnock(){
  EEPROM.write(0,0);  // clear out the signature. That way we know if we didn't finish the write successfully.
  EEPROM.commit();
 
  for (int i=0;i < maximumKnocks; i++){
    EEPROM.write(i+1,secretCode[i]);
    
    EEPROM.commit();
    
  }
  EEPROM.write(0,eepromValid); // all good. Write the signature so we'll know it's all good.
  
   EEPROM.commit();
}

// Plays back the pattern of the knock in blinks and beeps
void playbackKnock(int maxKnockInterval){
      digitalWrite(led, LOW);
      delay(1000);
      digitalWrite(led, HIGH);
     
      for (int i = 0; i < maximumKnocks ; i++){
        digitalWrite(led, LOW);
        // only turn it on if there's a delay
        if (secretCode[i] > 0){                                   
          delay(map(secretCode[i], 0, 100, 0, maxKnockInterval)); // Expand the time back out to what it was. Roughly. 
          digitalWrite(led, HIGH);
         
        }
      }
      digitalWrite(led, LOW);
}

// Deals with the knock delay thingy.
void knockDelay(){
  int itterations = (knockFadeTime / 20);      // Wait for the peak to dissipate before listening to next one.
  for (int i=0; i < itterations; i++){
    delay(10);
    analogRead(knockSensor);                  // This is done in an attempt to defuse the analog sensor's capacitor that will give false readings on high impedance sensors.
    delay(10);
  } 
}
