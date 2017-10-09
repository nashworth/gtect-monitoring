#include "math.h"         // include math library for sqrt function
#include "High_Temp.h"    // include library for thermocouple
#include <Wire.h>         // include i2c library
#define PS1I2C 0x28       // TE MSP100 pressure sensor I2C address
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
int pdata[4];
int Acal1, Acal2, Acal3, Acal4;
unsigned long DataInterval;
double Adata[8], A1data[8], A2data[8], A3data[8], A4data[8];
float RoomTemp, Thermocouple;
double Pressure, PTemp, Pressure1, PTemp1, Pressure2, PTemp2;
String event;

//int configAcc(String command);  // function declaration for calibration
int configInterval(String interval);
HighTemp ht1(A1, A0);
HighTemp ht2(A5, A4);

void setup()
{
  Particle.variable("Firmware","GTect.ino");  // define particle variable
//  Particle.function("AccSetup", configAcc);   // define particle function
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
    DataInterval = nvmobj.interval;
  }
  else { DataInterval = OneMin;}
}

void loop()
{
  RoomTemp = ht1.getRoomTmp();   // read and calculate room temp 1
  Thermocouple = ht1.getThmc();  // read and calculate thermocouple temp 1
  SwitchMux(0x01);                  // switch to i2c mux input 1
  ReadAccelerometer(GAccI2C);       // read the accelerometer
  event = "Pump1Acc1";
  PublishData();                    // publish data to Particle cloud
//  for(int j = 0; j < 8; j++){A1data[j] = Adata[j];}  // copy data
  SwitchMux(0x02);                  // switch to i2c mux input 2
  ReadAccelerometer(GAccI2C);       // read the accelerometer
  event = "Pump1Acc2";
  PublishData();                    // publish data to Particle cloud
//  for(int j = 0; j < 8; j++){A2data[j] = Adata[j];} // copy data
  RoomTemp = ht2.getRoomTmp();   // read and calculate room temp 2
  Thermocouple = ht2.getThmc();  // read and calculate thermocouple temp 2
  SwitchMux(0x04);                  // switch to i2c mux input 1
  ReadAccelerometer(GAccI2C);       // read the accelerometer
  event = "Pump2Acc3";
  PublishData();                    // publish data to Particle cloud
//  for(int j = 0; j < 8; j++){A3data[j] = Adata[j];}  // copy data
  SwitchMux(0x08);                  // switch to i2c mux input 1
  ReadAccelerometer(GAccI2C);       // read the accelerometer
  event = "Pump2Acc4";
  PublishData();                    // publish data to Particle cloud
//  for(int j = 0; j < 8; j++){A4data[j] = Adata[j];}  // copy data
//  ReadPressure(PS1I2C);             // read pressure sensor
//  Pressure1 = Pressure;             // copy pressure data
//  PTemp1 = PTemp;                   // copy temperature data
  delay(DataInterval);          // wait interval, then repeat main loop
}

int configAcc(String command)     // Particle function for accelerometer calibration
{
    if (command == "calibrate")   // if function call = "calibrate"
    {
      Acal1 = A1data[1];          // set calilbration values to amplitude 1
      Acal2 = A2data[1];
      Acal3 = A3data[1];
      Acal4 = A4data[1];

      int addr = 10;              // define EEPROM data structure
      struct nvmobject
      {
        uint16_t verifyEE;
        int Acal1;
        int Acal2;
        int Acal3;
        int Acal4;
      };

      nvmobject nvmobj = { 0xaa55, Acal1, Acal2, Acal3, Acal4};
      EEPROM.put(addr, nvmobj);     // write values to the EEPROM
      return 1;
    }
    else if (command == "reset")    // if function call = "reset"
    {
      Acal1 = 0;                    // set calibration values to zero
      Acal2 = 0;
      Acal3 = 0;
      Acal4 = 0;
      int addr = 10;
      uint16_t verifyEE = 0;
      EEPROM.write(addr, verifyEE); // reset EEPROM verification value
      return 0;
    }
    else return -1;
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
    Adata[f] = sqrt(Adata[f]);  // Amplitude = squareroot of sum of the squares
  }
}

