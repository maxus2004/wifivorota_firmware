#include <esp_log.h>
#include <errno.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "camera.h"
#include "config.h"
#include "query_decoder.h"
#include <esp_camera.h>
#include "api.h"
#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/ssl_cache.h"

static const char* TAG = "WiFIVorota-LocalServerSsl";

static void sendString(mbedtls_ssl_context* ssl, mbedtls_net_context* fd, const char* str) {
    char headers[1024];
    sprintf(headers, "HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Request-Private-Network: true\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %i\r\n\r\n", strlen(str));
    mbedtls_ssl_write(ssl, (unsigned char*)headers, strlen(headers));
    mbedtls_ssl_write(ssl, (unsigned char*)str, strlen(str));
    mbedtls_net_free(fd);
    mbedtls_ssl_free(ssl);
}
static void sendOK(mbedtls_ssl_context* ssl, mbedtls_net_context* fd) {
    char response[] = "HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Request-Private-Network: true\r\nContent-Length: 0\r\n\r\n";
    mbedtls_ssl_write(ssl, (unsigned char*)response, strlen(response));
    mbedtls_net_free(fd);
    mbedtls_ssl_free(ssl);
}
static void sendUnauthorized(mbedtls_ssl_context* ssl, mbedtls_net_context* fd) {
    char response[] = "HTTP/1.0 401 Unauthorized\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Request-Private-Network: true\r\nContent-Length: 0\r\n\r\n";
    mbedtls_ssl_write(ssl, (unsigned char*)response, strlen(response));
    mbedtls_net_free(fd);
    mbedtls_ssl_free(ssl);
}
static void sendBadRequest(mbedtls_ssl_context* ssl, mbedtls_net_context* fd) {
    char response[] = "HTTP/1.0 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Request-Private-Network: true\r\nContent-Length: 0\r\n\r\n";
    mbedtls_ssl_write(ssl, (unsigned char*)response, strlen(response));
    mbedtls_net_free(fd);
    mbedtls_ssl_free(ssl);
}
static void sendNotFound(mbedtls_ssl_context* ssl, mbedtls_net_context* fd) {
    char response[] = "HTTP/1.0 404 Not Found\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Request-Private-Network: true\r\nContent-Length: 0\r\n\r\n";
    mbedtls_ssl_write(ssl, (unsigned char*)response, strlen(response));
    mbedtls_net_free(fd);
    mbedtls_ssl_free(ssl);
}
static void sendInternalError(mbedtls_ssl_context* ssl, mbedtls_net_context* fd) {
    char response[] = "HTTP/1.0 500 Internal Server Error\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Request-Private-Network: true\r\nContent-Length: 0\r\n\r\n";
    mbedtls_ssl_write(ssl, (unsigned char*)response, strlen(response));
    mbedtls_net_free(fd);
    mbedtls_ssl_free(ssl);
}

static void local_stream_task(void* pvParameters) {
    mbedtls_ssl_context* ssl = ((mbedtls_ssl_context**)pvParameters)[0];
    mbedtls_net_context* fd = ((mbedtls_net_context**)pvParameters)[1];
    free(pvParameters);

    ESP_LOGI(TAG, "stream started");

    const char headers[] =
        "--JPEG_FRAME\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n";

    camera_fb_t* fb;

    while (1) {
        fb = esp_camera_fb_get();
        int res = mbedtls_ssl_write(ssl, (unsigned char*)headers, strlen(headers));
        if (res != strlen(headers))break;
        size_t sent = 0;
        while (sent < fb->len) {
            res = mbedtls_ssl_write(ssl, fb->buf + sent, fb->len - sent);
            if (res < 0)break;
            sent += res;
        }
        esp_camera_fb_return(fb);
    }

    ESP_LOGI(TAG, "stream stopped");
    esp_camera_fb_return(fb);
    mbedtls_net_free(fd);
    mbedtls_ssl_free(ssl);
    vTaskDelete(NULL);
}

