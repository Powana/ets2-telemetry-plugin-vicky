#include "Keyboard.h"
String serialRxString;
char serialRxBytes[3];
int readUntilCode;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.setTimeout(100);

  int outMin = 0;  // Lowest pin
  int outMax = 13; // Highest pin 
  for(int i=outMin; i<=outMax; i++)
  {
    pinMode(i, OUTPUT);
  }
}     

void loop() {
    if (Serial.available() > 0) { 
      memset(serialRxBytes, 0 , sizeof(serialRxBytes));
      readUntilCode = Serial.readBytesUntil(';', serialRxBytes, 3);
      // serialRxBytes[0] = serialRxBytes[0] - '0';
      // serialRxBytes[1] = serialRxBytes[0] - '0';
      /*Serial.print(serialRxBytes);
      Serial.print(" PLATS 0: ");
      Serial.print(serialRxBytes[0]);
      Serial.print(" PLATS 1: ");
      Serial.print(serialRxBytes[1]);
      Serial.print("\n");*/
  
      if ( serialRxBytes[0] >= 0 && serialRxBytes[0] <= 13 && serialRxBytes[1] >= 0 && serialRxBytes[1] <= 1) {
        // Serial.print("WRITING TO PIN:");Serial.print(serialRxBytes[0]);Serial.print(" STATE: ");Serial.print(serialRxBytes[1]);
        digitalWrite(serialRxBytes[0], serialRxBytes[1]);
      }
    }
}
