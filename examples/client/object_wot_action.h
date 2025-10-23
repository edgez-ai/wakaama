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

#ifndef OBJECT_WOT_ACTION_H
#define OBJECT_WOT_ACTION_H

#include "liblwm2m.h"

#ifdef __cplusplus
extern "C" {
#endif

// W3C WoT Action Object ID
#define WOT_ACTION_OBJECT_ID 26252

// Resource IDs based on XML definition
#define RES_WOT_ACTION_IDENTIFIER     0  // Action Identifier (R, String, Mandatory)
#define RES_WOT_ACTION_SCRIPT         1  // Script (RW, Opaque, Mandatory)
#define RES_WOT_ACTION_SCRIPT_FORMAT  2  // Script Format (RW, String, Optional)
#define RES_WOT_ACTION_OWNING_THING   3  // Owning Thing (RW, Objlnk, Optional)
#define RES_WOT_ACTION_LAST_UPDATED   4  // Last Updated (R, Time, Optional)

// Maximum script size
#define MAX_WOT_ACTION_SCRIPT_SIZE 2048

// Script execution callback type
typedef int (*wot_action_execute_callback_t)(const char* action_id, const uint8_t* script, 
                                             size_t script_size, const char* script_format);

// WoT Action instance data structure
typedef struct _wot_action_instance_
{
    struct _wot_action_instance_ * next;     // matches lwm2m_list_t
    uint16_t instanceId;                     // matches lwm2m_list_t
    
    // Resource data
    char action_identifier[128];             // Resource 0: Immutable identifier
    
    // Script data
    uint8_t script[MAX_WOT_ACTION_SCRIPT_SIZE];  // Resource 1: Executable script
    size_t script_size;                      // Size of script data
    
    char script_format[64];                  // Resource 2: Script format/media type
    
    // Owning Thing object link
    uint16_t owning_thing_obj_id;            // Resource 3: Object ID of owning thing
    uint16_t owning_thing_instance_id;       // Resource 3: Instance ID of owning thing
    bool has_owning_thing;                   // Whether owning thing is set
    
    time_t last_updated;                     // Resource 4: Last updated timestamp
    
} wot_action_instance_t;

/*
 * WoT Action Object functions
 */
lwm2m_object_t * get_object_wot_action(void);
void free_object_wot_action(lwm2m_object_t * objectP);
void display_wot_action_object(lwm2m_object_t * objectP);

// Instance management functions
uint8_t wot_action_add_instance(lwm2m_object_t * objectP, uint16_t instanceId, 
                                const char* action_id);
uint8_t wot_action_remove_instance(lwm2m_object_t * objectP, uint16_t instanceId);

// Script management
uint8_t wot_action_update_script(lwm2m_object_t * objectP, uint16_t instanceId,
                                 const uint8_t* script, size_t script_size, 
                                 const char* script_format);

// Owning thing management
uint8_t wot_action_set_owning_thing(lwm2m_object_t * objectP, uint16_t instanceId,
                                    uint16_t thing_obj_id, uint16_t thing_instance_id);
uint8_t wot_action_clear_owning_thing(lwm2m_object_t * objectP, uint16_t instanceId);

// Execution callback management
void wot_action_set_execute_callback(lwm2m_object_t * objectP, wot_action_execute_callback_t callback);

// Utility functions
const char* wot_action_get_identifier(lwm2m_object_t * objectP, uint16_t instanceId);
size_t wot_action_get_script(lwm2m_object_t * objectP, uint16_t instanceId, 
                             const uint8_t** script_ptr);
const char* wot_action_get_script_format(lwm2m_object_t * objectP, uint16_t instanceId);

#ifdef __cplusplus
}
#endif

#endif // OBJECT_WOT_ACTION_H