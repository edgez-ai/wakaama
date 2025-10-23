/*******************************************************************************
 *
 * Copyright (c) 2024 edgez.ai - W3C WoT Action Object
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * Based on LwM2M Object 26252: W3C WoT Action
 *
 *******************************************************************************/

#include "object_wot_action.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"

static const char *TAG = "wot_action";

// Global execute callback
static wot_action_execute_callback_t g_execute_callback = NULL;

// Forward declarations
static uint8_t prv_read(lwm2m_context_t *contextP, uint16_t instanceId, int *numDataP, 
                        lwm2m_data_t **dataArrayP, lwm2m_object_t *objectP);
static uint8_t prv_write(lwm2m_context_t *contextP, uint16_t instanceId, int numData, 
                         lwm2m_data_t *dataArray, lwm2m_object_t *objectP, lwm2m_write_type_t writeType);
static uint8_t prv_execute(lwm2m_context_t *contextP, uint16_t instanceId, uint16_t resourceId,
                           uint8_t *buffer, int length, lwm2m_object_t *objectP);
static uint8_t prv_create(lwm2m_context_t *contextP, uint16_t instanceId, int numData, 
                          lwm2m_data_t *dataArray, lwm2m_object_t *objectP);
static uint8_t prv_delete(lwm2m_context_t *contextP, uint16_t instanceId, lwm2m_object_t *objectP);

// Helper functions
static wot_action_instance_t * prv_find_instance(lwm2m_object_t *objectP, uint16_t instanceId)
{
    wot_action_instance_t *targetP = (wot_action_instance_t *)objectP->instanceList;
    
    while (targetP != NULL && targetP->instanceId != instanceId)
    {
        targetP = targetP->next;
    }
    
    return targetP;
}

static wot_action_instance_t * prv_create_instance(uint16_t instanceId)
{
    wot_action_instance_t *instanceP = (wot_action_instance_t *)malloc(sizeof(wot_action_instance_t));
    
    if (instanceP != NULL)
    {
        memset(instanceP, 0, sizeof(wot_action_instance_t));
        instanceP->instanceId = instanceId;
        instanceP->last_updated = time(NULL);
    }
    
    return instanceP;
}

