/*******************************************************************************
 *
 * Copyright (c) 2014 Bosch Software Innovations GmbH Germany.
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
 *    Bosch Software Innovations GmbH - Please refer to git log
 *    Pascal Rieux - Please refer to git log
 *    Scott Bertin, AMETEK, Inc. - Please refer to git log
 *
 *******************************************************************************/

/*
 *  This Connectivity Monitoring object is optional and has a single instance
 * 
 *  Resources:
 *
 *          Name             | ID | Oper. | Inst. | Mand.|  Type   | Range | Units |
 *  Network Bearer           |  0 |  R    | Single|  Yes | Integer |       |       |
 *  Available Network Bearer |  1 |  R    | Multi |  Yes | Integer |       |       |
 *  Radio Signal Strength    |  2 |  R    | Single|  Yes | Integer |       | dBm   |
 *  Link Quality             |  3 |  R    | Single|  No  | Integer | 0-100 |   %   |
 *  IP Addresses             |  4 |  R    | Multi |  Yes | String  |       |       |
 *  Router IP Addresses      |  5 |  R    | Multi |  No  | String  |       |       |
 *  Link Utilization         |  6 |  R    | Single|  No  | Integer | 0-100 |   %   |
 *  APN                      |  7 |  R    | Multi |  No  | String  |       |       |
 *  Cell ID                  |  8 |  R    | Single|  No  | Integer |       |       |
 *  SMNC                     |  9 |  R    | Single|  No  | Integer | 0-999 |   %   |
 *  SMCC                     | 10 |  R    | Single|  No  | Integer | 0-999 |       |
 *
 */

#include "liblwm2m.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef ESP_PLATFORM
#include "esp_log.h"
#endif

// Resource Id's:
#define RES_M_NETWORK_BEARER            0
#define RES_M_AVL_NETWORK_BEARER        1
#define RES_M_RADIO_SIGNAL_STRENGTH     2
#define RES_O_LINK_QUALITY              3
#define RES_M_IP_ADDRESSES              4
#define RES_O_ROUTER_IP_ADDRESS         5
#define RES_O_LINK_UTILIZATION          6
#define RES_O_APN                       7
#define RES_O_CELL_ID                   8
#define RES_O_SMNC                      9
#define RES_O_SMCC                      10

#define VALUE_NETWORK_BEARER_GSM    0   //GSM see 
#define VALUE_AVL_NETWORK_BEARER_1  0   //GSM
#define VALUE_AVL_NETWORK_BEARER_2  21  //WLAN
#define VALUE_AVL_NETWORK_BEARER_3  41  //Ethernet
#define VALUE_AVL_NETWORK_BEARER_4  42  //DSL
#define VALUE_AVL_NETWORK_BEARER_5  43  //PLC
#define VALUE_IP_ADDRESS_1              "192.168.178.101"
#define VALUE_IP_ADDRESS_2              "192.168.178.102"
#define VALUE_ROUTER_IP_ADDRESS_1       "192.168.178.001"
#define VALUE_ROUTER_IP_ADDRESS_2       "192.168.178.002"
#define VALUE_APN_1                     "web.vodafone.de"
#define VALUE_APN_2                     "cda.vodafone.de"
#define VALUE_CELL_ID                   69696969
#define VALUE_RADIO_SIGNAL_STRENGTH     80                  //dBm
#define VALUE_LINK_QUALITY              98     
#define VALUE_LINK_UTILIZATION          666
#define VALUE_SMNC                      33
#define VALUE_SMCC                      44



// Extended structure for multiple instance support
typedef struct _conn_m_instance_t
{
    struct _conn_m_instance_t * next;   // matches lwm2m_list_t::next
    uint16_t shortID;                   // matches lwm2m_list_t::id
    char ipAddresses[2][16];            // limited to 2!
    char routerIpAddresses[2][16];      // limited to 2!
    long cellId;
    int signalStrength;
    int linkQuality;
    int linkUtilization;
    uint32_t deviceId;                  // ID of the proxied device
} conn_m_instance_t;



