#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <EEPROM.h>
#include <HTTPClient.h>

//needed for library
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include "esp_wifi.h"
#include "time.h"
#include "Ticker.h"

//custom
#include "gpios.h"
#include "main.h"




//memory-areas
#define max_mem 	210

#define ntp_add 	0	//100
#define mstart_add 	100	//10
#define mstop_add 	110	//10
#define astart_add 	120	//10
#define astop_add 	130	//10
#define apiKey_add 	140	//32
#define city_add 	180 //20
//




AsyncWebServer server(80);
DNSServer dns;

bool clear_credentials_flag = false;


char ntpServer[101] = "";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

//schedule
typedef struct timestamp
{
	uint8_t stunden = 0;
	uint8_t minuten = 0;
	uint16_t inMinuten = 0;
};

timestamp morgens_start;
timestamp morgens_stop;
timestamp abends_start;
timestamp abends_stop;
//

//everything for the scheuduler
bool morgen_quiet = false;
bool abend_quiet = false;
bool forced_on = false;

void check_time(void);
Ticker time_schedule(check_time, 1000, 0, MILLIS);
//

//everything for the weather forecaste
char api_key[50];
char city[50];
#define API_KEY_LENGTH 32

HTTPClient http;
//






int getTomorrowWeather()
{
	char url[100];
	String resp;

	sprintf(url, "http://api.openweathermap.org/data/2.5/forecast/?q=%s,de&appid=%s", city, api_key);
	http.begin(url);

	int httpCode = http.GET();
	if(  httpCode!= 200)
	{
		Serial.printf("Failed to fetch weather forecast: %d\n", httpCode);
		return -1;
	}

	Serial.println("Got weather forecaste.");
	
	resp = http.getString();

	Serial.println(resp);

	return 0;
}


void check_time()
{
	tm timeinfo;
	getLocalTime(&timeinfo);

	uint32_t currentTime = timeinfo.tm_hour * 60 + timeinfo.tm_min;  // [min]

	if( (morgens_start.inMinuten <= currentTime) && (currentTime <= morgens_stop.inMinuten) && !morgen_quiet)
	{
		Rel_switch(1, 1);
	}

	else if( (abends_start.inMinuten <= currentTime) && (currentTime <= abends_stop.inMinuten) && !abend_quiet)
	{
		Rel_switch(1, 1);
	}

	else if (forced_on)
	{
		Serial.println(">> forced on = true");
		Rel_switch(1, 1);
	}

	else
	{
		Rel_switch(1, 0);
	}
	
}


