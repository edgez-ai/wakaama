/*******************************************************************************
 *
 * Copyright (c) 2024 edgez.ai - Gateway Management Object
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v20.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 *******************************************************************************/

/*
 * This Gateway Management Object (ID: 25) provides monitoring and control
 * of gateway-specific functionality including device management, statistics,
 * and configuration.
 *
 * Resources:
 *
 *        Name                    | ID | Oper. | Inst. | Mand.| Type    | Range | Units |
 * Gateway ID                    |  0 | R     | Single| Yes  | String  |       |       |
 * Connected Devices             |  1 | R     | Single| Yes  | Integer | 0-    |       |
 * Max Devices                   |  2 | RW    | Single| Yes  | Integer | 1-    |       |
 * Active Sessions               |  3 | R     | Single| Yes  | Integer | 0-    |       |
 * Total Data RX                 |  4 | R     | Single| No   | Integer | 0-    | bytes |
 * Total Data TX                 |  5 | R     | Single| No   | Integer | 0-    | bytes |
 * Uptime                        |  6 | R     | Single| Yes  | Integer | 0-    | sec   |
 * Gateway Status                |  7 | RW    | Single| Yes  | String  |       |       |
 * Firmware Version              |  8 | R     | Single| No   | String  |       |       |
 * Auto Registration             |  9 | RW    | Single| No   | Boolean |       |       |
 * Reset Statistics              | 10 | E     | Single| No   |         |       |       |
 * Reboot Gateway                | 11 | E     | Single| No   |         |       |       |
 */

#include "liblwm2m.h"
#include "object_gateway.h"
#include "lwm2mclient.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

// Logging helper: use ESP-IDF logging if available, fallback to printf otherwise
#ifdef ESP_PLATFORM
#include "esp_log.h"
#define GATEWAY_LOGI(fmt, ...) ESP_LOGI("LWM2M_GATEWAY", fmt, ##__VA_ARGS__)
#else
#define GATEWAY_LOGI(fmt, ...) do { printf("[LWM2M_GATEWAY] " fmt "\n", ##__VA_ARGS__); } while (0)
#endif

// Default values
#define PRV_GATEWAY_ID           "ESP32-LWM2M-GW"
#define PRV_MAX_DEVICES          100
#define PRV_FIRMWARE_VERSION     "1.0.0"
#define PRV_STATUS_ACTIVE        "active"
#define PRV_STATUS_INACTIVE      "inactive"
#define PRV_STATUS_MAINTENANCE   "maintenance"

// Resource IDs
#define RES_O_GATEWAY_ID            0
#define RES_O_CONNECTED_DEVICES     1
#define RES_M_MAX_DEVICES           2
#define RES_O_ACTIVE_SESSIONS       3
#define RES_O_TOTAL_DATA_RX         4
#define RES_O_TOTAL_DATA_TX         5
#define RES_M_UPTIME                6
#define RES_M_GATEWAY_STATUS        7
#define RES_O_FIRMWARE_VERSION      8
#define RES_O_AUTO_REGISTRATION     9
#define RES_E_RESET_STATISTICS     10
#define RES_E_REBOOT_GATEWAY       11

