/*******************************************************************************
 *
 * Copyright (c) 2013, 2014 Intel Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v20.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Julien Vermillard - initial implementation
 *    Fabien Fleutot - Please refer to git log
 *    David Navarro, Intel Corporation - Please refer to git log
 *    Bosch Software Innovations GmbH - Please refer to git log
 *    Pascal Rieux - Please refer to git log
 *    Gregory Lemercier - Please refer to git log
 *    Scott Bertin, AMETEK, Inc. - Please refer to git log
 *
 *******************************************************************************/

/*
 * This object is single instance only, and provide firmware upgrade functionality.
 * Object ID is 5.
 */

/*
 * resources:
 * 0 package                   write
 * 1 package url               write
 * 2 update                    exec
 * 3 state                     read
 * 5 update result             read
 * 6 package name              read
 * 7 package version           read
 * 8 update protocol support   read
 * 9 update delivery method    read
 * 10 package resume pointer   read/write
 */

#include "liblwm2m.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_app_format.h"
#include "esp_idf_version.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "sys/time.h"

#include "er-coap-13/er-coap-13.h"
#endif

// ---- private object "Firmware" specific defines ----
// Resource Id's:
#define RES_M_PACKAGE                   0
#define RES_M_PACKAGE_URI               1
#define RES_M_UPDATE                    2
#define RES_M_STATE                     3
#define RES_M_UPDATE_RESULT             5
#define RES_O_PKG_NAME                  6
#define RES_O_PKG_VERSION               7
#define RES_O_UPDATE_PROTOCOL           8
#define RES_M_UPDATE_METHOD             9
#define RES_O_PACKAGE_POINTER           10

#define LWM2M_FIRMWARE_PROTOCOL_NUM     4
#define LWM2M_FIRMWARE_PROTOCOL_NULL    ((uint8_t)-1)

/* LwM2M Firmware Update States (Resource 3) */
#define FW_STATE_IDLE           0  // Idle
#define FW_STATE_DOWNLOADING    1  // Downloading
#define FW_STATE_DOWNLOADED     2  // Downloaded
#define FW_STATE_UPDATING       3  // Updating

/* LwM2M Firmware Update Results (Resource 5) */
#define FW_RESULT_INITIAL       0  // Initial value
#define FW_RESULT_SUCCESS       1  // Firmware updated successfully
#define FW_RESULT_NOT_ENOUGH_STORAGE 2  // Not enough storage
#define FW_RESULT_OUT_OF_MEMORY 3  // Out of memory
#define FW_RESULT_CONNECTION_LOST 4  // Connection lost during download
#define FW_RESULT_CRC_FAILED    5  // CRC check failure
#define FW_RESULT_UNSUPPORTED_PKG 6  // Unsupported package type
#define FW_RESULT_INVALID_URI   7  // Invalid URI
#define FW_RESULT_UPDATE_FAILED 8  // Firmware update failed
#define FW_RESULT_UNSUPPORTED_PROTOCOL 9  // Unsupported protocol

typedef struct firmware_data_s
{
    uint8_t state;
    uint8_t result;
    char pkg_name[256];
    char pkg_version[256];
    uint8_t protocol_support[LWM2M_FIRMWARE_PROTOCOL_NUM];
    uint8_t delivery_method;
    char package_uri[512];  // Store firmware URL
    lwm2m_context_t *lwm2mH;  // LwM2M context for notifications
    size_t package_bytes_written;
#ifdef ESP_PLATFORM
    esp_ota_handle_t package_ota_handle;
    const esp_partition_t *package_partition;
    bool package_ota_active;
#endif
} firmware_data_t;

#ifdef ESP_PLATFORM
#define FW_TAG "FW_OTA"

/* OTA throughput tuning for high-latency links (e.g. HaLow mesh). */
#define OTA_HTTP_RX_BUFFER_SIZE        (16 * 1024)
#define OTA_HTTP_TX_BUFFER_SIZE        (16 * 1024)
#define OTA_HTTP_REQUEST_CHUNK_SIZE    (64 * 1024)

#define OTA_COAP_BLOCK_SIZE            1024
#define OTA_COAP_RX_BUFFER_SIZE        1536
#define OTA_COAP_TX_BUFFER_SIZE        512
#define OTA_COAP_RECV_TIMEOUT_MS       8000
#define OTA_COAP_MAX_RETRIES           4
#define OTA_NVS_NAMESPACE               "fw_ota"
#define OTA_NVS_KEY_PACKAGE_POINTER     "pkg_ptr"

static void ota_log_heap(const char *stage)
{
    ESP_LOGI(FW_TAG,
             "heap %s: free8=%u min8=%u",
             stage,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
}

typedef struct
{
    bool is_coaps;
    char host[128];
    char port[8];
    char path_query[384];
} ota_coap_uri_t;

static bool ota_str_starts_with(const char *value, const char *prefix)
{
    if (value == NULL || prefix == NULL) return false;
    return strncmp(value, prefix, strlen(prefix)) == 0;
}

static void ota_notify_resource_changed(firmware_data_t *data, uint16_t resource_id)
{
    if (data->lwm2mH == NULL) return;

    lwm2m_uri_t uri = {
        .objectId = LWM2M_FIRMWARE_UPDATE_OBJECT_ID,
        .instanceId = 0,
        .resourceId = resource_id,
    };
    lwm2m_resource_value_changed(data->lwm2mH, &uri);
}

static void ota_notify_state_and_result(firmware_data_t *data)
{
    ota_notify_resource_changed(data, RES_M_STATE);
    ota_notify_resource_changed(data, RES_M_UPDATE_RESULT);
}

static uint32_t ota_nvs_load_package_pointer(void)
{
    nvs_handle_t nvs_handle;
    uint32_t pointer = 0;

    esp_err_t err = nvs_open(OTA_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGW(FW_TAG, "NVS open failed for pointer load: %s", esp_err_to_name(err));
        return 0;
    }

    err = nvs_get_u32(nvs_handle, OTA_NVS_KEY_PACKAGE_POINTER, &pointer);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(FW_TAG, "NVS get pointer failed: %s", esp_err_to_name(err));
        pointer = 0;
    }

    nvs_close(nvs_handle);
    return pointer;
}

