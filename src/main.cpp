#include <Arduino.h>
#include <WiFi.h>
#include <BluetoothSerial.h>
#include <neotimer.h>

//TODO: Check if it is necessary
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif



#define SERIAL_BAUDS    115200

#define END_CHAR    0
#define SPACE   32
#define DEL 127 
#define BLANK_SPACE 255

#define INIT_SEQ_NULL   4
#define DATA_NULL_NUMBER    1   

enum {NUL, CONTROL_CHAR, PRINTABLE_CHAR};
enum {START_SEQUENCE, SSID_INCOMING, PASS_INCOMING, END_TRANSMISSION};

#define STRING_SIZE 40 

#define SECOND  1000
#define HALF_SECOND int(SECOND/2)

#define TIMEOUT_REP 30 


char ssid[STRING_SIZE] = "";
char pass[STRING_SIZE] = "";

BluetoothSerial ESP32_BT;

void initSerialTerm(void);

bool receiveBTstring(void);
byte checkCharType(byte);
void checkRxData(byte);
//void printCurrentState(byte);
//void printAvailaibility(BluetoothSerial);
void printUserMessages(byte);
void printLocalIPaddress(void);
void printWiFistatus(int);

void setupBTdevice(void);
bool pairBTDevice(void);

bool connectToWiFiNetwork(void);
bool checkSSID(void);


Neotimer longTimer = Neotimer(SECOND);
Neotimer shortTimer = Neotimer(HALF_SECOND);
Neotimer oneTimer = Neotimer(TIMEOUT_REP * SECOND);


void setup() {
  // put your setup code here, to run once:
  initSerialTerm();

  setupBTdevice();

  if(pairBTDevice()) {
      if(receiveBTstring()) {
          if(connectToWiFiNetwork()) {
              printLocalIPaddress();
          }
      }
  } 
}

void loop() {
  // put your main code here, to run repeatedly:
}

void initSerialTerm(void) {
    Serial.begin(SERIAL_BAUDS);
}

void setupBTdevice(void) {
    ESP32_BT.begin("ESP32_BT_RECEIVER");
    Serial.println("Waiting for pairing device");
}

bool pairBTDevice(void) {
    //longTimer.start();
    bool pairSuccess = false;
    while(!ESP32_BT.available()) {
        if (longTimer.repeat(TIMEOUT_REP)) {
            Serial.print(".");  
            if (!longTimer.repetitions) {
                Serial.println("Incoming connection Time Out");
                pairSuccess = false;
                break;
            }
        }
        //delay(1000);
    }
    if (ESP32_BT.available()) {
        Serial.println(" ");
        Serial.println("Pairing done.");
        Serial.println(" ");
        pairSuccess = true;
    }
    return pairSuccess;
}


bool receiveBTstring(void) {
    bool strRXsuccess = false;

    byte incChar = 0;
    byte charType;

    byte ind = 0;
    byte nullCounter = 0;

    byte transmissionState = START_SEQUENCE;

    oneTimer.start();

    while(transmissionState != END_TRANSMISSION && oneTimer.waiting()) {
        if (ESP32_BT.available()) {
            incChar = ESP32_BT.read();
            charType = checkCharType(incChar);

            switch (charType) {
                case NUL:
                    nullCounter += 1;

                    if (transmissionState == START_SEQUENCE && nullCounter == INIT_SEQ_NULL) {
                        transmissionState = SSID_INCOMING;
                        nullCounter = 0;
                        printUserMessages(transmissionState);
                    }      

                    if (transmissionState == SSID_INCOMING && nullCounter == DATA_NULL_NUMBER) {
                        transmissionState = PASS_INCOMING;
                        nullCounter = 0;
                        ssid[ind] = '\0';
                        ind = 0;
                        checkRxData(transmissionState);
                        printUserMessages(transmissionState);
                    }

                    if (transmissionState == PASS_INCOMING && nullCounter == DATA_NULL_NUMBER) {
                        transmissionState = END_TRANSMISSION;
                        nullCounter = 0;    // Capaz sea innecesario 
                        pass[ind] = '\0';
                        ind = 0;
                        checkRxData(transmissionState);
                        printUserMessages(transmissionState);
                    }
                            
                break;
                
                case PRINTABLE_CHAR:
                    if (transmissionState == SSID_INCOMING)
                        ssid[ind] = (char) incChar;
                    else if (transmissionState == PASS_INCOMING)
                        pass[ind] = (char) incChar;
                    ind += 1;
                break;

                case CONTROL_CHAR:
                break;
            }
        }
    }
    if (transmissionState == END_TRANSMISSION) {
        strRXsuccess = true;
    }
    else if (oneTimer.done()) {
        strRXsuccess = false;
        Serial.println("Bluetooth connection timeout.");
    }
    //ESP32_BT.end();
    return strRXsuccess;
}

