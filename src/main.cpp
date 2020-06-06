#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"

//needed for library
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include "esp_wifi.h"
#include "time.h"

//custom
#include "gpios.h"
#include "main.h"

AsyncWebServer server(80);
DNSServer dns;

bool clear_credentials_flag = false;


char ntpServer[100] = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;



void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}


void handleRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
	char* body = (char*)malloc(100);
	char* elemts[10];
	int elemts_cnt = 0;

	//cut requeset-body out of data
	strncpy(body, (char*)(data+1), len-2);  //removes quotation marks around the body as well. I know... nasty...
	body[len-2] = '\0';		//terminates te string propper
	//



	//split request-body in it's elemts
	elemts[elemts_cnt] = strtok(body, ";");
	while(elemts[elemts_cnt] != NULL)
	{
		elemts[++elemts_cnt] = strtok(NULL, ";");
	}
	//



	Serial.printf("found %d elemts in response\n", elemts_cnt);

	//if there was no elemt in the body, terminate
	if(elemts_cnt<1)
	{
		request->send(400);
		Serial.println("No element was found in the request-body");
		return;
	}
	//

	if(strcmp(elemts[0], "pump_on") == 0)
	{
		request->send(200);

		Serial.println("switching on relais 1");
		Rel_switch(1, 1);
	}

	else if(strcmp(elemts[0], "pump_off") == 0)
	{
		request->send(200);

		Serial.println("switching off relais 1");
		Rel_switch(1, 0);
	}

	else if(strcmp(elemts[0], "GetServerTime") == 0)
	{
		char buff[100] = {0};
		for(int i=0; i<100; i++)
			buff[0] = 0;

		Serial.println("Server time was requested");

		struct tm timeinfo;

		if(!getLocalTime(&timeinfo))
  		{
    		request->send(500);
    		return;
 		}

  		sprintf(buff, "%02d:%02d:%02d  %02d.%02d.%04d;%s", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_mday, timeinfo.tm_mon, (timeinfo.tm_year + 1900), ntpServer);

		request->send(200, "text/plain", buff); 
	}


	else if(strcmp(elemts[0], "SetNTP") == 0)
	{
		//if no new ntp server adress was send, termiante
		if(elemts_cnt<2)
		{
			request->send(401);
			return;
		}
		//

		strcpy(ntpServer, elemts[1]);
		Serial.printf("New NTP-server adress was set to '%s'\n", ntpServer );

		//get time from NTP 
		configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);


		struct tm timeinfo;
		if(!getLocalTime(&timeinfo))	//try to get time from new ntp-server adress
  		{
    		Serial.println("Failed to obtain time");
			request->send(402);
    		return;
  		}
		//

		//loop back new ntp-server adress
		request->send(200, "text/plain", ntpServer); 
		//
	}

	else
	{
		Serial.println("Invalid request");
		request->send(400);
	}


	free(body);
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


	//NTP
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  	printLocalTime();

  
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