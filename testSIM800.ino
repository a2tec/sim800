/* Test sketch for SIM800 library */
/* Place this file in testSIM800 folder */

#include "SIM800.h"
#define SIMCOM800_RESET_PIN 53

SIM800* gsm;

void setup() {
  Serial.begin(9600);
  gsm = new SIM800(Serial1, Serial, SIMCOM800_RESET_PIN, true);
  
  Serial.println(gsm->getOperatorName());
  Serial.println(gsm->getSignalLevel());
  Serial.println(gsm->getBatteryVoltage());
  gsm->setup("telefonica.es", "telefonica", "telefonica");
  //Serial.println(gsm->receiveSMS());
  
  if (gsm->httpGET("http://www.google.com")) {
    Serial.println("GET OK");
    Serial.print(gsm->getResponse());
  }
  else {
    Serial.println("GET KO");
    Serial.print(gsm->getResponse());
  }
  if (gsm->httpPOST("http://www.google.com", "{\"text\":\"POST request from SIMCOM800L\"}")) {
    Serial.println("POST OK, resource created");    
    Serial.print(gsm->getResponse());
  }
  else {
    Serial.println("POST KO, no resource created"); 
    Serial.print(gsm->getResponse()); 
  }
}

void loop() {
    if(Serial.available()) {
    byte ch = Serial.read();
    
    if (ch == 'z') {  
      gsm->sleep();     
    }    
    else if (ch == 'x') {
      gsm->wakeUp();      
    }
    else if (ch == 'q') {      
      gsm->powerOff();
    }    
    else if (ch == 'w') {
      gsm->powerUp();   
    }
    Serial1.write(ch);
  }
  if(Serial1.available()) {    
    Serial.write(Serial1.read());
  }
}
