#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"


const char* ssid = "WLAN HOUSE";
const char* password =  "wioed9dxjsndcio38i3ndcklw";

AsyncWebServer server(80);



void setup()
{
	//inti serial console
	Serial.begin(115200);


	//mounting filesystem
	if(!SPIFFS.begin())
	{
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
  	}
 

	//starting wifi and attempt do connect to network
	WiFi.begin(ssid, password);

	//wait for connection esablished
	while (WiFi.status() != WL_CONNECTED)
	{
  		delay(500);
  		Serial.println("Connecting to WiFi..");
	}

	//connection established
	Serial.println("Connected to the WiFi network");

	//starting mDNS service
	if(!MDNS.begin("pumpe"))
	{
     	Serial.println("Error starting mDNS");
     	return;
  	}
  
	//printing device's IP
  	Serial.println(WiFi.localIP());
  
	//invoke index.html for website request
  	server.on("/html", HTTP_GET, [](AsyncWebServerRequest *request)
	{
    	request->send(SPIFFS, "/index.html", "text/html");
	});
  
	//start server
  	server.begin();
}



void loop()
{
	Serial.println("idle");

	delay(5000);
}