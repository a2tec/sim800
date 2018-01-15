#include "SIM800.h"

SIM800::SIM800(HardwareSerial& serial, int resetPin) : serial(serial), resetPin(resetPin) {
    if (resetPin != 0) {
      pinMode(resetPin, OUTPUT);
      digitalWrite(resetPin, HIGH);      
      delay(100);
    }
    serial.begin(SIM800_BAUDRATE);
    debug = false;
}

SIM800::SIM800(HardwareSerial& serial, HardwareSerial& serialDebug, int resetPin, bool verboseError) : serial(serial), resetPin(resetPin), serialDebug(serialDebug) {
    if (resetPin != 0) {
      pinMode(resetPin, OUTPUT);
      digitalWrite(resetPin, HIGH);      
      delay(100);
    }
    serial.begin(SIM800_BAUDRATE);
    serialDebug.begin(SIM800_BAUDRATE);
    debug = true;
    if (verboseError) {
      // Enable detailed errors messages in AT Command responses
      sendATCommand("AT+CMEE=2"); 
    }
}

void SIM800::setup(const char* apn, const char* user, const char* pwd) {
    // Configure GPRS connection (create bearer profile)
    sendATCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");

    prepareCommand("AT+SAPBR=3,1,\"APN\",\"", apn, "\"");
    sendATCommand(cmd_buff);
    if (user != 0) {
        prepareCommand("AT+SAPBR=3,1,\"USER\",\"", user, "\"");
        sendATCommand(cmd_buff);
    }
    if (pwd != 0) {
        prepareCommand("AT+SAPBR=3,1,\"PWD\",\"", pwd, "\"");
        sendATCommand(cmd_buff);
    }
    // Start GPRS connection  
    sendATCommand("AT+SAPBR=1,1", 1000);
    // Initialize HTTP Service
    sendATCommand("AT+HTTPINIT");
    // Set HTTP bearer profile
    sendATCommand("AT+HTTPPARA=\"CID\",1");
}

void SIM800::close() {
    // Terminate HTTP Service
    sendATCommand("AT+HTTPTERM");
    // Close GPRS connection  
    sendATCommand("AT+SAPBR=0,1");
}

void SIM800::sleep() {
    // Set module to minimum functionality (RF and SIM disabled)
    sendATCommand("AT+CFUN=0", 3000);    
    // Enable auto sleep mode (serial interface is disabled when no data coming)    
    sendATCommand("AT+CSCLK=2", 3000);
}

void SIM800::wakeUp() {
    // Send anything to wake up
    serial.print("aaaaa");
    // Waits for the serial interface to be ready
    delay(200);
    // Disable slow clock mode
    sendATCommand("AT+CSCLK=0", 3000);
    // Set module to full funtionality
    sendATCommand("AT+CFUN=1", 3000);
}

void SIM800::powerOff() {
  sendATCommand("AT+CPOWD=1", 3000, "NORMAL POWER DOWN");
  if (resetPin != 0) {
    digitalWrite(resetPin, LOW);
  }   
}

void SIM800::powerUp() {
  if (resetPin != 0) {
    if (debug) { 
      serialDebug.println("Powering up...");
    }
    // Trigger reset
    digitalWrite(resetPin, HIGH);    
    // Send some commands to wait for the module to be ready
    while(sendATCommand("AT") == false) delay(1000);
    while(sendATCommand("AT+CFUN?", "+CFUN: 1") == false) delay(2500); 
    while(getSignalLevel() == 0) delay(2500);          
    while(getOperatorName() == 0) delay(2500);
    if (debug) { 
      serialDebug.println("Module Ready");
    }    
  }   
}

const char* SIM800::getOperatorName() {
    sendATCommand("AT+COPS?");
    // Parse response
    char* p = strchr(resp_buff, '"');
    if (p == 0) {
      return 0;
    }
    char aux[20] = {0};
    for(int i=0; p[i+1]!= '"'; i++) {
      aux[i]=p[i+1];
    }
    // Copy parsed response to response array
    cleanResponseBuffer();
    strcpy(resp_buff, aux);
    return resp_buff;
}

int SIM800::getSignalLevel() {
    sendATCommand("AT+CSQ");
    // Parse response
    char* p = strchr(resp_buff, ':');
    char aux[3] = {0};
    for(int i=0; p[i+2]!= ','; i++) {
      aux[i]=p[i+2];
    }
    return atoi(aux);
}

int SIM800::getBatteryVoltage() {
    sendATCommand("AT+CBC");    
    // Parse response
    char* p = strrchr(resp_buff, ',');
    char aux[5] = {0};
    for(int i=0; p[i+1]!= '\r'; i++) {
      aux[i]=p[i+1];
    }
    return atoi(aux);
}

void SIM800::getGSMLocAndTime() {     
    sendATCommand("AT+CIPGSMLOC=1,1");    
}