static void ota_nvs_save_package_pointer(uint32_t pointer)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(OTA_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGW(FW_TAG, "NVS open failed for pointer save: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_u32(nvs_handle, OTA_NVS_KEY_PACKAGE_POINTER, pointer);
    if (err == ESP_OK)
    {
        err = nvs_commit(nvs_handle);
    }

    if (err != ESP_OK)
    {
        ESP_LOGW(FW_TAG, "NVS save pointer failed: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
}

static bool ota_parse_coap_uri(const char *uri, ota_coap_uri_t *out)
{
    const char *p;
    const char *host_start;
    const char *host_end;
    const char *authority_end;
    const char *path_start;
    size_t host_len;

    if (uri == NULL || out == NULL) return false;
    memset(out, 0, sizeof(*out));

    if (ota_str_starts_with(uri, "coap://"))
    {
        out->is_coaps = false;
        p = uri + strlen("coap://");
    }
    else if (ota_str_starts_with(uri, "coaps://"))
    {
        out->is_coaps = true;
        p = uri + strlen("coaps://");
    }
    else
    {
        return false;
    }

    host_start = p;
    authority_end = strpbrk(p, "/?");
    if (authority_end == NULL)
    {
        authority_end = uri + strlen(uri);
    }

    if (host_start >= authority_end)
    {
        return false;
    }

    if (*host_start == '[')
    {
        host_start++;
        host_end = memchr(host_start, ']', (size_t)(authority_end - host_start));
        if (host_end == NULL || host_end >= authority_end)
        {
            return false;
        }

        host_len = (size_t)(host_end - host_start);
        if (host_len == 0 || host_len >= sizeof(out->host))
        {
            return false;
        }
        memcpy(out->host, host_start, host_len);
        out->host[host_len] = '\0';

        if ((host_end + 1) < authority_end && *(host_end + 1) == ':')
        {
            size_t port_len = (size_t)(authority_end - (host_end + 2));
            if (port_len == 0 || port_len >= sizeof(out->port)) return false;
            memcpy(out->port, host_end + 2, port_len);
            out->port[port_len] = '\0';
        }
    }
    else
    {
        const char *port_sep = memchr(host_start, ':', (size_t)(authority_end - host_start));
        if (port_sep != NULL)
        {
            host_end = port_sep;
        }
        else
        {
            host_end = authority_end;
        }

        host_len = (size_t)(host_end - host_start);
        if (host_len == 0 || host_len >= sizeof(out->host))
        {
            return false;
        }
        memcpy(out->host, host_start, host_len);
        out->host[host_len] = '\0';

        if (port_sep != NULL)
        {
            size_t port_len = (size_t)(authority_end - (port_sep + 1));
            if (port_len == 0 || port_len >= sizeof(out->port)) return false;
            memcpy(out->port, port_sep + 1, port_len);
            out->port[port_len] = '\0';
        }
    }

    if (out->port[0] == '\0')
    {
        snprintf(out->port, sizeof(out->port), "%s", out->is_coaps ? "5684" : "5683");
    }

    path_start = authority_end;
    if (*path_start == '\0')
    {
        snprintf(out->path_query, sizeof(out->path_query), "/");
    }
    else if (*path_start == '?')
    {
        snprintf(out->path_query, sizeof(out->path_query), "/%s", path_start);
    }
    else
    {
        snprintf(out->path_query, sizeof(out->path_query), "%s", path_start);
    }

    return true;
}

static void ota_package_abort(firmware_data_t *data)
{
    if (!data->package_ota_active) {
        return;
    }

    esp_ota_abort(data->package_ota_handle);
    data->package_ota_active = false;
    data->package_partition = NULL;
    data->package_ota_handle = 0;
}

static void ota_package_reset_progress(firmware_data_t *data)
{
    data->package_bytes_written = 0;
    ota_nvs_save_package_pointer(0);
}

static esp_err_t ota_package_begin(firmware_data_t *data)
{
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        data->result = FW_RESULT_NOT_ENOUGH_STORAGE;
        return ESP_ERR_NOT_FOUND;
    }

    esp_ota_handle_t ota_handle = 0;
    uint32_t resume_offset = (uint32_t)data->package_bytes_written;
    esp_err_t err;

    if (resume_offset > 0)
    {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        err = esp_ota_resume(update_partition, OTA_SIZE_UNKNOWN, resume_offset, &ota_handle);
        if (err != ESP_OK)
        {
            ESP_LOGW(FW_TAG,
                     "esp_ota_resume failed at %u (%s), restarting from zero",
                     (unsigned)resume_offset,
                     esp_err_to_name(err));
            ota_package_reset_progress(data);
            err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
        }
#else
        ESP_LOGW(FW_TAG, "Resume offset present (%u) but esp_ota_resume unsupported; restarting", (unsigned)resume_offset);
        ota_package_reset_progress(data);
        err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
#endif
    }
    else
    {
        err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    }

    if (err != ESP_OK)
    {
        data->result = FW_RESULT_NOT_ENOUGH_STORAGE;
        return err;
    }

    data->package_partition = update_partition;
    data->package_ota_handle = ota_handle;
    data->package_ota_active = true;
    data->result = FW_RESULT_INITIAL;
    return ESP_OK;
}

static esp_err_t ota_package_write_chunk(firmware_data_t *data,
                                         const uint8_t *chunk,
                                         size_t chunk_len,
                                         lwm2m_write_type_t write_type)
{
    esp_err_t err;

    if (chunk == NULL || chunk_len == 0) {
        data->result = FW_RESULT_INVALID_URI;
        return ESP_ERR_INVALID_ARG;
    }

    if (write_type != LWM2M_WRITE_PARTIAL_UPDATE && data->package_ota_active) {
        ota_package_abort(data);
    }

    if (!data->package_ota_active) {
        err = ota_package_begin(data);
        if (err != ESP_OK) {
            return err;
        }
    }

    err = esp_ota_write(data->package_ota_handle, chunk, chunk_len);
    if (err != ESP_OK) {
        data->result = FW_RESULT_NOT_ENOUGH_STORAGE;
        ota_package_abort(data);
        return err;
    }

    data->package_bytes_written += chunk_len;
    ota_nvs_save_package_pointer((uint32_t)data->package_bytes_written);
    if (write_type == LWM2M_WRITE_PARTIAL_UPDATE) {
        data->state = FW_STATE_DOWNLOADING;
    } else {
        data->state = FW_STATE_DOWNLOADED;
    }

    ESP_LOGI(FW_TAG,
             "Firmware package write: +%u bytes total=%u writeType=%d",
             (unsigned)chunk_len,
             (unsigned)data->package_bytes_written,
             (int)write_type);
    return ESP_OK;
}

static esp_err_t ota_package_finalize(firmware_data_t *data)
{
    esp_err_t err;

    if (!data->package_ota_active || data->package_partition == NULL || data->package_bytes_written == 0) {
        data->result = FW_RESULT_INVALID_URI;
        return ESP_ERR_INVALID_STATE;
    }

    err = esp_ota_end(data->package_ota_handle);
    if (err != ESP_OK) {
        data->result = (err == ESP_ERR_OTA_VALIDATE_FAILED) ? FW_RESULT_CRC_FAILED : FW_RESULT_UPDATE_FAILED;
        ota_package_abort(data);
        return err;
    }

    err = esp_ota_set_boot_partition(data->package_partition);
    if (err != ESP_OK) {
        data->result = FW_RESULT_UPDATE_FAILED;
        ota_package_abort(data);
        return err;
    }

    data->package_ota_active = false;
    data->package_partition = NULL;
    data->package_ota_handle = 0;
    ota_package_reset_progress(data);
    return ESP_OK;
}

static esp_err_t ota_download_http(firmware_data_t *data)
{
    esp_err_t err;
    esp_https_ota_handle_t https_ota_handle = NULL;

    esp_http_client_config_t config = {
        .url = data->package_uri,
        .timeout_ms = 240000,
        .keep_alive_enable = true,
        .disable_auto_redirect = false,
        .max_redirection_count = 8,
        .buffer_size = OTA_HTTP_RX_BUFFER_SIZE,
        .buffer_size_tx = OTA_HTTP_TX_BUFFER_SIZE,
        .user_agent = "edge-device-esp32-ota/1.0",
    };

#if CONFIG_ESP_TLS_INSECURE
    // Test-only mode: skip server certificate verification.
    config.skip_cert_common_name_check = true;
#endif

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
        .partial_http_download = true,
        .max_http_request_size = OTA_HTTP_REQUEST_CHUNK_SIZE,
    };

    ESP_LOGI(FW_TAG,
             "OTA HTTP tuning: rx_buf=%d tx_buf=%d chunk=%d keep_alive=%d",
             config.buffer_size,
             config.buffer_size_tx,
             ota_config.max_http_request_size,
             config.keep_alive_enable);

    ota_log_heap("before_begin");

    const int max_begin_retries = 5;
    for (int begin_retry = 0; begin_retry < max_begin_retries; begin_retry++) {
        err = esp_https_ota_begin(&ota_config, &https_ota_handle);
        if (err == ESP_OK) {
            break;
        }

        ESP_LOGW(FW_TAG, "OTA begin attempt %d/%d failed: %s", begin_retry + 1, max_begin_retries, esp_err_to_name(err));
        if (begin_retry < (max_begin_retries - 1)) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(FW_TAG, "OTA begin failed: %s", esp_err_to_name(err));
        if (err == ESP_ERR_NO_MEM) {
            data->result = FW_RESULT_OUT_OF_MEMORY;
        } else if (err == ESP_ERR_INVALID_ARG) {
            data->result = FW_RESULT_INVALID_URI;
        } else {
            data->result = FW_RESULT_CONNECTION_LOST;
        }
        data->state = FW_STATE_IDLE;
        return err;
    }

    ota_log_heap("after_begin");

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(FW_TAG, "Failed to get image descriptor: %s", esp_err_to_name(err));
        data->result = FW_RESULT_UNSUPPORTED_PKG;
        data->state = FW_STATE_IDLE;
        esp_https_ota_abort(https_ota_handle);
        ota_log_heap("after_abort_img_desc");
        return err;
    }

    ESP_LOGI(FW_TAG, "New firmware version: %s, project: %s", app_desc.version, app_desc.project_name);
    snprintf(data->pkg_version, sizeof(data->pkg_version), "%s", app_desc.version);
    snprintf(data->pkg_name, sizeof(data->pkg_name), "%s", app_desc.project_name);

    int last_progress_percent = -1;
    int last_progress_bytes = 0;
    const int progress_step_percent = 5;
    const int progress_step_bytes = 64 * 1024;

    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }

        int bytes_read = esp_https_ota_get_image_len_read(https_ota_handle);
        int image_size = esp_https_ota_get_image_size(https_ota_handle);

        if (image_size > 0) {
            int percent = (bytes_read * 100) / image_size;
            if ((percent >= (last_progress_percent + progress_step_percent)) ||
                ((bytes_read - last_progress_bytes) >= progress_step_bytes)) {
                ESP_LOGI(FW_TAG, "OTA download progress: %d/%d bytes (%d%%)",
                         bytes_read, image_size, percent);
                last_progress_percent = percent;
                last_progress_bytes = bytes_read;
            }
        } else if ((bytes_read - last_progress_bytes) >= progress_step_bytes) {
            ESP_LOGI(FW_TAG, "OTA download progress: %d bytes", bytes_read);
            last_progress_bytes = bytes_read;
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(FW_TAG, "OTA download failed: %s", esp_err_to_name(err));
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            data->result = FW_RESULT_CRC_FAILED;
        } else if (err == ESP_ERR_NO_MEM || err == ESP_ERR_OTA_SELECT_INFO_INVALID) {
            data->result = FW_RESULT_NOT_ENOUGH_STORAGE;
        } else {
            data->result = FW_RESULT_CONNECTION_LOST;
        }
        data->state = FW_STATE_IDLE;
        esp_https_ota_abort(https_ota_handle);
        ota_log_heap("after_abort_download");
        return err;
    }

    data->state = FW_STATE_DOWNLOADED;
    ota_notify_resource_changed(data, RES_M_STATE);

    ESP_LOGI(FW_TAG, "OTA download progress: %d/%d bytes (100%%)",
             esp_https_ota_get_image_len_read(https_ota_handle),
             esp_https_ota_get_image_size(https_ota_handle));
    ESP_LOGI(FW_TAG, "OTA download completed successfully");

    data->state = FW_STATE_UPDATING;
    err = esp_https_ota_finish(https_ota_handle);
    ota_log_heap("after_finish");
    if (err != ESP_OK) {
        ESP_LOGE(FW_TAG, "OTA finish failed: %s", esp_err_to_name(err));
        data->result = FW_RESULT_UPDATE_FAILED;
        data->state = FW_STATE_IDLE;
        return err;
    }

    return ESP_OK;
}