static uint8_t prv_read(lwm2m_context_t *contextP, uint16_t instanceId, int *numDataP, 
                        lwm2m_data_t **dataArrayP, lwm2m_object_t *objectP)
{
    wot_action_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    
    if (instanceP == NULL)
    {
        return COAP_404_NOT_FOUND;
    }
    
    // If no specific resource is requested, return all readable resources
    if (*numDataP == 0)
    {
        *dataArrayP = lwm2m_data_new(5);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = 5;
        
        (*dataArrayP)[0].id = RES_WOT_ACTION_IDENTIFIER;
        (*dataArrayP)[1].id = RES_WOT_ACTION_SCRIPT;
        (*dataArrayP)[2].id = RES_WOT_ACTION_SCRIPT_FORMAT;
        (*dataArrayP)[3].id = RES_WOT_ACTION_OWNING_THING;
        (*dataArrayP)[4].id = RES_WOT_ACTION_LAST_UPDATED;
    }
    
    for (int i = 0; i < *numDataP; i++)
    {
        switch ((*dataArrayP)[i].id)
        {
            case RES_WOT_ACTION_IDENTIFIER:
                lwm2m_data_encode_string(instanceP->action_identifier, &((*dataArrayP)[i]));
                break;
                
            case RES_WOT_ACTION_SCRIPT:
                if (instanceP->script_size > 0)
                {
                    lwm2m_data_encode_opaque(instanceP->script, instanceP->script_size, 
                                            &((*dataArrayP)[i]));
                }
                else
                {
                    (*dataArrayP)[i].type = LWM2M_TYPE_UNDEFINED;
                }
                break;
                
            case RES_WOT_ACTION_SCRIPT_FORMAT:
                if (strlen(instanceP->script_format) > 0)
                {
                    lwm2m_data_encode_string(instanceP->script_format, &((*dataArrayP)[i]));
                }
                else
                {
                    (*dataArrayP)[i].type = LWM2M_TYPE_UNDEFINED;
                }
                break;
                
            case RES_WOT_ACTION_OWNING_THING:
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
                
            case RES_WOT_ACTION_LAST_UPDATED:
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
    ESP_LOGI(TAG, "🔽 BOOTSTRAP WRITE - WoT Action Object (26252) - Instance %d, Resources: %d, WriteType: %d", 
             instanceId, numData, writeType);
    
    wot_action_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    
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
            case RES_WOT_ACTION_IDENTIFIER:
                // Allow writes during bootstrap/creation, otherwise read-only
                if (dataArray[i].type == LWM2M_TYPE_STRING)
                {
                    size_t len = dataArray[i].value.asBuffer.length;
                    if (len >= sizeof(instanceP->action_identifier)) len = sizeof(instanceP->action_identifier) - 1;
                    memcpy(instanceP->action_identifier, dataArray[i].value.asBuffer.buffer, len);
                    instanceP->action_identifier[len] = '\0';
                    instanceP->last_updated = time(NULL);
                    ESP_LOGI(TAG, "🔽 BOOTSTRAP WRITE - Action Identifier set to: %s", instanceP->action_identifier);
                }
                else
                {
                    return COAP_400_BAD_REQUEST;
                }
                break;
                
            case RES_WOT_ACTION_SCRIPT:
                if (dataArray[i].type == LWM2M_TYPE_OPAQUE)
                {
                    size_t size = dataArray[i].value.asBuffer.length;
                    if (size > MAX_WOT_ACTION_SCRIPT_SIZE) size = MAX_WOT_ACTION_SCRIPT_SIZE;
                    
                    memcpy(instanceP->script, dataArray[i].value.asBuffer.buffer, size);
                    instanceP->script_size = size;
                    instanceP->last_updated = time(NULL);
                    
                    ESP_LOGI(TAG, "Updated script for action %u (size: %zu)", instanceId, size);
                }
                else
                {
                    return COAP_400_BAD_REQUEST;
                }
                break;
                
            case RES_WOT_ACTION_SCRIPT_FORMAT:
                if (dataArray[i].type == LWM2M_TYPE_STRING)
                {
                    size_t len = dataArray[i].value.asBuffer.length;
                    if (len >= sizeof(instanceP->script_format)) len = sizeof(instanceP->script_format) - 1;
                    memcpy(instanceP->script_format, dataArray[i].value.asBuffer.buffer, len);
                    instanceP->script_format[len] = '\0';
                    instanceP->last_updated = time(NULL);
                    
                    ESP_LOGI(TAG, "Updated script format for action %u: %s", instanceId, instanceP->script_format);
                }
                else
                {
                    return COAP_400_BAD_REQUEST;
                }
                break;
                
            case RES_WOT_ACTION_OWNING_THING:
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
                
            case RES_WOT_ACTION_LAST_UPDATED:
                // Read-only resource
                return COAP_405_METHOD_NOT_ALLOWED;
                
            default:
                return COAP_404_NOT_FOUND;
        }
    }
    
    return COAP_204_CHANGED;
}

static uint8_t prv_execute(lwm2m_context_t *contextP, uint16_t instanceId, uint16_t resourceId,
                           uint8_t *buffer, int length, lwm2m_object_t *objectP)
{
    wot_action_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    
    if (instanceP == NULL)
    {
        return COAP_404_NOT_FOUND;
    }
    
    // The script resource (resource 1) is executable
    if (resourceId == RES_WOT_ACTION_SCRIPT)
    {
        if (instanceP->script_size == 0)
        {
            ESP_LOGW(TAG, "No script available for action %u", instanceId);
            return COAP_400_BAD_REQUEST;
        }
        
        ESP_LOGI(TAG, "Executing action %u: %s", instanceId, instanceP->action_identifier);
        
        // Execute the callback if available
        if (g_execute_callback != NULL)
        {
            int result = g_execute_callback(instanceP->action_identifier, 
                                           instanceP->script, instanceP->script_size,
                                           strlen(instanceP->script_format) > 0 ? instanceP->script_format : NULL);
            
            if (result == 0)
            {
                ESP_LOGI(TAG, "Action executed successfully: %s", instanceP->action_identifier);
                return COAP_204_CHANGED;
            }
            else
            {
                ESP_LOGE(TAG, "Action execution failed: %s (error: %d)", instanceP->action_identifier, result);
                return COAP_500_INTERNAL_SERVER_ERROR;
            }
        }
        else
        {
            ESP_LOGW(TAG, "No execution callback registered for action: %s", instanceP->action_identifier);
            return COAP_501_NOT_IMPLEMENTED;
        }
    }
    
    return COAP_405_METHOD_NOT_ALLOWED;
}