void SIM800::sendSMS(const char* number, const char* text) {
    // Set SMS to text mode
    sendATCommand("AT+CMGF=1");
    // Send AT Command for new SMS
    serial.write("AT+CMGS=\"");
    serial.write(number);
    serial.println("\"");
    // Wait for prompt to input sms text
    for (int c = serial.read(); c != ' '; c = serial.read()) {
        if (c != -1 && debug) {
            serialDebug.print((char) c);
        }
    }
    //Send SMS content (append Cntrl+Z to text)
    prepareCommand(text, (char) 26);
    sendATCommand(cmd_buff, 5000);
}

const char* SIM800::receiveSMS() {
  // Set SMS to text mode
  sendATCommand("AT+CMGF=1");
  cleanResponseBuffer();
  int n = 0;

  // Check for +CMTI: "ME",number
  while (1) {
    if (serial.available()) {
      resp_buff[n++] = serial.read();
      if (resp_buff[n - 1] == '\n') {
        if (strstr(resp_buff, "+CMTI:")) {
          // Parse sms index: +CMTI: "ME",123\r\n
          char* p = strchr(resp_buff, ',');
          char auxIndex[5] = {0};
          for(int i=0; p[i+1] != '\r' || p[i+1] != '\n'; i++) {
            auxIndex[i]=p[i+1];
          }

          // Read sms in auxIndex
          prepareCommand("AT+CMGR=", auxIndex); 
          sendATCommand(cmd_buff); 
          
          // Parse text from response   
          return resp_buff;
        } 
        else {
          n = 0;                    
          cleanResponseBuffer();
        }
      }
    }
  }
}

bool SIM800::httpGET(const char* url) {
    // Set HTTP url
    prepareCommand("AT+HTTPPARA=\"URL\",\"", url, "\"");
    sendATCommand(cmd_buff);
    
    // Perform GET request
    if (!sendATCommand("AT+HTTPACTION=0", 100000, "+HTTPACTION:")) {
      return false; //AT command result in Error or timeout 
    }    
    // Parse HTTP response code
    char* p = strchr(resp_buff, ',');
    if (p == 0 || p[1]!='2') {
      return false; // HTTP response code is not 2xx
    }
    
    // Read HTTP Response Data
    sendATCommand("AT+HTTPREAD");
    return true;
}

bool SIM800::httpPOST(const char* url, const char* payload, const char* contenttype) {
    // Set HTTP url
    prepareCommand("AT+HTTPPARA=\"URL\",\"", url, "\"");
    sendATCommand(cmd_buff);
    // Set HTTP content type
    prepareCommand("AT+HTTPPARA=\"CONTENT\",\"", contenttype, "\"");
    sendATCommand(cmd_buff);
    
    // Set length of data for HTTP POST. Send data to simcom next
    char length[5] = {0};
    itoa(strlen(payload), length, 10);
    prepareCommand("AT+HTTPDATA=", length, ",1000");
    if (sendATCommand(cmd_buff, 1500, "DOWNLOAD")) {
        serial.println(payload);
        delay(1000);
    }
    else {
      return false;
    }

    // Perform POST request
    if (!sendATCommand("AT+HTTPACTION=1", 100000, "+HTTPACTION:")){
      return false; //AT command result in Error or timeout 
    } 
    // Parse HTTP response code
    char* p = strchr(resp_buff, ',');
    if (p == 0 || p[1]!='2') {
      return false; // HTTP response code is not 2xx
    }  
    
    // Read HTTP Response Data
    sendATCommand("AT+HTTPREAD");
    return true;
}

bool SIM800::sendATCommand(const char* command, int timeout, const char* expected, const char* error) {   
    //Clean response buffer
    cleanResponseBuffer();

    // Purge any incoming data from the module 
    purgeSerial();

    // Send command, appending \r\n
    serial.println(command);

    // Receive data until timeout, error or expected is received  
    uint32_t t = millis();
    uint8_t n = 0;
    while (millis() - t < timeout) {
        if (serial.available()) {
            resp_buff[n++] = serial.read();
            if (resp_buff[n - 1] == '\n') {
                if (strstr(resp_buff, expected)) {
                    if (debug) {
                      serialDebug.print(resp_buff);
                    }
                    return true;
                } else if (strstr(resp_buff, error)) {
                    if (debug) {
                      serialDebug.print(resp_buff);
                    }
                    return false;
                }
            }
        }
    }
    if (debug) {
      serialDebug.print("TIMEOUT:");
      serialDebug.println(command);
      serialDebug.println(resp_buff);
    }
    return false;
}

const char* SIM800::getResponse() {
    return resp_buff;
}

void SIM800::prepareCommand(const char* c1, const char* c2, const char* c3) {
    // Clean command buffer
    for (int i = 0; i < CMD_BUFF_SIZE; cmd_buff[i++] = 0);
    strcpy(cmd_buff, c1);
    strcat(cmd_buff, c2);
    strcat(cmd_buff, c3);
}

void SIM800::purgeSerial() {
    while (serial.available()) {
        serial.read();
    }
}

void SIM800::cleanCommandBuffer() {
    for (int i = 0; i < CMD_BUFF_SIZE; cmd_buff[i++] = 0);
}

void SIM800::cleanResponseBuffer() {
    for (int i = 0; i < RESP_BUFF_SIZE; resp_buff[i++] = 0);
}


