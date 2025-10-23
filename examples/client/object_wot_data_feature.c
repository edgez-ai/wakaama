/*******************************************************************************
 *
 * Copyright (c) 2024 edgez.ai - W3C WoT Data Feature Object
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * Based on LwM2M Object 26251: W3C WoT Data Feature
 *
 *******************************************************************************/

#include "object_wot_data_feature.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"

static const char *TAG = "wot_data_feature";

// Forward declarations
static uint8_t prv_read(lwm2m_context_t *contextP, uint16_t instanceId, int *numDataP, 
                        lwm2m_data_t **dataArrayP, lwm2m_object_t *objectP);
static uint8_t prv_write(lwm2m_context_t *contextP, uint16_t instanceId, int numData, 
                         lwm2m_data_t *dataArray, lwm2m_object_t *objectP, lwm2m_write_type_t writeType);
static uint8_t prv_create(lwm2m_context_t *contextP, uint16_t instanceId, int numData, 
                          lwm2m_data_t *dataArray, lwm2m_object_t *objectP);
static uint8_t prv_delete(lwm2m_context_t *contextP, uint16_t instanceId, lwm2m_object_t *objectP);

// Helper functions
static wot_data_feature_instance_t * prv_find_instance(lwm2m_object_t *objectP, uint16_t instanceId)
{
    wot_data_feature_instance_t *targetP = (wot_data_feature_instance_t *)objectP->instanceList;
    
    while (targetP != NULL && targetP->instanceId != instanceId)
    {
        targetP = targetP->next;
    }
    
    return targetP;
}

static wot_data_feature_instance_t * prv_create_instance(uint16_t instanceId)
{
    wot_data_feature_instance_t *instanceP = (wot_data_feature_instance_t *)malloc(sizeof(wot_data_feature_instance_t));
    
    if (instanceP != NULL)
    {
        memset(instanceP, 0, sizeof(wot_data_feature_instance_t));
        instanceP->instanceId = instanceId;
        instanceP->last_updated = time(NULL);
    }
    
    return instanceP;
}

