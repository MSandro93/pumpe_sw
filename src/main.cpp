#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <EEPROM.h>
#include <HTTPClient.h>
#include <Syslog.h>
#include <Udp.h>

//needed for library
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include "esp_wifi.h"
#include "time.h"
#include "Ticker.h"

//custom
#include "gpios.h"
#include "main.h"
#include "weather.h"
#include "scheduler.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"


//memory-areas
#define max_mem 	210

#define ntp_add 	0	//100
#define mstart_add 	100	//10
#define mstop_add 	110	//10
#define astart_add 	120	//10
#define astop_add 	130	//10
#define apiKey_add 	140	//32
#define city_add 	180 //20
#define th_add		200 //4
//



FILE* logfile;
char logfile_name[15];  //name of todays logfile

AsyncWebServer server(80);
DNSServer dns;

bool clear_credentials_flag = false;


char ntpServer[101] = "";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;


////everything for the scheuduler
struct timestamp
{
	int stunden = 0;
	int minuten = 0;
	int inMinuten = 0;
};

float threshold = 0.0f;
Ticker time_schedule(scheuduler_loop, one_tick_in_mills, 0, MILLIS);
//

//everything for the weather forecaste
char api_key[33];
char city[20];
#define API_KEY_LENGTH 32
//


//everything for logging
#define SYSLOG_SERVER "ftp-host"
#define SYSLOG_PORT 514

#define DEVICE_HOSTNAME "gartenpumpe"
#define APP_NAME "gartenpumpe"

WiFiUDP udpClient;
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_KERN);
//


void pump_on();
void pump_off();




void firstTaskOfDay()
{
	//reactivate all appointments for today
	scheuduler_reactivateAll();

	//get weatherforecast of today and deactivate watering-periodes, if nessaccary
    float rain = getRainVolumeToday(api_key, city);

	Serial.printf("new weatherforecast was requested (rain today: %.3fmm)\n", rain);
	syslog.logf(LOG_INFO, "new weatherforecast was requested (rain today: %.3fmm)\n", rain);

	if(rain > threshold)
	{
		scheuduler_setActive("morgens_an", false);
		scheuduler_setActive("morgens_aus", false);
		scheuduler_setActive("abends_an", false);
		scheuduler_setActive("abends_aus", false);

		printf("Rain today (%.3fmm) more than threshold (%.3fmm). -> all watering periodes for today disabled", rain, threshold);
		syslog.logf(LOG_INFO, "Rain today (%.3fmm) more than threshold (%.3fmm). -> all watering periodes for today disabled", rain, threshold);
	}
	else
	{
		scheuduler_setActive("morgens_an", true);
		scheuduler_setActive("morgens_aus", true);
		scheuduler_setActive("abends_an", true);
		scheuduler_setActive("abends_aus", true);

		printf("Rain today (%.3fmm) less than threshold (%.3fmm). -> all watering periodes stay enabled", rain, threshold);
		syslog.logf(LOG_INFO, "Rain today (%.3fmm) more than threshold (%.3fmm). -> all watering periodes stay enabled", rain, threshold);
	}



	//

	//create new logfile for today
	tm timeinfo;
	getLocalTime(&timeinfo);

	fclose(logfile);
	sprintf(logfile_name, "%02d:%02d:%04d.log", timeinfo.tm_mday, timeinfo.tm_mon+1, (timeinfo.tm_year+1900));
	fopen(logfile_name, "a");
	//
}



