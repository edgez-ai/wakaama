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
 * This Gateway Device Object (ID: 25) provides information about
 * individual devices connected to the LwM2M gateway.
 *
 * Resources:
 *
 *        Name                    | ID | Oper. | Inst. | Mand.| Type    | Range | Units |
 * Device ID                     |  0 | R     | Single| Yes  | Integer |       |       |
 * Instance ID                   |  1 | RW    | Single| Yes  | Integer | 0-65535|      |
 * Connection Type               |  2 | R     | Single| Yes  | Integer | 0-3   |       |
 * Last Seen                     |  3 | R     | Single| Yes  | Time    |       | s     |
 * Online                        |  4 | R     | Single| Yes  | Boolean |       |       |
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
#define PRV_DEFAULT_DEVICE_ID        0
#define PRV_DEFAULT_CONNECTION_TYPE  CONNECTION_WIFI

// Resource IDs
#define RES_O_DEVICE_ID             0
#define RES_M_INSTANCE_ID           1
#define RES_O_CONNECTION_TYPE       2
#define RES_O_LAST_SEEN             3
#define RES_O_ONLINE                4

static uint8_t prv_set_value(lwm2m_data_t * dataP,
                             gateway_instance_t * gwDataP)
{
    // Handle resource reads
    switch (dataP->id)
    {
    case RES_O_DEVICE_ID:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_int(gwDataP->device_id, dataP);
        GATEWAY_LOGI("inst=%u READ DEVICE_ID=%u", gwDataP->instanceId, gwDataP->device_id);
        return COAP_205_CONTENT;

    case RES_M_INSTANCE_ID:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_int(gwDataP->server_instance_id, dataP);
        // Use debug level to reduce log spam for frequent reads
#ifdef ESP_PLATFORM
        ESP_LOGD("LWM2M_GATEWAY", "inst=%u READ INSTANCE_ID=%u", gwDataP->instanceId, gwDataP->server_instance_id);
#endif
        return COAP_205_CONTENT;

    case RES_O_CONNECTION_TYPE:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_int(gwDataP->connection_type, dataP);
        GATEWAY_LOGI("inst=%u READ CONNECTION_TYPE=%d", gwDataP->instanceId, gwDataP->connection_type);
        return COAP_205_CONTENT;

    case RES_O_LAST_SEEN:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_int(gwDataP->last_seen, dataP);
        GATEWAY_LOGI("inst=%u READ LAST_SEEN=%lld", gwDataP->instanceId, (long long)gwDataP->last_seen);
        return COAP_205_CONTENT;

    case RES_O_ONLINE:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_bool(gwDataP->online, dataP);
        GATEWAY_LOGI("inst=%u READ ONLINE=%s", gwDataP->instanceId, gwDataP->online ? "true" : "false");
        return COAP_205_CONTENT;

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
                RES_O_DEVICE_ID,
                RES_M_INSTANCE_ID,
                RES_O_CONNECTION_TYPE,
                RES_O_LAST_SEEN,
                RES_O_ONLINE
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
            RES_O_DEVICE_ID,
            RES_M_INSTANCE_ID,
            RES_O_CONNECTION_TYPE,
            RES_O_LAST_SEEN,
            RES_O_ONLINE
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
            case RES_O_DEVICE_ID:
            case RES_M_INSTANCE_ID:
            case RES_O_CONNECTION_TYPE:
            case RES_O_LAST_SEEN:
            case RES_O_ONLINE:
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
        case RES_M_INSTANCE_ID:
        {
            int64_t value;
            if (1 == lwm2m_data_decode_int(dataArray + i, &value))
            {
                if (value >= 0 && value <= 65535) // 16-bit unsigned int range
                {
                    uint16_t new_instance_id = (uint16_t)value;
                    
                    // Get callbacks from userData
                    gateway_callbacks_t *callbacks = (gateway_callbacks_t *)objectP->userData;
                    
                    // Store old values for instance recreation
                    uint32_t device_id = targetP->device_id;
                    connection_type_t conn_type = targetP->connection_type;
                    uint16_t old_instanceId = targetP->instanceId;
                    
                    // Update the server-assigned instance ID
                    targetP->server_instance_id = new_instance_id;
                    
                    // Call the device update callback if set
                    if (callbacks != NULL && callbacks->device_update_callback != NULL) {
                        callbacks->device_update_callback(device_id, new_instance_id);
                        GATEWAY_LOGI("Device ring buffer updated for device serial %u", device_id);
                    }
                    
                    // Remove current instance and recreate with new instance_id
                    // This ensures the LwM2M server sees the instance as "new" with the updated ID
                    GATEWAY_LOGI("Recreating instance %u->%u for device %u", old_instanceId, new_instance_id, device_id);
                    
                    // Remove the current instance from the list
                    objectP->instanceList = lwm2m_list_remove(objectP->instanceList, old_instanceId, (lwm2m_list_t **)&targetP);
                    if (targetP != NULL) {
                        lwm2m_free(targetP);
                    }
                    
                    // Create a new instance with the new instance_id
                    gateway_add_instance(objectP, new_instance_id, device_id, conn_type);
                    
                    // Trigger registration update to notify server of instance changes
                    if (callbacks != NULL && callbacks->registration_update_callback != NULL) {
                        callbacks->registration_update_callback();
                        GATEWAY_LOGI("Registration update triggered for instance recreation");
                    }
                    
                    result = COAP_204_CHANGED;
                    GATEWAY_LOGI("inst=%u WRITE INSTANCE_ID=%u (instance recreated)", old_instanceId, new_instance_id);
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
        fprintf(stdout, "    Instance %u: device_id: %lu, server_instance_id: %u, conn_type: %d, online: %s, last_seen: %lld\r\n",
                instanceP->instanceId,
                (unsigned long)instanceP->device_id,
                instanceP->server_instance_id,
                instanceP->connection_type,
                instanceP->online ? "true" : "false",
                (long long)instanceP->last_seen);
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
        
        // Allocate callback structure
        gateway_callbacks_t *callbacks = (gateway_callbacks_t *)lwm2m_malloc(sizeof(gateway_callbacks_t));
        if (callbacks != NULL) {
            memset(callbacks, 0, sizeof(gateway_callbacks_t));
            gatewayObj->userData = callbacks;
        } else {
            gatewayObj->userData = NULL;
        }
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

    // Free callback structure
    if (objectP->userData != NULL) {
        lwm2m_free(objectP->userData);
    }

    lwm2m_free(objectP);
}

// Add a new device instance with specified instanceId and device information
uint8_t gateway_add_instance(lwm2m_object_t * objectP, uint16_t instanceId, uint32_t device_id, connection_type_t conn_type)
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
    
    // Initialize device-specific values
    targetP->device_id = device_id;
    targetP->server_instance_id = instanceId; // Initialize with same value as instanceId
    targetP->connection_type = conn_type;
    targetP->last_seen = time(NULL);  // Current timestamp
    targetP->online = true;           // Assume online when added
    
    // Add to instance list
    objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, targetP);
    
    GATEWAY_LOGI("Device instance %u added: device_id=%u, conn_type=%d", instanceId, device_id, conn_type);
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

// Update device status (last_seen and online status)
uint8_t gateway_update_device_status(lwm2m_object_t * objectP, uint16_t instanceId, bool online)
{
    gateway_instance_t * targetP;
    
    targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP)
    {
        return COAP_404_NOT_FOUND;
    }
    
    targetP->online = online;
    targetP->last_seen = time(NULL);  // Update timestamp
    
    GATEWAY_LOGI("Device instance %u status updated: online=%s", instanceId, online ? "true" : "false");
    return COAP_204_CHANGED;
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
    case RES_M_INSTANCE_ID:
        if (value >= 0 && value <= 65535)
        {
            targetP->server_instance_id = (uint16_t)value;
            return COAP_204_CHANGED;
        }
        return COAP_400_BAD_REQUEST;
        
    case RES_O_DEVICE_ID:
        targetP->device_id = (uint32_t)value;
        return COAP_204_CHANGED;
        
    case RES_O_CONNECTION_TYPE:
        if (value >= 0 && value <= 3) // Valid connection types
        {
            targetP->connection_type = (connection_type_t)value;
            return COAP_204_CHANGED;
        }
        return COAP_400_BAD_REQUEST;
        
    case RES_O_LAST_SEEN:
        targetP->last_seen = (time_t)value;
        return COAP_204_CHANGED;
        
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
    case RES_O_ONLINE:
        targetP->online = value;
        return COAP_204_CHANGED;
        
    default:
        return COAP_405_METHOD_NOT_ALLOWED;
    }
}

// Getter functions for external access
int gateway_get_device_id(lwm2m_object_t * objectP, uint16_t instanceId, uint32_t *out)
{
    gateway_instance_t * targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (!targetP || !out) return -1;
    *out = targetP->device_id;
    return 0;
}

int gateway_get_connection_type(lwm2m_object_t * objectP, uint16_t instanceId, connection_type_t *out)
{
    gateway_instance_t * targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (!targetP || !out) return -1;
    *out = targetP->connection_type;
    return 0;
}

int gateway_get_last_seen(lwm2m_object_t * objectP, uint16_t instanceId, time_t *out)
{
    gateway_instance_t * targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (!targetP || !out) return -1;
    *out = targetP->last_seen;
    return 0;
}

int gateway_get_online_status(lwm2m_object_t * objectP, uint16_t instanceId, bool *out)
{
    gateway_instance_t * targetP = (gateway_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (!targetP || !out) return -1;
    *out = targetP->online;
    return 0;
}

// Set callback for device instance_id updates
void gateway_set_device_update_callback(lwm2m_object_t * objectP, gateway_device_update_callback_t callback)
{
    if (objectP != NULL && objectP->userData != NULL) {
        gateway_callbacks_t *callbacks = (gateway_callbacks_t *)objectP->userData;
        callbacks->device_update_callback = callback;
        GATEWAY_LOGI("Device update callback set for gateway object");
    }
}

// Set callback for triggering registration updates
void gateway_set_registration_update_callback(lwm2m_object_t * objectP, gateway_registration_update_callback_t callback)
{
    if (objectP != NULL && objectP->userData != NULL) {
        gateway_callbacks_t *callbacks = (gateway_callbacks_t *)objectP->userData;
        callbacks->registration_update_callback = callback;
        GATEWAY_LOGI("Registration update callback set for gateway object");
    }
}