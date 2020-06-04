#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"

//needed for library
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include "esp_wifi.h"

//custom
#include "gpios.h"
#include "main.h"

AsyncWebServer server(80);
DNSServer dns;

bool clear_credentials_flag = false;



void handleRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
	if(strcmp((char*)data, "\"pump_on\"") == 0)
	{
		request->send(202);

		Serial.println("switching on relais 1");
		Rel_switch(1, 1);
	}

	else if(strcmp((char*)data, "\"pump_off\"") == 0)
	{
		request->send(202);

		Serial.println("switching off relais 1");
		Rel_switch(1, 0);
	}

	else
	{
		request->send(400);
	}
}

void setup()
{
	//inti serial console
	Serial.begin(115200);

	//inti GPIOs
	GPIO_init_custom();

	//WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    AsyncWiFiManager wifiManager(&server,&dns);
    
    //fetches ssid and pass from eeprom and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "AutoConnectAP"
    //and goes into a blocking loop awaiting configuration
    wifiManager.autoConnect("pumpe_ard");
    //or use this for auto generated name ESP + ChipID
    //wifiManager.autoConnect();
	
	//if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");

	//mounting filesystem
	if(!SPIFFS.begin())
	{
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
  	}
 
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
		Serial.println("Sending requested index.html");
	});

	server.onRequestBody(handleRequest);


  
	//start server
  	server.begin();
}

void loop()
{

	if(clear_credentials_flag)
	{
		clear_wifi_credentials();
		clear_credentials_flag = false;
	}
}

void clear_wifi_credentials()
{
	Serial.println(">>>> ENTERED RESET-FUNCTION!!! <<<<");
	

	//reset saved settings
	//wifiManager.resetSettings();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); //load the flash-saved configs
	esp_wifi_init(&cfg); //initiate and allocate wifi resources (does not matter if connection fails)
	delay(2000); //wait a bit

	if(esp_wifi_restore()!=ESP_OK)
	{
		Serial.println("WiFi is not initialized by esp_wifi_init ");
	}
	else
	{
		Serial.println("WiFi Configurations Cleared!");
	}

		//continue
	delay(1000);

	ESP.restart();
	
}

void set_clear_credetial_flag()
{
	clear_credentials_flag = true;
}