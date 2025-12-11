/*******************************************************************************
 *
 * Copyright (c) 2024 EdgeZ.ai
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

#ifndef OBJECT_MQTT_BROKER_H_
#define OBJECT_MQTT_BROKER_H_

#include "liblwm2m.h"

/**
 * @brief Create and initialize an MQTT Broker object (LwM2M Object 18830)
 * 
 * @param uri MQTT broker URI (e.g., "mqtt://broker.example.com:1883" or "mqtts://broker.example.com:8883")
 * @param clientId MQTT client identifier
 * @param username MQTT username (can be NULL if not required)
 * @param password MQTT password (can be NULL if not required)
 * @return lwm2m_object_t* Pointer to the created object, or NULL on error
 */
lwm2m_object_t * get_mqtt_broker_object(const char* uri,
                                        const char* clientId,
                                        const char* username,
                                        const char* password);

/**
 * @brief Copy an MQTT Broker object
 * 
 * @param objectDest Destination object
 * @param objectSrc Source object
 */
void copy_mqtt_broker_object(lwm2m_object_t * objectDest, lwm2m_object_t * objectSrc);

/**
 * @brief Display the MQTT Broker object information
 * 
 * @param object Pointer to the MQTT Broker object
 */
void display_mqtt_broker_object(lwm2m_object_t * object);

/**
 * @brief Clean up and free the MQTT Broker object
 * 
 * @param object Pointer to the MQTT Broker object
 */
void clean_mqtt_broker_object(lwm2m_object_t * object);

#endif /* OBJECT_MQTT_BROKER_H_ */
