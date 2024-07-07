#include <esp_log.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include "camera.h"
#include "config.h"
#include <string.h>
#include "query_decoder.h"
#include <esp_camera.h>
#include "api.h"

static const char* TAG = "WiFIVorota-LocalServer";

#define PORT                        80
#define KEEPALIVE_IDLE              5
#define KEEPALIVE_INTERVAL          5
#define KEEPALIVE_COUNT             3

static void sendString(const int sock, const char* str) {
    char headers[1024];
    sprintf(headers, "HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Request-Private-Network: true\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %i\r\n\r\n", strlen(str));
    send(sock, headers, strlen(headers), 0);
    send(sock, str, strlen(str), 0);
    shutdown(sock, 0);
    close(sock);
}
static void sendOK(const int sock) {
    char response[] = "HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Request-Private-Network: true\r\nContent-Length: 0\r\n\r\n";
    send(sock, response, strlen(response), 0);
    shutdown(sock, 0);
    close(sock);
}
static void sendUnauthorized(const int sock) {
    char response[] = "HTTP/1.0 401 Unauthorized\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Request-Private-Network: true\r\nContent-Length: 0\r\n\r\n";
    send(sock, response, strlen(response), 0);
    shutdown(sock, 0);
    close(sock);
}
static void sendBadRequest(const int sock) {
    char response[] = "HTTP/1.0 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Request-Private-Network: true\r\nContent-Length: 0\r\n\r\n";
    send(sock, response, strlen(response), 0);
    shutdown(sock, 0);
    close(sock);
}
static void sendNotFound(const int sock) {
    char response[] = "HTTP/1.0 404 Not Found\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Request-Private-Network: true\r\nContent-Length: 0\r\n\r\n";
    send(sock, response, strlen(response), 0);
    shutdown(sock, 0);
    close(sock);
}
static void sendInternalError(const int sock) {
    char response[] = "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Request-Private-Network: true\r\nContent-Length: 0\r\n\r\n";
    send(sock, response, strlen(response), 0);
    shutdown(sock, 0);
    close(sock);
}

static void local_stream_task(void* pvParameters) {
    const int sock = (const int)pvParameters;

    ESP_LOGI(TAG, "stream started");

    const char headers[] =
            "--JPEG_FRAME\r\n"
            "Content-Type: image/jpeg\r\n"
            "\r\n";

    camera_fb_t* fb;

    while (1) {
        fb = esp_camera_fb_get();
        int res = send(sock, headers, strlen(headers), 0);
        if (res != strlen(headers))break;
        res = send(sock, fb->buf, fb->len, 0);
        if (res != fb->len)break;
        esp_camera_fb_return(fb);
    }

    ESP_LOGI(TAG, "stream stopped");
    esp_camera_fb_return(fb);
    shutdown(sock, 0);
    close(sock);
    vTaskDelete(NULL);
}

static void handle_request(const int sock) {
    char request[1024];

    int len = recv(sock, request, sizeof(request) - 1, 0);

    if (len < 0) {
        ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        return;
    }

    request[len] = '\0';

    ESP_LOGI(TAG, "Received %d bytes: %s", len, request);

    char* path = strchr(request, ' ') + 1;
    if (path == NULL) {
        sendBadRequest(sock);
        return;
    }
    char* query_start = strchr(path, '?');
    if (query_start == NULL) {
        sendUnauthorized(sock);
        return;
    }
    char* query_end = strchr(query_start, ' ');
    if (query_end == NULL) {
        sendBadRequest(sock);
        return;
    }
    *query_end = '\0';
    query_t query = queryDecode(query_start);

    if (path[strlen(path) - 1] == '/') {
        path[strlen(path) - 1] = '\0';
    }

    ESP_LOGI(TAG, "path: %s", path);

    if (strcmp(path, "/api") == 0) {
        char response[256];
        api_process(query, response, true);
        ESP_LOGI(TAG, "API response: %s", response);
        sendString(sock, response);
    } else if (strcmp(path, "/stream") == 0) {
        char* login = queryStr(query, "login");
        char* password = queryStr(query, "password");
        if (strcmp(login, config_getUserLogin()) != 0 || strcmp(password, config_getUserPassword()) != 0) {
            sendUnauthorized(sock);
        } else {
            char response[] =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: multipart/x-mixed-replace; boundary=JPEG_FRAME\r\n"
                "\r\n";
            send(sock, response, strlen(response), 0);
            xTaskCreate(local_stream_task, "local_stream", 2048, (void* const)sock, 5, NULL);
        }
    } else {
        sendNotFound(sock);
    }

}

static void tcp_server_task(void* pvParameters) {
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;

    if (addr_family == AF_INET) {
        struct sockaddr_in* dest_addr_ip4 = (struct sockaddr_in*)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;
    }

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1) {

        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr*)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
        }

        // Set tcp keepalive option
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
        // Convert ip address to string
        if (source_addr.ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in*)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }

        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        handle_request(sock);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

void localServer_start() {
    xTaskCreate(tcp_server_task, "tcp_server", 10240, NULL, 5, NULL);
}