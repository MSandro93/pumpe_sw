#include <stdlib.h>
#include "scheduler.h"
#include <string.h>
#include "time.h"
#include <Arduino.h>
#include "gpios.h"
#include "main.h"
#include "Syslog.h"


extern Syslog syslog;



appointment** appointment_list = nullptr;
int appointment_list_cnt;
uint32_t real_watering_time_today = 0;   //[sec.]


void scheuduler_init()
{
    appointment_list = (appointment**)malloc(sizeof(appointment*) * max_nbr_appointsments);
    appointment_list_cnt = 0;
}

int scheuduler_addAppointment(int h_, int m_, void (*fp_)(), const char* desc_)
{
    if ((appointment_list_cnt + 1) > max_nbr_appointsments)
    {
        Serial.printf("failed to add appointment \"%s\": maximum number (%d)of appointments reached.\n", desc_, max_nbr_appointsments);
        syslog.logf(LOG_DEBUG, "failed to add appointment \"%s\": maximum number (%d)of appointments reached.", desc_, max_nbr_appointsments);

        return -1;
    }

    else
    {
        appointment_list[appointment_list_cnt] = (appointment*)malloc(sizeof(appointment));

        appointment_list[appointment_list_cnt]->hour = h_;
        appointment_list[appointment_list_cnt]->min = m_;
        appointment_list[appointment_list_cnt]->func_ptr = fp_;
        appointment_list[appointment_list_cnt]->pending_today = true;
        appointment_list[appointment_list_cnt]->active = true;

        appointment_list[appointment_list_cnt]->description = (char*)malloc(strlen(desc_)+1);
        appointment_list[appointment_list_cnt]->description = strcpy(appointment_list[appointment_list_cnt]->description, desc_);

        appointment_list_cnt++;
    }

    Serial.printf("appointment \"%s\" added for %02d:%02d\n", desc_, h_, m_);
    syslog.logf(LOG_DEBUG, "appointment \"%s\" added for %02d:%02d", desc_, h_, m_);

    return 0;
}


int scheuduler_trigger(const char* desc_)
{
    for (int i = 0; i < appointment_list_cnt; i++)
    {
        if (strcmp(appointment_list[i]->description, desc_) == 0)
        {
            (*appointment_list[i]->func_ptr)();
            Serial.printf("appointment \"%s\" was triggered manually", desc_);
            syslog.logf(LOG_DEBUG, "appointment \"%s\" was triggered manually", desc_);
            return 0;
        }
    }
    return -1;
}


int scheuduler_overwrite(const char* desc_, int h_, int m_, void (*fp_)(), const char* desc_new)
{
    if( (m_<0)||(m_>59)||(h_<0)||(h_>59))
    {
        m_ = -1;
        h_ = -1;
    }
    

    for (int i = 0; i < appointment_list_cnt; i++)
    {
        if (strcmp(appointment_list[i]->description, desc_) == 0)
        {
            appointment_list[i]->hour = h_;
            appointment_list[i]->min = m_;
            appointment_list[i]->func_ptr = fp_;
            appointment_list[i]->pending_today = true;
            appointment_list[i]-> active = true;

            free(appointment_list[i]->description);
            appointment_list[i]->description = (char*)malloc(strlen(desc_new) + 1);
            appointment_list[i]->description = strcpy(appointment_list[i]->description, desc_new);

            return 0;
        }
    }
    return -1;
}


int scheuduler_setActive(const char* desc_, bool active_)
{
    for (int i = 0; i < appointment_list_cnt; i++)
    {
        if (strcmp(appointment_list[i]->description, desc_) == 0)
        {
            appointment_list[i]->active = active_;
            
            if(active_)
            {
                syslog.logf(LOG_INFO, "appointment \"%s\" was enabled.", desc_);
                Serial.printf("appointment \"%s\" was enabled.\n", desc_);
            }
            else
            {
                syslog.logf(LOG_INFO, "appointment \"%s\" was disabled.", desc_);
                Serial.printf("appointment \"%s\" was disabled.\n", desc_);
            }
            return 0;
        }
    }
    return -1;
}

