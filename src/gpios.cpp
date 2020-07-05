#include <Arduino.h>
#include "main.h"

#define butt_pin 34
#define rel1_pin 32
#define rel2_pin 33
#define heartbeat_pin 7 //pin 31 at main-connector
#define WifiStatus_pin 6 //pin 32 at main-connector

extern bool clear_credentials_flag;

//ISRs
void but1_isr(void)
{
    Serial.println(">>>> INTERRUPT TRIGGERED!!! <<<<");

    clear_credentials_flag = true;
}
//


void GPIO_init_custom()
{
    //modes
    pinMode(butt_pin, INPUT);
    pinMode(rel1_pin, OUTPUT);
    pinMode(rel2_pin, OUTPUT);
    pinMode(heartbeat_pin, OUTPUT);
    pinMode(WifiStatus_pin, OUTPUT);

    //ext-interrupts
    attachInterrupt(digitalPinToInterrupt(butt_pin), but1_isr, FALLING);

}



void Rel_switch(uint8_t nbr_, uint8_t state_)
{
    if(state_>1)
        return;

    switch(nbr_)
    {
        case 1: 
        {
            digitalWrite(rel1_pin, state_);
            break;
        }

        case 2: 
        {
            digitalWrite(rel2_pin, state_);
            break;
        }
    }
}



void Rel_toggle(uint8_t nbr_)
{
    switch(nbr_)
    {
        case 1: 
        {
            uint8_t state = (uint8_t)(digitalRead(rel1_pin) & 1);
            state ^= 1;
            digitalWrite(rel1_pin, state);
            break;
        }

        case 2: 
        {
            uint8_t state = (uint8_t)(digitalRead(rel2_pin) & 1);
            state ^= 1;
            digitalWrite(rel2_pin, state);
            break;
        }
    }
}


int get_pump_state()
{
    int resp = digitalRead(rel1_pin) + digitalRead(rel2_pin);

    if(resp == 0)
    {
        return 0;
    }

    if(resp == 2)
    {
        return 1;
    }

    else
    {
        return -1;
    }
}


void heartbeat()
{
    if(digitalRead(heartbeat_pin) > 0)
    {
        digitalWrite(heartbeat_pin, 0);
        Serial.println("heartbeat -> 1");
    }
    else
    {
        digitalWrite(heartbeat_pin, 1);
        Serial.println("heartbeat -> 0");
    }
}


void setWifiStatusLED(bool state_)
{
    digitalWrite(WifiStatus_pin, (state_ & 1));

    Serial.printf("wifistate -> %d\n", state_);
}