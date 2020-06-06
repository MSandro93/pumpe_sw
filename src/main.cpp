#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <EEPROM.h>

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

//schedule
typedef struct timestamp
{
	uint8_t stunden = 0;
	uint8_t minuten = 0;
};

timestamp morgens_start;
timestamp morgens_stop;
timestamp abends_start;
timestamp abends_stop;
//


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

void setTimestamp(timestamp* ts_, char* str_)
{
	char* parts[4];
	int cnt = 0;

	parts[cnt] = strtok(str_, ":");
	while(parts[cnt] != NULL)
	{
		parts[++cnt] = strtok(NULL, ":");
	}

	ts_->stunden = 	atoi(parts[0]);
	ts_->minuten = 	atoi(parts[1]);

}

int getMinutesFromStr(char* str_)
{
	char buff[20];

	strcpy(buff, str_);
	buff[strlen(str_)] = 0;

	char* parts[4];
	int cnt = 0;

	parts[cnt] = strtok(buff, ":");
	while(parts[cnt] != NULL)
	{
		parts[++cnt] = strtok(NULL, ":");
	}

	if(cnt != 2)
		return -1;
		
	else
		return atoi(parts[0])*60 +  atoi(parts[1]);
}



void handleRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
	char* body = (char*)malloc(100);
	char* elements[10];
	int elements_cnt = 0;

	//cut requeset-body out of data
	strncpy(body, (char*)(data+1), len-2);  //removes quotation marks around the body as well. I know... nasty...
	body[len-2] = '\0';		//terminates te string propper
	//



	//split request-body in it's elements
	elements[elements_cnt] = strtok(body, ";");
	while(elements[elements_cnt] != NULL)
	{
		elements[++elements_cnt] = strtok(NULL, ";");
	}
	//



	for(int i=0; i<elements_cnt; i++)
	{
		Serial.printf(">> |%s|\n", elements[i]);
	}


	Serial.printf("found %d elements in response\n", elements_cnt);

	//if there was no elemt in the body, terminate
	if(elements_cnt<1)
	{
		request->send(400);
		Serial.println("No element was found in the request-body");
		return;
	}
	//

	if(strcmp(elements[0], "pump_on") == 0)
	{
		request->send(200);

		Serial.println("switching on relais 1");
		Rel_switch(1, 1);
	}

	else if(strcmp(elements[0], "pump_off") == 0)
	{
		request->send(200);

		Serial.println("switching off relais 1");
		Rel_switch(1, 0);
	}

	else if(strcmp(elements[0], "GetServerTime") == 0)
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


	else if(strcmp(elements[0], "SetNTP") == 0)
	{
		//if no new ntp server adress was send, termiante
		if(elements_cnt<2)
		{
			request->send(401);
			return;
		}
		//

		strcpy(ntpServer, elements[1]);
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
		else
			request->send(200, "text/plain", ntpServer); 
		//
	}

	else if(strcmp(elements[0], "getSchedule") == 0)
	{
		char response[100] = {0};


		sprintf(response, "%02d:%02d;%02d:%02d;%02d:%02d;%02d:%02d",
				morgens_start.stunden,
				morgens_start.minuten,
				morgens_stop.stunden,
				morgens_stop.minuten,

				abends_start.stunden,
				abends_start.minuten,
				abends_stop.stunden,
				abends_stop.minuten
				);

		request->send(200, "text/plain", response);

		Serial.printf(">> schedule send: %s\n", response);
	}
	
	else if(strcmp(elements[0], "setSchedule") == 0)
	{
		//if no new ntp server adress was send, termiante
		if(elements_cnt<5)
		{
			request->send(401);
			return;
		}
		//

		if( (getMinutesFromStr(elements[2]) > getMinutesFromStr(elements[1])) && 
		    (getMinutesFromStr(elements[3]) > getMinutesFromStr(elements[2])) &&
			(getMinutesFromStr(elements[4]) > getMinutesFromStr(elements[3])) )
		{
			setTimestamp(&morgens_start, elements[1]);
			setTimestamp(&morgens_stop,  elements[2]);
			setTimestamp(&abends_start,  elements[3]);
			setTimestamp(&abends_stop,   elements[4]);

			Serial.println("New schedule was set.");
			request->send(200);
		}

		else
		{
			Serial.println("New schedule was invalid.");
			request->send(400);
		}
		
		
		
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

	
	//set schedule
	morgens_start.stunden = 8;
	morgens_start.minuten = 0;
	morgens_stop.stunden = 8;
	morgens_stop.minuten = 30;

	abends_start.stunden = 19;
	abends_start.minuten = 15;
	abends_stop.stunden = 20;
	abends_stop.minuten = 15;
	//

  
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