static uint8_t prv_read(lwm2m_context_t *contextP, uint16_t instanceId, int *numDataP, 
                        lwm2m_data_t **dataArrayP, lwm2m_object_t *objectP)
{
    wot_data_feature_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    
    if (instanceP == NULL)
    {
        return COAP_404_NOT_FOUND;
    }
    
    // If no specific resource is requested, return all readable resources
    if (*numDataP == 0)
    {
        *dataArrayP = lwm2m_data_new(4);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = 4;
        
        (*dataArrayP)[0].id = RES_WOT_FEATURE_IDENTIFIER;
        (*dataArrayP)[1].id = RES_WOT_LINKED_RESOURCES;
        (*dataArrayP)[2].id = RES_WOT_OWNING_THING;
        (*dataArrayP)[3].id = RES_WOT_FEATURE_LAST_UPDATED;
    }
    
    for (int i = 0; i < *numDataP; i++)
    {
        switch ((*dataArrayP)[i].id)
        {
            case RES_WOT_FEATURE_IDENTIFIER:
                lwm2m_data_encode_string(instanceP->feature_identifier, &((*dataArrayP)[i]));
                break;
                
            case RES_WOT_LINKED_RESOURCES:
                if (instanceP->linked_resources_count > 0)
                {
                    (*dataArrayP)[i].type = LWM2M_TYPE_MULTIPLE_RESOURCE;
                    (*dataArrayP)[i].value.asChildren.count = instanceP->linked_resources_count;
                    (*dataArrayP)[i].value.asChildren.array = lwm2m_data_new(instanceP->linked_resources_count);
                    if ((*dataArrayP)[i].value.asChildren.array == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
                    
                    for (int j = 0; j < instanceP->linked_resources_count; j++)
                    {
                        (*dataArrayP)[i].value.asChildren.array[j].id = j;
                        lwm2m_data_encode_string(instanceP->linked_resources[j], 
                                                &((*dataArrayP)[i].value.asChildren.array[j]));
                    }
                }
                else
                {
                    (*dataArrayP)[i].type = LWM2M_TYPE_UNDEFINED;
                }
                break;
                
            case RES_WOT_OWNING_THING:
                if (instanceP->has_owning_thing)
                {
                    lwm2m_data_encode_objlink(instanceP->owning_thing_obj_id, 
                                             instanceP->owning_thing_instance_id, 
                                             &((*dataArrayP)[i]));
                }
                else
                {
                    (*dataArrayP)[i].type = LWM2M_TYPE_UNDEFINED;
                }
                break;
                
            case RES_WOT_FEATURE_LAST_UPDATED:
                lwm2m_data_encode_int(instanceP->last_updated, &((*dataArrayP)[i]));
                break;
                
            default:
                return COAP_404_NOT_FOUND;
        }
    }
    
    return COAP_205_CONTENT;
}

static uint8_t prv_write(lwm2m_context_t *contextP, uint16_t instanceId, int numData, 
                         lwm2m_data_t *dataArray, lwm2m_object_t *objectP, lwm2m_write_type_t writeType)
{
    ESP_LOGI(TAG, "🔽 BOOTSTRAP WRITE - WoT Data Feature Object (26251) - Instance %d, Resources: %d, WriteType: %d", 
             instanceId, numData, writeType);
    
    wot_data_feature_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    
    if (instanceP == NULL)
    {
        ESP_LOGW(TAG, "🔽 BOOTSTRAP WRITE - Instance %d not found, returning 404", instanceId);
        return COAP_404_NOT_FOUND;
    }
    
    for (int i = 0; i < numData; i++)
    {
        ESP_LOGI(TAG, "🔽 BOOTSTRAP WRITE - Resource %d, Type: %d", dataArray[i].id, dataArray[i].type);
        
        switch (dataArray[i].id)
        {
            case RES_WOT_FEATURE_IDENTIFIER:
                // Allow writes during bootstrap/creation, otherwise read-only
                if (dataArray[i].type == LWM2M_TYPE_STRING)
                {
                    size_t len = dataArray[i].value.asBuffer.length;
                    if (len >= sizeof(instanceP->feature_identifier)) len = sizeof(instanceP->feature_identifier) - 1;
                    memcpy(instanceP->feature_identifier, dataArray[i].value.asBuffer.buffer, len);
                    instanceP->feature_identifier[len] = '\0';
                    instanceP->last_updated = time(NULL);
                    ESP_LOGI(TAG, "🔽 BOOTSTRAP WRITE - Feature Identifier set to: %s", instanceP->feature_identifier);
                }
                else
                {
                    return COAP_400_BAD_REQUEST;
                }
                break;
                
            case RES_WOT_LINKED_RESOURCES:
                // Handle multiple resource array updates
                if (dataArray[i].type == LWM2M_TYPE_MULTIPLE_RESOURCE)
                {
                    int count = dataArray[i].value.asChildren.count;
                    if (count > MAX_WOT_LINKED_RESOURCES) count = MAX_WOT_LINKED_RESOURCES;
                    
                    // Clear existing resources
                    instanceP->linked_resources_count = 0;
                    
                    // Add new resources
                    for (int j = 0; j < count; j++)
                    {
                        if (dataArray[i].value.asChildren.array[j].type == LWM2M_TYPE_STRING)
                        {
                            size_t len = dataArray[i].value.asChildren.array[j].value.asBuffer.length;
                            if (len >= sizeof(instanceP->linked_resources[j])) 
                                len = sizeof(instanceP->linked_resources[j]) - 1;
                            
                            memcpy(instanceP->linked_resources[j], 
                                   dataArray[i].value.asChildren.array[j].value.asBuffer.buffer, len);
                            instanceP->linked_resources[j][len] = '\0';
                            instanceP->linked_resources_count++;
                        }
                    }
                    instanceP->last_updated = time(NULL);
                }
                else if (dataArray[i].type == LWM2M_TYPE_STRING)
                {
                    // Single resource update - replace all linked resources with this one
                    size_t len = dataArray[i].value.asBuffer.length;
                    if (len >= sizeof(instanceP->linked_resources[0])) 
                        len = sizeof(instanceP->linked_resources[0]) - 1;
                    
                    memcpy(instanceP->linked_resources[0], dataArray[i].value.asBuffer.buffer, len);
                    instanceP->linked_resources[0][len] = '\0';
                    instanceP->linked_resources_count = 1;
                    instanceP->last_updated = time(NULL);
                }
                else
                {
                    return COAP_400_BAD_REQUEST;
                }
                break;
                
            case RES_WOT_OWNING_THING:
                if (dataArray[i].type == LWM2M_TYPE_OBJECT_LINK)
                {
                    instanceP->owning_thing_obj_id = dataArray[i].value.asObjLink.objectId;
                    instanceP->owning_thing_instance_id = dataArray[i].value.asObjLink.objectInstanceId;
                    instanceP->has_owning_thing = true;
                    instanceP->last_updated = time(NULL);
                }
                else
                {
                    return COAP_400_BAD_REQUEST;
                }
                break;
                
            case RES_WOT_FEATURE_LAST_UPDATED:
                // Read-only resource
                return COAP_405_METHOD_NOT_ALLOWED;
                
            default:
                return COAP_404_NOT_FOUND;
        }
    }
    
    return COAP_204_CHANGED;
}

static uint8_t prv_create(lwm2m_context_t *contextP, uint16_t instanceId, int numData, 
                          lwm2m_data_t *dataArray, lwm2m_object_t *objectP)
{
    ESP_LOGI(TAG, "🔽 BOOTSTRAP CREATE - WoT Data Feature Object (26251) - Instance %d, Resources: %d", 
             instanceId, numData);
    
    wot_data_feature_instance_t *instanceP;
    uint8_t result;
    
    instanceP = prv_find_instance(objectP, instanceId);
    if (instanceP != NULL)
    {
        ESP_LOGW(TAG, "🔽 BOOTSTRAP CREATE - Instance %d already exists", instanceId);
        return COAP_406_NOT_ACCEPTABLE;
    }
    
    instanceP = prv_create_instance(instanceId);
    if (instanceP == NULL)
    {
        return COAP_500_INTERNAL_SERVER_ERROR;
    }
    
    // Set mandatory default values
    strcpy(instanceP->feature_identifier, "");
    
    // Process provided data
    result = prv_write(contextP, instanceId, numData, dataArray, objectP, LWM2M_WRITE_REPLACE_RESOURCES);
    
    if (result == COAP_204_CHANGED)
    {
        // Validate mandatory resources
        if (strlen(instanceP->feature_identifier) == 0 || instanceP->linked_resources_count == 0)
        {
            free(instanceP);
            return COAP_400_BAD_REQUEST;
        }
        
        objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, instanceP);
        result = COAP_201_CREATED;
    }
    else
    {
        free(instanceP);
    }
    
    return result;
}

