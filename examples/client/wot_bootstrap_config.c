/*******************************************************************************
 *
 * Copyright (c) 2024 edgez.ai - W3C WoT Bootstrap Configuration
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * Bootstrap configuration for W3C WoT LwM2M objects
 *
 *******************************************************************************/

#include "wot_bootstrap_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "wot_bootstrap";

wot_objects_t* wot_bootstrap_init_objects(void)
{
    wot_objects_t* objects = (wot_objects_t*)malloc(sizeof(wot_objects_t));
    if (objects == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for WoT objects");
        return NULL;
    }
    
    memset(objects, 0, sizeof(wot_objects_t));
    
    // Initialize WoT Thing Definition object
    objects->wot_thing_obj = get_object_wot_thing();
    if (objects->wot_thing_obj == NULL)
    {
        ESP_LOGE(TAG, "Failed to create WoT Thing object");
        free(objects);
        return NULL;
    }
    
    // Initialize WoT Data Feature object
    objects->wot_data_feature_obj = get_object_wot_data_feature();
    if (objects->wot_data_feature_obj == NULL)
    {
        ESP_LOGE(TAG, "Failed to create WoT Data Feature object");
        free_object_wot_thing(objects->wot_thing_obj);
        free(objects);
        return NULL;
    }
    
    // Initialize WoT Action object
    objects->wot_action_obj = get_object_wot_action();
    if (objects->wot_action_obj == NULL)
    {
        ESP_LOGE(TAG, "Failed to create WoT Action object");
        free_object_wot_thing(objects->wot_thing_obj);
        free_object_wot_data_feature(objects->wot_data_feature_obj);
        free(objects);
        return NULL;
    }
    
    // Initialize WoT Event object
    objects->wot_event_obj = get_object_wot_event();
    if (objects->wot_event_obj == NULL)
    {
        ESP_LOGE(TAG, "Failed to create WoT Event object");
        free_object_wot_thing(objects->wot_thing_obj);
        free_object_wot_data_feature(objects->wot_data_feature_obj);
        free_object_wot_action(objects->wot_action_obj);
        free(objects);
        return NULL;
    }
    
    // Set up callbacks
    wot_action_set_execute_callback(objects->wot_action_obj, wot_bootstrap_action_execute_callback);
    wot_event_set_execute_callback(objects->wot_event_obj, wot_bootstrap_event_execute_callback);
    wot_event_set_emit_callback(objects->wot_event_obj, wot_bootstrap_event_emit_callback);
    
    ESP_LOGI(TAG, "WoT objects initialized successfully");
    return objects;
}

void wot_bootstrap_free_objects(wot_objects_t* objects)
{
    if (objects != NULL)
    {
        if (objects->wot_thing_obj) free_object_wot_thing(objects->wot_thing_obj);
        if (objects->wot_data_feature_obj) free_object_wot_data_feature(objects->wot_data_feature_obj);
        if (objects->wot_action_obj) free_object_wot_action(objects->wot_action_obj);
        if (objects->wot_event_obj) free_object_wot_event(objects->wot_event_obj);
        free(objects);
        ESP_LOGI(TAG, "WoT objects freed");
    }
}

