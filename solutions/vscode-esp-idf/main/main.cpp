/*
 *	Author: Ilia Vasilchikov
 *	mail: gravity@hotmail.ru
 *	gihub page: https://github.com/Singular112/
 *	Licence: MIT
*/

#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include "esp_system.h"
#include "driver/timer.h"

#include "esp_wifi.h"
#include "esp_log.h"

#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

#include <time.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "../../../src/ftp_server.h"

#define DEFAULT_AP_SSID				""
#define DEFAULT_AP_ACCESS_PWD		""

//#define DEFAULT_CL_SSID			CONFIG_WIFI_SSID
//#define DEFAULT_CL_ACCESS_PWD	CONFIG_WIFI_PASSWORD
#define DEFAULT_CL_SSID				""
#define DEFAULT_CL_ACCESS_PWD		""

static const char* TAG = "MAIN";

static EventGroupHandle_t g_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int g_wifi_retry_num = 0;

RTC_DATA_ATTR sdmmc_card_t* g_sd_card;

//

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
	int32_t event_id,
	void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
        esp_wifi_connect();
    }
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
        if (g_wifi_retry_num < 5)
		{
            esp_wifi_connect();
            g_wifi_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
		else
		{
            xEventGroupSetBits(g_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    }
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        g_wifi_retry_num = 0;
        xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
    }
	else if (event_id == WIFI_EVENT_AP_STACONNECTED)
	{
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
			MAC2STR(event->mac), event->aid);
    }
	else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
	{
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
			MAC2STR(event->mac), event->aid);
    }
}


void initialize_wifi_sta()
{
    g_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
		ESP_EVENT_ANY_ID,
		&wifi_event_handler,
		NULL,
		&instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
		IP_EVENT_STA_GOT_IP,
		&wifi_event_handler,
		NULL,
		&instance_got_ip));

	wifi_config_t wifi_config;
	memset((void*)&wifi_config, 0, sizeof(wifi_config));

#if 1
	strcpy((char*)wifi_config.sta.ssid, DEFAULT_CL_SSID);
	strcpy((char*)wifi_config.sta.password, DEFAULT_CL_ACCESS_PWD);
#else
	char buf[32] = { 0 };

	strcpy((char*)wifi_config.sta.ssid,
		g_settings.getString("wifi-client-ssid", buf, sizeof(buf)) > 0 ?
			buf : DEFAULT_CL_SSID);

	strcpy((char*)wifi_config.sta.password,
		g_settings.getString("wifi-client-access-password", buf, sizeof(buf)) > 0 ?
			buf : DEFAULT_CL_ACCESS_PWD);
#endif

	wifi_config.sta.pmf_cfg =
	{
		.capable	= true,
		.required	= false
	};

	wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(g_wifi_event_group,
		WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
		pdFALSE,
		pdFALSE,
		portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
	{
        ESP_LOGI(TAG, "Connected to ap");
    }
	else if (bits & WIFI_FAIL_BIT)
	{
        ESP_LOGE(TAG, "Failed to connect to ap");
    }
	else
	{
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    //ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    //ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    //vEventGroupDelete(s_wifi_event_group);
}


void initialize_wifi_soft_ap()
{
	ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
	tcpip_adapter_ip_info_t info;
	memset(&info, 0, sizeof(info));
	IP4_ADDR(&info.ip, 192, 168, 1, 1);
	IP4_ADDR(&info.gw, 192, 168, 1, 1);
	IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
	ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

	//

	ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
		ESP_EVENT_ANY_ID,
		&wifi_event_handler,
		NULL,
		NULL));

    wifi_config_t wifi_config;
	{
#if 0
	char buf[32] = { 0 };

		strcpy((char*)wifi_config.ap.ssid,
			g_settings.getString("wifi-ap-ssid", buf, sizeof(buf)) > 0 ?
				buf : DEFAULT_AP_SSID);

		strcpy((char*)wifi_config.ap.password,
			g_settings.getString("wifi-ap-access-password", buf, sizeof(buf)) > 0 ?
				buf : DEFAULT_AP_ACCESS_PWD);
#else
		strcpy((char*)wifi_config.ap.ssid, DEFAULT_AP_SSID);
        wifi_config.ap.ssid_len = strlen((const char*)wifi_config.ap.ssid);
        wifi_config.ap.channel = 1;	// range 1 .. 13
        strcpy((char*)wifi_config.ap.password, DEFAULT_AP_ACCESS_PWD);
        wifi_config.ap.max_connection = 5;
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
#endif
    };

    if (strlen((const char*)wifi_config.ap.password) == 0)
	{
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
		wifi_config.ap.ssid,
		wifi_config.ap.password,
		wifi_config.ap.channel);
}