static uint8_t prv_set_value(lwm2m_data_t * dataP,
                             gateway_instance_t * gwDataP)
{
    // Handle resource reads
    switch (dataP->id)
    {
    case RES_O_GATEWAY_ID:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_string(gwDataP->gateway_id, dataP);
        GATEWAY_LOGI("inst=%u READ GATEWAY_ID=%s", gwDataP->instanceId, gwDataP->gateway_id);
        return COAP_205_CONTENT;

    case RES_O_CONNECTED_DEVICES:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_int(gwDataP->connected_devices, dataP);
        GATEWAY_LOGI("inst=%u READ CONNECTED_DEVICES=%lld", gwDataP->instanceId, (long long)gwDataP->connected_devices);
        return COAP_205_CONTENT;

    case RES_M_MAX_DEVICES:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_int(gwDataP->max_devices, dataP);
        GATEWAY_LOGI("inst=%u READ MAX_DEVICES=%lld", gwDataP->instanceId, (long long)gwDataP->max_devices);
        return COAP_205_CONTENT;

    case RES_O_ACTIVE_SESSIONS:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_int(gwDataP->active_sessions, dataP);
        GATEWAY_LOGI("inst=%u READ ACTIVE_SESSIONS=%lld", gwDataP->instanceId, (long long)gwDataP->active_sessions);
        return COAP_205_CONTENT;

    case RES_O_TOTAL_DATA_RX:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_int(gwDataP->total_data_rx, dataP);
        GATEWAY_LOGI("inst=%u READ TOTAL_DATA_RX=%lld", gwDataP->instanceId, (long long)gwDataP->total_data_rx);
        return COAP_205_CONTENT;

    case RES_O_TOTAL_DATA_TX:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_int(gwDataP->total_data_tx, dataP);
        GATEWAY_LOGI("inst=%u READ TOTAL_DATA_TX=%lld", gwDataP->instanceId, (long long)gwDataP->total_data_tx);
        return COAP_205_CONTENT;

    case RES_M_UPTIME:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        // Update uptime with current time
        gwDataP->uptime = time(NULL) - gwDataP->uptime; // uptime field stores boot time
        lwm2m_data_encode_int(gwDataP->uptime, dataP);
        GATEWAY_LOGI("inst=%u READ UPTIME=%lld", gwDataP->instanceId, (long long)gwDataP->uptime);
        return COAP_205_CONTENT;

    case RES_M_GATEWAY_STATUS:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_string(gwDataP->status, dataP);
        GATEWAY_LOGI("inst=%u READ GATEWAY_STATUS=%s", gwDataP->instanceId, gwDataP->status);
        return COAP_205_CONTENT;

    case RES_O_FIRMWARE_VERSION:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_string(gwDataP->firmware_version, dataP);
        GATEWAY_LOGI("inst=%u READ FIRMWARE_VERSION=%s", gwDataP->instanceId, gwDataP->firmware_version);
        return COAP_205_CONTENT;

    case RES_O_AUTO_REGISTRATION:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_bool(gwDataP->auto_registration, dataP);
        GATEWAY_LOGI("inst=%u READ AUTO_REGISTRATION=%s", gwDataP->instanceId, gwDataP->auto_registration ? "true" : "false");
        return COAP_205_CONTENT;

    case RES_E_RESET_STATISTICS:
    case RES_E_REBOOT_GATEWAY:
        return COAP_405_METHOD_NOT_ALLOWED;

    default:
        return COAP_404_NOT_FOUND;
    }
}

static uint8_t prv_gateway_read(lwm2m_context_t *contextP,
                               uint16_t instanceId,
                               int * numDataP,
                               lwm2m_data_t ** dataArrayP,
                               lwm2m_object_t * objectP)
{
    uint8_t result;
    int i;
    gateway_instance_t * targetP;

    /* unused parameter */
    (void)contextP;

    // Find the requested instance
    targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP)
    {
        return COAP_404_NOT_FOUND;
    }

    // is the server asking for the full object ?
    if (*numDataP == 0)
    {
        uint16_t resList[] = {
                RES_O_GATEWAY_ID,
                RES_O_CONNECTED_DEVICES,
                RES_M_MAX_DEVICES,
                RES_O_ACTIVE_SESSIONS,
                RES_O_TOTAL_DATA_RX,
                RES_O_TOTAL_DATA_TX,
                RES_M_UPTIME,
                RES_M_GATEWAY_STATUS,
                RES_O_FIRMWARE_VERSION,
                RES_O_AUTO_REGISTRATION
        };
        int nbRes = sizeof(resList)/sizeof(uint16_t);

        *dataArrayP = lwm2m_data_new(nbRes);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = nbRes;
        for (i = 0 ; i < nbRes ; i++)
        {
            (*dataArrayP)[i].id = resList[i];
        }
    }

    i = 0;
    do
    {
        result = prv_set_value((*dataArrayP) + i, targetP);
        i++;
    } while (i < *numDataP && result == COAP_205_CONTENT);

    return result;
}

