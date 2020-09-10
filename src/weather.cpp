#include <WString.h>
#include <Arduino.h>
#include <time.h>
#include <HTTPClient.h>
#include <Syslog.h>


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




float getRainVolumeToday(const char* api_key_, const char* city_, Syslog* sys)
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
	uint8_t today_day = 0;
	uint8_t today_mon = 0;



    //fetch forecast in JSON-format
	sprintf(url, "http://api.openweathermap.org/data/2.5/forecast/?q=%s,de&appid=%s", city_, api_key_);
	http.begin(url);

	free(url);

	int httpCode = http.GET();
	if(  httpCode!= 200)
	{
		Serial.printf("Failed to fetch weather forecast: %d\n", httpCode);
        sys->logf("Failed to fetch weather forecast: %d. API-Key:|%s|; http-code: %d", httpCode, api_key_, httpCode);
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
	getLocalTime(&timeinfo);	

	today_day = timeinfo.tm_mday;
	today_mon = timeinfo.tm_mon + 1;


	for (int ei = 0; ei < elementcnt; ei++)
    {
        sscanf(elemente[ei], "\"dt\":%d,", &ts);

        t = (time_t)ts;
        timeinfo = *localtime(&t);

        if ((timeinfo.tm_mday == today_day) && (timeinfo.tm_mon + 1 == today_mon))  //wenns morgen ist
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