bool getValidString(uint8_t* str_, char* str_dest, int len_)
{
	int i = 0;
	bool foundTermination = false;

	for (; i < len_; i++)	//search for string termiantion
	{
		if (str_[i] == 0)
		{
			foundTermination = true;
			break;
		}
	}

	if (!foundTermination)	//no termiantion was found
	{
		str_dest[0] = '\0'; //terminate output sring
		return false;
	}

	for (int j = 0; j < i; j++)
	{
		if ((str_[j] < 32) || (str_[j] > 127))  //a not 'printable' char was found
		{
			str_dest[0] = '\n'; //terminate output sring
			return false;
		}
	}

	memcpy(str_dest, str_, i + 1); //copy valid string

	return true;
}

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
	syslog.logf(LOG_ERR, "Failed to obtain time");
    return;
  }

  syslog.logf(LOG_INFO, "got time from NTP server: %02d.%02d.%04d  %02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  Serial.printf("got time from NTP server: %02d.%02d.%04d  %02d:%02d:%02d\n", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}


int getMinutesFromStr(char* str_)
{
	char* buff = (char*)malloc(20);

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
	{	
		free(buff);
		return -1;
	}
		
	else
	{
		free(buff);
		return atoi(parts[0])*60 +  atoi(parts[1]);
	}
}



void handleRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
	char* body = (char*)malloc(100);
	char* elements[10];
	int elements_cnt = 0;

	//cut requeset-body out of data
	strncpy(body, (char*)(data+1), len-2);  //removes quotation marks around the body as well. I know... nasty...
	body[len-2] = '\0';		//terminates the string propper
	//


	//split request-body in it's elements
	elements[elements_cnt] = strtok(body, ";");
	while(elements[elements_cnt] != NULL)
	{
		elements[++elements_cnt] = strtok(NULL, ";");
	}
	//


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
		pump_on();
	}

	else if(strcmp(elements[0], "pump_off") == 0)
	{
		request->send(200);
		pump_off();
	}

	else if(strcmp(elements[0], "GetServerTime") == 0)
	{
		char* buff = (char*)malloc(200);
		for(int i=0; i<100; i++)
			buff[0] = 0;


		struct tm timeinfo;

		if(!getLocalTime(&timeinfo))
  		{
			request->send(500);
			return;
		}
				
  		sprintf(buff, "%02d:%02d:%02d  %02d.%02d.%04d;%s;%s;%s;%.3f;", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_mday, timeinfo.tm_mon, (timeinfo.tm_year + 1900), ntpServer, api_key, city, threshold);

		request->send(200, "text/plain", buff); 

		syslog.log(LOG_INFO, "Server time was requested.");
		Serial.println("Server time was requested.");

		free(buff);
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

		syslog.logf(LOG_INFO, "New NTP-server adress was set to '%s'\n", ntpServer );
		Serial.printf("New NTP-server adress was set to '%s'\n", ntpServer );

		//get time from NTP 
		configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);


		struct tm timeinfo;
		if(!getLocalTime(&timeinfo))	//try to get time from new ntp-server adress
  		{
			syslog.log(LOG_ERR, "Failed to obtain time");
    		Serial.println("Failed to obtain time");
			request->send(402);
    		return;
  		}
		//

		//loop back new ntp-server address
		else
		{
			char buff[100];
			sprintf(buff, "%s;%02d:%02d:%02d  %02d.%02d.%04d;", ntpServer, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_mday, timeinfo.tm_mon, (timeinfo.tm_year + 1900));
			request->send(200, "text/plain", buff); 
		}
		//
	}

	else if(strcmp(elements[0], "getScheudule") == 0)
	{
		char *response = (char*)malloc(100);
		appointment* morgens_an = scheuduler_getAppointment("morgens_an");
		appointment* morgens_aus = scheuduler_getAppointment("morgens_aus");
		appointment* abends_an = scheuduler_getAppointment("abends_an");
		appointment* abends_aus = scheuduler_getAppointment("abends_aus");

		sprintf(response, "%02d:%02d;%02d:%02d;%02d:%02d;%02d:%02d",
				morgens_an->hour,
				morgens_an->min,
				morgens_aus->hour,
				morgens_aus->min,

				abends_an->hour,
				abends_an->min,
				abends_aus->hour,
				abends_aus->min
				);

		request->send(200, "text/plain", response);

		syslog.log(LOG_INFO, "scheudule was requested.");
		Serial.printf("scheudule was requested.");

		free(response);
	}
	
	else if(strcmp(elements[0], "setScheudule") == 0)
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
			
			request->send(200);

			timestamp t1, t2, t3, t4;

			sscanf(elements[1], "%d:%d", &t1.stunden, &t1.minuten);
			sscanf(elements[2], "%d:%d", &t2.stunden, &t2.minuten);
			sscanf(elements[3], "%d:%d", &t3.stunden, &t3.minuten);
			sscanf(elements[4], "%d:%d", &t4.stunden, &t4.minuten);

			scheuduler_getAppointment("morgens_an")->hour  = t1.stunden;
			scheuduler_getAppointment("morgens_an")->min   = t1.minuten;
			scheuduler_getAppointment("morgens_aus")->hour = t2.stunden;
			scheuduler_getAppointment("morgens_aus")->min  = t2.minuten;
			scheuduler_getAppointment("abends_an")->hour   = t3.stunden;
			scheuduler_getAppointment("abends_an")->min	   = t3.minuten;
			scheuduler_getAppointment("abends_aus")->hour  = t4.stunden;
			scheuduler_getAppointment("abends_aus")->min   = t4.minuten;


			//store timestamps to EEPROM
			EEPROM.begin(max_mem);

			EEPROM.writeBytes(mstart_add, &t1, sizeof(timestamp));
			EEPROM.writeBytes(mstop_add,  &t2, sizeof(timestamp));
			EEPROM.writeBytes(astart_add, &t3, sizeof(timestamp));
			EEPROM.writeBytes(astop_add,  &t4, sizeof(timestamp));

			EEPROM.end();
			//

			syslog.log(LOG_INFO, "new scheudule was set.");
			Serial.println("New schedule was set.");
		}

		else
		{
			syslog.log(LOG_ERR, "New schedule was invalid.");
			Serial.println("New schedule was invalid.");
			request->send(400);
		}
		
		
		
	}

	else if(strcmp(elements[0], "reboot") == 0)
	{	
		request->send(200);

		Serial.println("rebooting...");
		syslog.log(LOG_INFO, "rebooting...");

		ESP.restart();
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
		api_key[API_KEY_LENGTH] = 0;  //termiante string by 0

		EEPROM.begin(max_mem);
		EEPROM.writeBytes(apiKey_add, api_key, API_KEY_LENGTH + 1);
		EEPROM.end();

		request->send(200, "text/plain", api_key); 

		Serial.printf("new API-Key: %s\n", api_key);
		syslog.logf(LOG_INFO, "new API-Key: %s\n", api_key);
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

		EEPROM.begin(max_mem);
		EEPROM.writeString(city_add, city);
		EEPROM.end();

		request->send(200, "text/plain", city); 

		Serial.printf("new City: |%s|\n", city);
		syslog.logf(LOG_INFO, "new City: |%s|\n", city);
	}

	else if(strcmp(elements[0], "getForecast") == 0)
	{
		char* buff = (char*)malloc(10);
		float rain = getRainVolumeToday(api_key, city);

		sprintf(buff, "%.3f", rain);

		if(rain > threshold)
		{
			scheuduler_setActive("morgens_an", false);
			scheuduler_setActive("morgens_aus", false);
			scheuduler_setActive("abends_an", false);
			scheuduler_setActive("abends_aus", false);

			printf("Rain today (%.3fmm) more than threshold (%.3fmm). -> all watering periodes for today disabled", rain, threshold);
			syslog.logf(LOG_INFO, "Rain today (%.3fmm) more than threshold (%.3fmm). -> all watering periodes for today disabled", rain, threshold);
		}
		else
		{
			scheuduler_setActive("morgens_an", true);
			scheuduler_setActive("morgens_aus", true);
			scheuduler_setActive("abends_an", true);
			scheuduler_setActive("abends_aus", true);

			printf("Rain today (%.3fmm) less than threshold (%.3fmm). -> all watering periodes stay enabled", rain, threshold);
			syslog.logf(LOG_INFO, "Rain today (%.3fmm) less than threshold (%.3fmm). -> all watering periodes stay enabled", rain, threshold);
		}

		request->send(200, "text/plain", buff); 

		Serial.printf("new weatherforecast was requested (rain today: %smm)\n", buff);
		syslog.logf(LOG_INFO, "new weatherforecast was requested (rain today: %smm)\n", buff);

		free(buff);
	}

	else if(strcmp(elements[0], "setThreshold") == 0)
	{
		float f = 0.0f;
		if( sscanf(elements[1], "%f", &f) == 1)
		{
			threshold = f;

			EEPROM.begin(max_mem);
			EEPROM.writeFloat(th_add, threshold);
			EEPROM.end();

			char* buff = (char*)malloc(10);
			sprintf(buff, "%.3f", threshold);
			
			request->send(200, "text/plain", buff);

			Serial.printf("threshold set to %.3f mm.\n", threshold);
			syslog.logf(LOG_INFO, "threshold set to %.3f mm.\n", threshold);
		}

		else
		{
			Serial.println("invalid threshold was trying to set.");
			syslog.log(LOG_ERR, "invalid threshold was trying to set.");
			request->send(400);
		}
	}
	
	

	else
	{
		Serial.println("Invalid request");
		syslog.log(LOG_ERR, "Invalid request");
		request->send(400);
	}


	free(body);
}





