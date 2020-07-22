#include <Arduino.h>


void GPIO_init_custom(void);
void Rel_switch(uint8_t nbr_, uint8_t state_);
void Rel_toggle(uint8_t nbr_);
int get_pump_state(void);
void heartbeat();
void setWifiStatusLED(bool state_);
int get_erase_switch_state(void);