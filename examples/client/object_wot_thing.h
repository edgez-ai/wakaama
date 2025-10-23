/*******************************************************************************
 *
 * Copyright (c) 2024 edgez.ai - W3C WoT Thing Definition Object
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * Based on LwM2M Object 26250: W3C WoT Thing Definition
 *
 *******************************************************************************/

#ifndef OBJECT_WOT_THING_H
#define OBJECT_WOT_THING_H

#include "liblwm2m.h"

#ifdef __cplusplus
extern "C" {
#endif

// W3C WoT Thing Definition Object ID
#define WOT_THING_OBJECT_ID 26250

// Resource IDs based on XML definition
#define RES_WOT_THING_IDENTIFIER      0  // Thing Identifier (R, String, Mandatory)
#define RES_WOT_THING_TITLE           1  // Title (RW, String, Mandatory)
#define RES_WOT_THING_DESCRIPTION     2  // Description (RW, String, Optional)
#define RES_WOT_THING_PROPERTY_REFS   3  // Property References (RW, Objlnk, Multiple, Optional)
#define RES_WOT_THING_ACTION_REFS     4  // Action References (RW, Objlnk, Multiple, Optional)
#define RES_WOT_THING_EVENT_REFS      5  // Event References (RW, Objlnk, Multiple, Optional)
#define RES_WOT_THING_VERSION         6  // Version (RW, String, Optional)
#define RES_WOT_THING_LAST_UPDATED    7  // Last Updated (R, Time, Optional)

// Maximum number of references for properties, actions, and events
#define MAX_WOT_REFERENCES 16

// WoT Thing instance data structure
typedef struct _wot_thing_instance_
{
    struct _wot_thing_instance_ * next;   // matches lwm2m_list_t
    uint16_t instanceId;                  // matches lwm2m_list_t
    
    // Resource data
    char thing_identifier[128];           // Resource 0: Immutable identifier
    char title[256];                      // Resource 1: Human readable title
    char description[512];                // Resource 2: Rich description
    
    // Object link arrays for references
    lwm2m_data_t property_refs[MAX_WOT_REFERENCES];  // Resource 3: Property references
    uint8_t property_refs_count;
    
    lwm2m_data_t action_refs[MAX_WOT_REFERENCES];    // Resource 4: Action references
    uint8_t action_refs_count;
    
    lwm2m_data_t event_refs[MAX_WOT_REFERENCES];     // Resource 5: Event references
    uint8_t event_refs_count;
    
    char version[32];                     // Resource 6: Semantic version string
    time_t last_updated;                  // Resource 7: Last updated timestamp
    
} wot_thing_instance_t;

/*
 * WoT Thing Object functions
 */
lwm2m_object_t * get_object_wot_thing(void);
void free_object_wot_thing(lwm2m_object_t * objectP);
void display_wot_thing_object(lwm2m_object_t * objectP);

// Instance management functions
uint8_t wot_thing_add_instance(lwm2m_object_t * objectP, uint16_t instanceId, 
                               const char* thing_id, const char* title);
uint8_t wot_thing_remove_instance(lwm2m_object_t * objectP, uint16_t instanceId);

// Property reference management
uint8_t wot_thing_add_property_ref(lwm2m_object_t * objectP, uint16_t instanceId, 
                                   uint16_t obj_id, uint16_t obj_instance_id);
uint8_t wot_thing_remove_property_ref(lwm2m_object_t * objectP, uint16_t instanceId, 
                                      uint16_t obj_id, uint16_t obj_instance_id);

// Action reference management  
uint8_t wot_thing_add_action_ref(lwm2m_object_t * objectP, uint16_t instanceId,
                                 uint16_t obj_id, uint16_t obj_instance_id);
uint8_t wot_thing_remove_action_ref(lwm2m_object_t * objectP, uint16_t instanceId,
                                    uint16_t obj_id, uint16_t obj_instance_id);

// Event reference management
uint8_t wot_thing_add_event_ref(lwm2m_object_t * objectP, uint16_t instanceId,
                                uint16_t obj_id, uint16_t obj_instance_id);
uint8_t wot_thing_remove_event_ref(lwm2m_object_t * objectP, uint16_t instanceId,
                                   uint16_t obj_id, uint16_t obj_instance_id);

// Utility functions
uint8_t wot_thing_update_title(lwm2m_object_t * objectP, uint16_t instanceId, const char* title);
uint8_t wot_thing_update_description(lwm2m_object_t * objectP, uint16_t instanceId, const char* description);
uint8_t wot_thing_update_version(lwm2m_object_t * objectP, uint16_t instanceId, const char* version);

#ifdef __cplusplus
}
#endif

#endif // OBJECT_WOT_THING_H