uint8_t wot_bootstrap_apply_config(wot_objects_t* objects, const wot_bootstrap_config_t* config)
{
    if (objects == NULL || config == NULL)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    ESP_LOGI(TAG, "Applying WoT bootstrap configuration");
    
    // Create Thing Definition instance
    uint8_t result = wot_thing_add_instance(objects->wot_thing_obj, 0, 
                                           config->thing_id, config->thing_title);
    if (result != COAP_201_CREATED)
    {
        ESP_LOGE(TAG, "Failed to create Thing instance: %d", result);
        return result;
    }
    
    // Update thing description and version
    if (strlen(config->thing_description) > 0)
    {
        wot_thing_update_description(objects->wot_thing_obj, 0, config->thing_description);
    }
    if (strlen(config->thing_version) > 0)
    {
        wot_thing_update_version(objects->wot_thing_obj, 0, config->thing_version);
    }
    
    // Create Data Feature instances
    for (int i = 0; i < config->data_features_count; i++)
    {
        result = wot_data_feature_add_instance(objects->wot_data_feature_obj, i, 
                                              config->data_features[i].feature_id);
        if (result != COAP_201_CREATED)
        {
            ESP_LOGE(TAG, "Failed to create Data Feature instance %d: %d", i, result);
            continue;
        }
        
        // Add linked resources
        for (int j = 0; j < config->data_features[i].linked_resources_count; j++)
        {
            wot_data_feature_add_linked_resource(objects->wot_data_feature_obj, i,
                                                 config->data_features[i].linked_resources[j]);
        }
        
        // Link to owning thing
        wot_data_feature_set_owning_thing(objects->wot_data_feature_obj, i, 
                                         WOT_THING_OBJECT_ID, 0);
        
        ESP_LOGI(TAG, "Created Data Feature instance %d: %s", i, config->data_features[i].feature_id);
    }
    
    // Create Action instances
    for (int i = 0; i < config->actions_count; i++)
    {
        result = wot_action_add_instance(objects->wot_action_obj, i, 
                                        config->actions[i].action_id);
        if (result != COAP_201_CREATED)
        {
            ESP_LOGE(TAG, "Failed to create Action instance %d: %d", i, result);
            continue;
        }
        
        // Update script
        if (config->actions[i].script_size > 0)
        {
            wot_action_update_script(objects->wot_action_obj, i,
                                    config->actions[i].script, 
                                    config->actions[i].script_size,
                                    config->actions[i].script_format);
        }
        
        // Link to owning thing
        wot_action_set_owning_thing(objects->wot_action_obj, i, 
                                   WOT_THING_OBJECT_ID, 0);
        
        ESP_LOGI(TAG, "Created Action instance %d: %s", i, config->actions[i].action_id);
    }
    
    // Create Event instances
    for (int i = 0; i < config->events_count; i++)
    {
        result = wot_event_add_instance(objects->wot_event_obj, i, 
                                       config->events[i].event_id);
        if (result != COAP_201_CREATED)
        {
            ESP_LOGE(TAG, "Failed to create Event instance %d: %d", i, result);
            continue;
        }
        
        // Update script
        if (config->events[i].script_size > 0)
        {
            wot_event_update_script(objects->wot_event_obj, i,
                                   config->events[i].script, 
                                   config->events[i].script_size,
                                   config->events[i].script_format);
        }
        
        // Link to owning thing
        wot_event_set_owning_thing(objects->wot_event_obj, i, 
                                  WOT_THING_OBJECT_ID, 0);
        
        ESP_LOGI(TAG, "Created Event instance %d: %s", i, config->events[i].event_id);
    }
    
    ESP_LOGI(TAG, "WoT bootstrap configuration applied successfully");
    return COAP_204_CHANGED;
}

wot_bootstrap_config_t* wot_bootstrap_create_default_config(void)
{
    wot_bootstrap_config_t* config = (wot_bootstrap_config_t*)malloc(sizeof(wot_bootstrap_config_t));
    if (config == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for bootstrap config");
        return NULL;
    }
    
    memset(config, 0, sizeof(wot_bootstrap_config_t));
    
    // Configure Thing Definition
    strcpy(config->thing_id, "urn:edgez:esp32:gateway:001");
    strcpy(config->thing_title, "ESP32 IoT Gateway");
    strcpy(config->thing_description, "ESP32-based IoT gateway with temperature monitoring and device management capabilities");
    strcpy(config->thing_version, "1.0.0");
    
    // Add temperature sensor data feature
    wot_bootstrap_add_temperature_sensor_feature(config, "/3303/0/5700");
    
    // Add LED control action
    wot_bootstrap_add_led_control_action(config);
    
    // Add alarm event
    wot_bootstrap_add_alarm_event(config);
    
    ESP_LOGI(TAG, "Created default WoT bootstrap configuration");
    return config;
}

void wot_bootstrap_free_config(wot_bootstrap_config_t* config)
{
    if (config != NULL)
    {
        free(config);
        ESP_LOGI(TAG, "Bootstrap configuration freed");
    }
}

int wot_bootstrap_action_execute_callback(const char* action_id, const uint8_t* script, 
                                         size_t script_size, const char* script_format)
{
    ESP_LOGI(TAG, "Executing action: %s (script size: %zu, format: %s)", 
             action_id, script_size, script_format ? script_format : "none");
    
    // Simple script interpreter for demonstration
    if (strcmp(action_id, "led_control") == 0)
    {
        // Parse simple LED control script: "LED:ON" or "LED:OFF"
        if (script_size >= 6 && strncmp((char*)script, "LED:", 4) == 0)
        {
            if (strncmp((char*)script + 4, "ON", 2) == 0)
            {
                ESP_LOGI(TAG, "LED turned ON");
                // Here you would actually control the LED
                return 0; // Success
            }
            else if (strncmp((char*)script + 4, "OFF", 3) == 0)
            {
                ESP_LOGI(TAG, "LED turned OFF");
                // Here you would actually control the LED
                return 0; // Success
            }
        }
        ESP_LOGW(TAG, "Invalid LED control script");
        return -1; // Invalid script
    }
    
    ESP_LOGW(TAG, "Unknown action: %s", action_id);
    return -2; // Unknown action
}

