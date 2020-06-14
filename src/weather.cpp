#include <WString.h>
#include <Arduino.h>
#include <time.h>
#include <HTTPClient.h>


bool isHere(const char* str_, int index_, const char* term_)
{
    bool b = true;

    for (int j = strlen(term_) - 1; j > -1; j--)
    {
        b &= (str_[index_] == term_[j]);
        index_--;
    }

    return b;
}




float getRainVolumeTomorrow(const char* api_key_, const char* city_)
{
    HTTPClient http;
	char* url = (char*)malloc(200);
	struct tm timeinfo;
    float regen = 0.0f;
	String resp;

	char* buff;
	char* elemente[40];
 	char* oldbuff;
	uint32_t offset;
	int elementcnt = 0;

	uint32_t ts = 0;
    float rain = 0.0f;
    time_t t;
	uint8_t tomorrow_day = 0;
	uint8_t tomorrow_mon = 0;



    //fetch forcast in JSON-format
	sprintf(url, "http://api.openweathermap.org/data/2.5/forecast/?q=%s,de&appid=%s", city_, api_key_);
	http.begin(url);

	free(url);

	int httpCode = http.GET();
	if(  httpCode!= 200)
	{
		Serial.printf("Failed to fetch weather forecast: %d\n", httpCode);
		return -1.0f;
	}

	Serial.println("Got weather forecaste.");
	
	resp = http.getString();

	buff = (char*)malloc(resp.length() + 1);
	resp.getBytes((unsigned char*)buff, resp.length());
	buff[resp.length()] = '\0';
	//



	////"parse" JSON
	offset = (int)(strstr(buff, "\"list\":[{") - buff) + 9;
    oldbuff = buff;
    buff += offset;

	//get all parts
	for (; elementcnt < 40; elementcnt++)
    {
        for (int i = 2; i < strlen(buff) - 1; i++)
        {
            if ((isHere(buff, i, "},{")) || (isHere(buff, i, "}],\"city")) )
            {
                int cnt = i - 2;  //anzahl der zu kopierenden Zeichen

                elemente[elementcnt] = (char*)malloc(cnt + 1);

                memcpy(elemente[elementcnt], buff, cnt);
                elemente[elementcnt][cnt] = '\0';  //mit 0 terminieren

                buff += i + 1;
                break;
            }
        }
    }
	//

	//sum up rain of  tomorrow
	getLocalTime(&timeinfo);  //get get current time

	time_t tt= (time_t)((uint32_t)mktime(&timeinfo) + 86400);	//add one day. 86400 ^= one day in secounds
	timeinfo = *localtime( &tt );  								//make struct from timestamp	

	tomorrow_day = timeinfo.tm_mday;
	tomorrow_mon = timeinfo.tm_mon + 1;

	Serial.printf("> morgen: %d.%d\n", tomorrow_day, tomorrow_mon);


	for (int ei = 0; ei < elementcnt; ei++)
    {
        sscanf(elemente[ei], "\"dt\":%d,", &ts);

        t = (time_t)ts;
        timeinfo = *localtime(&t);

        if ((timeinfo.tm_mday == tomorrow_day) && (timeinfo.tm_mon + 1 == tomorrow_mon))  //wenns morgen ist
        {
            for (int i = 13; i < strlen(elemente[ei]); i++)
            {
                if (isHere(elemente[ei], i, "\"rain\":{\"3h\":"))
                {
                    sscanf(elemente[ei]+i+1, "%f", &rain);
                    regen += rain;
                }
            }
        }

        free(elemente[ei]);
    }
	////

	
	free(oldbuff);
   	return regen;
}