#pragma once

#include <stdbool.h>

void buttons_init();
void buttons_press(int id);
void buttons_hold(int id);
void buttons_release(int id);
bool led_status();
bool buttons_prog(int id);