static void handle_request(mbedtls_ssl_context* ssl, mbedtls_net_context* fd) {
    char request[1024];

    int len = mbedtls_ssl_read(ssl, (unsigned char*)request, sizeof(request) - 1);

    if (len < 0) {
        ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        mbedtls_net_free(fd);
        mbedtls_ssl_free(ssl);
        return;
    }

    request[len] = '\0';

    ESP_LOGI(TAG, "Received %d bytes: %s", len, request);

    char* path = strchr(request, ' ') + 1;
    if (path == NULL) {
        sendBadRequest(ssl, fd);
        return;
    }
    char* query_start = strchr(path, '?');
    if (query_start == NULL) {
        sendUnauthorized(ssl, fd);
        return;
    }
    char* query_end = strchr(query_start, ' ');
    if (query_end == NULL) {
        sendBadRequest(ssl, fd);
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
        api_process(query, response);
        ESP_LOGI(TAG, "API response: %s", response);
        sendString(ssl, fd, response);
    } else if (strcmp(path, "/stream") == 0) {
        char* login = queryStr(query, "login");
        char* password = queryStr(query, "password");
        if (strcmp(login, config_getUserLogin()) != 0 || strcmp(password, config_getUserPassword()) != 0) {
            sendUnauthorized(ssl, fd);
        } else {
            char response[] =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: multipart/x-mixed-replace; boundary=JPEG_FRAME\r\n"
                "\r\n";
            mbedtls_ssl_write(ssl, (unsigned char*)response, strlen(response));
            void* parameters =  malloc(sizeof(void*)*2);
            ((mbedtls_ssl_context**)parameters)[0] = ssl;
            ((mbedtls_net_context**)parameters)[1] = fd;
            xTaskCreate(local_stream_task, "ssl_stream", 2048, parameters, 5, NULL);
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    } else {
        sendNotFound(ssl, fd);
    }

}

static void tcp_server_task(void* pvParameters) {
    int ret;
    mbedtls_net_context listen_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt srvcert;
    mbedtls_pk_context pkey;

#if defined(MBEDTLS_SSL_CACHE_C)
    mbedtls_ssl_cache_context cache;
#endif

    const char cacert_pem_start[] = "-----BEGIN CERTIFICATE-----\n\
MIIDwDCCAqigAwIBAgIUULIdY8KP+Hsf5ca6DJiooGRPf9swDQYJKoZIhvcNAQEL\n\
BQAwZDELMAkGA1UEBhMCUlUxDzANBgNVBAgMBk1vc2NvdzEPMA0GA1UEBwwGTW9z\n\
Y293MRgwFgYDVQQKDA9NYWtzaW0gRG9yb3NoaW4xGTAXBgNVBAMMEHdpZml2b3Jv\n\
dGEubG9jYWwwHhcNMjMxMDAzMTc1MTQ1WhcNMzMwOTMwMTc1MTQ1WjBkMQswCQYD\n\
VQQGEwJSVTEPMA0GA1UECAwGTW9zY293MQ8wDQYDVQQHDAZNb3Njb3cxGDAWBgNV\n\
BAoMD01ha3NpbSBEb3Jvc2hpbjEZMBcGA1UEAwwQd2lmaXZvcm90YS5sb2NhbDCC\n\
ASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAImMVotq/rHCcDn5dmaHwmei\n\
S3TMj6PPG4ixvXOXdUZG+OUXPKGQbNAW9FlvRQzZr2YdI9KFXaCXlqy4hMenoi7G\n\
EjbaCUCRIc/RvSf/fEUSZ928aAkRWdHdHJFL4RLtgY/G5uZmVUCcpb0GuVKZHNSZ\n\
EJwwWCnTZM+Fkz0hBEPQPuvQTK1rgN6mnoCftOugdUzqL13rgUubqC7RjXw7QR/i\n\
7Y1CCrkn/WE+F7+lz1NMGNx1PiB2AKQZU5rT7V5RAv+cPOtvoIWHKlRf/zhaBObh\n\
ppEV7CztUwo9VG8RW0CSoiypTleqBcURj56C1NAFeg5XVnGh9QfUIvCFTF/3ovMC\n\
AwEAAaNqMGgwHwYDVR0jBBgwFoAUPNnuQao5atwlD02RpWyxq6jhaK0wCQYDVR0T\n\
BAIwADAbBgNVHREEFDASghB3aWZpdm9yb3RhLmxvY2FsMB0GA1UdDgQWBBS8SHuV\n\
JvCoyGmaqvJ9rkNmRh5loTANBgkqhkiG9w0BAQsFAAOCAQEATc1qpqxEs52mK5nw\n\
mkMI19PIP0wV32aYJnq1ATPqes7DfCuVwYhnXA9Yidthzhhpq9RRdqb8dMGrl7Jp\n\
co+XKDrYuU9MQGw5ECjAqjQe4Ae3TfA42AhiDb8iUnW3AoCRCcIM3ZguMJywvFo3\n\
3EcPwmMDc42owLWdQJ5D3l4hSEdwcVpceaqFOAcKVOIpgBSx/OAXAXG7+Uhuf0vX\n\
wWit4yOvhft0K+IeE7tOS7AKdngqF2sP8cgIpYLZ7rp9M26rGyT9+lqQux8Mvcsd\n\
R4KLuySpZEDFF+RTFv6mk4lXAFzVqkGpchLPb7SMqhBa1YpeBtIZboy0v1bWRskM\n\
WOc8wA==\n\
-----END CERTIFICATE-----";

    const unsigned int cacert_pem_bytes = strlen(cacert_pem_start) + 1;

    const char prvtkey_pem_start[] = "-----BEGIN PRIVATE KEY-----\n\
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCJjFaLav6xwnA5\n\
+XZmh8Jnokt0zI+jzxuIsb1zl3VGRvjlFzyhkGzQFvRZb0UM2a9mHSPShV2gl5as\n\
uITHp6IuxhI22glAkSHP0b0n/3xFEmfdvGgJEVnR3RyRS+ES7YGPxubmZlVAnKW9\n\
BrlSmRzUmRCcMFgp02TPhZM9IQRD0D7r0Eyta4Depp6An7TroHVM6i9d64FLm6gu\n\
0Y18O0Ef4u2NQgq5J/1hPhe/pc9TTBjcdT4gdgCkGVOa0+1eUQL/nDzrb6CFhypU\n\
X/84WgTm4aaRFews7VMKPVRvEVtAkqIsqU5XqgXFEY+egtTQBXoOV1ZxofUH1CLw\n\
hUxf96LzAgMBAAECggEANT8IeawPsIyOmULC3OoNDcU75JkTudwA0qfLqSWmlm//\n\
BeA67mhUVzGrobt7RYA3cBYzudk652IHSBGeBRQVqnmur2E0V7RXHHYa/ZLfbnWz\n\
k5jNxUTcOmuYFX8EQCmFED11QRe4ROzfFA+4SgtPp9UhIvVsC7Tzv4n795wCZKV1\n\
jQlj4kPL3J65uW1Fs8TF3Q+wyef47MUdig6d1Nfqz6W0/A4g6DwVafBGAo4WMHj6\n\
se7H1l0JP7Hvx6h/7htqIV5NRll8ReB920igrAKIuhaIlq/ctwFSYFzr0YYKvFDG\n\
zsUuSy1dv7b7j6eUyczphddrK9jnW6O+DG86VZ4/4QKBgQC8fManohsvK10P2hWD\n\
0fIqC2A2GWJcFYxFNATVoOsLZpzbo2fXxrDUF2EkOzuOHKgs98B3zakIsEuDcrJt\n\
jrdokB2ZeNnT9BknCIm7udN43fLe/AR2TMEFeJJo9TTEfWGe5ywbiMvZkVeNIKqZ\n\
BEnq3NczUS/lRMujjMSJRjTq+QKBgQC60LnWp0IMt2X9PhyTX3Feyw8XV4nxuJzV\n\
mQUGcrx8cYYefSUAXnxuoZ1XGor7OA+34H5ZFzJIXhZE8zuZDTheBmLSdqdzky2E\n\
HahALZYZcGjo/YCBDCCqJNY/TgJTSu+YAyc3H45i9pcK9hf2MluQS7ThhlHw4QLz\n\
bjGrN7QsSwKBgBR69lbmnU+NxangR3AwUsDQxZ57OZ3J1Zj7Yv8XYhK2Dpsq8TCX\n\
7UTOWYbHTNxPLtLcBLS/yvsftMTOpKaU2EbrSdwQLpMCNe1w1w5nzZuXejlSZuW3\n\
x01h4X64Dgi/mujaM4e2YHf+e+Xgw8iml6WGY3e2/Z5K0FmwBPbpgvx5AoGBALjx\n\
SgknLExKg71hcAi1xBaEEDybfQTALwOGqWLo05CfEpe8bJUg9S7Q8GL68/wgU+9F\n\
X8/zFuRtwL4hzi6G1/a9e7e/n6bbXYQdmCNw/dfRYQrHbCBVUUEflrq1D7hFx0xG\n\
UtxPcTanyIAhgTdKQztmt3tM7nH5UGKOBL3sN4JhAoGAaETaDvtGVOUEmOuE0IUX\n\
YfxWGjaQi/ZRD1tjUl/53TQ3e5UcRX3El+SIU7hXEn2ZuEbyvPg3+NJbpcNRreCv\n\
iT8bPJRqKNdI0A63Lx32Flt1MdvZoubYFgtzF2oixVkmU1AWS6yKGMS7KcY2dVo9\n\
YOdvHWt6w/f5EmKmluv8rtQ=\n\
-----END PRIVATE KEY-----";
    const unsigned int prvtkey_pem_bytes = strlen(prvtkey_pem_start) + 1;

    mbedtls_net_init(&listen_fd);
    ESP_LOGI(TAG, "SSL server context create ......");
    ESP_LOGI(TAG, "OK");
    mbedtls_ssl_config_init(&conf);
    mbedtls_ssl_cache_init(&cache);
    mbedtls_x509_crt_init(&srvcert);
    mbedtls_pk_init(&pkey);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    /*
     * 1. Load the certificates and private RSA key
     */
    mbedtls_printf("\n  . Loading the server cert. and key...");

    /*
     * This demonstration program uses embedded test certificates.
     * Instead, you may want to use mbedtls_x509_crt_parse_file() to read the
     * server and CA certificates, as well as mbedtls_pk_parse_keyfile().
     */
    ESP_LOGI(TAG, "SSL server context set own certification......");
    ESP_LOGI(TAG, "Parsing test srv_crt......");
    ret = mbedtls_x509_crt_parse(&srvcert, (const unsigned char*)cacert_pem_start,
        cacert_pem_bytes);
    if (ret != 0) {
        ESP_LOGI(TAG, " failed\n  !  mbedtls_x509_crt_parse returned %d\n\n", ret);
        goto exit;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server context set private key......");
    ret = mbedtls_pk_parse_key(&pkey, (const unsigned char*)prvtkey_pem_start,
        prvtkey_pem_bytes, NULL, 0, time, 0);
    if (ret != 0) {
        ESP_LOGI(TAG, " failed\n  !  mbedtls_pk_parse_key returned %d\n\n", ret);
        goto exit;
    }
    ESP_LOGI(TAG, "OK");

    /*
     * 2. Setup the listening TCP socket
     */
    ESP_LOGI(TAG, "SSL server socket bind at localhost:443 ......");
    if ((ret = mbedtls_net_bind(&listen_fd, NULL, "443", MBEDTLS_NET_PROTO_TCP)) != 0) {
        ESP_LOGI(TAG, " failed\n  ! mbedtls_net_bind returned %d\n\n", ret);
        goto exit;
    }
    ESP_LOGI(TAG, "OK");

    /*
     * 3. Seed the RNG
     */
    ESP_LOGI(TAG, "  . Seeding the random number generator...");
    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
        (const unsigned char*)TAG,
        strlen(TAG))) != 0) {
        ESP_LOGI(TAG, " failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret);
        goto exit;
    }
    ESP_LOGI(TAG, "OK");

    /*
     * 4. Setup stuff
     */
    ESP_LOGI(TAG, "  . Setting up the SSL data....");

    if ((ret = mbedtls_ssl_config_defaults(&conf,
        MBEDTLS_SSL_IS_SERVER,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        ESP_LOGI(TAG, " failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret);
        goto exit;
    }

    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    mbedtls_ssl_conf_session_cache(&conf, &cache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);

    mbedtls_ssl_conf_ca_chain(&conf, srvcert.next, NULL);
    if ((ret = mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey)) != 0) {
        ESP_LOGI(TAG, " failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n\n", ret);
        goto exit;
    }

    while (true) {
    reset:

        if (ret != 0) {
            char error_buf[100];
            mbedtls_strerror(ret, error_buf, 100);
            ESP_LOGI(TAG, "Last error was: %d - %s\n\n", ret, error_buf);
        }

        mbedtls_ssl_context* ssl = (mbedtls_ssl_context*)malloc(sizeof(mbedtls_ssl_context));
        mbedtls_ssl_init(ssl);

        if ((ret = mbedtls_ssl_setup(ssl, &conf)) != 0) {
            ESP_LOGI(TAG, " failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret);
            goto exit;
        }

        ESP_LOGI(TAG, "OK");

        if (ret != 0) {
            char error_buf[100];
            mbedtls_strerror(ret, error_buf, 100);
            ESP_LOGI(TAG, "Last error was: %d - %s\n\n", ret, error_buf);
        }

        /*
         * 3. Wait until a client connects
         */
        mbedtls_net_context* client_fd = (mbedtls_net_context*)malloc(sizeof(mbedtls_net_context));
        mbedtls_net_free(client_fd);
        mbedtls_ssl_session_reset(ssl);
        ESP_LOGI(TAG, "  . Waiting for a remote connection ...");
        if ((ret = mbedtls_net_accept(&listen_fd, client_fd,
            NULL, 0, NULL)) != 0) {
            ESP_LOGI(TAG, " failed\n  ! mbedtls_net_accept returned %d\n\n", ret);
            goto exit;
        }
        mbedtls_ssl_set_bio(ssl, client_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
        ESP_LOGI(TAG, "OK");

        /*
         * 5. Handshake
         */
        ESP_LOGI(TAG, "  . Performing the SSL/TLS handshake...");
        while ((ret = mbedtls_ssl_handshake(ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                ESP_LOGI(TAG, " failed\n  ! mbedtls_ssl_handshake returned %d\n\n", ret);
                mbedtls_net_free(client_fd);
                mbedtls_ssl_free(ssl);
                goto reset;
            }
        }
        ESP_LOGI(TAG, "OK");

        /*
         * 6. Read the HTTP Request
         */

        ESP_LOGI(TAG, "Processing request...");
        handle_request(ssl, client_fd);
        ESP_LOGI(TAG, "OK");

        ret = 0;
    }

exit:

    if (ret != 0) {
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, 100);
        ESP_LOGI(TAG, "Last error was: %d - %s\n\n", ret, error_buf);
    }

    mbedtls_net_free(&listen_fd);

    mbedtls_x509_crt_free(&srvcert);
    mbedtls_pk_free(&pkey);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ssl_cache_free(&cache);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    vTaskDelete(NULL);
}

void localServerSsl_start() {
    xTaskCreate(tcp_server_task, "ssl_server", 10240, NULL, 5, NULL);
}