static uint8_t prv_gateway_discover(lwm2m_context_t *contextP,
                                   uint16_t instanceId,
                                   int * numDataP,
                                   lwm2m_data_t ** dataArrayP,
                                   lwm2m_object_t * objectP)
{
    uint8_t result;
    int i;
    gateway_instance_t * targetP;

    /* unused parameter */
    (void)contextP;

    // Find the requested instance
    targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP)
    {
        return COAP_404_NOT_FOUND;
    }

    result = COAP_205_CONTENT;

    // is the server asking for the full object ?
    if (*numDataP == 0)
    {
        uint16_t resList[] = {
            RES_O_GATEWAY_ID,
            RES_O_CONNECTED_DEVICES,
            RES_M_MAX_DEVICES,
            RES_O_ACTIVE_SESSIONS,
            RES_O_TOTAL_DATA_RX,
            RES_O_TOTAL_DATA_TX,
            RES_M_UPTIME,
            RES_M_GATEWAY_STATUS,
            RES_O_FIRMWARE_VERSION,
            RES_O_AUTO_REGISTRATION,
            RES_E_RESET_STATISTICS,
            RES_E_REBOOT_GATEWAY
        };
        int nbRes = sizeof(resList) / sizeof(uint16_t);

        *dataArrayP = lwm2m_data_new(nbRes);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = nbRes;
        for (i = 0; i < nbRes; i++)
        {
            (*dataArrayP)[i].id = resList[i];
        }
    }
    else
    {
        for (i = 0; i < *numDataP && result == COAP_205_CONTENT; i++)
        {
            switch ((*dataArrayP)[i].id)
            {
            case RES_O_GATEWAY_ID:
            case RES_O_CONNECTED_DEVICES:
            case RES_M_MAX_DEVICES:
            case RES_O_ACTIVE_SESSIONS:
            case RES_O_TOTAL_DATA_RX:
            case RES_O_TOTAL_DATA_TX:
            case RES_M_UPTIME:
            case RES_M_GATEWAY_STATUS:
            case RES_O_FIRMWARE_VERSION:
            case RES_O_AUTO_REGISTRATION:
            case RES_E_RESET_STATISTICS:
            case RES_E_REBOOT_GATEWAY:
                break;
            default:
                result = COAP_404_NOT_FOUND;
            }
        }
    }

    return result;
}

