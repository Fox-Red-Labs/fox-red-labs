                {
                    static const char *cfg[3] = { "homeassistant/binary_sensor/fxl_therm_presence/config", "homeassistant/sensor/fxl_therm_setpoint/config", "homeassistant/sensor/fxl_therm_temp/config" };
                    static const char *pl[3] = {
                      "{\"name\":\"Presence\",\"uniq_id\":\"fxl_therm_presence\",\"obj_id\":\"fxl_therm_presence\",\"stat_t\":\"thermostat/presence\",\"pl_on\":\"ON\",\"pl_off\":\"OFF\",\"dev_cla\":\"occupancy\",\"dev\":{\"ids\":[\"fxl_therm\"],\"name\":\"Fox Red Labs thermostat\"}}",
                      "{\"name\":\"Setpoint\",\"uniq_id\":\"fxl_therm_setpoint\",\"obj_id\":\"fxl_therm_setpoint\",\"stat_t\":\"thermostat/setpoint\",\"unit_of_meas\":\"\xC2\xB0" "C\",\"dev_cla\":\"temperature\",\"dev\":{\"ids\":[\"fxl_therm\"],\"name\":\"Fox Red Labs thermostat\"}}",
                      "{\"name\":\"Temperature\",\"uniq_id\":\"fxl_therm_temp\",\"obj_id\":\"fxl_therm_temp\",\"stat_t\":\"thermostat/temp\",\"unit_of_meas\":\"\xC2\xB0" "C\",\"dev_cla\":\"temperature\",\"dev\":{\"ids\":[\"fxl_therm\"],\"name\":\"Fox Red Labs thermostat\"}}" };
                    int ret = 0;
                    for (int k = 0; k < 3; k++) { osi_uSleep(k ? 100000 : 400000); ret |= MQTT_IF_publish(hMqttConn, (char*)cfg[k], (char*)pl[k], strlen(pl[k]), MQTT_QOS_0 | 0x100); }
                    UART_PRINT("HA discovery published\r\n");
                    ASSERT_GENERAL(ret == 0);
                }
