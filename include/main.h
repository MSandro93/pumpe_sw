
void clear_wifi_credentials(void);
void clearEEPROM(void);
void predict_rain(void);

//returns the rotation speed of the flow sensor via uart. Return: rotation speed of sensor's fan in rotions/0.5s
uint16_t get_pump_flow();

void pump_on();
void pump_off();