static uint8_t prv_gateway_write(lwm2m_context_t *contextP,
                                uint16_t instanceId,
                                int numData,
                                lwm2m_data_t * dataArray,
                                lwm2m_object_t * objectP,
                                lwm2m_write_type_t writeType)
{
    int i;
    uint8_t result;
    gateway_instance_t * targetP;

    /* unused parameter */
    (void)contextP;
    (void)writeType;

    // Find the requested instance
    targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP)
    {
        return COAP_404_NOT_FOUND;
    }

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
        case RES_M_MAX_DEVICES:
        {
            int64_t value;
            if (1 == lwm2m_data_decode_int(dataArray + i, &value))
            {
                if (value >= 1 && value <= 10000) // reasonable limits
                {
                    targetP->max_devices = value;
                    result = COAP_204_CHANGED;
                    GATEWAY_LOGI("inst=%u WRITE MAX_DEVICES=%lld", instanceId, (long long)value);
                }
                else
                {
                    result = COAP_400_BAD_REQUEST;
                }
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;
        }

        case RES_M_GATEWAY_STATUS:
        {
            if (dataArray[i].value.asBuffer.length < sizeof(targetP->status))
            {
                strncpy(targetP->status, (char*)dataArray[i].value.asBuffer.buffer, dataArray[i].value.asBuffer.length);
                targetP->status[dataArray[i].value.asBuffer.length] = 0;
                result = COAP_204_CHANGED;
                GATEWAY_LOGI("inst=%u WRITE GATEWAY_STATUS=%s", instanceId, targetP->status);
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;
        }

        case RES_O_AUTO_REGISTRATION:
        {
            bool value;
            if (1 == lwm2m_data_decode_bool(dataArray + i, &value))
            {
                targetP->auto_registration = value;
                result = COAP_204_CHANGED;
                GATEWAY_LOGI("inst=%u WRITE AUTO_REGISTRATION=%s", instanceId, value ? "true" : "false");
            }
            else
            {
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

static uint8_t prv_gateway_execute(lwm2m_context_t *contextP,
                                  uint16_t instanceId,
                                  uint16_t resourceId,
                                  uint8_t * buffer,
                                  int length,
                                  lwm2m_object_t * objectP)
{
    gateway_instance_t * targetP;
    
    /* unused parameter */
    (void)contextP;

    // Find the requested instance
    targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP)
    {
        return COAP_404_NOT_FOUND;
    }

    if (length != 0) return COAP_400_BAD_REQUEST;

    switch (resourceId)
    {
    case RES_E_RESET_STATISTICS:
        GATEWAY_LOGI("inst=%u EXECUTE RESET_STATISTICS", instanceId);
        targetP->total_data_rx = 0;
        targetP->total_data_tx = 0;
        // Reset boot time to current time for uptime calculation
        targetP->uptime = time(NULL);
        return COAP_204_CHANGED;

    case RES_E_REBOOT_GATEWAY:
        GATEWAY_LOGI("inst=%u EXECUTE REBOOT_GATEWAY", instanceId);
        fprintf(stdout, "\n\t GATEWAY REBOOT\r\n\n");
        // Set global reboot flag if it exists
        extern int g_reboot;
        g_reboot = 1;
        return COAP_204_CHANGED;

    default:
        return COAP_405_METHOD_NOT_ALLOWED;
    }
}

void display_gateway_object(lwm2m_object_t * object)
{
    gateway_instance_t * instanceP = (gateway_instance_t *)object->instanceList;
    fprintf(stdout, "  /%u: Gateway object:\r\n", object->objID);
    while (NULL != instanceP)
    {
        fprintf(stdout, "    Instance %u: id: %s, devices: %lld/%lld, status: %s, uptime: %lld\r\n",
                instanceP->instanceId,
                instanceP->gateway_id,
                (long long) instanceP->connected_devices,
                (long long) instanceP->max_devices,
                instanceP->status,
                (long long) instanceP->uptime);
        instanceP = instanceP->next;
    }
}

lwm2m_object_t *get_object_gateway(void) {
    lwm2m_object_t * gatewayObj;

    gatewayObj = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));

    if (NULL != gatewayObj)
    {
        memset(gatewayObj, 0, sizeof(lwm2m_object_t));

        // Assign unique ID for Gateway Management Object
        gatewayObj->objID = GATEWAY_OBJECT_ID;

        // Initialize with empty instance list - instances will be added via gateway_add_instance
        gatewayObj->instanceList = NULL;
        
        // Set callback functions
        gatewayObj->readFunc     = prv_gateway_read;
        gatewayObj->discoverFunc = prv_gateway_discover;
        gatewayObj->writeFunc    = prv_gateway_write;
        gatewayObj->executeFunc  = prv_gateway_execute;
        gatewayObj->userData     = NULL;
    }

    return gatewayObj;
}

void free_object_gateway(lwm2m_object_t * objectP)
{
    gateway_instance_t * instanceP;
    
    while (NULL != objectP->instanceList)
    {
        instanceP = (gateway_instance_t *)objectP->instanceList;
        objectP->instanceList = objectP->instanceList->next;
        lwm2m_free(instanceP);
    }

    lwm2m_free(objectP);
}

// Add a new gateway instance with specified instanceId
uint8_t gateway_add_instance(lwm2m_object_t * objectP, uint16_t instanceId)
{
    gateway_instance_t * targetP;
    
    // Check if instance already exists
    targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL != targetP)
    {
        return COAP_406_NOT_ACCEPTABLE;  // Instance already exists
    }
    
    // Create new instance
    targetP = (gateway_instance_t *)lwm2m_malloc(sizeof(gateway_instance_t));
    if (NULL == targetP)
    {
        return COAP_500_INTERNAL_SERVER_ERROR;
    }
    
    memset(targetP, 0, sizeof(gateway_instance_t));
    targetP->instanceId = instanceId;
    
    // Initialize with default values
    strncpy(targetP->gateway_id, PRV_GATEWAY_ID, sizeof(targetP->gateway_id)-1);
    targetP->gateway_id[sizeof(targetP->gateway_id)-1] = '\0';
    
    targetP->connected_devices = 0;
    targetP->max_devices = PRV_MAX_DEVICES;
    targetP->active_sessions = 0;
    targetP->total_data_rx = 0;
    targetP->total_data_tx = 0;
    targetP->uptime = time(NULL); // Store boot time for uptime calculation
    
    strncpy(targetP->firmware_version, PRV_FIRMWARE_VERSION, sizeof(targetP->firmware_version)-1);
    targetP->firmware_version[sizeof(targetP->firmware_version)-1] = '\0';
    
    strncpy(targetP->status, PRV_STATUS_ACTIVE, sizeof(targetP->status)-1);
    targetP->status[sizeof(targetP->status)-1] = '\0';
    
    targetP->auto_registration = true;
    
    // Add to instance list
    objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, targetP);
    
    GATEWAY_LOGI("Gateway instance %u added", instanceId);
    return COAP_201_CREATED;
}

