
void clear_wifi_credentials(void);
void clearEEPROM(void);
void predict_rain(void);

//gets water flow from i2c sensor with the address "sensor_add". Return: ml/min
uint16_t get_pump_flow(uint8_t sensor_add);