bool onlyACSII(char* str_, int len_)
{
	for(int i=0; i<len_; i++)
	{
		if(str_[i]<32 || str_[i]>216)
			return false;
	}

	return true;
}

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
	ts_->inMinuten = ts_->stunden*60 + ts_->minuten;
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

		forced_on = true;

		//if the pump is supposed to run at the moment, the dedicated quiet-flag shall be cleared
		tm timeinfo;
		getLocalTime(&timeinfo);
		uint32_t currentTime = timeinfo.tm_hour * 60 + timeinfo.tm_min;  // [min]

		if((morgens_start.inMinuten <= currentTime) && (currentTime <= morgens_stop.inMinuten))
		{
			morgen_quiet = false;
		}
		else if((abends_start.inMinuten <= currentTime) && (currentTime <= abends_stop.inMinuten))
		{
			abend_quiet = false;
		}
		//
	}

	else if(strcmp(elements[0], "pump_off") == 0)
	{
		request->send(200);

		Serial.println("switching off relais 1");
		Rel_switch(1, 0);

		forced_on = false;

		//if the pump is supposed to run at the moment, the dedicated quiet-flag shall be set
		tm timeinfo;
		getLocalTime(&timeinfo);
		uint32_t currentTime = timeinfo.tm_hour * 60 + timeinfo.tm_min;  // [min]

		if((morgens_start.inMinuten <= currentTime) && (currentTime <= morgens_stop.inMinuten))
		{
			morgen_quiet = true;
		}
		else if((abends_start.inMinuten <= currentTime) && (currentTime <= abends_stop.inMinuten))
		{
			abend_quiet = true;
		}
		//
	}

	else if(strcmp(elements[0], "GetServerTime") == 0)
	{
		char buff[200] = {0};
		for(int i=0; i<100; i++)
			buff[0] = 0;

		Serial.println("Server time was requested");

		struct tm timeinfo;

		if(!getLocalTime(&timeinfo), 1000)
  		{
			request->send(500);
			return;
		}
		
		Serial.println("  >> got here! <<");

  		sprintf(buff, "%02d:%02d:%02d  %02d.%02d.%04d;%s;%s;%s;", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_mday, timeinfo.tm_mon, (timeinfo.tm_year + 1900), ntpServer, api_key, city);

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

		//write NTP server address to EEPROM
		EEPROM.begin(max_mem);
		EEPROM.writeBytes(ntp_add, ntpServer, strlen(ntpServer));
		EEPROM.writeByte( (ntp_add + strlen(ntpServer)), 0);  //termiante string by 0
		EEPROM.end();
		//

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
		{
			char buff[100];
			sprintf(buff, "%s;%02d:%02d:%02d  %02d.%02d.%04d;", ntpServer, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_mday, timeinfo.tm_mon, (timeinfo.tm_year + 1900));
			request->send(200, "text/plain", buff); 
		}
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
			Serial.println("New schedule was set.");
			request->send(200);

			setTimestamp(&morgens_start, elements[1]);
			setTimestamp(&morgens_stop,  elements[2]);
			setTimestamp(&abends_start,  elements[3]);
			setTimestamp(&abends_stop,   elements[4]);

			//store timestamps to EEPROM
			EEPROM.begin(max_mem);

			EEPROM.writeBytes(mstart_add, &morgens_start, sizeof(timestamp));
			EEPROM.writeBytes(mstop_add, &morgens_stop, sizeof(timestamp));
			EEPROM.writeBytes(astart_add, &abends_start, sizeof(timestamp));
			EEPROM.writeBytes(astop_add, &abends_stop, sizeof(timestamp));

			EEPROM.end();
			//

			
		}

		else
		{
			Serial.println("New schedule was invalid.");
			request->send(400);
		}
		
		
		
	}

	else if(strcmp(elements[0], "reboot") == 0)
	{
		ESP.restart();
		request->send(200);
	}	

	else if(strcmp(elements[0], "SetAPIKey") == 0)
	{
		//if no new API-Key was send, termiante
		if(elements_cnt<2)
		{
			request->send(401);
			return;
		}
		//


		strncpy(api_key, elements[1], API_KEY_LENGTH);
		api_key[API_KEY_LENGTH + 1] = 0;  //termiante string by 0

		Serial.printf(">> new API-Key: |%s|\n", api_key);

		EEPROM.begin(max_mem);
		EEPROM.writeBytes(apiKey_add, api_key, API_KEY_LENGTH + 1);
		EEPROM.end();

		request->send(200, "text/plain", api_key); 

	}

	else if(strcmp(elements[0], "SetCity") == 0)
	{
		//if no new city was send, termiante
		if(elements_cnt<2)
		{
			request->send(401);
			return;
		}
		//

		sprintf(city, "%s", elements[1]);

		Serial.printf(">> new City: |%s|\n", city);

		EEPROM.begin(max_mem);
		EEPROM.writeString(city_add, city);
		EEPROM.end();

		request->send(200, "text/plain", city); 
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


	////EEPROM
	char buff[100];
	EEPROM.begin(max_mem);

	//load ntp server address
	EEPROM.readBytes(ntp_add, buff, 100);


	if(onlyACSII(buff, strlen(buff)))
	{
		sprintf(ntpServer, "%s", buff);
		Serial.println("loaded ntp server address from EEPROM.");
	}
	//

	//load schedule
	EEPROM.readBytes(mstart_add, &morgens_start, sizeof(timestamp));
	Serial.println("loaded 'morgens_start' from EEPROM.");

	EEPROM.readBytes(mstop_add, &morgens_stop, sizeof(timestamp));
	Serial.println("loaded 'morgens_stop' from EEPROM.");

	EEPROM.readBytes(astart_add, &abends_start, sizeof(timestamp));
	Serial.println("loaded 'abends_start' from EEPROM.");

	EEPROM.readBytes(astop_add, &abends_stop, sizeof(timestamp));
	Serial.println("loaded 'abends_stop' from EEPROM.");
	//

	//load API-Key for Openweathermaps
	EEPROM.readBytes(apiKey_add, api_key,  API_KEY_LENGTH);
	Serial.println("API Key for waether API loaded.");
	//

	//load city for Openweathermaps
	EEPROM.readBytes(city_add, buff, 20);
	sprintf(city, "%s", buff);
	Serial.println("City loaded.");
	//

	EEPROM.end();
	////

	//inti GPIOs
	GPIO_init_custom();
	Serial.println("GPIOs initilized.");

	//WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    AsyncWiFiManager wifiManager(&server,&dns);
	Serial.println("Webserver initilized.");
    
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
	else
	{
		Serial.println("Filesystem mounted.");
	}
	//

	//starting mDNS service
	if(!MDNS.begin("pumpe"))
	{
     	Serial.println("Error starting mDNS");
     	return;
  	}
	else
	{
		Serial.println("mDNS service started.");
	}
	//
  
	//printing device's IP
  	Serial.println(WiFi.localIP());
	//

	//NTP
	if(onlyACSII(ntpServer, strlen(ntpServer)))
	{
		Serial.printf("Trying to connect to NTP server '%s'\n", ntpServer);
		configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  		printLocalTime();
	}
	else
	{
		Serial.println("no valid NTP server address was loaded.");

		struct tm tm;
		tm.tm_year = 0;
    	tm.tm_mon = 0;
    	tm.tm_mday = 1;
    	tm.tm_hour = 0;
    	tm.tm_min = 0;
    	tm.tm_sec = 0;

    	time_t t = mktime(&tm);

   		printf("Setting time: %s", asctime(&tm));

    	struct timeval now = { .tv_sec = t };

    	settimeofday(&now, NULL);	
	}
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


	//start Ticker
	time_schedule.start();






}

void loop()
{
	time_schedule.update();

	if(clear_credentials_flag)
	{
		clearEEPROM();
	//	clear_wifi_credentials();
		clear_credentials_flag = false;
	}

}




void clearEEPROM()
{
	char buff[max_mem];
	for(int i=0; i<max_mem; i++)
		buff[i] = 255;

	EEPROM.begin(max_mem);
	EEPROM.writeBytes(0, buff, max_mem);
	EEPROM.end();

	Serial.println(">> EEPROM cleared!");
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


	clearEEPROM();

	//continue
	delay(1000);

	ESP.restart();
}