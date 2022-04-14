

#include <infrared.h>


void setup() {

Serial.begin(115200);
delay(2000);
Serial.println();

infraredSetup();


}

void loop() {
  infraredLoop();
  delay(100);
}