void PublishData()        // function to publish all sensor data in json format
{
  // A1Data = data from accelerometer 1
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
/*     delay(1000);
  // A2Data = data from accelerometer 2
     Particle.publish("P1A2Data", "{ \"a2f1\": \"" + String(A2data[0],2) + "\"," +
     "\"a2f2\": \"" + String(A2data[1],2) + "\"," +
     "\"a2f3\": \"" + String(A2data[2],2) + "\"," +
     "\"a2f4\": \"" + String(A2data[3],2) + "\"," +
     "\"a2f5\": \"" + String(A2data[4],2) + "\"," +
     "\"a2f6\": \"" + String(A2data[5],2) + "\"," +
     "\"a2f7\": \"" + String(A2data[6],2) + "\"," +
     "\"a2f8\": \"" + String(A2data[7],2) + "\"," +
     "\"RT1\": \"" + String(RoomTemp1,2) + "\"," +
     "\"TC1\": \"" + String(Thermocouple1,2) + "\" }", PRIVATE);
     delay(1000);
// A2Data = data from accelerometer 2
     Particle.publish("P2A3Data", "{ \"a3f1\": \"" + String(A3data[0],2) + "\"," +
     "\"a3f2\": \"" + String(A3data[1],2) + "\"," +
     "\"a3f3\": \"" + String(A3data[2],2) + "\"," +
     "\"a3f4\": \"" + String(A3data[3],2) + "\"," +
     "\"a3f5\": \"" + String(A3data[4],2) + "\"," +
     "\"a3f6\": \"" + String(A3data[5],2) + "\"," +
     "\"a3f7\": \"" + String(A3data[6],2) + "\"," +
     "\"a3f8\": \"" + String(A3data[7],2) + "\"," +
     "\"RT2\": \"" + String(RoomTemp2,2) + "\"," +
     "\"TC2\": \"" + String(Thermocouple2,2) + "\" }", PRIVATE);
     delay(1000);
// A2Data = data from accelerometer 2
     Particle.publish("P2A4Data", "{ \"a4f1\": \"" + String(A4data[0],2) + "\"," +
     "\"a4f2\": \"" + String(A4data[1],2) + "\"," +
     "\"a4f3\": \"" + String(A4data[2],2) + "\"," +
     "\"a4f4\": \"" + String(A4data[3],2) + "\"," +
     "\"a4f5\": \"" + String(A4data[4],2) + "\"," +
     "\"a4f6\": \"" + String(A4data[5],2) + "\"," +
     "\"a4f7\": \"" + String(A4data[6],2) + "\"," +
     "\"a4f8\": \"" + String(A4data[7],2) + "\"," +
     "\"RT2\": \"" + String(RoomTemp2,2) + "\"," +
     "\"TC2\": \"" + String(Thermocouple2,2) + "\" }", PRIVATE);
*/
}

void ReadPressure(int i2c_address)      // function to read pressure/temp from pressure sensor
{
  Wire.beginTransmission(i2c_address);
  Wire.requestFrom(i2c_address,4);       // request 4 bytes of data from sensor
  while(Wire.available())
  {
    pdata[1] = Wire.read();
    pdata[2] = Wire.read();
    pdata[3] = Wire.read();
    pdata[4] = Wire.read();
  }
  Pressure = ((pdata[1]) & 0x3F)<<8 | ((pdata[2]));   // combine 2 bytes of pressure data
  Pressure = (Pressure - 1000)*(250/14000);           // calculate pressure in PSI
  PTemp = ((pdata[3])<<3 | ((pdata[4]) & 0xE0)>>5);   // combine 2 bytes of temp data
  PTemp = (PTemp * 200)/2048 - 50;                    // calculate temp in C
  Wire.endTransmission();
}

void SwitchMux(int select)          // function to change the i2c mux channel
{
  Wire.beginTransmission(MuxI2C);
  Wire.write(select);
  Wire.endTransmission();
}


int configInterval(String interval) // Particle function for data publishing interval
{
    if (interval == "10S")        // if function call = 10 seconds
    {
      DataInterval = TenSec;
      return 10;
    }
    else if (interval == "1M")    // if function call = 1 minute
    {
      DataInterval = OneMin;
      return 60;
    }
    else if (interval == "5M")    // if function call = 5 minutes
    {
      DataInterval = FiveMin;
      return 300;
    }
    else if (interval == "10M")   // if function call = 10 minutes
    {
      DataInterval = TenMin;
      return 600;
    }
    else if (interval == "15M")   // if function call = 10 minutes
    {
      DataInterval = FifteenMin;
      return 900;
    }
    else return -1;

    int addr = 10;              // define EEPROM data structure
    struct nvmobject
    {
      uint16_t verifyEE;
      unsigned long DataInterval;
    };
    nvmobject nvmobj = { 0xaa55, DataInterval};
    EEPROM.put(addr, nvmobj);
}
