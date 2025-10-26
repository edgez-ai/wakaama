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

#ifndef OBJECT_GATEWAY_H
#define OBJECT_GATEWAY_H

#include "liblwm2m.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
typedef struct _gateway_instance_ gateway_instance_t;

// Gateway Management Object ID (custom)
#define GATEWAY_OBJECT_ID 25

// Connection type enumeration
typedef enum {
    CONNECTION_WIFI = 0,
    CONNECTION_BLE = 1,
    CONNECTION_LORA = 2,
    CONNECTION_RS485 = 3
} connection_type_t;

// Callback function type for updating device instance_id in external storage
typedef void (*gateway_device_update_callback_t)(uint32_t device_id, uint16_t new_instance_id);

// Callback function type for triggering LwM2M registration updates
typedef void (*gateway_registration_update_callback_t)(void);

// Structure to hold callback functions
typedef struct {
    gateway_device_update_callback_t device_update_callback;
    gateway_registration_update_callback_t registration_update_callback;
} gateway_callbacks_t;

// Gateway Device instance data structure
typedef struct _gateway_instance_
{
    struct _gateway_instance_ * next;   // matches lwm2m_list_t
    uint16_t instanceId;               // matches lwm2m_list_t (16-bit, internal LwM2M instance ID)
    
    // Device-specific data
    uint32_t device_id;                // 32-bit device ID (read-only)
    uint16_t server_instance_id;       // Server-assigned instance ID (read-write via resource 1)
    connection_type_t connection_type; // Connection type enum (read-only)
    int64_t last_seen;                 // Last seen timestamp (read-only)
    bool online;                       // Online status (read-only)
} gateway_instance_t;

/*
 * Gateway Object functions
 */
lwm2m_object_t * get_object_gateway(void);
void free_object_gateway(lwm2m_object_t * objectP);
void display_gateway_object(lwm2m_object_t * objectP);

// Instance management functions
uint8_t gateway_add_instance(lwm2m_object_t * objectP, uint16_t instanceId, uint32_t device_id, connection_type_t conn_type);
uint8_t gateway_remove_instance(lwm2m_object_t * objectP, uint16_t instanceId);

// Find instance by internal instanceId (ring buffer index)
gateway_instance_t * gateway_find_by_internal_id(lwm2m_object_t * objectP, uint16_t internal_instance_id);

// Find instance by server_instance_id (device->instance_id)
gateway_instance_t * gateway_find_by_server_id(lwm2m_object_t * objectP, uint16_t server_instance_id);

// Debug helper to list all instances
void gateway_debug_list_instances(lwm2m_object_t * objectP);

// Getter functions for external access
int gateway_get_device_id(lwm2m_object_t * objectP, uint16_t instanceId, uint32_t *out);
int gateway_get_connection_type(lwm2m_object_t * objectP, uint16_t instanceId, connection_type_t *out);
int gateway_get_last_seen(lwm2m_object_t * objectP, uint16_t instanceId, time_t *out);
int gateway_get_online_status(lwm2m_object_t * objectP, uint16_t instanceId, bool *out);

// Device status update helpers
uint8_t gateway_update_device_status(lwm2m_object_t * objectP, uint16_t instanceId, bool online);

// Set callback for device instance_id updates
void gateway_set_device_update_callback(lwm2m_object_t * objectP, gateway_device_update_callback_t callback);

// Set callback for triggering registration updates
void gateway_set_registration_update_callback(lwm2m_object_t * objectP, gateway_registration_update_callback_t callback);

// Remove instance and trigger registration update
uint8_t gateway_remove_and_recreate_instance(lwm2m_object_t * objectP, uint16_t old_instanceId, 
                                             uint32_t device_id, uint16_t new_instance_id, connection_type_t conn_type);

#ifdef __cplusplus
}
#endif

#endif // OBJECT_GATEWAY_H