static uint8_t prv_set_value_extended(lwm2m_data_t * dataP, conn_m_instance_t * connDataP)
{
    lwm2m_data_t * subTlvP;
    size_t count;
    size_t i;

    switch (dataP->id)
    {
    case RES_M_NETWORK_BEARER:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_int(VALUE_NETWORK_BEARER_GSM, dataP);
        return COAP_205_CONTENT;

    case RES_M_AVL_NETWORK_BEARER:
    {
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE)
        {
            count = dataP->value.asChildren.count;
            subTlvP = dataP->value.asChildren.array;
        }
        else
        {
            count = 1; // reduced to 1 instance to fit in one block size
            subTlvP = lwm2m_data_new(count);
            for (i = 0; i < count; i++) subTlvP[i].id = i;
            lwm2m_data_encode_instances(subTlvP, count, dataP);
        }

        for (i = 0; i < count; i++)
        {
            switch (subTlvP[i].id)
            {
            case 0:
                lwm2m_data_encode_int(VALUE_AVL_NETWORK_BEARER_1, subTlvP + i);
                break;
            default:
                return COAP_404_NOT_FOUND;
            }
        }
        return COAP_205_CONTENT ;
    }

    case RES_M_RADIO_SIGNAL_STRENGTH: //s-int
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_int(connDataP->signalStrength, dataP);
        return COAP_205_CONTENT;

    case RES_O_LINK_QUALITY: //s-int
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_int(connDataP->linkQuality, dataP);
        return COAP_205_CONTENT ;

    case RES_M_IP_ADDRESSES:
    {
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE)
        {
            count = dataP->value.asChildren.count;
            subTlvP = dataP->value.asChildren.array;
        }
        else
        {
            count = 1; // reduced to 1 instance to fit in one block size
            subTlvP = lwm2m_data_new(count);
            for (i = 0; i < count; i++) subTlvP[i].id = i;
            lwm2m_data_encode_instances(subTlvP, count, dataP);
        }

        for (i = 0; i < count; i++)
        {
            switch (subTlvP[i].id)
            {
            case 0:
                lwm2m_data_encode_string(connDataP->ipAddresses[i], subTlvP + i);
                break;
            default:
                return COAP_404_NOT_FOUND;
            }
        }
        return COAP_205_CONTENT ;
    }

    case RES_O_ROUTER_IP_ADDRESS:
    {
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE)
        {
            count = dataP->value.asChildren.count;
            subTlvP = dataP->value.asChildren.array;
        }
        else
        {
            count = 1; // reduced to 1 instance to fit in one block size
            subTlvP = lwm2m_data_new(count);
            for (i = 0; i < count; i++) subTlvP[i].id = i;
            lwm2m_data_encode_instances(subTlvP, count, dataP);
        }

        for (i = 0; i < count; i++)
        {
            switch (subTlvP[i].id)
            {
            case 0:
                lwm2m_data_encode_string(connDataP->routerIpAddresses[i], subTlvP + i);
                break;
            default:
                return COAP_404_NOT_FOUND;
            }
        }
        return COAP_205_CONTENT ;
    }

    case RES_O_LINK_UTILIZATION:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_int(connDataP->linkUtilization, dataP);
        return COAP_205_CONTENT;

    case RES_O_APN:
    {
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE)
        {
            count = dataP->value.asChildren.count;
            subTlvP = dataP->value.asChildren.array;
        }
        else
        {
            count = 1; // reduced to 1 instance to fit in one block size
            subTlvP = lwm2m_data_new(count);
            for (i = 0; i < count; i++) subTlvP[i].id = i;
            lwm2m_data_encode_instances(subTlvP, count, dataP);
        }

        for (i = 0; i < count; i++)
        {
            switch (subTlvP[i].id)
            {
            case 0:
                lwm2m_data_encode_string(VALUE_APN_1, subTlvP + i);
                break;
            default:
                return COAP_404_NOT_FOUND;
            }
        }
        return COAP_205_CONTENT;
    }

    case RES_O_CELL_ID:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_int(connDataP->cellId, dataP);
        return COAP_205_CONTENT ;

    case RES_O_SMNC:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_int(VALUE_SMNC, dataP);
        return COAP_205_CONTENT ;

    case RES_O_SMCC:
        if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
        lwm2m_data_encode_int(VALUE_SMCC, dataP);
        return COAP_205_CONTENT ;

    default:
        return COAP_404_NOT_FOUND ;
    }
}