static uint8_t prv_delete(lwm2m_context_t *contextP, uint16_t instanceId, lwm2m_object_t *objectP)
{
    wot_data_feature_instance_t *instanceP;
    
    objectP->instanceList = lwm2m_list_remove(objectP->instanceList, instanceId, (lwm2m_list_t **)&instanceP);
    if (instanceP == NULL)
    {
        return COAP_404_NOT_FOUND;
    }
    
    free(instanceP);
    return COAP_202_DELETED;
}

lwm2m_object_t * get_object_wot_data_feature(void)
{
    lwm2m_object_t * wotDataFeatureObj;
    
    wotDataFeatureObj = (lwm2m_object_t *)malloc(sizeof(lwm2m_object_t));
    
    if (wotDataFeatureObj != NULL)
    {
        memset(wotDataFeatureObj, 0, sizeof(lwm2m_object_t));
        
        wotDataFeatureObj->objID = WOT_DATA_FEATURE_OBJECT_ID;
        wotDataFeatureObj->readFunc = prv_read;
        wotDataFeatureObj->writeFunc = prv_write;
        wotDataFeatureObj->createFunc = prv_create;
        wotDataFeatureObj->deleteFunc = prv_delete;
        wotDataFeatureObj->executeFunc = NULL; // No executable resources
    }
    
    return wotDataFeatureObj;
}