int wot_bootstrap_event_execute_callback(const char* event_id, const uint8_t* script, 
                                        size_t script_size, const char* script_format,
                                        wot_event_emit_callback_t emit_callback)
{
    ESP_LOGI(TAG, "Executing event script: %s (script size: %zu, format: %s)", 
             event_id, script_size, script_format ? script_format : "none");
    
    // Simple event script interpreter for demonstration
    if (strcmp(event_id, "temperature_alarm") == 0)
    {
        // Generate alarm event data
        const char* alarm_data = "{\"type\":\"temperature_alarm\",\"severity\":\"high\",\"timestamp\":1234567890}";
        
        if (emit_callback != NULL)
        {
            int result = emit_callback(event_id, (uint8_t*)alarm_data, strlen(alarm_data), "application/json");
            if (result == 0)
            {
                ESP_LOGI(TAG, "Temperature alarm event emitted successfully");
                return 0;
            }
            else
            {
                ESP_LOGE(TAG, "Failed to emit temperature alarm event: %d", result);
                return result;
            }
        }
        else
        {
            ESP_LOGW(TAG, "No emit callback available for event: %s", event_id);
            return -1;
        }
    }
    
    ESP_LOGW(TAG, "Unknown event: %s", event_id);
    return -2; // Unknown event
}

int wot_bootstrap_event_emit_callback(const char* event_id, const uint8_t* event_data, 
                                     size_t data_size, const char* script_format)
{
    ESP_LOGI(TAG, "Event emitted: %s (data size: %zu, format: %s)", 
             event_id, data_size, script_format ? script_format : "none");
    
    // Log event data for debugging (in production, this would send to server or process the event)
    if (data_size > 0 && event_data != NULL)
    {
        char data_str[257]; // Limit to 256 chars for logging
        size_t log_size = data_size < 256 ? data_size : 256;
        memcpy(data_str, event_data, log_size);
        data_str[log_size] = '\0';
        ESP_LOGI(TAG, "Event data: %s", data_str);
    }
    
    // Here you would typically:
    // 1. Send event data to a server
    // 2. Trigger LwM2M notifications
    // 3. Process the event locally
    
    return 0; // Success
}

uint8_t wot_bootstrap_add_temperature_sensor_feature(wot_bootstrap_config_t* config, 
                                                     const char* sensor_path)
{
    if (config == NULL || sensor_path == NULL)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    if (config->data_features_count >= 8)
    {
        ESP_LOGW(TAG, "Cannot add more data features (limit: 8)");
        return COAP_413_ENTITY_TOO_LARGE;
    }
    
    int idx = config->data_features_count;
    
    strcpy(config->data_features[idx].feature_id, "temperature_sensor");
    strcpy(config->data_features[idx].linked_resources[0], sensor_path);
    config->data_features[idx].linked_resources_count = 1;
    
    config->data_features_count++;
    
    ESP_LOGI(TAG, "Added temperature sensor feature with path: %s", sensor_path);
    return COAP_204_CHANGED;
}

uint8_t wot_bootstrap_add_led_control_action(wot_bootstrap_config_t* config)
{
    if (config == NULL)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    if (config->actions_count >= 8)
    {
        ESP_LOGW(TAG, "Cannot add more actions (limit: 8)");
        return COAP_413_ENTITY_TOO_LARGE;
    }
    
    int idx = config->actions_count;
    
    strcpy(config->actions[idx].action_id, "led_control");
    
    // Simple LED control script
    const char* script = "LED:OFF"; // Default to OFF
    size_t script_size = strlen(script);
    memcpy(config->actions[idx].script, script, script_size);
    config->actions[idx].script_size = script_size;
    strcpy(config->actions[idx].script_format, "text/plain");
    
    config->actions_count++;
    
    ESP_LOGI(TAG, "Added LED control action");
    return COAP_204_CHANGED;
}

uint8_t wot_bootstrap_add_alarm_event(wot_bootstrap_config_t* config)
{
    if (config == NULL)
    {
        return COAP_400_BAD_REQUEST;
    }
    
    if (config->events_count >= 8)
    {
        ESP_LOGW(TAG, "Cannot add more events (limit: 8)");
        return COAP_413_ENTITY_TOO_LARGE;
    }
    
    int idx = config->events_count;
    
    strcpy(config->events[idx].event_id, "temperature_alarm");
    
    // Simple alarm generation script
    const char* script = "if (temperature > 30) { emit_alarm('high_temperature', temperature); }";
    size_t script_size = strlen(script);
    memcpy(config->events[idx].script, script, script_size);
    config->events[idx].script_size = script_size;
    strcpy(config->events[idx].script_format, "text/javascript");
    
    config->events_count++;
    
    ESP_LOGI(TAG, "Added temperature alarm event");
    return COAP_204_CHANGED;
}