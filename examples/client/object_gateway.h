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

// Gateway Management Object ID (custom)
#define GATEWAY_OBJECT_ID 25

// Gateway instance data structure
typedef struct _gateway_instance_
{
    struct _gateway_instance_ * next;   // matches lwm2m_list_t
    uint16_t instanceId;               // matches lwm2m_list_t
    
    // Gateway-specific data
    int64_t connected_devices;         // Number of connected devices
    int64_t max_devices;              // Maximum number of devices supported
    int64_t active_sessions;          // Number of active sessions
    int64_t total_data_rx;            // Total data received (bytes)
    int64_t total_data_tx;            // Total data transmitted (bytes)
    int64_t uptime;                   // Gateway uptime (seconds)
    char gateway_id[32];              // Gateway identifier
    char firmware_version[16];        // Gateway firmware version
    char status[16];                  // Gateway status (active/inactive/maintenance)
    bool auto_registration;           // Auto device registration enabled
} gateway_instance_t;

/*
 * Gateway Object functions
 */
lwm2m_object_t * get_object_gateway(void);
void free_object_gateway(lwm2m_object_t * objectP);
void display_gateway_object(lwm2m_object_t * objectP);

// Instance management functions
uint8_t gateway_add_instance(lwm2m_object_t * objectP, uint16_t instanceId);
uint8_t gateway_remove_instance(lwm2m_object_t * objectP, uint16_t instanceId);

// Value update functions
uint8_t gateway_update_instance_value(lwm2m_object_t * objectP, uint16_t instanceId, uint16_t resourceId, int64_t value);
uint8_t gateway_update_instance_string(lwm2m_object_t * objectP, uint16_t instanceId, uint16_t resourceId, const char* value);
uint8_t gateway_update_instance_bool(lwm2m_object_t * objectP, uint16_t instanceId, uint16_t resourceId, bool value);

// Getter functions for external access
int gateway_get_connected_devices(lwm2m_object_t * objectP, uint16_t instanceId, int64_t *out);
int gateway_get_active_sessions(lwm2m_object_t * objectP, uint16_t instanceId, int64_t *out);
int gateway_get_uptime(lwm2m_object_t * objectP, uint16_t instanceId, int64_t *out);
int gateway_get_status(lwm2m_object_t * objectP, uint16_t instanceId, char *buffer, size_t bufLen);

// Statistics update helpers
uint8_t gateway_increment_rx_data(lwm2m_object_t * objectP, uint16_t instanceId, uint64_t bytes);
uint8_t gateway_increment_tx_data(lwm2m_object_t * objectP, uint16_t instanceId, uint64_t bytes);

#ifdef __cplusplus
}
#endif

#endif // OBJECT_GATEWAY_H