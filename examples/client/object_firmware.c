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
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

#define LWM2M_FIRMWARE_PROTOCOL_NUM     4
#define LWM2M_FIRMWARE_PROTOCOL_NULL    ((uint8_t)-1)

#ifdef ESP_PLATFORM
#define FW_TAG "FW_OTA"
#endif

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

typedef struct
{
    uint8_t state;
    uint8_t result;
    char pkg_name[256];
    char pkg_version[256];
    uint8_t protocol_support[LWM2M_FIRMWARE_PROTOCOL_NUM];
    uint8_t delivery_method;
    char package_uri[512];  // Store firmware URL
    lwm2m_context_t *lwm2mH;  // LwM2M context for notifications
} firmware_data_t;

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
        *dataArrayP = lwm2m_data_new(6);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = 6;
        (*dataArrayP)[0].id = 3;
        (*dataArrayP)[1].id = 5;
        (*dataArrayP)[2].id = 6;
        (*dataArrayP)[3].id = 7;
        (*dataArrayP)[4].id = 8;
        (*dataArrayP)[5].id = 9;
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

    // All write types are treated the same here
    (void)writeType;

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
            // inline firmware binary (not commonly used)
#ifdef ESP_PLATFORM
            ESP_LOGI(FW_TAG, "Received inline firmware package (size: %zu bytes)", dataArray[i].value.asBuffer.length);
#endif
            data->state = FW_STATE_DOWNLOADED;
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
    
    ESP_LOGI(FW_TAG, "Starting OTA update from: %s", data->package_uri);
    data->state = FW_STATE_DOWNLOADING;
    
    // Notify LwM2M server of state change
    if (data->lwm2mH) {
        lwm2m_uri_t uri = {.objectId = LWM2M_FIRMWARE_UPDATE_OBJECT_ID, .instanceId = 0, .resourceId = RES_M_STATE};
        lwm2m_resource_value_changed(data->lwm2mH, &uri);
    }
    
    // Configure HTTP client for OTA
    esp_http_client_config_t config = {
        .url = data->package_uri,
        .timeout_ms = 30000,
        .keep_alive_enable = true,
    };
    
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    
    esp_https_ota_handle_t https_ota_handle = NULL;
    err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    
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
        goto ota_end;
    }
    
    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(FW_TAG, "Failed to get image descriptor: %s", esp_err_to_name(err));
        data->result = FW_RESULT_UNSUPPORTED_PKG;
        data->state = FW_STATE_IDLE;
        esp_https_ota_abort(https_ota_handle);
        goto ota_end;
    }
    
    ESP_LOGI(FW_TAG, "New firmware version: %s, project: %s", app_desc.version, app_desc.project_name);
    snprintf(data->pkg_version, sizeof(data->pkg_version), "%s", app_desc.version);
    snprintf(data->pkg_name, sizeof(data->pkg_name), "%s", app_desc.project_name);
    
    // Download and flash firmware
    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        // Download in progress
        ESP_LOGD(FW_TAG, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
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
        goto ota_end;
    }
    
    data->state = FW_STATE_DOWNLOADED;
    ESP_LOGI(FW_TAG, "OTA download completed successfully");
    
    // Notify download complete
    if (data->lwm2mH) {
        lwm2m_uri_t uri = {.objectId = LWM2M_FIRMWARE_UPDATE_OBJECT_ID, .instanceId = 0, .resourceId = RES_M_STATE};
        lwm2m_resource_value_changed(data->lwm2mH, &uri);
    }
    
    // Verify and finish OTA
    data->state = FW_STATE_UPDATING;
    err = esp_https_ota_finish(https_ota_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(FW_TAG, "OTA update successful! Rebooting in 3 seconds...");
        data->result = FW_RESULT_SUCCESS;
        data->state = FW_STATE_IDLE;
        
        // Notify success before reboot
        if (data->lwm2mH) {
            lwm2m_uri_t uri_state = {.objectId = LWM2M_FIRMWARE_UPDATE_OBJECT_ID, .instanceId = 0, .resourceId = RES_M_STATE};
            lwm2m_uri_t uri_result = {.objectId = LWM2M_FIRMWARE_UPDATE_OBJECT_ID, .instanceId = 0, .resourceId = RES_M_UPDATE_RESULT};
            lwm2m_resource_value_changed(data->lwm2mH, &uri_state);
            lwm2m_resource_value_changed(data->lwm2mH, &uri_result);
        }
        
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    } else {
        ESP_LOGE(FW_TAG, "OTA finish failed: %s", esp_err_to_name(err));
        data->result = FW_RESULT_UPDATE_FAILED;
        data->state = FW_STATE_IDLE;
    }
    
ota_end:
    // Notify final state
    if (data->lwm2mH) {
        lwm2m_uri_t uri_state = {.objectId = LWM2M_FIRMWARE_UPDATE_OBJECT_ID, .instanceId = 0, .resourceId = RES_M_STATE};
        lwm2m_uri_t uri_result = {.objectId = LWM2M_FIRMWARE_UPDATE_OBJECT_ID, .instanceId = 0, .resourceId = RES_M_UPDATE_RESULT};
        lwm2m_resource_value_changed(data->lwm2mH, &uri_state);
        lwm2m_resource_value_changed(data->lwm2mH, &uri_result);
    }
    
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
            // Check if package URI is set
            if (strlen(data->package_uri) == 0)
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
            ESP_LOGI(FW_TAG, "Firmware update triggered for URI: %s", data->package_uri);
            
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

            data->state = FW_STATE_IDLE;
            data->result = FW_RESULT_INITIAL;
            strcpy(data->pkg_name, "esp32-lwm2m-gateway");
            strcpy(data->pkg_version, "1.0.0");
            memset(data->package_uri, 0, sizeof(data->package_uri));
            data->lwm2mH = NULL;

            /* Support CoAP, CoAPs, HTTP, and HTTPS protocols */
            data->protocol_support[0] = 0;  // CoAP
            data->protocol_support[1] = 1;  // CoAPs
            data->protocol_support[2] = 2;  // HTTP
            data->protocol_support[3] = 3;  // HTTPS

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