byte checkCharType(byte incoming) {
    byte charType;
    if (incoming == END_CHAR)
        charType = NUL;
    else if (incoming >= SPACE && incoming != BLANK_SPACE && incoming != DEL)
        charType = PRINTABLE_CHAR;
    else
        charType = CONTROL_CHAR;
    return charType;
}

void printUserMessages(byte txState){
    switch (txState)
    {
    case SSID_INCOMING:
        Serial.println("Bluetooth transmission has started.");
        Serial.println(" ");
        Serial.println("Please, write your network SSID.");
    break;
    
    case PASS_INCOMING:
        Serial.print("Your network name is ");
        Serial.println(ssid);
        Serial.println(" ");
        
        Serial.println("Please, write your network password");
    break;

    case END_TRANSMISSION:
        Serial.print("Your network pass is ");
        Serial.println(pass);
        Serial.println(" ");
        Serial.println("End of Transmission.");
    break;
    }
}

bool connectToWiFiNetwork(void) {
    bool correctWiFiconnection = false;
    //const char timeOutMsg[] = "WiFi connection Time Out"
    
    if (checkSSID()) {
        WiFi.begin(ssid, pass);
        Serial.println("Connecting to the WiFi network...");
        while(WiFi.status() != WL_CONNECTED) {
            delay(500);
            if (shortTimer.repeat(2 * TIMEOUT_REP)) {
                Serial.print(".");
                if(!shortTimer.repetitions) {
                    Serial.println(" ");
                    Serial.println("WiFi connection Time Out");
                    correctWiFiconnection = false;
                    break; 
                }
            }
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Connected to the WiFi network");
            correctWiFiconnection = true;
        }
        /*else {
            printWiFistatus(WiFi.status());
        }*/
    }
    else {
        correctWiFiconnection = false;  
        Serial.println("Received SSID doesn't match with any scanned network");     
    }
    return correctWiFiconnection;
}

bool checkSSID(void) {
    bool validSSID = false;
    //WiFi.mode("WIFI_STA");
    int n = WiFi.scanNetworks();

    Serial.print("Found networks: ");
    Serial.println(n);

    if (!n) {
        Serial.println("No Networks found");
        validSSID = false;
    }
    else {
        for (int i = 0; i < n; i++) {
            Serial.print(i);
            Serial.print(")");
            Serial.println(WiFi.SSID(i));
            if ((WiFi.SSID(i)).equals(ssid)) {
                validSSID = true;
                break;
            }
        }
//        Serial.println("Received SSID doesn't match with any scanned network");
    }
    return validSSID;
}

// TODO: Check write without arguments 
// https://github.com/espressif/arduino-esp32/blob/master/libraries/BluetoothSerial/examples/SerialToSerialBT/SerialToSerialBT.ino
void checkRxData(byte txState) {
    String okMsg = "OK";
    String errorMsg = "NOK";
     
    switch (txState) {
      case PASS_INCOMING:
        /*// Empty array
        if (ssid[0] == '\0') {
          Serial.println("Invalid ssid");
          ESP32_BT.println(errorMsg);
          //
        } else {
          Serial.println("SSID received OK");
          ESP32_BT.println(okMsg);
        }*/
        ESP32_BT.println(String(ssid));
      break;
      
      case END_TRANSMISSION:
        /*if (pass[0] == '\0') {
          Serial.println("Invalid pass");
          ESP32_BT.println(errorMsg);
          //
        } else {
          Serial.println("Password received OK");
          ESP32_BT.println(okMsg);
        }*/
        ESP32_BT.println(String(pass));
      break;
    }
}

void printLocalIPaddress(void) {
    Serial.print("Local IP Address: ");
    Serial.println(WiFi.localIP());
}


// Debugging Functions

/*void printCurrentState(byte state) {
    switch (state)
    {
    case START_SEQUENCE:
        Serial.println("Start Sequence");
    break;

    case WAITING_DATA:
        Serial.println("Waiting Data");
    break;    
    
    case DATA_RECEIVED:
        Serial.println("Data Received");    
    break;
    }
}

void printAvailaibility(BluetoothSerial BTobj) {
    if (BTobj.available())
        Serial.println("Dispositivo conectado");
    else if (!BTobj.available())
        Serial.println("Dispositivo desconectado");
}*/

/*void printWiFistatus(int WiFistatus) {
    switch (WiFistatus)
    {
    case WL_NO_SHIELD:
        Serial.println("No shield");
        break;

    case WL_IDLE_STATUS:
        Serial.println("Idle status");
        break;

    case WL_NO_SSID_AVAIL:
        Serial.println("No SSID available");
        break;

    case WL_SCAN_COMPLETED:
        Serial.println("Scan completed");
        break;

    case WL_CONNECT_FAILED:
        Serial.println("Connection failed");
        break;

    case WL_CONNECTION_LOST:
        Serial.println("Connection lost");
        break;                    
    
    case WL_DISCONNECTED:
        Serial.println("Disconnected");
        break; 


    default:
        Serial.print("Error code number ");
        Serial.println(WiFistatus);
        break;
    }
}*/