static uint8_t prv_create(lwm2m_context_t *contextP, uint16_t instanceId, int numData, 
                          lwm2m_data_t *dataArray, lwm2m_object_t *objectP)
{
    ESP_LOGI(TAG, "🔽 BOOTSTRAP CREATE - WoT Action Object (26252) - Instance %d, Resources: %d", 
             instanceId, numData);
    
    wot_action_instance_t *instanceP;
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
    strcpy(instanceP->action_identifier, "");
    
    // Process provided data
    result = prv_write(contextP, instanceId, numData, dataArray, objectP, LWM2M_WRITE_REPLACE_RESOURCES);
    
    if (result == COAP_204_CHANGED)
    {
        // Validate mandatory resources
        if (strlen(instanceP->action_identifier) == 0 || instanceP->script_size == 0)
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
    wot_action_instance_t *instanceP;
    
    objectP->instanceList = lwm2m_list_remove(objectP->instanceList, instanceId, (lwm2m_list_t **)&instanceP);
    if (instanceP == NULL)
    {
        return COAP_404_NOT_FOUND;
    }
    
    free(instanceP);
    return COAP_202_DELETED;
}

lwm2m_object_t * get_object_wot_action(void)
{
    lwm2m_object_t * wotActionObj;
    
    wotActionObj = (lwm2m_object_t *)malloc(sizeof(lwm2m_object_t));
    
    if (wotActionObj != NULL)
    {
        memset(wotActionObj, 0, sizeof(lwm2m_object_t));
        
        wotActionObj->objID = WOT_ACTION_OBJECT_ID;
        wotActionObj->readFunc = prv_read;
        wotActionObj->writeFunc = prv_write;
        wotActionObj->executeFunc = prv_execute;
        wotActionObj->createFunc = prv_create;
        wotActionObj->deleteFunc = prv_delete;
    }
    
    return wotActionObj;
}

void free_object_wot_action(lwm2m_object_t * objectP)
{
    if (objectP != NULL)
    {
        while (objectP->instanceList != NULL)
        {
            wot_action_instance_t * instanceP = (wot_action_instance_t *)objectP->instanceList;
            objectP->instanceList = objectP->instanceList->next;
            free(instanceP);
        }
        free(objectP);
    }
}

void display_wot_action_object(lwm2m_object_t * objectP)
{
    ESP_LOGI(TAG, "  /%u: WoT Action object, instances:", objectP->objID);
    
    wot_action_instance_t * instanceP = (wot_action_instance_t *)objectP->instanceList;
    while (instanceP != NULL)
    {
        ESP_LOGI(TAG, "    /%u/%u: id=%s, script_size=%zu, format=%s", 
                 objectP->objID, instanceP->instanceId,
                 instanceP->action_identifier, instanceP->script_size, instanceP->script_format);
        
        if (instanceP->has_owning_thing)
        {
            ESP_LOGI(TAG, "      Owning Thing: %u/%u", 
                     instanceP->owning_thing_obj_id, instanceP->owning_thing_instance_id);
        }
        
        instanceP = instanceP->next;
    }
}

// Public API functions
uint8_t wot_action_add_instance(lwm2m_object_t * objectP, uint16_t instanceId, 
                                const char* action_id)
{
    if (objectP == NULL || action_id == NULL)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    wot_action_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    if (instanceP != NULL)
    {
        return COAP_406_NOT_ACCEPTABLE;
    }
    
    instanceP = prv_create_instance(instanceId);
    if (instanceP == NULL)
    {
        return COAP_500_INTERNAL_SERVER_ERROR;
    }
    
    strncpy(instanceP->action_identifier, action_id, sizeof(instanceP->action_identifier) - 1);
    instanceP->action_identifier[sizeof(instanceP->action_identifier) - 1] = '\0';
    
    objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, instanceP);
    
    ESP_LOGI(TAG, "Added WoT Action instance %u: %s", instanceId, action_id);
    return COAP_201_CREATED;
}