// Remove a gateway instance
uint8_t gateway_remove_instance(lwm2m_object_t * objectP, uint16_t instanceId)
{
    gateway_instance_t * targetP;
    
    // Find and remove the instance
    objectP->instanceList = lwm2m_list_remove(objectP->instanceList, instanceId, (lwm2m_list_t **)&targetP);
    
    if (NULL == targetP)
    {
        return COAP_404_NOT_FOUND;
    }
    
    lwm2m_free(targetP);
    GATEWAY_LOGI("Gateway instance %u removed", instanceId);
    return COAP_202_DELETED;
}

// Update instance value with validation
uint8_t gateway_update_instance_value(lwm2m_object_t * objectP, uint16_t instanceId, uint16_t resourceId, int64_t value)
{
    gateway_instance_t * targetP;
    
    // Find the requested instance
    targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP)
    {
        return COAP_404_NOT_FOUND;
    }
    
    switch (resourceId)
    {
    case RES_O_CONNECTED_DEVICES:
        if (value >= 0)
        {
            targetP->connected_devices = value;
            return COAP_204_CHANGED;
        }
        return COAP_400_BAD_REQUEST;
        
    case RES_M_MAX_DEVICES:
        if (value >= 1 && value <= 10000)
        {
            targetP->max_devices = value;
            return COAP_204_CHANGED;
        }
        return COAP_400_BAD_REQUEST;
        
    case RES_O_ACTIVE_SESSIONS:
        if (value >= 0)
        {
            targetP->active_sessions = value;
            return COAP_204_CHANGED;
        }
        return COAP_400_BAD_REQUEST;
        
    case RES_O_TOTAL_DATA_RX:
        if (value >= 0)
        {
            targetP->total_data_rx = value;
            return COAP_204_CHANGED;
        }
        return COAP_400_BAD_REQUEST;
        
    case RES_O_TOTAL_DATA_TX:
        if (value >= 0)
        {
            targetP->total_data_tx = value;
            return COAP_204_CHANGED;
        }
        return COAP_400_BAD_REQUEST;
        
    default:
        return COAP_405_METHOD_NOT_ALLOWED;
    }
}

