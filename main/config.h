#pragma once

#include <stdbool.h>

void config_setWifiSsid(char* value);
void config_setWifiPassword(char* value);
void config_setUserLogin(char* value);
void config_setUserPassword(char* value);
void config_setButton(int id, char* value);

char* config_getWifiSsid();
char* config_getWifiPassword();
char* config_getUserLogin();
char* config_getUserPassword();
char* config_getHostname();
char* config_getButton(int id);
char* config_getVersion();
bool config_getBtnInvert(int id);

void config_save();
void config_load();