uint8_t wot_action_remove_instance(lwm2m_object_t * objectP, uint16_t instanceId)
{
    if (objectP == NULL)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    wot_action_instance_t *instanceP;
    objectP->instanceList = lwm2m_list_remove(objectP->instanceList, instanceId, (lwm2m_list_t **)&instanceP);
    
    if (instanceP == NULL)
    {
        return COAP_404_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Removed WoT Action instance %u", instanceId);
    free(instanceP);
    return COAP_202_DELETED;
}

uint8_t wot_action_update_script(lwm2m_object_t * objectP, uint16_t instanceId,
                                 const uint8_t* script, size_t script_size, 
                                 const char* script_format)
{
    if (objectP == NULL || script == NULL || script_size == 0)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    wot_action_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    if (instanceP == NULL)
    {
        return COAP_404_NOT_FOUND;
    }
    
    if (script_size > MAX_WOT_ACTION_SCRIPT_SIZE)
    {
        script_size = MAX_WOT_ACTION_SCRIPT_SIZE;
    }
    
    memcpy(instanceP->script, script, script_size);
    instanceP->script_size = script_size;
    
    if (script_format != NULL)
    {
        strncpy(instanceP->script_format, script_format, sizeof(instanceP->script_format) - 1);
        instanceP->script_format[sizeof(instanceP->script_format) - 1] = '\0';
    }
    
    instanceP->last_updated = time(NULL);
    
    ESP_LOGI(TAG, "Updated script for action %u (size: %zu, format: %s)", 
             instanceId, script_size, script_format ? script_format : "none");
    return COAP_204_CHANGED;
}

uint8_t wot_action_set_owning_thing(lwm2m_object_t * objectP, uint16_t instanceId,
                                    uint16_t thing_obj_id, uint16_t thing_instance_id)
{
    if (objectP == NULL)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    wot_action_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    if (instanceP == NULL)
    {
        return COAP_404_NOT_FOUND;
    }
    
    instanceP->owning_thing_obj_id = thing_obj_id;
    instanceP->owning_thing_instance_id = thing_instance_id;
    instanceP->has_owning_thing = true;
    instanceP->last_updated = time(NULL);
    
    ESP_LOGI(TAG, "Set owning thing %u/%u for action instance %u", 
             thing_obj_id, thing_instance_id, instanceId);
    return COAP_204_CHANGED;
}

uint8_t wot_action_clear_owning_thing(lwm2m_object_t * objectP, uint16_t instanceId)
{
    if (objectP == NULL)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    wot_action_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    if (instanceP == NULL)
    {
        return COAP_404_NOT_FOUND;
    }
    
    instanceP->has_owning_thing = false;
    instanceP->last_updated = time(NULL);
    
    ESP_LOGI(TAG, "Cleared owning thing for action instance %u", instanceId);
    return COAP_204_CHANGED;
}

void wot_action_set_execute_callback(lwm2m_object_t * objectP, wot_action_execute_callback_t callback)
{
    g_execute_callback = callback;
    ESP_LOGI(TAG, "WoT Action execute callback %s", callback ? "registered" : "cleared");
}

const char* wot_action_get_identifier(lwm2m_object_t * objectP, uint16_t instanceId)
{
    if (objectP == NULL)
    {
        return NULL;
    }
    
    wot_action_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    if (instanceP == NULL)
    {
        return NULL;
    }
    
    return instanceP->action_identifier;
}

size_t wot_action_get_script(lwm2m_object_t * objectP, uint16_t instanceId, 
                             const uint8_t** script_ptr)
{
    if (objectP == NULL || script_ptr == NULL)
    {
        return 0;
    }
    
    wot_action_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    if (instanceP == NULL)
    {
        return 0;
    }
    
    *script_ptr = instanceP->script;
    return instanceP->script_size;
}

const char* wot_action_get_script_format(lwm2m_object_t * objectP, uint16_t instanceId)
{
    if (objectP == NULL)
    {
        return NULL;
    }
    
    wot_action_instance_t *instanceP = prv_find_instance(objectP, instanceId);
    if (instanceP == NULL)
    {
        return NULL;
    }
    
    return strlen(instanceP->script_format) > 0 ? instanceP->script_format : NULL;
}