void initialize_wifi(void)
{
	//tcpip_adapter_init();
	initialize_wifi_sta();
}


bool initialize_sd_card_spimode(gpio_num_t miso,
	gpio_num_t mosi,
	gpio_num_t sck,
	gpio_num_t cs)
{
	ESP_LOGI(TAG, "Initializing SD card (spi-mode)");

	gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY);		// D0, needed in 4- and 1-line modes
#if 0
	gpio_set_pull_mode(GPIO_NUM_4, GPIO_PULLUP_ONLY);		// D0, needed in 4- and 1-line modes
	gpio_set_pull_mode(GPIO_NUM_12, GPIO_PULLUP_ONLY);		// D0, needed in 4- and 1-line modes
	gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLUP_ONLY);		// D0, needed in 4- and 1-line modes
#endif

	// Options for mounting the filesystem.
	// If format_if_mount_failed is set to true, SD card will be partitioned and
	// formatted in case when mounting fails.
	esp_vfs_fat_sdmmc_mount_config_t mount_config =
	{
		.format_if_mount_failed = false,
		.max_files = 5
	};

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
	//host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    host.max_freq_khz = 4000000;
	host.command_timeout_ms = 1000;

    // Set up SPI bus
    spi_bus_config_t bus_cfg = 
    {
        .mosi_io_num        = mosi,
        .miso_io_num        = miso,
        .sclk_io_num        = sck,
        .quadwp_io_num      = -1,
        .quadhd_io_num      = -1,
        .max_transfer_sz    = 4000
    };

    spi_bus_initialize(HSPI_HOST, &bus_cfg, 1);

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    {
        slot_config.host_id     = SPI2_HOST;
        slot_config.gpio_cs     = cs;
    }

    esp_err_t ret = esp_vfs_fat_sdspi_mount
    (
        "/spiflash",
        &host,
        &slot_config,
        &mount_config,
        &g_sd_card
    );

	if (ret != ESP_OK)
	{
		if (ret == ESP_FAIL)
		{
			ESP_LOGE(TAG, "Failed to mount filesystem. "
				"If you want the card to be formatted, set format_if_mount_failed = true.");
		}
		else
		{
			ESP_LOGE(TAG, "Failed to initialize the card (%d - %s). "
				"Make sure SD card lines have pull-up resistors in place.", ret, esp_err_to_name(ret));
		}

		return false;
	}

    //card_capacity = (uint64_t)g_sd_card->csd.capacity * g_sd_card->csd.sector_size;

	return true;
}


void initialize()
{
	esp_err_t err = nvs_flash_init();

	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
        // OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

	// initilize sd card
    while (true)
	{
		if (g_sd_card == 0
			&& initialize_sd_card_spimode(GPIO_NUM_2,
				GPIO_NUM_15,
				GPIO_NUM_14,
				GPIO_NUM_13))
		{
			sdmmc_card_print_info(stdout, g_sd_card);
			break;
		}

		vTaskDelay(1000 / portTICK_RATE_MS);
	}

	// wifi
	initialize_wifi();
}


void ftp_server_task(void* pvParameter)
{
	const char* basedir = "/spiflash/webroot";
	uint16_t port = 21;

	ftp_server::ftp_server_c ftp_server;

	ftp_server.set_homedir(basedir);

	ftp_server.start(port);
}


extern "C" void app_main()
{
	initialize();

	xTaskCreate(&ftp_server_task, "ftp_server_task", 8192, NULL, 5, NULL);

    while (true)
	{
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