int flag = 1;
int cnt = 0;
int cnt2 = 0;
bool was_disconnected = false;

void heartbeat_task(void *pvParameters)
{
	TickType_t xLastWakeTime;
 	const TickType_t xFrequency = 1000;

	Serial1.printf("free heap: %d", ESP.getFreeHeap());

     // Initialise the xLastWakeTime variable with the current time.
    xLastWakeTime = xTaskGetTickCount();

    for( ;; )
    {
        // Wait for the next cycle.
		vTaskDelayUntil( &xLastWakeTime, xFrequency );

        heartbeat();

		if(get_erase_switch_state() == 0)  //button shorted pin to GND (pressed)
		{
			cnt++;
		}
		else
		{
			clear_credentials_flag = false;
			cnt = 0;
		}

		if(cnt==3)
		{
			clear_credentials_flag = true;
			cnt = 0;
		}
		

		setWifiStatusLED( WiFi.status() == WL_CONNECTED );		//indicate current Wifi-status

		if(WiFi.status() != WL_CONNECTED)  
		{
			cnt2++;
		}
		if(cnt2==10) //attempt to reconnect every 10s if no connection is established
		{
			WiFi.reconnect();
			cnt2=0;
			was_disconnected = true;
		}

		if( (WiFi.status() == WL_CONNECTED) && was_disconnected )
		{
			syslog.log("reconnected");
			was_disconnected = false;
		}
    }
}