void free_object_wot_data_feature(lwm2m_object_t * objectP)
{
    if (objectP != NULL)
    {
        while (objectP->instanceList != NULL)
        {
            wot_data_feature_instance_t * instanceP = (wot_data_feature_instance_t *)objectP->instanceList;
            objectP->instanceList = objectP->instanceList->next;
            free(instanceP);
        }
        free(objectP);
    }
}

void display_wot_data_feature_object(lwm2m_object_t * objectP)
{
    ESP_LOGI(TAG, "  /%u: WoT Data Feature object, instances:", objectP->objID);
    
    wot_data_feature_instance_t * instanceP = (wot_data_feature_instance_t *)objectP->instanceList;
    while (instanceP != NULL)
    {
        ESP_LOGI(TAG, "    /%u/%u: id=%s, linked_resources=%u", 
                 objectP->objID, instanceP->instanceId,
                 instanceP->feature_identifier, instanceP->linked_resources_count);
        
        for (int i = 0; i < instanceP->linked_resources_count; i++)
        {
            ESP_LOGI(TAG, "      Resource[%d]: %s", i, instanceP->linked_resources[i]);
        }
        
        if (instanceP->has_owning_thing)
        {
            ESP_LOGI(TAG, "      Owning Thing: %u/%u", 
                     instanceP->owning_thing_obj_id, instanceP->owning_thing_instance_id);
        }
        
        instanceP = instanceP->next;
    }
}

// Public API functions
uint8_t wot_data_feature_add_instance(lwm2m_object_t * objectP, uint16_t instanceId, 
                                      const char* feature_id)
{
    if (objectP == NULL || feature_id == NULL)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    wot_data_feature_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    if (instanceP != NULL)
    {
        return COAP_406_NOT_ACCEPTABLE;
    }
    
    instanceP = prv_create_instance(instanceId);
    if (instanceP == NULL)
    {
        return COAP_500_INTERNAL_SERVER_ERROR;
    }
    
    strncpy(instanceP->feature_identifier, feature_id, sizeof(instanceP->feature_identifier) - 1);
    instanceP->feature_identifier[sizeof(instanceP->feature_identifier) - 1] = '\0';
    
    objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, instanceP);
    
    ESP_LOGI(TAG, "Added WoT Data Feature instance %u: %s", instanceId, feature_id);
    return COAP_201_CREATED;
}

