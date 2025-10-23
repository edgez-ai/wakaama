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

#ifndef WOT_BOOTSTRAP_CONFIG_H
#define WOT_BOOTSTRAP_CONFIG_H

#include "liblwm2m.h"
#include "object_wot_thing.h"
#include "object_wot_data_feature.h"
#include "object_wot_action.h"
#include "object_wot_event.h"

#ifdef __cplusplus
extern "C" {
#endif

// Bootstrap configuration structure
typedef struct {
    // Thing Definition configuration
    char thing_id[128];
    char thing_title[256];
    char thing_description[512];
    char thing_version[32];
    
    // Data Feature configurations
    struct {
        char feature_id[128];
        char linked_resources[MAX_WOT_LINKED_RESOURCES][64];
        uint8_t linked_resources_count;
    } data_features[8];
    uint8_t data_features_count;
    
    // Action configurations
    struct {
        char action_id[128];
        uint8_t script[MAX_WOT_ACTION_SCRIPT_SIZE];
        size_t script_size;
        char script_format[64];
    } actions[8];
    uint8_t actions_count;
    
    // Event configurations
    struct {
        char event_id[128];
        uint8_t script[MAX_WOT_EVENT_SCRIPT_SIZE];
        size_t script_size;
        char script_format[64];
    } events[8];
    uint8_t events_count;
    
} wot_bootstrap_config_t;

// Bootstrap functions
typedef struct {
    lwm2m_object_t *wot_thing_obj;
    lwm2m_object_t *wot_data_feature_obj;
    lwm2m_object_t *wot_action_obj;
    lwm2m_object_t *wot_event_obj;
} wot_objects_t;

/*
 * Bootstrap initialization functions
 */

// Initialize all WoT objects
wot_objects_t* wot_bootstrap_init_objects(void);

// Free all WoT objects
void wot_bootstrap_free_objects(wot_objects_t* objects);

// Apply bootstrap configuration
uint8_t wot_bootstrap_apply_config(wot_objects_t* objects, const wot_bootstrap_config_t* config);

// Create default bootstrap configuration for temperature sensor gateway
wot_bootstrap_config_t* wot_bootstrap_create_default_config(void);

// Free bootstrap configuration
void wot_bootstrap_free_config(wot_bootstrap_config_t* config);

// Bootstrap callback for action execution
int wot_bootstrap_action_execute_callback(const char* action_id, const uint8_t* script, 
                                         size_t script_size, const char* script_format);

// Bootstrap callback for event execution
int wot_bootstrap_event_execute_callback(const char* event_id, const uint8_t* script, 
                                        size_t script_size, const char* script_format,
                                        wot_event_emit_callback_t emit_callback);

// Bootstrap callback for event emission
int wot_bootstrap_event_emit_callback(const char* event_id, const uint8_t* event_data, 
                                     size_t data_size, const char* script_format);

// Utility functions for creating common configurations
uint8_t wot_bootstrap_add_temperature_sensor_feature(wot_bootstrap_config_t* config, 
                                                     const char* sensor_path);
uint8_t wot_bootstrap_add_led_control_action(wot_bootstrap_config_t* config);
uint8_t wot_bootstrap_add_alarm_event(wot_bootstrap_config_t* config);

#ifdef __cplusplus
}
#endif

#endif // WOT_BOOTSTRAP_CONFIG_H