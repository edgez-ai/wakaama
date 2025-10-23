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

#ifndef OBJECT_WOT_DATA_FEATURE_H
#define OBJECT_WOT_DATA_FEATURE_H

#include "liblwm2m.h"

#ifdef __cplusplus
extern "C" {
#endif

// W3C WoT Data Feature Object ID
#define WOT_DATA_FEATURE_OBJECT_ID 26251

// Resource IDs based on XML definition
#define RES_WOT_FEATURE_IDENTIFIER    0  // Feature Identifier (R, String, Mandatory)
#define RES_WOT_LINKED_RESOURCES      1  // Linked Resources (RW, String, Multiple, Mandatory)
#define RES_WOT_OWNING_THING          2  // Owning Thing (RW, Objlnk, Optional)
#define RES_WOT_FEATURE_LAST_UPDATED  3  // Last Updated (R, Time, Optional)

// Maximum number of linked resources per feature
#define MAX_WOT_LINKED_RESOURCES 16

// WoT Data Feature instance data structure
typedef struct _wot_data_feature_instance_
{
    struct _wot_data_feature_instance_ * next;   // matches lwm2m_list_t
    uint16_t instanceId;                         // matches lwm2m_list_t
    
    // Resource data
    char feature_identifier[128];                // Resource 0: Immutable identifier
    
    // Linked resources array (e.g., "/3303/0/5700")
    char linked_resources[MAX_WOT_LINKED_RESOURCES][64];  // Resource 1: LwM2M resource paths
    uint8_t linked_resources_count;
    
    // Owning Thing object link
    uint16_t owning_thing_obj_id;                // Resource 2: Object ID of owning thing
    uint16_t owning_thing_instance_id;           // Resource 2: Instance ID of owning thing
    bool has_owning_thing;                       // Whether owning thing is set
    
    time_t last_updated;                         // Resource 3: Last updated timestamp
    
} wot_data_feature_instance_t;

/*
 * WoT Data Feature Object functions
 */
lwm2m_object_t * get_object_wot_data_feature(void);
void free_object_wot_data_feature(lwm2m_object_t * objectP);
void display_wot_data_feature_object(lwm2m_object_t * objectP);

// Instance management functions
uint8_t wot_data_feature_add_instance(lwm2m_object_t * objectP, uint16_t instanceId, 
                                      const char* feature_id);
uint8_t wot_data_feature_remove_instance(lwm2m_object_t * objectP, uint16_t instanceId);

// Linked resource management
uint8_t wot_data_feature_add_linked_resource(lwm2m_object_t * objectP, uint16_t instanceId, 
                                             const char* resource_path);
uint8_t wot_data_feature_remove_linked_resource(lwm2m_object_t * objectP, uint16_t instanceId, 
                                                const char* resource_path);
uint8_t wot_data_feature_clear_linked_resources(lwm2m_object_t * objectP, uint16_t instanceId);

// Owning thing management
uint8_t wot_data_feature_set_owning_thing(lwm2m_object_t * objectP, uint16_t instanceId,
                                          uint16_t thing_obj_id, uint16_t thing_instance_id);
uint8_t wot_data_feature_clear_owning_thing(lwm2m_object_t * objectP, uint16_t instanceId);

// Utility functions
int wot_data_feature_get_linked_resource_count(lwm2m_object_t * objectP, uint16_t instanceId);
const char* wot_data_feature_get_linked_resource(lwm2m_object_t * objectP, uint16_t instanceId, int index);

#ifdef __cplusplus
}
#endif

#endif // OBJECT_WOT_DATA_FEATURE_H