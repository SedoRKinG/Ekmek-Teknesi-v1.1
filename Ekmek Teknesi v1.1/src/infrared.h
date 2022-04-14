
#include <server.h>


const  uint16_t  kRecvPin = 5 ;  // ESP32-C3 üzerindeki 14, bir önyükleme döngüsüne neden olur.
IRrecv irrecv(kRecvPin);

decode_results results;

void infraredSetup() {
    pinMode(kRecvPin, INPUT);
  irrecv.enableIRIn();  // Start the receiver
  Serial.print("IRrecvDemo basladı ");
  Serial.println(kRecvPin);
}

void infraredLoop() {
  if (irrecv.decode(&results)) {
    // print() & println() can't handle printing long longs. (uint64_t)
    serialPrintUint64(results.value, HEX);
    Serial.println("-");
    irrecv.resume();  // Receive the next value
  }
}
