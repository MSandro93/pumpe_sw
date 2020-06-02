#include <Arduino.h>
#include <Esp32WifiManager.h>


uint8_t rel1 = 32;
uint8_t rel2 = 33;
uint8_t user_butt = 34;

WifiManager manager;


void setup() {
  // put your setup code here, to run once:

  manager.setupScan();
  
}

void loop()
{

  manager.loop();
	if (manager.getState() == Connected)
  {
		// use the Wifi Stack now connected
	}

  //digitalWrite(rel1, 1);
  //digitalWrite(rel2, 1);

  //delay(1000);



  // put your main code here, to run repeatedly:
}