void setup()
{
	//init GPIOs
	GPIO_init_custom();
	//

	clear_credentials_flag = false;

	//spawn task for heartbeat
	TaskHandle_t heartbeat_task_Handle = NULL;
	xTaskCreateUniversal(heartbeat_task, "heartbeat_task", 8192, NULL, 1, &heartbeat_task_Handle, CONFIG_ARDUINO_RUNNING_CORE);  
	//

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


	//inti serial console
	Serial.begin(115200);


	////EEPROM
	uint8_t* buff = (uint8_t*)malloc(100);
	EEPROM.begin(max_mem);

	//load ntp server address
	EEPROM.readBytes(ntp_add, buff, 100);
	getValidString(buff, ntpServer, 100);
	
	//


	//scheuduler
	scheuduler_init();
	timestamp ts;

	EEPROM.readBytes(mstart_add, &ts, sizeof(timestamp));
	Serial.println("loaded 'morgens_start' from EEPROM.");
	scheuduler_addAppointment(ts.stunden, ts.minuten, &pump_on, "morgens_an");

	EEPROM.readBytes(mstop_add, &ts, sizeof(timestamp));
	Serial.println("loaded 'morgens_stop' from EEPROM.");
	scheuduler_addAppointment(ts.stunden, ts.minuten, &pump_off, "morgens_aus");

	EEPROM.readBytes(astart_add, &ts, sizeof(timestamp));
	Serial.println("loaded 'abends_start' from EEPROM.");
	scheuduler_addAppointment(ts.stunden, ts.minuten, &pump_on, "abends_an");

	EEPROM.readBytes(astop_add, &ts, sizeof(timestamp));
	Serial.println("loaded 'abends_stop' from EEPROM.");
	scheuduler_addAppointment(ts.stunden, ts.minuten, &pump_off, "abends_aus");

	scheuduler_addAppointment(00, 01, &firstTaskOfDay, "erste_aufgabe_des_tages");
	//



	//load API-Key for Openweathermaps
	EEPROM.readBytes(apiKey_add, api_key,  API_KEY_LENGTH + 1);  //+1 to get the termianting zero
	Serial.println("API Key for waether API loaded.");
	//

	//load city for Openweathermaps
	EEPROM.readBytes(city_add, buff, 20);
	getValidString(buff, city, 20);
	//

	//load threshold
	threshold = EEPROM.readFloat(th_add);
	//

	free(buff);
	EEPROM.end();
	////

	//inti GPIOs
	GPIO_init_custom();
	Serial.println("GPIOs initilized.");


	//mounting filesystem
	if(!SPIFFS.begin())
	{
        Serial.println("An Error has occurred while mounting SPIFFS");
		syslog.log(LOG_CRIT, "An Error has occurred while mounting SPIFFS");
        return;
  	}
	else
	{
		Serial.println("Filesystem mounted.");
		syslog.log(LOG_INFO, "Filesystem mounted.");
	}
	//

	//starting mDNS service
	if(!MDNS.begin("pumpe"))
	{
     	Serial.println("Error starting mDNS");
		syslog.log(LOG_ERR, "Error starting mDNS");
     	return;
  	}
	else
	{
		Serial.println("mDNS service started.");
		syslog.log(LOG_INFO, "mDNS service started.");
	}
	//
  
	//printing device's IP
  	Serial.println(WiFi.localIP()); 
	syslog.logf(LOG_INFO, "Got IP from DHCP");
	//

	//NTP
	if(strcmp(ntpServer, "") != 0)
	{
		Serial.printf("Trying to connect to NTP server '%s'\n", ntpServer);
		syslog.logf(LOG_INFO, "Trying to connect to NTP server '%s'\n", ntpServer);
		configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
	}
	
	else
	{
		Serial.println("no valid NTP server address was loaded.");
		syslog.log(LOG_ERR, "no valid NTP server address was loaded.");

		struct tm tm;
		tm.tm_year = 0;
    	tm.tm_mon = 0;
    	tm.tm_mday = 1;
    	tm.tm_hour = 0;
    	tm.tm_min = 0;
    	tm.tm_sec = 0;

    	time_t t = mktime(&tm);

   		printf("Setting time: %s", asctime(&tm));
		syslog.logf(LOG_INFO, "Setting time: %s", asctime(&tm));

    	struct timeval now = { .tv_sec = t };

    	settimeofday(&now, NULL);	
	}

	printLocalTime();
	//

  
	//invoke index.html for website request
  	server.on("/html", HTTP_GET, [](AsyncWebServerRequest *request)
	{
    	request->send(SPIFFS, "/index.html", "text/html");
		Serial.println("Sending requested index.html");
		syslog.logf(LOG_INFO, "Sending requested index.html");
	});

	//invloke callback-function for handling HTTP-requests
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
		Serial.printf("clear_credentials_flag = %d\n", clear_credentials_flag);

		clearEEPROM();
		clear_wifi_credentials();
		clear_credentials_flag = false;
	}

}




void clearEEPROM()
{
	char* buff = (char*)malloc(max_mem);
	for(int i=0; i<max_mem; i++)
		buff[i] = 255;

	EEPROM.begin(max_mem);
	EEPROM.writeBytes(0, buff, max_mem);
	EEPROM.end();

	free(buff);
	Serial.println("EEPROM cleared!");
	syslog.log("EEPROM cleared!");
}


void clear_wifi_credentials()
{
	syslog.log(LOG_WARNING, "clearing WIFI credentials!");

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





//callback-functions
void pump_on()
{	
    Rel_switch(1, 1);
    Rel_switch(2, 1);

	Serial.println("Switching on pump!");
	syslog.log(LOG_INFO, "Switching on pump!");
}

void pump_off()
{
    Rel_switch(1, 0);
    Rel_switch(2, 0);

	Serial.println("Switching off pump!");
	syslog.log(LOG_INFO, "Switching off pump!");
}