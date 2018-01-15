#ifndef SIM800_H
#define SIM800_H

#include <Arduino.h>

#define CMD_BUFF_SIZE  100
#define RESP_BUFF_SIZE  500
#define SIM800_BAUDRATE 9600
#define DEBUG_BAUDRATE 9600

class SIM800 {    
public:
    // Class constructors for regular use and debug mode
    SIM800(HardwareSerial& serial, int resetPin = 0);
    SIM800(HardwareSerial& serial, HardwareSerial& serialDebug, int resetPin = 0, bool verboseError = false);

    // Setup and close network connection
    void setup(const char* apn, const char* user = 0, const char* pwd = 0);
    void close();

    // Disable RF and Serial interface for minimum consumption. Call wakeUp before using any other function
    void sleep();
    void wakeUp();
    
    // Switch off module completely. Call powerUp before using any other function (it needs RESET pin))
    void powerOff();
    void powerUp();
    
    // Get network operator name
    const char* getOperatorName();

    // Get signal quality level (0-31: >9 OK, >14:Good, >19 Excellent)
    int getSignalLevel();
    
    // Get battery voltage in millivolts
    int getBatteryVoltage();
    
    // Get GSM Location and time
    void getGSMLocAndTime();
    
    // Send SMS to number
    void sendSMS(const char* number, const char* text);

    // Wait for sms
    const char* receiveSMS();
    
    // Perform http GET request to specified url
    bool httpGET(const char* url);
    
    // Perform http POST request with specified content type and payload
    bool httpPOST(const char* url, const char* payload, const char* contenttype = "application/json");
    
    // Send AT command and check for expected response
    bool sendATCommand(const char* command, int timeout = 500, const char* expected = "OK\r\n", const char* error = "ERROR");

    // Get response buffer
    const char* getResponse();
private:
    void prepareCommand(const char* c1, const char* c2, const char* c3 = 0);
    void purgeSerial();
    void cleanCommandBuffer();
    void cleanResponseBuffer();
    
    HardwareSerial& serial;
    HardwareSerial& serialDebug;
    int resetPin;
    bool debug;
    char cmd_buff[CMD_BUFF_SIZE];
    char resp_buff[RESP_BUFF_SIZE];
};

#endif /* SIM800_H */