static uint8_t prv_read(lwm2m_context_t *contextP,
                        uint16_t instanceId,
                        int * numDataP,
                        lwm2m_data_t ** dataArrayP,
                        lwm2m_object_t * objectP)
{
    uint8_t result;
    int i;
    conn_m_instance_t * targetP;

    /* unused parameter */
    (void)contextP;

    // Always use multi-instance mode - find the instance in the extended instance list
    targetP = (conn_m_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    
    // If not found, return 404
    if (NULL == targetP)
    {
        // Debug: Log which instances are available
#ifdef ESP_PLATFORM
        ESP_LOGW("CONN_MONI", "Instance %u not found. Available instances:", instanceId);
#else
        printf("CONN_MONI: Instance %u not found. Available instances:\n", instanceId);
#endif
        conn_m_instance_t * debugP = (conn_m_instance_t *)objectP->instanceList;
        int count = 0;
        while (debugP != NULL) {
#ifdef ESP_PLATFORM
            ESP_LOGW("CONN_MONI", "  Instance %u (device %u)", debugP->shortID, debugP->deviceId);
#else
            printf("CONN_MONI:   Instance %u (device %u)\n", debugP->shortID, debugP->deviceId);
#endif
            debugP = debugP->next;
            count++;
        }
#ifdef ESP_PLATFORM
        ESP_LOGW("CONN_MONI", "Total instances: %d", count);
#else
        printf("CONN_MONI: Total instances: %d\n", count);
#endif
        return COAP_404_NOT_FOUND;
    }

    // is the server asking for the full object ?
    if (*numDataP == 0)
    {
        uint16_t resList[] = {
                RES_M_NETWORK_BEARER,
                RES_M_AVL_NETWORK_BEARER,
                RES_M_RADIO_SIGNAL_STRENGTH,
                RES_O_LINK_QUALITY,
                RES_M_IP_ADDRESSES,
                RES_O_ROUTER_IP_ADDRESS,
                RES_O_LINK_UTILIZATION,
                RES_O_APN,
                RES_O_CELL_ID,
                RES_O_SMNC,
                RES_O_SMCC
        };
        int nbRes = sizeof(resList) / sizeof(uint16_t);

        *dataArrayP = lwm2m_data_new(nbRes);
        if (*dataArrayP == NULL)
            return COAP_500_INTERNAL_SERVER_ERROR ;
        *numDataP = nbRes;
        for (i = 0; i < nbRes; i++)
        {
            (*dataArrayP)[i].id = resList[i];
        }
    }

    i = 0;
    do
    {
        result = prv_set_value_extended((*dataArrayP) + i, targetP);
        i++;
    } while (i < *numDataP && result == COAP_205_CONTENT );

    return result;
}

lwm2m_object_t * get_object_conn_m(void)
{
    /*
     * The get_object_conn_m() function create the object itself and return a pointer to the structure that represent it.
     */
    lwm2m_object_t * connObj;
    conn_m_instance_t * instanceP;

    connObj = (lwm2m_object_t *) lwm2m_malloc(sizeof(lwm2m_object_t));

    if (NULL != connObj)
    {
        memset(connObj, 0, sizeof(lwm2m_object_t));

        /*
         * It assigns his unique ID
         */
        connObj->objID = LWM2M_CONN_MONITOR_OBJECT_ID;
        
        /*
         * Initialize empty instance list - instances will be added via connectivity_moni_add_instance
         */
        connObj->instanceList = NULL;
        
        /*
         * And the private function that will access the object.
         * Those function will be called when a read/write/execute query is made by the server. In fact the library don't need to
         * know the resources of the object, only the server does.
         */
        connObj->readFunc = prv_read;
        connObj->executeFunc = NULL;
        connObj->userData = NULL;  // Use multi-instance mode only
        
        // Always create default instance 0 for the gateway itself
        instanceP = (conn_m_instance_t *)lwm2m_malloc(sizeof(conn_m_instance_t));
        if (NULL != instanceP)
        {
            memset(instanceP, 0, sizeof(conn_m_instance_t));
            instanceP->shortID = 0;
            instanceP->deviceId = 0;  // Gateway device ID
            
            // Set default values
            instanceP->cellId = VALUE_CELL_ID;
            instanceP->signalStrength = VALUE_RADIO_SIGNAL_STRENGTH;
            instanceP->linkQuality = VALUE_LINK_QUALITY;
            instanceP->linkUtilization = VALUE_LINK_UTILIZATION;
            strcpy(instanceP->ipAddresses[0], VALUE_IP_ADDRESS_1);
            strcpy(instanceP->ipAddresses[1], VALUE_IP_ADDRESS_2);
            strcpy(instanceP->routerIpAddresses[0], VALUE_ROUTER_IP_ADDRESS_1);
            strcpy(instanceP->routerIpAddresses[1], VALUE_ROUTER_IP_ADDRESS_2);
            
            // Add to instance list
            connObj->instanceList = LWM2M_LIST_ADD(connObj->instanceList, instanceP);
        }
        else
        {
            lwm2m_free(connObj);
            return NULL;
        }
    }
    return connObj;
}

void free_object_conn_m(lwm2m_object_t * objectP)
{
    // Free all instances
    conn_m_instance_t * instanceP = (conn_m_instance_t *)objectP->instanceList;
    while (instanceP != NULL)
    {
        conn_m_instance_t * nextP = instanceP->next;
        lwm2m_free(instanceP);
        instanceP = nextP;
    }
    
    lwm2m_free(objectP->userData);
    lwm2m_free(objectP);
}

uint8_t connectivity_moni_change(lwm2m_data_t * dataArray,
                                 lwm2m_object_t * objectP)
{
    // This function is deprecated in multi-instance mode
    // Use connectivity_moni_update_* functions instead
    return COAP_405_METHOD_NOT_ALLOWED;
}

// Add a new connectivity monitoring instance for a device
uint8_t connectivity_moni_add_instance(lwm2m_object_t * objectP, uint16_t instanceId, uint32_t deviceId)
{
    conn_m_instance_t * instanceP;

    if (NULL == objectP)
    {
#ifdef ESP_PLATFORM
        ESP_LOGE("CONN_MONI", "Object pointer is NULL");
#else
        printf("CONN_MONI ERROR: Object pointer is NULL\n");
#endif
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    // Check if instance already exists
    instanceP = (conn_m_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL != instanceP)
    {
#ifdef ESP_PLATFORM
        ESP_LOGW("CONN_MONI", "Instance %u already exists for device %u (existing device %u)", 
                 instanceId, deviceId, instanceP->deviceId);
#else
        printf("CONN_MONI WARN: Instance %u already exists for device %u (existing device %u)\n", 
               instanceId, deviceId, instanceP->deviceId);
#endif
        
        // If it's the same device, just update and return success
        if (instanceP->deviceId == deviceId) {
#ifdef ESP_PLATFORM
            ESP_LOGI("CONN_MONI", "Instance %u already exists for same device %u, skipping", instanceId, deviceId);
#else
            printf("CONN_MONI INFO: Instance %u already exists for same device %u, skipping\n", instanceId, deviceId);
#endif
            return COAP_204_CHANGED; // Changed instead of error
        }
        
        return COAP_406_NOT_ACCEPTABLE; // Instance already exists for different device
    }

    // Create new instance
    instanceP = (conn_m_instance_t *)lwm2m_malloc(sizeof(conn_m_instance_t));
    if (NULL == instanceP)
    {
#ifdef ESP_PLATFORM
        ESP_LOGE("CONN_MONI", "Failed to allocate memory for instance %u", instanceId);
#else
        printf("CONN_MONI ERROR: Failed to allocate memory for instance %u\n", instanceId);
#endif
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    memset(instanceP, 0, sizeof(conn_m_instance_t));
    instanceP->shortID = instanceId;
    instanceP->deviceId = deviceId;
    
    // Set default values
    instanceP->cellId = VALUE_CELL_ID;
    instanceP->signalStrength = VALUE_RADIO_SIGNAL_STRENGTH;
    instanceP->linkQuality = VALUE_LINK_QUALITY;
    instanceP->linkUtilization = VALUE_LINK_UTILIZATION;
    strcpy(instanceP->ipAddresses[0], VALUE_IP_ADDRESS_1);
    strcpy(instanceP->ipAddresses[1], VALUE_IP_ADDRESS_2);
    strcpy(instanceP->routerIpAddresses[0], VALUE_ROUTER_IP_ADDRESS_1);
    strcpy(instanceP->routerIpAddresses[1], VALUE_ROUTER_IP_ADDRESS_2);

    // Add to instance list
    objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, instanceP);

#ifdef ESP_PLATFORM
    ESP_LOGI("CONN_MONI", "Created connectivity monitoring instance %u for device %u", instanceId, deviceId);
#else
    printf("CONN_MONI INFO: Created connectivity monitoring instance %u for device %u\n", instanceId, deviceId);
#endif
    return COAP_201_CREATED;
}

// Remove a connectivity monitoring instance
uint8_t connectivity_moni_remove_instance(lwm2m_object_t * objectP, uint16_t instanceId)
{
    conn_m_instance_t * instanceP;

    if (NULL == objectP)
    {
#ifdef ESP_PLATFORM
        ESP_LOGE("CONN_MONI", "Object pointer is NULL when removing instance %u", instanceId);
#else
        printf("CONN_MONI ERROR: Object pointer is NULL when removing instance %u\n", instanceId);
#endif
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    instanceP = (conn_m_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == instanceP)
    {
#ifdef ESP_PLATFORM
        ESP_LOGW("CONN_MONI", "Instance %u not found for removal", instanceId);
#else
        printf("CONN_MONI WARN: Instance %u not found for removal\n", instanceId);
#endif
        return COAP_404_NOT_FOUND;
    }

#ifdef ESP_PLATFORM
    ESP_LOGI("CONN_MONI", "Removing connectivity monitoring instance %u (device %u)", instanceId, instanceP->deviceId);
#else
    printf("CONN_MONI INFO: Removing connectivity monitoring instance %u (device %u)\n", instanceId, instanceP->deviceId);
#endif
    
    objectP->instanceList = LWM2M_LIST_RM(objectP->instanceList, instanceId, NULL);
    lwm2m_free(instanceP);

    return COAP_202_DELETED;
}

// Update RSSI for a specific device instance
uint8_t connectivity_moni_update_rssi(lwm2m_object_t * objectP, uint16_t instanceId, int rssi)
{
    conn_m_instance_t * instanceP;

    if (NULL == objectP)
    {
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    instanceP = (conn_m_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == instanceP)
    {
        return COAP_404_NOT_FOUND;
    }

    instanceP->signalStrength = rssi;
    return COAP_204_CHANGED;
}

// Update link quality for a specific device instance
uint8_t connectivity_moni_update_link_quality(lwm2m_object_t * objectP, uint16_t instanceId, int linkQuality)
{
    conn_m_instance_t * instanceP;

    if (NULL == objectP)
    {
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    instanceP = (conn_m_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == instanceP)
    {
        return COAP_404_NOT_FOUND;
    }

    instanceP->linkQuality = linkQuality;
    return COAP_204_CHANGED;
}

// Debug function to list all connectivity monitoring instances
void connectivity_moni_debug_instances(lwm2m_object_t * objectP)
{
    if (NULL == objectP) {
#ifdef ESP_PLATFORM
        ESP_LOGW("CONN_MONI", "Debug: Object pointer is NULL");
#else
        printf("CONN_MONI WARN: Debug: Object pointer is NULL\n");
#endif
        return;
    }
    
    conn_m_instance_t * instanceP = (conn_m_instance_t *)objectP->instanceList;
    int count = 0;
    
#ifdef ESP_PLATFORM
    ESP_LOGI("CONN_MONI", "=== Connectivity Monitoring Instances ===");
#else
    printf("CONN_MONI INFO: === Connectivity Monitoring Instances ===\n");
#endif
    while (instanceP != NULL) {
#ifdef ESP_PLATFORM
        ESP_LOGI("CONN_MONI", "Instance %u: device_id=%u, rssi=%d, quality=%d", 
                 instanceP->shortID, instanceP->deviceId, 
                 instanceP->signalStrength, instanceP->linkQuality);
#else
        printf("CONN_MONI INFO: Instance %u: device_id=%u, rssi=%d, quality=%d\n", 
               instanceP->shortID, instanceP->deviceId, 
               instanceP->signalStrength, instanceP->linkQuality);
#endif
        instanceP = instanceP->next;
        count++;
    }
#ifdef ESP_PLATFORM
    ESP_LOGI("CONN_MONI", "Total instances: %d", count);
    ESP_LOGI("CONN_MONI", "=========================================");
#else
    printf("CONN_MONI INFO: Total instances: %d\n", count);
    printf("CONN_MONI INFO: =========================================\n");
#endif
}