uint8_t wot_data_feature_remove_instance(lwm2m_object_t * objectP, uint16_t instanceId)
{
    if (objectP == NULL)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    wot_data_feature_instance_t *instanceP;
    objectP->instanceList = lwm2m_list_remove(objectP->instanceList, instanceId, (lwm2m_list_t **)&instanceP);
    
    if (instanceP == NULL)
    {
        return COAP_404_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Removed WoT Data Feature instance %u", instanceId);
    free(instanceP);
    return COAP_202_DELETED;
}

uint8_t wot_data_feature_add_linked_resource(lwm2m_object_t * objectP, uint16_t instanceId, 
                                             const char* resource_path)
{
    if (objectP == NULL || resource_path == NULL)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    wot_data_feature_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    if (instanceP == NULL)
    {
        return COAP_404_NOT_FOUND;
    }
    
    if (instanceP->linked_resources_count >= MAX_WOT_LINKED_RESOURCES)
    {
        return COAP_413_ENTITY_TOO_LARGE;
    }
    
    strncpy(instanceP->linked_resources[instanceP->linked_resources_count], 
            resource_path, sizeof(instanceP->linked_resources[0]) - 1);
    instanceP->linked_resources[instanceP->linked_resources_count][sizeof(instanceP->linked_resources[0]) - 1] = '\0';
    instanceP->linked_resources_count++;
    instanceP->last_updated = time(NULL);
    
    ESP_LOGI(TAG, "Added linked resource %s to feature instance %u", resource_path, instanceId);
    return COAP_204_CHANGED;
}

uint8_t wot_data_feature_remove_linked_resource(lwm2m_object_t * objectP, uint16_t instanceId, 
                                                const char* resource_path)
{
    if (objectP == NULL || resource_path == NULL)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    wot_data_feature_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    if (instanceP == NULL)
    {
        return COAP_404_NOT_FOUND;
    }
    
    // Find and remove the resource path
    for (int i = 0; i < instanceP->linked_resources_count; i++)
    {
        if (strcmp(instanceP->linked_resources[i], resource_path) == 0)
        {
            // Shift remaining resources down
            for (int j = i; j < instanceP->linked_resources_count - 1; j++)
            {
                strcpy(instanceP->linked_resources[j], instanceP->linked_resources[j + 1]);
            }
            instanceP->linked_resources_count--;
            instanceP->last_updated = time(NULL);
            
            ESP_LOGI(TAG, "Removed linked resource %s from feature instance %u", resource_path, instanceId);
            return COAP_204_CHANGED;
        }
    }
    
    return COAP_404_NOT_FOUND;
}

uint8_t wot_data_feature_clear_linked_resources(lwm2m_object_t * objectP, uint16_t instanceId)
{
    if (objectP == NULL)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    wot_data_feature_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    if (instanceP == NULL)
    {
        return COAP_404_NOT_FOUND;
    }
    
    instanceP->linked_resources_count = 0;
    instanceP->last_updated = time(NULL);
    
    ESP_LOGI(TAG, "Cleared all linked resources from feature instance %u", instanceId);
    return COAP_204_CHANGED;
}

uint8_t wot_data_feature_set_owning_thing(lwm2m_object_t * objectP, uint16_t instanceId,
                                          uint16_t thing_obj_id, uint16_t thing_instance_id)
{
    if (objectP == NULL)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    wot_data_feature_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    if (instanceP == NULL)
    {
        return COAP_404_NOT_FOUND;
    }
    
    instanceP->owning_thing_obj_id = thing_obj_id;
    instanceP->owning_thing_instance_id = thing_instance_id;
    instanceP->has_owning_thing = true;
    instanceP->last_updated = time(NULL);
    
    ESP_LOGI(TAG, "Set owning thing %u/%u for feature instance %u", 
             thing_obj_id, thing_instance_id, instanceId);
    return COAP_204_CHANGED;
}

uint8_t wot_data_feature_clear_owning_thing(lwm2m_object_t * objectP, uint16_t instanceId)
{
    if (objectP == NULL)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    wot_data_feature_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    if (instanceP == NULL)
    {
        return COAP_404_NOT_FOUND;
    }
    
    instanceP->has_owning_thing = false;
    instanceP->last_updated = time(NULL);
    
    ESP_LOGI(TAG, "Cleared owning thing for feature instance %u", instanceId);
    return COAP_204_CHANGED;
}

int wot_data_feature_get_linked_resource_count(lwm2m_object_t * objectP, uint16_t instanceId)
{
    if (objectP == NULL)
    {
        return -1;
    }
    
    wot_data_feature_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    if (instanceP == NULL)
    {
        return -1;
    }
    
    return instanceP->linked_resources_count;
}

const char* wot_data_feature_get_linked_resource(lwm2m_object_t * objectP, uint16_t instanceId, int index)
{
    if (objectP == NULL)
    {
        return NULL;
    }
    
    wot_data_feature_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    if (instanceP == NULL || index < 0 || index >= instanceP->linked_resources_count)
    {
        return NULL;
    }
    
    return instanceP->linked_resources[index];
}