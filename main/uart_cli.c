#include "uart_cli.h"
#include <stdbool.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include "query_decoder.h"
#include "api.h"

void uart_cli_task(void* pvParameters){
    char input[256];
    char response[256];
    while(true){
        int i = 0;
        int c = 0;
        while(c != '\n'){
            c = getchar();
            if(c!=-1){
                input[i] = c;
                i++;
                putchar(c);
            }else{
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
        input[i-1] = 0;
        query_t query = queryDecode(input);
        api_process(query, response, false);
        printf("response: %s\n",response);
    }
}

void uart_cli_start(){
    xTaskCreate(uart_cli_task, "uart_cli", 4096, NULL, 5, NULL);
}