#include <Arduino.h>
#include "main.h"

uint8_t butt_pin = 34;
uint8_t rel1_pin = 32;
uint8_t rel2_pin = 33;


//ISRs
void but1_isr(void)
{
    Serial.println(">>>> INTERRUPT TRIGGERED!!! <<<<");

    set_clear_credetial_flag();
}
//


void GPIO_init_custom()
{
    //modes
    pinMode(butt_pin, INPUT);
    pinMode(rel1_pin, OUTPUT);
    pinMode(rel2_pin, OUTPUT);

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