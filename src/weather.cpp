#include <WString.h>
#include <Arduino.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

typedef struct niederschlag
{
    uint64_t t;
    double mm;
};


float getRainVolumeTomorrow(const char* api_key_, const char* city_)
{
    HTTPClient http;
	char* url = (char*)malloc(200);

    int n_cnt = 0;
    niederschlag n[39];


    //fetch forcast in JSON-format
	String resp;

	sprintf(url, "http://api.openweathermap.org/data/2.5/forecast/?q=%s,de&appid=%s", city_, api_key_);
	http.begin(url);

	free(url);

	int httpCode = http.GET();
	if(  httpCode!= 200)
	{
		Serial.printf("Failed to fetch weather forecast: %d\n", httpCode);
		return -1.0f;
	}

	Serial.println("\n     >>>>>>>>  Got weather forecaste. <<<<<<<<<<\n\n");
	
	resp = http.getString();
	
	volatile int hs = ESP.getFreeHeap();

    //JSON parsing
    //StaticJsonDocument<26142> doc;

    //volatile DeserializationError error = deserializeJson(doc, resp);
    //

    

   return -2.0f;
}