static esp_err_t ota_download_coap_blockwise(firmware_data_t *data)
{
    ota_coap_uri_t uri;
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    int sockfd = -1;
    esp_err_t ret = ESP_FAIL;
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = NULL;
    bool ota_started = false;
    uint32_t block_num = 0;
    int total_bytes = 0;
    int image_size = -1;
    bool got_app_desc = false;

    if (!ota_parse_coap_uri(data->package_uri, &uri)) {
        data->result = FW_RESULT_INVALID_URI;
        return ESP_ERR_INVALID_ARG;
    }

    if (uri.is_coaps) {
        ESP_LOGE(FW_TAG, "coaps:// package URI is not implemented in OTA downloader yet");
        data->result = FW_RESULT_UNSUPPORTED_PROTOCOL;
        return ESP_ERR_NOT_SUPPORTED;
    }

    ESP_LOGI(FW_TAG, "Starting CoAP Block2 OTA host=%s port=%s path=%s", uri.host, uri.port, uri.path_query);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(uri.host, uri.port, &hints, &res) != 0 || res == NULL) {
        ESP_LOGE(FW_TAG, "Failed to resolve CoAP host: %s", uri.host);
        data->result = FW_RESULT_CONNECTION_LOST;
        return ESP_FAIL;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        ESP_LOGE(FW_TAG, "Failed to create UDP socket");
        data->result = FW_RESULT_CONNECTION_LOST;
        goto done;
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
        ESP_LOGE(FW_TAG, "Failed to connect UDP socket");
        data->result = FW_RESULT_CONNECTION_LOST;
        goto done;
    }

    struct timeval tv = {
        .tv_sec = OTA_COAP_RECV_TIMEOUT_MS / 1000,
        .tv_usec = (OTA_COAP_RECV_TIMEOUT_MS % 1000) * 1000,
    };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(FW_TAG, "No OTA partition available");
        data->result = FW_RESULT_NOT_ENOUGH_STORAGE;
        goto done;
    }

    if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle) != ESP_OK) {
        ESP_LOGE(FW_TAG, "esp_ota_begin failed");
        data->result = FW_RESULT_NOT_ENOUGH_STORAGE;
        goto done;
    }
    ota_started = true;

    while (1) {
        uint8_t tx_buf[OTA_COAP_TX_BUFFER_SIZE];
        uint8_t rx_buf[OTA_COAP_RX_BUFFER_SIZE];
        coap_packet_t request[1];
        coap_packet_t response[1];
        const uint8_t *payload = NULL;
        size_t payload_len;
        int sent;
        int recv_len = -1;
        uint8_t token[4];
        uint32_t rsp_num = 0;
        uint8_t rsp_more = 0;
        uint16_t rsp_block_size = 0;
        uint32_t rsp_offset = 0;
        int has_block2;
        int retry;

        token[0] = (uint8_t)(block_num & 0xFF);
        token[1] = (uint8_t)((block_num >> 8) & 0xFF);
        token[2] = (uint8_t)((block_num >> 16) & 0xFF);
        token[3] = (uint8_t)((block_num >> 24) & 0xFF);

        memset(request, 0, sizeof(request));
        coap_init_message(request, COAP_TYPE_CON, COAP_GET, coap_get_mid());
        coap_set_header_token(request, token, sizeof(token));
        coap_set_header_uri_path(request, uri.path_query);
        coap_set_header_block2(request, block_num, 0, OTA_COAP_BLOCK_SIZE);
        size_t tx_len = coap_serialize_message(request, tx_buf);
        if (tx_len == 0 || tx_len > sizeof(tx_buf)) {
            ESP_LOGE(FW_TAG, "Failed to serialize CoAP request for block %lu", (unsigned long)block_num);
            data->result = FW_RESULT_UPDATE_FAILED;
            goto done;
        }

        for (retry = 0; retry < OTA_COAP_MAX_RETRIES; retry++) {
            sent = (int)send(sockfd, tx_buf, tx_len, 0);
            if (sent < 0) {
                continue;
            }

            recv_len = (int)recv(sockfd, rx_buf, sizeof(rx_buf), 0);
            if (recv_len > 0) {
                break;
            }
        }

        if (recv_len <= 0) {
            ESP_LOGE(FW_TAG, "No CoAP response for block %lu after retries", (unsigned long)block_num);
            data->result = FW_RESULT_CONNECTION_LOST;
            goto done;
        }

        memset(response, 0, sizeof(response));
        if (coap_parse_message(response, rx_buf, (uint16_t)recv_len) != NO_ERROR) {
            ESP_LOGE(FW_TAG, "Failed to parse CoAP response for block %lu", (unsigned long)block_num);
            data->result = FW_RESULT_CONNECTION_LOST;
            goto done;
        }

        if (response->code != CONTENT_2_05) {
            ESP_LOGE(FW_TAG, "Unexpected CoAP response code: %u", response->code);
            data->result = FW_RESULT_UPDATE_FAILED;
            goto done;
        }

        payload_len = coap_get_payload(response, &payload);
        if (payload == NULL || payload_len == 0) {
            ESP_LOGE(FW_TAG, "Empty payload in CoAP block %lu", (unsigned long)block_num);
            data->result = FW_RESULT_CONNECTION_LOST;
            goto done;
        }

        if (esp_ota_write(ota_handle, payload, payload_len) != ESP_OK) {
            ESP_LOGE(FW_TAG, "esp_ota_write failed at block %lu", (unsigned long)block_num);
            data->result = FW_RESULT_NOT_ENOUGH_STORAGE;
            goto done;
        }

        total_bytes += (int)payload_len;

        if (!got_app_desc) {
            size_t app_desc_offset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
            if (payload_len >= app_desc_offset + sizeof(esp_app_desc_t)) {
                const esp_app_desc_t *desc = (const esp_app_desc_t *)(payload + app_desc_offset);
                snprintf(data->pkg_version, sizeof(data->pkg_version), "%s", desc->version);
                snprintf(data->pkg_name, sizeof(data->pkg_name), "%s", desc->project_name);
                ESP_LOGI(FW_TAG, "New firmware version: %s, project: %s", desc->version, desc->project_name);
                got_app_desc = true;
            }
        }

        if (coap_get_header_size(response, (uint32_t *)&image_size) != 1) {
            // no size option in response, keep unknown size
            image_size = -1;
        }

        if (image_size > 0) {
            int percent = (total_bytes * 100) / image_size;
            ESP_LOGI(FW_TAG, "OTA download progress: %d/%d bytes (%d%%)", total_bytes, image_size, percent);
        } else {
            ESP_LOGI(FW_TAG, "OTA download progress: %d bytes", total_bytes);
        }

        has_block2 = coap_get_header_block2(response, &rsp_num, &rsp_more, &rsp_block_size, &rsp_offset);
        if (has_block2 == 1) {
            if (rsp_num != block_num) {
                ESP_LOGE(FW_TAG, "Out-of-order CoAP block: expected=%lu got=%lu",
                         (unsigned long)block_num,
                         (unsigned long)rsp_num);
                data->result = FW_RESULT_CONNECTION_LOST;
                goto done;
            }

            if (rsp_more == 0) {
                break;
            }
            block_num = rsp_num + 1;
        } else {
            break;
        }
    }

    if (esp_ota_end(ota_handle) != ESP_OK) {
        ESP_LOGE(FW_TAG, "esp_ota_end failed");
        data->result = FW_RESULT_CRC_FAILED;
        goto done;
    }
    ota_started = false;

    if (esp_ota_set_boot_partition(update_partition) != ESP_OK) {
        ESP_LOGE(FW_TAG, "esp_ota_set_boot_partition failed");
        data->result = FW_RESULT_UPDATE_FAILED;
        goto done;
    }

    data->state = FW_STATE_DOWNLOADED;
    ota_notify_resource_changed(data, RES_M_STATE);
    data->state = FW_STATE_UPDATING;
    ESP_LOGI(FW_TAG, "CoAP blockwise OTA completed (%d bytes)", total_bytes);
    ret = ESP_OK;