// Update instance string value
uint8_t gateway_update_instance_string(lwm2m_object_t * objectP, uint16_t instanceId, uint16_t resourceId, const char* value)
{
    gateway_instance_t * targetP;
    
    // Find the requested instance
    targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP)
    {
        return COAP_404_NOT_FOUND;
    }
    
    if (value == NULL)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    switch (resourceId)
    {
    case RES_O_GATEWAY_ID:
        if (strlen(value) < sizeof(targetP->gateway_id))
        {
            strcpy(targetP->gateway_id, value);
            return COAP_204_CHANGED;
        }
        return COAP_400_BAD_REQUEST;
        
    case RES_M_GATEWAY_STATUS:
        if (strlen(value) < sizeof(targetP->status))
        {
            strcpy(targetP->status, value);
            return COAP_204_CHANGED;
        }
        return COAP_400_BAD_REQUEST;
        
    case RES_O_FIRMWARE_VERSION:
        if (strlen(value) < sizeof(targetP->firmware_version))
        {
            strcpy(targetP->firmware_version, value);
            return COAP_204_CHANGED;
        }
        return COAP_400_BAD_REQUEST;
        
    default:
        return COAP_405_METHOD_NOT_ALLOWED;
    }
}

// Update instance boolean value
uint8_t gateway_update_instance_bool(lwm2m_object_t * objectP, uint16_t instanceId, uint16_t resourceId, bool value)
{
    gateway_instance_t * targetP;
    
    // Find the requested instance
    targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP)
    {
        return COAP_404_NOT_FOUND;
    }
    
    switch (resourceId)
    {
    case RES_O_AUTO_REGISTRATION:
        targetP->auto_registration = value;
        return COAP_204_CHANGED;
        
    default:
        return COAP_405_METHOD_NOT_ALLOWED;
    }
}

// Getter functions for external access
int gateway_get_connected_devices(lwm2m_object_t * objectP, uint16_t instanceId, int64_t *out)
{
    gateway_instance_t * targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (!targetP || !out) return -1;
    *out = targetP->connected_devices;
    return 0;
}

int gateway_get_active_sessions(lwm2m_object_t * objectP, uint16_t instanceId, int64_t *out)
{
    gateway_instance_t * targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (!targetP || !out) return -1;
    *out = targetP->active_sessions;
    return 0;
}

int gateway_get_uptime(lwm2m_object_t * objectP, uint16_t instanceId, int64_t *out)
{
    gateway_instance_t * targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (!targetP || !out) return -1;
    *out = time(NULL) - targetP->uptime; // uptime field stores boot time
    return 0;
}

int gateway_get_status(lwm2m_object_t * objectP, uint16_t instanceId, char *buffer, size_t bufLen)
{
    gateway_instance_t * targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (!targetP || !buffer || bufLen == 0) return -1;
    strncpy(buffer, targetP->status, bufLen - 1);
    buffer[bufLen - 1] = '\0';
    return 0;
}

// Statistics update helpers
uint8_t gateway_increment_rx_data(lwm2m_object_t * objectP, uint16_t instanceId, uint64_t bytes)
{
    gateway_instance_t * targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (!targetP) return COAP_404_NOT_FOUND;
    
    targetP->total_data_rx += bytes;
    return COAP_204_CHANGED;
}

uint8_t gateway_increment_tx_data(lwm2m_object_t * objectP, uint16_t instanceId, uint64_t bytes)
{
    gateway_instance_t * targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (!targetP) return COAP_404_NOT_FOUND;
    
    targetP->total_data_tx += bytes;
    return COAP_204_CHANGED;
}