#include "Keyboard.h"
String serialRxString;
char serialRxBytes[3];
int readUntilCode;
bool hornOn;

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
  pinMode(A2, OUTPUT);
  hornOn = false;
}     

void loop() {
  
    if (Serial.available() > 0) { 
      memset(serialRxBytes, 0 , sizeof(serialRxBytes));
      readUntilCode = Serial.readBytesUntil(';', serialRxBytes, 3);
  
      if ( serialRxBytes[0] >= 0 && serialRxBytes[0] <= 13 && serialRxBytes[1] >= 0 && serialRxBytes[1] <= 1) {
        // Serial.print("WRITING TO PIN:");Serial.print(serialRxBytes[0]);Serial.print(" STATE: ");Serial.print(serialRxBytes[1]);
        digitalWrite(serialRxBytes[0], serialRxBytes[1]);
      }
    }

    // A0 Is connected to TM horn adn TM gnd
    if (analogRead(A0) < 10) {
      hornOn = true;
      digitalWrite(A2, 1);
    }
    else if (hornOn) {
      digitalWrite(A2, 0);
      hornOn = false;
    }
    Serial.println(hornOn);
}
