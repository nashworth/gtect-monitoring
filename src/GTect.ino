#include "math.h"         // include math library for sqrt function
#include "High_Temp.h"    // include library for thermocouple
#include <Wire.h>         // include i2c library
#define GAccI2C 0x53      // Grove ADXL345 I2C address is 0x53(83)
#define MuxI2C 0x73       // Digital Mux I2C Address
#define GScaling 0.003901 // 3.9mG per bit A2D Scaling
#define A2DRES 8192       // 2^13 = 8192
#define HALFSCALE 4095    // (2^13)/2 - 1
#define OneMin 60000
#define TenSec 10000
#define FiveMin 300000
#define TenMin 600000
#define FifteenMin 900000

int x0data[32], x1data[32], y0data[32], y1data[32], z0data[32], z1data[32];
int xdata[32], ydata[32], zdata[32];
unsigned long DataInterval;
double Adata[8];
float RoomTemp, Thermocouple;
String event;

int configInterval(String interval);
HighTemp ht1(A1, A0);
HighTemp ht2(A5, A4);

void setup()
{
  Particle.variable("Firmware","GTect.ino");  // define particle variable
  Particle.function("DataInterval", configInterval); // define particle function
  Wire.begin();                     // Initialise I2C communication as MASTER
  Serial.begin(9600);              // set baud rate
  delay(100);                       // wait 100mS
  Wire.beginTransmission(MuxI2C);   // configure the mux
  Wire.write(0x00);                 // reset the selection (all channels off)
  Wire.endTransmission();

  int addr = 10;                // define structure for EEPROM data
  struct nvmobject {
    uint16_t verifyEE;
    unsigned long interval;
  };
  nvmobject nvmobj;
  EEPROM.get(addr, nvmobj);       // read EEPROM data
  if (nvmobj.verifyEE == 0xaa55)  // if the verification value is true
  {
    DataInterval = nvmobj.interval; // retrieve interval value from EEPROM
  }
  else { DataInterval = OneMin;}  // otherwise, set interval to 1 min
}

void loop()
{
  RoomTemp = ht1.getRoomTmp();      // read and calculate room temp 1
  Thermocouple = ht1.getThmc();     // read and calculate thermocouple temp 1
  SwitchMux(0x01);                  // switch to i2c mux input 1
  ReadAccelerometer(GAccI2C);       // read the accelerometer
  event = "Pump1Acc1";
  PublishData();                    // publish data to Particle cloud
  SwitchMux(0x02);                  // switch to i2c mux input 2
  ReadAccelerometer(GAccI2C);       // read the accelerometer
  event = "Pump1Acc2";
  PublishData();                    // publish data to Particle cloud
  RoomTemp = ht2.getRoomTmp();      // read and calculate room temp 2
  Thermocouple = ht2.getThmc();     // read and calculate thermocouple temp 2
  SwitchMux(0x04);                  // switch to i2c mux input 1
  ReadAccelerometer(GAccI2C);       // read the accelerometer
  event = "Pump2Acc3";
  PublishData();                    // publish data to Particle cloud
  SwitchMux(0x08);                  // switch to i2c mux input 1
  ReadAccelerometer(GAccI2C);       // read the accelerometer
  event = "Pump2Acc4";
  PublishData();                    // publish data to Particle cloud
  delay(DataInterval);              // wait interval, then repeat main loop
}

