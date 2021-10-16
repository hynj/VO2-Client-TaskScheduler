#include <Arduino.h>
#include <wire.h>
#include <SPI.h>

#include <TaskScheduler.h>

#include <HardwareSerial.h>
#include "Adafruit_BME280.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID1 "b0e7c0af-a557-4f13-84df-9b7b713ee81d"
#define CHARACTERISTIC_UUID2 "02738e9d-282b-4ba3-b951-46b14c37ab4b"

boolean newData = false;
const byte numChars = 44;
char receivedChars[numChars];   // an array to store the received data

char *token;
int toknum = 0;
float temp;
float pressure;
float oxygen;
const char s[2] = " ";

char *token1;
int toknum1 = 0;
float co2;
float hum;


const unsigned int MAX_INPUT = 16;

HardwareSerial MySerial1(1);
HardwareSerial MySerial2(2);

Adafruit_BME280 bme;

void readO2Callback();
void readCO2Callback();
void readHMECallback();
void sendBLECallback();
void sendSensorData();
void flowCallback();
void flowAdder();
void sendPressureTemp();

//Tasks
Task t1(10, TASK_FOREVER, &readO2Callback);
Task t2(10, TASK_FOREVER, &readCO2Callback);
Task t3(2000, TASK_FOREVER, &readHMECallback);
Task t4(5000, TASK_FOREVER, &sendSensorData);
Task t5(301, TASK_FOREVER, &sendBLECallback);
Task t6(1, TASK_FOREVER, &flowCallback);
Task t7(100, TASK_FOREVER, &flowAdder);
Task t8(2500, TASK_FOREVER, &sendPressureTemp);

Scheduler runner;

float Flow;
float flowTotal;
float flowTime;
float flow100;
int flowCount = 0;

float flowArray[3] = {0, 0, 0};
int flowAdderCounter = 0;

int counts;
const int ledPin = 34;
int offset = 32000; // Offset for the sensor
float scale = 140.0; // Scale factor for Air and N2 is 140.0, O2 is 142.8

//Oxygen
#define RXD2 4
#define TXD2 2

// CO2
#define RXD1 16
#define TXD1 17

#define SDAPIN 21
#define SCLPIN 22

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLECharacteristic* pCharacteristic1 = NULL;
BLECharacteristic* pCharacteristic2 = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
//uint32_t value = 0;
uint8_t value[20];
uint8_t presTempVal[5];


float get_flow();

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0)
    {
      Serial.println("*********");
      Serial.print("New value: ");
      for (int i = 0; i < value.length(); i++)
      {
        Serial.print(value[i]);
        if (value[i] == 12){
            MySerial1.println("G\r");
            Serial.print("Cal Done");
        }
      }

      Serial.println();
      Serial.println("*********");
    }
  }
};

void setupBLE()
{
  // Create the BLE Device
  BLEDevice::init("VO2 Sensors");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
    // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic1 = pService->createCharacteristic(
                      CHARACTERISTIC_UUID1,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic2 = pService->createCharacteristic(
                      CHARACTERISTIC_UUID2,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );


  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());
  
  pCharacteristic1->addDescriptor(new BLE2902());
  
  pCharacteristic2->addDescriptor(new BLE2902());

  pCharacteristic1->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();


  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}
void process_data (char * data)
  {
      token1 = strtok(data, s);

      while( token1 != NULL ) {
          toknum1++;
          if (toknum1 == 2){
              co2 = atof(token1);
          }
          token1 = strtok(NULL, s);
      }
toknum1 = 0;
  // for now just display it
  // (but you could compare it to some value, convert to an integer, etc.)

  }  // end of process_data

void processIncomingByte (const byte inByte)
  {
  static char input_line [MAX_INPUT];
  static unsigned int input_pos = 0;

  switch (inByte)
    {
    case '\n':   // end of text
      input_line [input_pos] = 0;  // terminating null byte
      // terminator reached! process input_line here ...
      process_data (input_line);

      // reset buffer for next time
      input_pos = 0;

      break;

    case '\r':   // discard carriage return
      break;

    default:
      // keep adding if not full ... allow for terminating null byte
      if (input_pos < (MAX_INPUT - 1))
        input_line [input_pos++] = inByte;
      break;

    }  // end of switch

} // end of processIncomingByte