int scheuduler_setPendingToday(const char* desc_, bool pending_)
{
    for (int i = 0; i < appointment_list_cnt; i++)
    {
        if (strcmp(appointment_list[i]->description, desc_) == 0)
        {
            appointment_list[i]->pending_today = pending_;
            
            if(pending_)
            {
                syslog.logf(LOG_INFO, "appointment \"%s\" was set to pending.", desc_);
                Serial.printf("appointment \"%s\" was set to pending.\n", desc_);
            }
            else
            {
                syslog.logf(LOG_INFO, "pending of appointment \"%s\" was cleared.", desc_);
                Serial.printf("pending of appointment \"%s\" was cleared.\n", desc_);
            }
            return 0;
        }
    }
    return -1;
}


appointment* scheuduler_getAppointment(const char* desc_)
{
    for (int i = 0; i < appointment_list_cnt; i++)
    {
        if (strcmp(appointment_list[i]->description, desc_) == 0)
        {
            return appointment_list[i];
        }
    }

    return nullptr;
}


bool scheudler_getActive(const char* desc_)
{
    for (int i = 0; i < appointment_list_cnt; i++)
    {
        if (strcmp(appointment_list[i]->description, desc_) == 0)
        {
            return appointment_list[i]->active;
        }
    }
    return false;
}

void scheuduler_loop()
{
    tm timeinfo;
	getLocalTime(&timeinfo);
    uint32_t currentTime = timeinfo.tm_hour * 60 + timeinfo.tm_min;  // [min]

/*
    if(real_watering_time_today >= 2.5f * 3600)  //if too much water flew today, stop! (pump was active for 2.5 hours or more)
	{
		Rel_switch(1, 0);
		return;
	}
*/
    if(get_pump_state() == 1)
    {
        real_watering_time_today += (int)(one_tick_in_mills/1000.0f);
    }


    for (int i = 0; i < appointment_list_cnt; i++)
    {
        if( (appointment_list[i]->pending_today) && (appointment_list[i]->active) )
        {
            if(currentTime == (appointment_list[i]->hour*60 + appointment_list[i]->min))
            {
                appointment_list[i]->pending_today = false;  //if this line and the following are cahnged in there order, the "erste_aufgabe_des_tages" will become immediately deactivated (today_pending=0) after it's function pointer was executed. In this function it sets all appointments to pending. But after leaving it's function, "erste_aufgabe_des_Tages" will become non-pending. So it will not be executed the next night. This will lead to all apointments stay disabled, after they were exexuted this day.
                (*appointment_list[i]->func_ptr)();
                syslog.logf("execute appointment: %s", appointment_list[i]->description);
                return;
            }
        }
    }
}


void scheuduler_print_all_appointments(char* str)
{
    for (int i = 0; i < appointment_list_cnt; i++)
    {
        sprintf(str+strlen(str), "[%d]:\nhour=%d\nmin=%d\nfunc_ptr=%d\npending=%d\ndesc=%s\nactive=%d\n\n", i, appointment_list[i]->hour, appointment_list[i]->min, (int)appointment_list[i]->func_ptr, appointment_list[i]->pending_today, appointment_list[i]->description, (int)appointment_list[i]->active);
    }

    sprintf(str+strlen(str), "\nfreeHeap: %d\n\n", ESP.getFreeHeap());

    syslog.logf("%s", str);
}


void scheuduler_reset_real_watering_time_today()
{
    real_watering_time_today = 0;
}


void scheuduler_setAllToPendingToday(void)
{
    for (int i = 0; i < appointment_list_cnt; i++)
    {
        appointment_list[i]->pending_today = true;
    }
}

void sort_appointments()
{
    appointment buff;
    buff.description = (char*)malloc(100);

    int end = appointment_list_cnt - 1;

    while (end > 1)
    {
        for (int i = 0; i < end; i++)
        {
            if ((appointment_list[i]->hour * 60 + appointment_list[i]->min) > (appointment_list[i + 1]->hour * 60 + appointment_list[i + 1]->min)) //umsortieren
            {
                memcpy(&buff, appointment_list[i], sizeof(appointment));
                memcpy(appointment_list[i], appointment_list[i + 1], sizeof(appointment));
                memcpy(appointment_list[i + 1], &buff, sizeof(appointment));
            }
        }
        end--;
    }
}





