/*******************************************************************************
 *
 * Copyright (c) 2024 edgez.ai - W3C WoT Event Object
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * Based on LwM2M Object 26253: W3C WoT Event
 *
 *******************************************************************************/

#ifndef OBJECT_WOT_EVENT_H
#define OBJECT_WOT_EVENT_H

#include "liblwm2m.h"

#ifdef __cplusplus
extern "C" {
#endif

// W3C WoT Event Object ID
#define WOT_EVENT_OBJECT_ID 26253

// Resource IDs based on XML definition
#define RES_WOT_EVENT_IDENTIFIER     0  // Event Identifier (R, String, Mandatory)
#define RES_WOT_EVENT_SCRIPT         1  // Script (RW, Opaque, Mandatory)
#define RES_WOT_EVENT_SCRIPT_FORMAT  2  // Script Format (RW, String, Optional)
#define RES_WOT_EVENT_OWNING_THING   3  // Owning Thing (RW, Objlnk, Optional)
#define RES_WOT_EVENT_LAST_UPDATED   4  // Last Updated (R, Time, Optional)

// Maximum script size
#define MAX_WOT_EVENT_SCRIPT_SIZE 2048

// Event emission callback type - called when script generates event data
typedef int (*wot_event_emit_callback_t)(const char* event_id, const uint8_t* event_data, 
                                         size_t data_size, const char* script_format);

// Event script execution callback type - called to execute the event script
typedef int (*wot_event_execute_callback_t)(const char* event_id, const uint8_t* script, 
                                            size_t script_size, const char* script_format,
                                            wot_event_emit_callback_t emit_callback);

// WoT Event instance data structure
typedef struct _wot_event_instance_
{
    struct _wot_event_instance_ * next;      // matches lwm2m_list_t
    uint16_t instanceId;                     // matches lwm2m_list_t
    
    // Resource data
    char event_identifier[128];              // Resource 0: Immutable identifier
    
    // Script data
    uint8_t script[MAX_WOT_EVENT_SCRIPT_SIZE];   // Resource 1: Event generation script
    size_t script_size;                      // Size of script data
    
    char script_format[64];                  // Resource 2: Script format/media type
    
    // Owning Thing object link
    uint16_t owning_thing_obj_id;            // Resource 3: Object ID of owning thing
    uint16_t owning_thing_instance_id;       // Resource 3: Instance ID of owning thing
    bool has_owning_thing;                   // Whether owning thing is set
    
    time_t last_updated;                     // Resource 4: Last updated timestamp
    
} wot_event_instance_t;

/*
 * WoT Event Object functions
 */
lwm2m_object_t * get_object_wot_event(void);
void free_object_wot_event(lwm2m_object_t * objectP);
void display_wot_event_object(lwm2m_object_t * objectP);

// Instance management functions
uint8_t wot_event_add_instance(lwm2m_object_t * objectP, uint16_t instanceId, 
                               const char* event_id);
uint8_t wot_event_remove_instance(lwm2m_object_t * objectP, uint16_t instanceId);

// Script management
uint8_t wot_event_update_script(lwm2m_object_t * objectP, uint16_t instanceId,
                                const uint8_t* script, size_t script_size, 
                                const char* script_format);

// Owning thing management
uint8_t wot_event_set_owning_thing(lwm2m_object_t * objectP, uint16_t instanceId,
                                   uint16_t thing_obj_id, uint16_t thing_instance_id);
uint8_t wot_event_clear_owning_thing(lwm2m_object_t * objectP, uint16_t instanceId);

// Callback management
void wot_event_set_execute_callback(lwm2m_object_t * objectP, wot_event_execute_callback_t callback);
void wot_event_set_emit_callback(lwm2m_object_t * objectP, wot_event_emit_callback_t callback);

// Event execution
uint8_t wot_event_trigger(lwm2m_object_t * objectP, uint16_t instanceId);

// Utility functions
const char* wot_event_get_identifier(lwm2m_object_t * objectP, uint16_t instanceId);
size_t wot_event_get_script(lwm2m_object_t * objectP, uint16_t instanceId, 
                            const uint8_t** script_ptr);
const char* wot_event_get_script_format(lwm2m_object_t * objectP, uint16_t instanceId);

#ifdef __cplusplus
}
#endif

#endif // OBJECT_WOT_EVENT_H