void printSensors();
void readCO2Callback(){
  while (MySerial1.available()) {
      processIncomingByte (MySerial1.read ());
  }
};
void readHMECallback(){
   hum = bme.readHumidity();
    //Flow = get_flow();
    //Serial.println(Flow);
   //Serial.println(hum);
};
void flowCallback(){
    flowTotal += get_flow();
};
void flowAdder(){
  flowCount = millis() - flowTime;
  flow100 = flowTotal / flowCount;
  if (flowAdderCounter < 3){
    flowArray[flowAdderCounter] = flow100;
    flowAdderCounter++;
  }
  else
  {
    Serial.println("Flow Array overfill");
  }
  
  flowTime= millis(); 
  flowTotal = 0;
};

void readO2Callback(){
    static byte ndx = 0;
    char endMarker = '\n';
    char rc;
    //Serial.println("o2 callback");

    while (MySerial2.available() > 0 && newData == false) {
        rc = MySerial2.read();
        //Serial.println("new flase");

        if (rc != endMarker) {
            receivedChars[ndx] = rc;
            ndx++;
            if (ndx >= numChars) {
                ndx = numChars - 1;
            }
        }
        else {
            receivedChars[ndx] = '\0'; // terminate the string
            ndx = 0;
            newData = true;
        }
    }
    if (newData == true) {
      token = strtok(receivedChars, s);

      /* walk through other tokens */
      while( token != NULL ) {
          //printf( " %s %i \n", token, toknum );
          toknum++;

              if (toknum == 4){
                  temp = atof(token);

              }else if (toknum == 6){
                  pressure = atof(token);
              }
              else if (toknum == 8){
                  oxygen = atof(token);
              }

          token = strtok(NULL, s);
      }
      toknum = 0;


      newData = false;
  }
}
void sendSensorData(){
  printSensors();
  //Serial.println(flowCount);
  //Serial.println(flow100);
};
void sendPressureTemp() {
  int pb = (int)pressure;

  //Set up Pressure int16
  presTempVal[0] = (pb >> 8) & 0xFF;
  presTempVal[1] = pb & 0xFF;

  //Set up Temp int16
  presTempVal[2] = (byte)temp;

  //Set up hum
  presTempVal[3] = (byte)hum;

  presTempVal[4] = 0;

  if (deviceConnected) {
        pCharacteristic2->setValue((uint8_t*)&presTempVal, 5);
        pCharacteristic2->notify();
        //value++; // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
    }
};
void sendBLECallback(){
    byte *ob = (byte *)&oxygen;
    byte *cb = (byte *)&co2;
     
    byte *fb = (byte *)&flowArray[0];
    byte *fb1 = (byte *)&flowArray[1];
    byte *fb2 = (byte *)&flowArray[2];

    for (int i = 0; i < 3; i++) {
      flowArray[i] = 0;
    }
    flowAdderCounter = 0;

    //Serial.println(flowArray[0]);
    //Serial.println(flowArray[1]);


    //Set up oxygen Float
    value[0] = ob[0];
    value[1] = ob[1];
    value[2] = ob[2];
    value[3] = ob[3];
    //value[0] = 0;
    //value[1] = 1;
    //value[2] = 2;
    //value[3] = 3;

    //Set up CO2 Float
    value[4] = cb[0];
    value[5] = cb[1];
    value[6] = cb[2];
    value[7] = cb[3];

    //Set up Pressure int16
    //value[8] = (pb >> 8) & 0xFF;
    //value[9] = pb & 0xFF;

    //Set up Temp int16
    //value[10] = (byte)temp;

    //Set up hum
    //value[11] = (byte)hum;

    value[8] = fb[0];
    value[9] = fb[1];
    value[10] = fb[2];
    value[11] = fb[3];


    value[12] = fb1[0];
    value[13] = fb1[1];
    value[14] = fb1[2];
    value[15] = fb1[3];

    //Set up second flow
    value[16] = fb2[0];
    value[17] = fb2[1];
    value[18] = fb2[2];
    value[19] = fb2[3];


   if (deviceConnected) {
        pCharacteristic->setValue((uint8_t*)&value, 20);
        pCharacteristic->notify();
        //value++; // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
    }
}
void printSensors(){
      // Convert the value to a char array
    char tempString[8];
    dtostrf(temp, 1, 2, tempString);

    char pressureString[8];
    dtostrf(pressure, 1, 2, pressureString);

    char oxygenString[8];
    dtostrf(oxygen, 1, 2, oxygenString);

    char co2String[8];
    dtostrf(co2, 1, 2, co2String);

    //char flowString[8];
    //dtostrf((flowTotal/(counts*60)), 1, 2, flowString);

    char humString[8];
    dtostrf(hum, 1, 2, humString);

    //Serial.print("Humidity = ");
    //Serial.print(bme.readHumidity());
    //Serial.println(" %");

    char buffer [100];
    snprintf(buffer, 100, "T:%s P:%s O:%s C:%s H:%s", tempString, pressureString, oxygenString, co2String, humString);
    Serial.println(buffer);
};
void setupHME(){
  bool status;

  status = bme.begin();
    if (!status) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
    }
};

