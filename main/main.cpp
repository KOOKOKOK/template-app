#include <stdio.h>
#include "asio.hpp"
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi_types.h>
#include <string.h>
#include <ping/ping_sock.h>
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "coap_config_posix.h"
#include <freertos/task.h>

#define ROUTER_IP "192.168.2.1"
static const char *TAG = "wifi station";

const asio::ip::udp::endpoint ep(asio::ip::address::from_string("192.168.2.105"), 8001);
asio::io_service service;

extern "C" void asio_send_msg(void *params)
{
    std::string s = "hello world!";
    for (;;)
    {
        asio::ip::udp::socket sock(service, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));
        sock.send_to(asio::buffer(s.c_str(), s.size()), ep);
        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms
    }
}

extern "C" void wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info)
{
    static uint32_t s_count = 0;
    const wifi_pkt_rx_ctrl_t *rx_ctrl = &info->rx_ctrl;

    if (!s_count)
    {
        ESP_LOGI(TAG, "================ CSI RECV ================");
        ets_printf("type,id,mac,rssi,rate,sig_mode,mcs,bandwidth,smoothing,not_sounding,aggregation,stbc,fec_coding,sgi,noise_floor,ampdu_cnt,channel,secondary_channel,local_timestamp,ant,sig_len,rx_state,len,first_word,data\n");
    }

    ets_printf("CSI_DATA,%d," MACSTR ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
               s_count++, MAC2STR(info->mac), rx_ctrl->rssi, rx_ctrl->rate, rx_ctrl->sig_mode,
               rx_ctrl->mcs, rx_ctrl->cwb, rx_ctrl->smoothing, rx_ctrl->not_sounding,
               rx_ctrl->aggregation, rx_ctrl->stbc, rx_ctrl->fec_coding, rx_ctrl->sgi,
               rx_ctrl->noise_floor, rx_ctrl->ampdu_cnt, rx_ctrl->channel, rx_ctrl->secondary_channel,
               rx_ctrl->timestamp, rx_ctrl->ant, rx_ctrl->sig_len, rx_ctrl->rx_state);

    ets_printf(",%d,%d,\"[%d", info->len, info->first_word_invalid, info->buf[0]);

    for (int i = 1; i < info->len; i++)
    {
        ets_printf(",%d", info->buf[i]);
    }

    ets_printf("]\"\n");
    // asio_send_msg(NULL);
}

extern "C" void wifi_init()
{
    // 创建LwIP核心任务并初始化与LwIP相关的工作。
    esp_netif_init();
    // // 创建系统事件任务并初始化应用程序事件的回调函数。
    esp_event_loop_create_default();
    // 创建具有TCP / IP堆栈的默认网络接口实例绑定基站
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&config);

    esp_wifi_set_mode(WIFI_MODE_STA);
    // esp_wifi_set_storage(WIFI_STORAGE_RAM);
    // esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT40);
    // uint8_t a = 11;
    // esp_wifi_set_channel(a, WIFI_SECOND_CHAN_BELOW);

    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.sta.ssid, "pan(1)");
    strcpy((char *)wifi_config.sta.password, "panning2011");
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

    esp_wifi_start();
    esp_wifi_connect();
}

// 开启csi  不要忘记在设置里打开
void csi_init()
{
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    wifi_csi_config_t wifi_csi_cfg = {
        .lltf_en = true,
        .htltf_en = true,
        .stbc_htltf2_en = true,
        .ltf_merge_en = true,
        .channel_filter_en = true,
        .manu_scale = false,
        .shift = false,
    };
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&wifi_csi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(wifi_csi_rx_cb, NULL));
    (esp_wifi_set_csi(true));
}
// 下面3个ping回调函数
static void on_ping_success_cb(esp_ping_handle_t hdl, void *args)
{
    printf("on_ping_success_cb has callbacked\n");
    // const portTickType xDelay = pdMS_TO_TICKS(500);
    // vTaskDelay(xDelay);
}

static void on_ping_timeout_cb(esp_ping_handle_t hdl, void *args)
{
    printf("on_ping_timeout_cb has callbacked\n");
    // const portTickType xDelay = pdMS_TO_TICKS(500);
    // vTaskDelay(xDelay);
}

static void on_ping_end_cb(esp_ping_handle_t hdl, void *args)
{
    printf("on_ping_end_cb has callbacked\n");
    // const portTickType xDelay = pdMS_TO_TICKS(500);
    // vTaskDelay(xDelay);
}

extern "C" void ping_work_start()
{
    struct addrinfo hint;
    struct addrinfo *res = NULL;
    in_addr addr;
    ip_addr_t target_addr;

    memset(&target_addr, 0, sizeof(target_addr));
    memset(&hint, 0, sizeof(hint));

    int err;

    if ((err = getaddrinfo(ROUTER_IP, NULL, &hint, &res)) != 0)
    {
        printf("error %d : %s\n", err, gai_strerror(err));
    }
    else
    {
        addr.s_addr = ((sockaddr_in *)(res->ai_addr))->sin_addr.s_addr;
        // printf("ip addresss: %s\n", inet_ntoa(addr)); //返回之前设置的地址
        struct in_addr addr4 = ((struct sockaddr_in *)(res->ai_addr))->sin_addr;
        inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
        freeaddrinfo(res);
    }
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.interval_ms = 1;
    ping_config.target_addr = target_addr;               // target IP address
    printf("ip addresss: %s\n", inet_ntoa(target_addr)); //返回之前设置的地址

    ping_config.count = ESP_PING_COUNT_INFINITE; // ping in infinite mode, esp_ping_stop can stop it

    // 创建必要ping回调函数
    esp_ping_callbacks_t cbs;
    // cb : callback
    cbs.on_ping_success = on_ping_success_cb;
    cbs.on_ping_timeout = on_ping_timeout_cb;
    cbs.on_ping_end = on_ping_end_cb;

    // 为什么 不行
    cbs.cb_args = NULL;
    // strcpy((char *)cbs.cb_args, "foo");

    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, &cbs, &ping);
    esp_ping_start(ping);
}

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

    wifi_init();
    csi_init();
    // 等待WIFI freertos 中task创建完成 给1秒时间 不然一开始ping不到
    vTaskDelay(pdMS_TO_TICKS(1000));
    ping_work_start();
}