done:
    if (ota_started) {
        esp_ota_abort(ota_handle);
    }
    if (sockfd >= 0) {
        close(sockfd);
    }
    if (res != NULL) {
        freeaddrinfo(res);
    }
    return ret;
}
#endif

static uint8_t prv_firmware_read(lwm2m_context_t *contextP,
                                 uint16_t instanceId,
                                 int * numDataP,
                                 lwm2m_data_t ** dataArrayP,
                                 lwm2m_object_t * objectP)
{
    int i;
    uint8_t result;
    firmware_data_t * data = (firmware_data_t*)(objectP->userData);

    /* unused parameter */
    (void)contextP;

    // this is a single instance object
    if (instanceId != 0)
    {
        return COAP_404_NOT_FOUND;
    }

    // is the server asking for the full object ?
    if (*numDataP == 0)
    {
        *dataArrayP = lwm2m_data_new(7);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = 7;
        (*dataArrayP)[0].id = 3;
        (*dataArrayP)[1].id = 5;
        (*dataArrayP)[2].id = 6;
        (*dataArrayP)[3].id = 7;
        (*dataArrayP)[4].id = 8;
        (*dataArrayP)[5].id = 9;
        (*dataArrayP)[6].id = 10;
    }

    i = 0;
    do
    {
        switch ((*dataArrayP)[i].id)
        {
        case RES_M_PACKAGE:
        case RES_M_PACKAGE_URI:
        case RES_M_UPDATE:
            result = COAP_405_METHOD_NOT_ALLOWED;
            break;

        case RES_M_STATE:
            // firmware update state (int)
            if ((*dataArrayP)[i].type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
            lwm2m_data_encode_int(data->state, *dataArrayP + i);
            result = COAP_205_CONTENT;
            break;

        case RES_M_UPDATE_RESULT:
            if ((*dataArrayP)[i].type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
            lwm2m_data_encode_int(data->result, *dataArrayP + i);
            result = COAP_205_CONTENT;
            break;

        case RES_O_PKG_NAME:
            if ((*dataArrayP)[i].type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
            lwm2m_data_encode_string(data->pkg_name, *dataArrayP + i);
            result = COAP_205_CONTENT;
            break;

        case RES_O_PKG_VERSION:
            if ((*dataArrayP)[i].type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
            lwm2m_data_encode_string(data->pkg_version, *dataArrayP + i);
            result = COAP_205_CONTENT;
            break;

        case RES_O_UPDATE_PROTOCOL:
        {
            lwm2m_data_t * subTlvP;
            size_t count;
            size_t ri;
            int num = 0;

            while ((num < LWM2M_FIRMWARE_PROTOCOL_NUM) &&
                    (data->protocol_support[num] != LWM2M_FIRMWARE_PROTOCOL_NULL))
                num++;

            if ((*dataArrayP)[i].type == LWM2M_TYPE_MULTIPLE_RESOURCE)
            {
                count = (*dataArrayP)[i].value.asChildren.count;
                subTlvP = (*dataArrayP)[i].value.asChildren.array;
            }
            else
            {
                count = num;
                if (!count) count = 1;
                subTlvP = lwm2m_data_new(count);
                for (ri = 0; ri < count; ri++) subTlvP[ri].id = ri;
                lwm2m_data_encode_instances(subTlvP, count, *dataArrayP + i);
            }

            if (num)
            {
                for (ri = 0; ri < count; ri++)
                {
                    if (subTlvP[ri].id >= num) return COAP_404_NOT_FOUND;
                    lwm2m_data_encode_int(data->protocol_support[subTlvP[ri].id],
                                          subTlvP + ri);
                }
            }
            else
            {
                /* If no protocol is provided, use CoAP as default (per spec) */
                for (ri = 0; ri < count; ri++)
                {
                    if (subTlvP[ri].id != 0) return COAP_404_NOT_FOUND;
                    lwm2m_data_encode_int(0, subTlvP + ri);
                }
            }
            result = COAP_205_CONTENT;
            break;
        }

        case RES_M_UPDATE_METHOD:
            if ((*dataArrayP)[i].type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
            lwm2m_data_encode_int(data->delivery_method, *dataArrayP + i);
            result = COAP_205_CONTENT;
            break;

        case RES_O_PACKAGE_POINTER:
            if ((*dataArrayP)[i].type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
            lwm2m_data_encode_int((int64_t)data->package_bytes_written, *dataArrayP + i);
            result = COAP_205_CONTENT;
            break;

        default:
            result = COAP_404_NOT_FOUND;
        }

        i++;
    } while (i < *numDataP && result == COAP_205_CONTENT);

    return result;
}

static uint8_t prv_firmware_write(lwm2m_context_t *contextP,
                                  uint16_t instanceId,
                                  int numData,
                                  lwm2m_data_t * dataArray,
                                  lwm2m_object_t * objectP,
                                  lwm2m_write_type_t writeType)
{
    int i;
    uint8_t result;
    firmware_data_t * data = (firmware_data_t*)(objectP->userData);

    // this is a single instance object
    if (instanceId != 0)
    {
        return COAP_404_NOT_FOUND;
    }

    // Store context for notifications
    data->lwm2mH = contextP;

    i = 0;

    do
    {
        /* No multiple instance resources */
        if (dataArray[i].type == LWM2M_TYPE_MULTIPLE_RESOURCE)
        {
            result = COAP_404_NOT_FOUND;
            continue;
        }

        switch (dataArray[i].id)
        {
        case RES_M_PACKAGE:
            // Push mode: stream package bytes written to /5/0/0 into OTA partition.
#ifdef ESP_PLATFORM
            if (dataArray[i].type != LWM2M_TYPE_OPAQUE && dataArray[i].type != LWM2M_TYPE_STRING)
            {
                data->result = FW_RESULT_UNSUPPORTED_PKG;
                result = COAP_400_BAD_REQUEST;
                break;
            }

            if (ota_package_write_chunk(data,
                                        dataArray[i].value.asBuffer.buffer,
                                        dataArray[i].value.asBuffer.length,
                                        writeType) != ESP_OK)
            {
                result = COAP_400_BAD_REQUEST;
                break;
            }

            ota_notify_state_and_result(data);
#else
            data->state = FW_STATE_DOWNLOADED;
#endif
            result = COAP_204_CHANGED;
            break;

        case RES_M_PACKAGE_URI:
        {
            // URL for download the firmware
            size_t uri_len = 0;
            const char *uri_str = NULL;
            
            if (dataArray[i].type == LWM2M_TYPE_STRING)
            {
                uri_str = (const char *)dataArray[i].value.asBuffer.buffer;
                uri_len = dataArray[i].value.asBuffer.length;
            }
            
            if (uri_str && uri_len > 0 && uri_len < sizeof(data->package_uri))
            {
#ifdef ESP_PLATFORM
                // Switching to URI pull mode discards any staged push package.
                ota_package_abort(data);
                ota_package_reset_progress(data);
#endif
                memcpy(data->package_uri, uri_str, uri_len);
                data->package_uri[uri_len] = '\0';
                
#ifdef ESP_PLATFORM
                ESP_LOGI(FW_TAG, "Firmware package URI set: %s", data->package_uri);
#else
                fprintf(stdout, "Firmware package URI set: %s\n", data->package_uri);
#endif
                data->state = FW_STATE_IDLE;
                data->result = FW_RESULT_INITIAL;
                result = COAP_204_CHANGED;
            }
            else
            {
#ifdef ESP_PLATFORM
                ESP_LOGE(FW_TAG, "Invalid package URI (length: %zu)", uri_len);
#endif
                data->result = FW_RESULT_INVALID_URI;
                result = COAP_400_BAD_REQUEST;
            }
            break;
        }

        case RES_O_PACKAGE_POINTER:
        {
            int64_t pointer_value = 0;
            if (lwm2m_data_decode_int(dataArray + i, &pointer_value) != 1 || pointer_value < 0)
            {
                result = COAP_400_BAD_REQUEST;
                break;
            }

#ifdef ESP_PLATFORM
            ota_package_abort(data);
#endif
            data->package_bytes_written = (size_t)pointer_value;
#ifdef ESP_PLATFORM
            ota_nvs_save_package_pointer((uint32_t)data->package_bytes_written);
#endif
            data->state = (data->package_bytes_written > 0) ? FW_STATE_DOWNLOADING : FW_STATE_IDLE;
            result = COAP_204_CHANGED;
            break;
        }

        default:
            result = COAP_405_METHOD_NOT_ALLOWED;
        }

        i++;
    } while (i < numData && result == COAP_204_CHANGED);

    return result;
}

#ifdef ESP_PLATFORM
/* OTA update task that runs the actual firmware update */
static void ota_task(void *pvParameter)
{
    firmware_data_t *data = (firmware_data_t *)pvParameter;
    esp_err_t err;

    if (data->package_ota_active && data->package_bytes_written > 0) {
        ESP_LOGI(FW_TAG, "Finalizing pushed firmware package (%u bytes)", (unsigned)data->package_bytes_written);
        data->state = FW_STATE_UPDATING;
        ota_notify_resource_changed(data, RES_M_STATE);
        err = ota_package_finalize(data);
    } else {
        ESP_LOGI(FW_TAG, "Starting OTA update from: %s", data->package_uri);
        data->state = FW_STATE_DOWNLOADING;
        ota_notify_resource_changed(data, RES_M_STATE);

        if (ota_str_starts_with(data->package_uri, "coap://") ||
            ota_str_starts_with(data->package_uri, "coaps://")) {
            err = ota_download_coap_blockwise(data);
        } else {
            err = ota_download_http(data);
        }
    }

    if (err == ESP_OK) {
        ESP_LOGI(FW_TAG, "OTA update successful! Rebooting in 3 seconds...");
        data->result = FW_RESULT_SUCCESS;
        data->state = FW_STATE_IDLE;
        ota_notify_state_and_result(data);
        
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    }

    if (data->state != FW_STATE_IDLE) {
        data->state = FW_STATE_IDLE;
    }

    // Notify final state
    ota_notify_state_and_result(data);
    
    vTaskDelete(NULL);
}
#endif

static uint8_t prv_firmware_execute(lwm2m_context_t *contextP,
                                    uint16_t instanceId,
                                    uint16_t resourceId,
                                    uint8_t * buffer,
                                    int length,
                                    lwm2m_object_t * objectP)
{
    firmware_data_t * data = (firmware_data_t*)(objectP->userData);

    // this is a single instance object
    if (instanceId != 0)
    {
        return COAP_404_NOT_FOUND;
    }

    if (length != 0) return COAP_400_BAD_REQUEST;

    // Store context for state notifications
    data->lwm2mH = contextP;

    // for execute callback, resId is always set.
    switch (resourceId)
    {
    case RES_M_UPDATE:
        if (data->state == FW_STATE_IDLE || data->state == FW_STATE_DOWNLOADED)
        {
            bool has_pushed_package = false;
#ifdef ESP_PLATFORM
            has_pushed_package = (data->package_ota_active && data->package_bytes_written > 0);
#endif

            // Pull mode requires /5/0/1 Package URI. Push mode executes staged /5/0/0 package.
            if (!has_pushed_package && strlen(data->package_uri) == 0)
            {
#ifdef ESP_PLATFORM
                ESP_LOGE(FW_TAG, "Cannot execute update: no package URI set");
#else
                fprintf(stderr, "Cannot execute update: no package URI set\n");
#endif
                data->result = FW_RESULT_INVALID_URI;
                return COAP_400_BAD_REQUEST;
            }
            
#ifdef ESP_PLATFORM
            if (has_pushed_package)
            {
                ESP_LOGI(FW_TAG, "Firmware update triggered from pushed /5/0/0 package");
            }
            else
            {
                ESP_LOGI(FW_TAG, "Firmware update triggered for URI: %s", data->package_uri);
            }
            
            // Launch OTA task
            BaseType_t ret = xTaskCreate(&ota_task, "ota_task", 8192, data, 5, NULL);
            if (ret != pdPASS)
            {
                ESP_LOGE(FW_TAG, "Failed to create OTA task");
                data->result = FW_RESULT_OUT_OF_MEMORY;
                return COAP_500_INTERNAL_SERVER_ERROR;
            }
#else
            fprintf(stdout, "\n\t FIRMWARE UPDATE TRIGGERED (URI: %s)\r\n\n", data->package_uri);
            // Simulate state change for non-ESP platforms
            data->state = FW_STATE_DOWNLOADING;
#endif
            return COAP_204_CHANGED;
        }
        else
        {
            // firmware update already running
#ifdef ESP_PLATFORM
            ESP_LOGW(FW_TAG, "Firmware update already in progress (state: %d)", data->state);
#endif
            return COAP_400_BAD_REQUEST;
        }
    default:
        return COAP_405_METHOD_NOT_ALLOWED;
    }
}

void display_firmware_object(lwm2m_object_t * object)
{
    firmware_data_t * data = (firmware_data_t *)object->userData;
    fprintf(stdout, "  /%u: Firmware object:\r\n", object->objID);
    if (NULL != data)
    {
        fprintf(stdout, "    state: %u, result: %u\r\n", data->state,
                data->result);
    }
}

lwm2m_object_t * get_object_firmware(void)
{
    /*
     * The get_object_firmware function create the object itself and return a pointer to the structure that represent it.
     */
    lwm2m_object_t * firmwareObj;

    firmwareObj = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));

    if (NULL != firmwareObj)
    {
        memset(firmwareObj, 0, sizeof(lwm2m_object_t));

        /*
         * It assigns its unique ID
         * The 5 is the standard ID for the optional object "Object firmware".
         */
        firmwareObj->objID = LWM2M_FIRMWARE_UPDATE_OBJECT_ID;

        /*
         * and its unique instance
         *
         */
        firmwareObj->instanceList = (lwm2m_list_t *)lwm2m_malloc(sizeof(lwm2m_list_t));
        if (NULL != firmwareObj->instanceList)
        {
            memset(firmwareObj->instanceList, 0, sizeof(lwm2m_list_t));
        }
        else
        {
            lwm2m_free(firmwareObj);
            return NULL;
        }

        /*
         * And the private function that will access the object.
         * Those function will be called when a read/write/execute query is made by the server. In fact the library don't need to
         * know the resources of the object, only the server does.
         */
        firmwareObj->readFunc    = prv_firmware_read;
        firmwareObj->writeFunc   = prv_firmware_write;
        firmwareObj->executeFunc = prv_firmware_execute;
        firmwareObj->userData    = lwm2m_malloc(sizeof(firmware_data_t));

        /*
         * Also some user data can be stored in the object with a private structure containing the needed variables
         */
        if (NULL != firmwareObj->userData)
        {
            firmware_data_t *data = (firmware_data_t*)(firmwareObj->userData);

            memset(data, 0, sizeof(*data));

            data->state = FW_STATE_IDLE;
            data->result = FW_RESULT_INITIAL;
            strcpy(data->pkg_name, "esp32-lwm2m-gateway");
            strcpy(data->pkg_version, "1.0.0");
            memset(data->package_uri, 0, sizeof(data->package_uri));
            data->lwm2mH = NULL;

#ifdef ESP_PLATFORM
            data->package_ota_handle = 0;
            data->package_partition = NULL;
            data->package_ota_active = false;
            data->package_bytes_written = (size_t)ota_nvs_load_package_pointer();

            if (data->package_bytes_written > 0)
            {
                data->state = FW_STATE_DOWNLOADING;
                ESP_LOGI(FW_TAG, "Restored OTA package pointer from NVS: %u", (unsigned)data->package_bytes_written);
            }
#else
            data->package_bytes_written = 0;
#endif

            /* Protocol support: CoAP (blockwise), HTTP, HTTPS */
            data->protocol_support[0] = 0;  // CoAP
            data->protocol_support[1] = 2;  // HTTP
            data->protocol_support[2] = 3;  // HTTPS
            data->protocol_support[3] = LWM2M_FIRMWARE_PROTOCOL_NULL;

           /* Support both push and pull methods */
           data->delivery_method = 2;  // Both push and pull
        }
        else
        {
            lwm2m_free(firmwareObj);
            firmwareObj = NULL;
        }
    }

    return firmwareObj;
}

void free_object_firmware(lwm2m_object_t * objectP)
{
    if (NULL != objectP->userData)
    {
#ifdef ESP_PLATFORM
        firmware_data_t *data = (firmware_data_t *)objectP->userData;
        ota_package_abort(data);
#endif
        lwm2m_free(objectP->userData);
        objectP->userData = NULL;
    }
    if (NULL != objectP->instanceList)
    {
        lwm2m_free(objectP->instanceList);
        objectP->instanceList = NULL;
    }
    lwm2m_free(objectP);
}