void setup_SFM(){
    Wire.setClock(1000000);
    Wire.begin();
    int a = 0;
    int b = 0;
    int c = 0;

    delay(1000);

    Wire.beginTransmission(byte(0x40)); // transmit to device #064 (0x40)
    Wire.write(byte(0x10));      //
    Wire.write(byte(0x00));      //
    Wire.endTransmission();

    delay(5);

    Wire.requestFrom(0x40, 3); //
    a = Wire.read(); // first received byte stored here
    b = Wire.read(); // second received byte stored here
    c = Wire.read(); // third received byte stored here
    Wire.endTransmission();
    //Serial.print(a);
    //Serial.print(b);
    //Serial.println(c);

    delay(5);

    Wire.requestFrom(0x40, 3); //
    a = Wire.read(); // first received byte stored here
    b = Wire.read(); // second received byte stored here
    c = Wire.read(); // third received byte stored here
    Wire.endTransmission();
    //Serial.print(a);
    //Serial.print(b);
    //Serial.println(c);

    delay(5);
}
uint8_t crc8(const uint8_t data, uint8_t crc) {
  crc ^= data;

  for ( uint8_t i = 8; i; --i ) {
    crc = ( crc & 0x80 )
      ? (crc << 1) ^ 0x31
      : (crc << 1);
  }
  return crc;
}
float get_flow(){
  Wire.requestFrom(0x40, 3); // read 3 bytes from device with address 0x40
  uint16_t a = Wire.read(); // first received byte stored here. The variable "uint16_t" can hold 2 bytes, this will be relevant later
  uint8_t b = Wire.read(); // second received byte stored here
  uint8_t crc = Wire.read(); // crc value stored here
  uint8_t mycrc = 0xFF; // initialize crc variable
  mycrc = crc8(a, mycrc); // let first byte through CRC calculation
  mycrc = crc8(b, mycrc); // and the second byte too
  if (mycrc != crc) { // check if the calculated and the received CRC byte matches
    //Serial.println("Error: wrong CRC");
  }
  a = (a << 8) | b; // combine the two received bytes to a 16bit integer value
  // a >>= 2; // remove the two least significant bits
  return ((float)a - offset) / scale;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Wire.begin();
  MySerial1.begin(9600, SERIAL_8N1, RXD1, TXD1); // CO2

  MySerial2.begin(9600, SERIAL_8N1, RXD2, TXD2); // Oxygen

  Serial.println("Serial Txd is on pin: "+String(TX));
  Serial.println("Serial Rxd is on pin: "+String(RX));

  runner.init();
  runner.addTask(t1);
  runner.addTask(t2);
  runner.addTask(t3);
  runner.addTask(t4);
  runner.addTask(t5);
  runner.addTask(t6);
  runner.addTask(t7);
  runner.addTask(t8);

  t1.enable();
  t2.enable();
  t3.enable();
  t4.enable();

  delay(1000);

  setupBLE();
  setup_SFM();
  setupHME();

  t5.enable();
  t6.enable();
  t7.enable();
  t8.enable();
}

void loop() {
  // put your main code here, to run repeatedly:
  runner.execute();
}