void ReadAccelerometer(int i2c_address)   // function to read/average accelerometer values
{
  Wire.beginTransmission(i2c_address);    // configure the accelerometer
  Wire.write(0x2D);                       // Select power control register
  Wire.write(0x08);                       // disable auto sleep
  Wire.endTransmission();
  Wire.beginTransmission(i2c_address);
  Wire.write(0x31);                       // Select data format register
  Wire.write(0b00001011);                 // Select full resolution, +/-16g
  Wire.endTransmission();
  delay(100);                             // wait 100mS
  for(int f = 0; f < 8; f++)              // step through 8 frequencies starting with ODR = 25Hz
  {
    Wire.beginTransmission(i2c_address);  // Start I2C Transmission
    Wire.write(0x2C);                     // Select bandwidth rate register
    Wire.write(8 + f);                    // 25, 50, 100, 200, 400, 800, 1.6k, 3.2k
    Wire.write(0x38);                     // Set FIFO Control register
    Wire.write(0b01011111);               // FIFO mode
    Wire.endTransmission();
    delay(100);                           // wait 100mS

    for(int i = 0; i < 32; i++)           // Read all 32 values from FIFO
    {
      Wire.beginTransmission(i2c_address);
      Wire.write(0x32);                   // Point to the DATAX0 register
      Wire.requestFrom(i2c_address,6);    // request 6 bytes of data (X0 - Z1)
      while(Wire.available())
      {
        x0data[i] = Wire.read();          // put x,y,z data into arrays
        x1data[i] = Wire.read();
        y0data[i] = Wire.read();
        y1data[i] = Wire.read();
        z0data[i] = Wire.read();
        z1data[i] = Wire.read();
      }
      Wire.endTransmission();
    }

    for (int i=1; i<32; i++)            // read data from arrays to calculate amplitude
    {
      xdata[i] = ((x1data[i]) & 0x1F)<<8 | (x0data[i] & 0xFF);    // combine 5 MSB and 8 LSB into one variable
      ydata[i] = ((y1data[i]) & 0x1F)<<8 | (y0data[i] & 0xFF);
      zdata[i] = ((z1data[i]) & 0x1F)<<8 | (z0data[i] & 0xFF);

      if (xdata[i]>HALFSCALE) xdata[i] -= A2DRES;  // convert to signed value
      if (ydata[i]>HALFSCALE) ydata[i] -= A2DRES;  // convert to signed value
      if (zdata[i]>HALFSCALE) zdata[i] -= A2DRES;  // convert to signed value

  // calculate amplitude by summing the squares, divide vectors by 100 to keep number manageable
      Adata[f] += (xdata[i]*xdata[i])/100 + (ydata[i]*ydata[i])/100 + (zdata[i]*zdata[i])/100;
    }
    Adata[f] = sqrt(Adata[f]);          // Amplitude = squareroot of sum of the squares
  }
}

void PublishData()        // function to publish all sensor data in json format
{
    Particle.publish("ATData", "{ \"f1\": \"" + String(Adata[0],2) + "\"," +
     "\"f2\": \"" + String(Adata[1],2) + "\"," +
     "\"f3\": \"" + String(Adata[2],2) + "\"," +
     "\"f4\": \"" + String(Adata[3],2) + "\"," +
     "\"f5\": \"" + String(Adata[4],2) + "\"," +
     "\"f6\": \"" + String(Adata[5],2) + "\"," +
     "\"f7\": \"" + String(Adata[6],2) + "\"," +
     "\"f8\": \"" + String(Adata[7],2) + "\"," +
     "\"RT\": \"" + String(RoomTemp,2) + "\"," +
     "\"TC\": \"" + String(Thermocouple,2) + "\"," +
     "\"event\": \"" + String(event) + "\" }", PRIVATE);
}

void SwitchMux(int select)          // function to change the i2c mux channel
{
  Wire.beginTransmission(MuxI2C);
  Wire.write(select);
  Wire.endTransmission();
}

int configInterval(String interval) // Particle function for data publishing interval
{
    if (interval == "10S")          // if function call = 10 seconds
    {
      DataInterval = TenSec;
      return 10;
    }
    else if (interval == "1M")      // if function call = 1 minute
    {
      DataInterval = OneMin;
      return 60;
    }
    else if (interval == "5M")      // if function call = 5 minutes
    {
      DataInterval = FiveMin;
      return 300;
    }
    else if (interval == "10M")     // if function call = 10 minutes
    {
      DataInterval = TenMin;
      return 600;
    }
    else if (interval == "15M")     // if function call = 10 minutes
    {
      DataInterval = FifteenMin;
      return 900;
    }
    else return -1;

    int addr = 10;                  // define EEPROM data structure
    struct nvmobject
    {
      uint16_t verifyEE;
      unsigned long DataInterval;
    };
    nvmobject nvmobj = { 0xaa55, DataInterval};
    EEPROM.put(addr, nvmobj);
}
