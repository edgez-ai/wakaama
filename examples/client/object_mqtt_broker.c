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

/*
 *  MQTT Broker Object - Object ID: 18830
 *  URN: urn:oma:lwm2m:x:18830
 *  LwM2M Version: 1.1
 *  Object Version: 1.0
 * 
 *  Resources:
 *
 *          Name                      | ID | Operations | Instances | Mandatory |  Type            | Range      | Units |
 *  URI                               |  0 |    R/W     |  Single   |    Yes    | String           |            |       |
 *  Client Identifier                 |  1 |    R/W     |  Single   |    Yes    | String           |            |       |
 *  Clean Session                     |  2 |    R/W     |  Single   |    Yes    | Boolean          |            |       |
 *  Keep Alive                        |  3 |    R/W     |  Single   |    Yes    | Unsigned Integer | 0-65535    |   s   |
 *  User Name                         |  4 |    R/W     |  Single   |    No     | String           |            |       |
 *  Password                          |  5 |    R/W     |  Single   |    No     | Opaque           | 0-65535    |       |
 *  Security Mode                     |  6 |    R/W     |  Single   |    Yes    | Integer          | 0-4        |       |
 *  Public Key or Identity            |  7 |    R/W     |  Single   |    No     | Opaque           |            |       |
 *  MQTT Broker Public Key            |  8 |    R/W     |  Single   |    No     | Opaque           |            |       |
 *  Secret Key                        |  9 |     W      |  Single   |    No     | Opaque           |            |       |
 *  Certificate Usage                 | 10 |    R/W     |  Single   |    No     | Integer          | 0-3        |       |
 *
 *  Security Mode values:
 *    0: PreShared Key mode
 *    1: Raw Public Key mode
 *    2: Certificate mode
 *    3: NoSec mode
 *    4: Certificate mode with EST
 *
 *  Certificate Usage values:
 *    0: CA constraint
 *    1: service certificate constraint
 *    2: trust anchor assertion
 *    3: domain-issued certificate (default)
 */

#include "liblwm2m.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Resource IDs
#define RES_M_URI                   0
#define RES_M_CLIENT_ID             1
#define RES_M_CLEAN_SESSION         2
#define RES_M_KEEP_ALIVE            3
#define RES_O_USERNAME              4
#define RES_O_PASSWORD              5
#define RES_M_SECURITY_MODE         6
#define RES_O_PUBLIC_KEY_ID         7
#define RES_O_BROKER_PUBLIC_KEY     8
#define RES_O_SECRET_KEY            9
#define RES_O_CERTIFICATE_USAGE     10

// Default values
#define DEFAULT_URI                 "mqtt://mqtt.broker.com:1883"
#define DEFAULT_CLIENT_ID           "lwm2m_client"
#define DEFAULT_KEEP_ALIVE          60
#define DEFAULT_CLEAN_SESSION       true
#define DEFAULT_SECURITY_MODE       3  // NoSec mode
#define DEFAULT_CERTIFICATE_USAGE   3  // domain-issued certificate

// Security Mode values
#define SECURITY_MODE_PSK           0
#define SECURITY_MODE_RPK           1
#define SECURITY_MODE_CERTIFICATE   2
#define SECURITY_MODE_NOSEC         3
#define SECURITY_MODE_CERT_EST      4

// Certificate Usage values
#define CERT_USAGE_CA_CONSTRAINT            0
#define CERT_USAGE_SERVICE_CERT_CONSTRAINT  1
#define CERT_USAGE_TRUST_ANCHOR             2
#define CERT_USAGE_DOMAIN_ISSUED            3

typedef struct _mqtt_broker_instance_
{
    struct _mqtt_broker_instance_ * next;   // matches lwm2m_list_t::next
    uint16_t    instanceId;                 // matches lwm2m_list_t::id
    char        uri[256];
    char        clientId[128];
    bool        cleanSession;
    uint16_t    keepAlive;
    char        username[128];
    uint8_t     password[256];
    size_t      passwordLen;
    uint8_t     securityMode;
    uint8_t     publicKeyOrIdentity[1024];
    size_t      publicKeyOrIdentityLen;
    uint8_t     brokerPublicKey[1024];
    size_t      brokerPublicKeyLen;
    uint8_t     secretKey[1024];
    size_t      secretKeyLen;
    uint8_t     certificateUsage;
} mqtt_broker_instance_t;

static uint8_t prv_mqtt_broker_delete(lwm2m_context_t *contextP,
                                      uint16_t id,
                                      lwm2m_object_t * objectP);
static uint8_t prv_mqtt_broker_create(lwm2m_context_t *contextP,
                                      uint16_t instanceId,
                                      int numData,
                                      lwm2m_data_t * dataArray,
                                      lwm2m_object_t * objectP);

static uint8_t prv_get_value(lwm2m_data_t * dataP,
                             mqtt_broker_instance_t * targetP)
{
    switch (dataP->id)
    {
    case RES_M_URI:
        lwm2m_data_encode_string(targetP->uri, dataP);
        return COAP_205_CONTENT;

    case RES_M_CLIENT_ID:
        lwm2m_data_encode_string(targetP->clientId, dataP);
        return COAP_205_CONTENT;

    case RES_M_CLEAN_SESSION:
        lwm2m_data_encode_bool(targetP->cleanSession, dataP);
        return COAP_205_CONTENT;

    case RES_M_KEEP_ALIVE:
        lwm2m_data_encode_int(targetP->keepAlive, dataP);
        return COAP_205_CONTENT;

    case RES_O_USERNAME:
        if (strlen(targetP->username) > 0)
        {
            lwm2m_data_encode_string(targetP->username, dataP);
            return COAP_205_CONTENT;
        }
        return COAP_404_NOT_FOUND;

    case RES_O_PASSWORD:
        if (targetP->passwordLen > 0)
        {
            lwm2m_data_encode_opaque(targetP->password, targetP->passwordLen, dataP);
            return COAP_205_CONTENT;
        }
        return COAP_404_NOT_FOUND;

    case RES_M_SECURITY_MODE:
        lwm2m_data_encode_int(targetP->securityMode, dataP);
        return COAP_205_CONTENT;

    case RES_O_PUBLIC_KEY_ID:
        if (targetP->publicKeyOrIdentityLen > 0)
        {
            lwm2m_data_encode_opaque(targetP->publicKeyOrIdentity, targetP->publicKeyOrIdentityLen, dataP);
            return COAP_205_CONTENT;
        }
        return COAP_404_NOT_FOUND;

    case RES_O_BROKER_PUBLIC_KEY:
        if (targetP->brokerPublicKeyLen > 0)
        {
            lwm2m_data_encode_opaque(targetP->brokerPublicKey, targetP->brokerPublicKeyLen, dataP);
            return COAP_205_CONTENT;
        }
        return COAP_404_NOT_FOUND;

    case RES_O_SECRET_KEY:
        // Write-only resource
        return COAP_405_METHOD_NOT_ALLOWED;

    case RES_O_CERTIFICATE_USAGE:
        lwm2m_data_encode_int(targetP->certificateUsage, dataP);
        return COAP_205_CONTENT;

    default:
        return COAP_404_NOT_FOUND;
    }
}

static uint8_t prv_mqtt_broker_read(lwm2m_context_t *contextP,
                                    uint16_t instanceId,
                                    int * numDataP,
                                    lwm2m_data_t ** dataArrayP,
                                    lwm2m_object_t * objectP)
{
    mqtt_broker_instance_t * targetP;
    uint8_t result;
    int i;

    /* unused parameter */
    (void)contextP;

    targetP = (mqtt_broker_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    // is the server asking for the full instance ?
    if (*numDataP == 0)
    {
        uint16_t resList[] = {
            RES_M_URI,
            RES_M_CLIENT_ID,
            RES_M_CLEAN_SESSION,
            RES_M_KEEP_ALIVE,
            RES_O_USERNAME,
            RES_O_PASSWORD,
            RES_M_SECURITY_MODE,
            RES_O_PUBLIC_KEY_ID,
            RES_O_BROKER_PUBLIC_KEY,
            RES_O_CERTIFICATE_USAGE
        };
        int nbRes = sizeof(resList)/sizeof(uint16_t);

        // Remove optional resources if not set
        if (strlen(targetP->username) == 0)
        {
            for (i = 0; i < nbRes; i++)
            {
                if (resList[i] == RES_O_USERNAME)
                {
                    nbRes -= 1;
                    memmove(&resList[i], &resList[i+1], (nbRes-i)*sizeof(resList[i]));
                    break;
                }
            }
        }
        if (targetP->passwordLen == 0)
        {
            for (i = 0; i < nbRes; i++)
            {
                if (resList[i] == RES_O_PASSWORD)
                {
                    nbRes -= 1;
                    memmove(&resList[i], &resList[i+1], (nbRes-i)*sizeof(resList[i]));
                    break;
                }
            }
        }
        if (targetP->publicKeyOrIdentityLen == 0)
        {
            for (i = 0; i < nbRes; i++)
            {
                if (resList[i] == RES_O_PUBLIC_KEY_ID)
                {
                    nbRes -= 1;
                    memmove(&resList[i], &resList[i+1], (nbRes-i)*sizeof(resList[i]));
                    break;
                }
            }
        }
        if (targetP->brokerPublicKeyLen == 0)
        {
            for (i = 0; i < nbRes; i++)
            {
                if (resList[i] == RES_O_BROKER_PUBLIC_KEY)
                {
                    nbRes -= 1;
                    memmove(&resList[i], &resList[i+1], (nbRes-i)*sizeof(resList[i]));
                    break;
                }
            }
        }

        *dataArrayP = lwm2m_data_new(nbRes);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = nbRes;
        for (i = 0 ; i < nbRes ; i++)
        {
            (*dataArrayP)[i].id = resList[i];
        }
    }

    i = 0;
    do
    {
        if ((*dataArrayP)[i].type == LWM2M_TYPE_MULTIPLE_RESOURCE)
        {
            result = COAP_404_NOT_FOUND;
        }
        else
        {
            result = prv_get_value((*dataArrayP) + i, targetP);
        }
        i++;
    } while (i < *numDataP && result == COAP_205_CONTENT);

    return result;
}

static uint8_t prv_mqtt_broker_discover(lwm2m_context_t *contextP,
                                        uint16_t instanceId,
                                        int * numDataP,
                                        lwm2m_data_t ** dataArrayP,
                                        lwm2m_object_t * objectP)
{
    mqtt_broker_instance_t * targetP;
    uint8_t result;
    int i;

    /* unused parameter */
    (void)contextP;

    result = COAP_205_CONTENT;

    targetP = (mqtt_broker_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    // is the server asking for the full object ?
    if (*numDataP == 0)
    {
        uint16_t resList[] = {
            RES_M_URI,
            RES_M_CLIENT_ID,
            RES_M_CLEAN_SESSION,
            RES_M_KEEP_ALIVE,
            RES_O_USERNAME,
            RES_O_PASSWORD,
            RES_M_SECURITY_MODE,
            RES_O_PUBLIC_KEY_ID,
            RES_O_BROKER_PUBLIC_KEY,
            RES_O_SECRET_KEY,
            RES_O_CERTIFICATE_USAGE
        };
        int nbRes = sizeof(resList) / sizeof(uint16_t);

        *dataArrayP = lwm2m_data_new(nbRes);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = nbRes;
        for (i = 0; i < nbRes; i++)
        {
            (*dataArrayP)[i].id = resList[i];
        }
    }
    else
    {
        for (i = 0; i < *numDataP && result == COAP_205_CONTENT; i++)
        {
            switch ((*dataArrayP)[i].id)
            {
            case RES_M_URI:
            case RES_M_CLIENT_ID:
            case RES_M_CLEAN_SESSION:
            case RES_M_KEEP_ALIVE:
            case RES_M_SECURITY_MODE:
            case RES_O_CERTIFICATE_USAGE:
                break;

            case RES_O_USERNAME:
                if (strlen(targetP->username) == 0)
                {
                    result = COAP_404_NOT_FOUND;
                }
                break;

            case RES_O_PASSWORD:
                if (targetP->passwordLen == 0)
                {
                    result = COAP_404_NOT_FOUND;
                }
                break;

            case RES_O_PUBLIC_KEY_ID:
                if (targetP->publicKeyOrIdentityLen == 0)
                {
                    result = COAP_404_NOT_FOUND;
                }
                break;

            case RES_O_BROKER_PUBLIC_KEY:
                if (targetP->brokerPublicKeyLen == 0)
                {
                    result = COAP_404_NOT_FOUND;
                }
                break;

            case RES_O_SECRET_KEY:
                // Write-only resource
                break;

            default:
                result = COAP_404_NOT_FOUND;
                break;
            }
        }
    }

    return result;
}

static uint8_t prv_set_int_value(lwm2m_data_t * dataArray, uint32_t * data)
{
    uint8_t result;
    int64_t value;

    if (1 == lwm2m_data_decode_int(dataArray, &value))
    {
        if (value >= 0 && value <= 0xFFFFFFFF)
        {
            *data = value;
            result = COAP_204_CHANGED;
        }
        else
        {
            result = COAP_406_NOT_ACCEPTABLE;
        }
    }
    else
    {
        result = COAP_400_BAD_REQUEST;
    }
    return result;
}

static uint8_t prv_mqtt_broker_write(lwm2m_context_t *contextP,
                                     uint16_t instanceId,
                                     int numData,
                                     lwm2m_data_t * dataArray,
                                     lwm2m_object_t * objectP,
                                     lwm2m_write_type_t writeType)
{
    mqtt_broker_instance_t * targetP;
    int i;
    uint8_t result;

    targetP = (mqtt_broker_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP)
    {
        return COAP_404_NOT_FOUND;
    }

    if (writeType == LWM2M_WRITE_REPLACE_INSTANCE)
    {
        result = prv_mqtt_broker_delete(contextP, instanceId, objectP);
        if (result == COAP_202_DELETED)
        {
            result = prv_mqtt_broker_create(contextP, instanceId, numData, dataArray, objectP);
            if (result == COAP_201_CREATED)
            {
                result = COAP_204_CHANGED;
            }
        }
        return result;
    }

    i = 0;
    do
    {
        /* No multiple instance resources */
        if (dataArray[i].type == LWM2M_TYPE_MULTIPLE_RESOURCE)
        {
            result = COAP_404_NOT_FOUND;
            continue;
        }

        switch (dataArray[i].id)
        {
        case RES_M_URI:
            if ((dataArray[i].type == LWM2M_TYPE_STRING || dataArray[i].type == LWM2M_TYPE_OPAQUE)
             && dataArray[i].value.asBuffer.length > 0 && dataArray[i].value.asBuffer.length < 256)
            {
                memset(targetP->uri, 0, sizeof(targetP->uri));
                memcpy(targetP->uri, dataArray[i].value.asBuffer.buffer, dataArray[i].value.asBuffer.length);
                targetP->uri[dataArray[i].value.asBuffer.length] = '\0';
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;

        case RES_M_CLIENT_ID:
            if ((dataArray[i].type == LWM2M_TYPE_STRING || dataArray[i].type == LWM2M_TYPE_OPAQUE)
             && dataArray[i].value.asBuffer.length > 0 && dataArray[i].value.asBuffer.length < 128)
            {
                memset(targetP->clientId, 0, sizeof(targetP->clientId));
                memcpy(targetP->clientId, dataArray[i].value.asBuffer.buffer, dataArray[i].value.asBuffer.length);
                targetP->clientId[dataArray[i].value.asBuffer.length] = '\0';
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;

        case RES_M_CLEAN_SESSION:
        {
            bool value;
            if (1 == lwm2m_data_decode_bool(dataArray + i, &value))
            {
                targetP->cleanSession = value;
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;
        }

        case RES_M_KEEP_ALIVE:
        {
            uint32_t value = targetP->keepAlive;
            result = prv_set_int_value(dataArray + i, &value);
            if (COAP_204_CHANGED == result)
            {
                if (value <= 0xFFFF)
                {
                    targetP->keepAlive = value;
                }
                else
                {
                    result = COAP_406_NOT_ACCEPTABLE;
                }
            }
            break;
        }

        case RES_O_USERNAME:
            if ((dataArray[i].type == LWM2M_TYPE_STRING || dataArray[i].type == LWM2M_TYPE_OPAQUE)
             && dataArray[i].value.asBuffer.length < 128)
            {
                memset(targetP->username, 0, sizeof(targetP->username));
                if (dataArray[i].value.asBuffer.length > 0)
                {
                    memcpy(targetP->username, dataArray[i].value.asBuffer.buffer, dataArray[i].value.asBuffer.length);
                    targetP->username[dataArray[i].value.asBuffer.length] = '\0';
                }
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;

        case RES_O_PASSWORD:
            if ((dataArray[i].type == LWM2M_TYPE_STRING || dataArray[i].type == LWM2M_TYPE_OPAQUE)
             && dataArray[i].value.asBuffer.length <= 0xFFFF)
            {
                memset(targetP->password, 0, sizeof(targetP->password));
                targetP->passwordLen = 0;
                if (dataArray[i].value.asBuffer.length > 0)
                {
                    if (dataArray[i].value.asBuffer.length <= sizeof(targetP->password))
                    {
                        memcpy(targetP->password, dataArray[i].value.asBuffer.buffer, dataArray[i].value.asBuffer.length);
                        targetP->passwordLen = dataArray[i].value.asBuffer.length;
                    }
                    else
                    {
                        result = COAP_413_ENTITY_TOO_LARGE;
                        break;
                    }
                }
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;

        case RES_M_SECURITY_MODE:
        {
            int64_t value;
            if (1 == lwm2m_data_decode_int(dataArray + i, &value))
            {
                if (value >= 0 && value <= 4)
                {
                    targetP->securityMode = value;
                    result = COAP_204_CHANGED;
                }
                else
                {
                    result = COAP_406_NOT_ACCEPTABLE;
                }
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;
        }

        case RES_O_PUBLIC_KEY_ID:
            if (dataArray[i].type == LWM2M_TYPE_OPAQUE)
            {
                memset(targetP->publicKeyOrIdentity, 0, sizeof(targetP->publicKeyOrIdentity));
                targetP->publicKeyOrIdentityLen = 0;
                if (dataArray[i].value.asBuffer.length > 0)
                {
                    if (dataArray[i].value.asBuffer.length <= sizeof(targetP->publicKeyOrIdentity))
                    {
                        memcpy(targetP->publicKeyOrIdentity, dataArray[i].value.asBuffer.buffer, 
                               dataArray[i].value.asBuffer.length);
                        targetP->publicKeyOrIdentityLen = dataArray[i].value.asBuffer.length;
                    }
                    else
                    {
                        result = COAP_413_ENTITY_TOO_LARGE;
                        break;
                    }
                }
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;

        case RES_O_BROKER_PUBLIC_KEY:
            if (dataArray[i].type == LWM2M_TYPE_OPAQUE)
            {
                memset(targetP->brokerPublicKey, 0, sizeof(targetP->brokerPublicKey));
                targetP->brokerPublicKeyLen = 0;
                if (dataArray[i].value.asBuffer.length > 0)
                {
                    if (dataArray[i].value.asBuffer.length <= sizeof(targetP->brokerPublicKey))
                    {
                        memcpy(targetP->brokerPublicKey, dataArray[i].value.asBuffer.buffer, 
                               dataArray[i].value.asBuffer.length);
                        targetP->brokerPublicKeyLen = dataArray[i].value.asBuffer.length;
                    }
                    else
                    {
                        result = COAP_413_ENTITY_TOO_LARGE;
                        break;
                    }
                }
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;

        case RES_O_SECRET_KEY:
            if (dataArray[i].type == LWM2M_TYPE_OPAQUE)
            {
                memset(targetP->secretKey, 0, sizeof(targetP->secretKey));
                targetP->secretKeyLen = 0;
                if (dataArray[i].value.asBuffer.length > 0)
                {
                    if (dataArray[i].value.asBuffer.length <= sizeof(targetP->secretKey))
                    {
                        memcpy(targetP->secretKey, dataArray[i].value.asBuffer.buffer, 
                               dataArray[i].value.asBuffer.length);
                        targetP->secretKeyLen = dataArray[i].value.asBuffer.length;
                    }
                    else
                    {
                        result = COAP_413_ENTITY_TOO_LARGE;
                        break;
                    }
                }
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;

        case RES_O_CERTIFICATE_USAGE:
        {
            int64_t value;
            if (1 == lwm2m_data_decode_int(dataArray + i, &value))
            {
                if (value >= 0 && value <= 3)
                {
                    targetP->certificateUsage = value;
                    result = COAP_204_CHANGED;
                }
                else
                {
                    result = COAP_406_NOT_ACCEPTABLE;
                }
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;
        }

        default:
            return COAP_404_NOT_FOUND;
        }
        i++;
    } while (i < numData && result == COAP_204_CHANGED);

    return result;
}

static uint8_t prv_mqtt_broker_delete(lwm2m_context_t *contextP,
                                      uint16_t id,
                                      lwm2m_object_t * objectP)
{
    mqtt_broker_instance_t * mqttBrokerInstance;

    /* unused parameter */
    (void)contextP;

    objectP->instanceList = lwm2m_list_remove(objectP->instanceList, id, (lwm2m_list_t **)&mqttBrokerInstance);
    if (NULL == mqttBrokerInstance) return COAP_404_NOT_FOUND;

    lwm2m_free(mqttBrokerInstance);

    return COAP_202_DELETED;
}

static uint8_t prv_mqtt_broker_create(lwm2m_context_t *contextP,
                                      uint16_t instanceId,
                                      int numData,
                                      lwm2m_data_t * dataArray,
                                      lwm2m_object_t * objectP)
{
    mqtt_broker_instance_t * mqttBrokerInstance;
    uint8_t result;

    mqttBrokerInstance = (mqtt_broker_instance_t *)lwm2m_malloc(sizeof(mqtt_broker_instance_t));
    if (NULL == mqttBrokerInstance) return COAP_500_INTERNAL_SERVER_ERROR;
    memset(mqttBrokerInstance, 0, sizeof(mqtt_broker_instance_t));

    mqttBrokerInstance->instanceId = instanceId;
    
    // Set default values
    strncpy(mqttBrokerInstance->uri, DEFAULT_URI, sizeof(mqttBrokerInstance->uri) - 1);
    strncpy(mqttBrokerInstance->clientId, DEFAULT_CLIENT_ID, sizeof(mqttBrokerInstance->clientId) - 1);
    mqttBrokerInstance->cleanSession = DEFAULT_CLEAN_SESSION;
    mqttBrokerInstance->keepAlive = DEFAULT_KEEP_ALIVE;
    mqttBrokerInstance->securityMode = DEFAULT_SECURITY_MODE;
    mqttBrokerInstance->certificateUsage = DEFAULT_CERTIFICATE_USAGE;

    objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, mqttBrokerInstance);

    result = prv_mqtt_broker_write(contextP, instanceId, numData, dataArray, objectP, LWM2M_WRITE_REPLACE_RESOURCES);

    if (result != COAP_204_CHANGED)
    {
        (void)prv_mqtt_broker_delete(contextP, instanceId, objectP);
    }
    else
    {
        result = COAP_201_CREATED;
    }

    return result;
}

void copy_mqtt_broker_object(lwm2m_object_t * objectDest, lwm2m_object_t * objectSrc)
{
    memcpy(objectDest, objectSrc, sizeof(lwm2m_object_t));
    objectDest->instanceList = NULL;
    objectDest->userData = NULL;
    mqtt_broker_instance_t * instanceSrc = (mqtt_broker_instance_t *)objectSrc->instanceList;
    mqtt_broker_instance_t * previousInstanceDest = NULL;
    while (instanceSrc != NULL)
    {
        mqtt_broker_instance_t * instanceDest = (mqtt_broker_instance_t *)lwm2m_malloc(sizeof(mqtt_broker_instance_t));
        if (NULL == instanceDest)
        {
            return;
        }
        memcpy(instanceDest, instanceSrc, sizeof(mqtt_broker_instance_t));
        instanceSrc = (mqtt_broker_instance_t *)instanceSrc->next;
        if (previousInstanceDest == NULL)
        {
            objectDest->instanceList = (lwm2m_list_t *)instanceDest;
        }
        else
        {
            previousInstanceDest->next = instanceDest;
        }
        previousInstanceDest = instanceDest;
    }
}

void display_mqtt_broker_object(lwm2m_object_t * object)
{
    fprintf(stdout, "  /%u: MQTT Broker object, instances:\r\n", object->objID);
    mqtt_broker_instance_t * mqttBrokerInstance = (mqtt_broker_instance_t *)object->instanceList;
    while (mqttBrokerInstance != NULL)
    {
        fprintf(stdout, "    /%u/%u: instanceId: %u, uri: %s, clientId: %s, ",
                object->objID, mqttBrokerInstance->instanceId,
                mqttBrokerInstance->instanceId, mqttBrokerInstance->uri,
                mqttBrokerInstance->clientId);
        
        if (strlen(mqttBrokerInstance->username) > 0)
        {
            fprintf(stdout, "username: %s, ", mqttBrokerInstance->username);
        }
        
        fprintf(stdout, "keepAlive: %u, cleanSession: %s, securityMode: %u",
                mqttBrokerInstance->keepAlive,
                mqttBrokerInstance->cleanSession ? "true" : "false",
                mqttBrokerInstance->securityMode);
        
        fprintf(stdout, "\r\n");
        mqttBrokerInstance = (mqtt_broker_instance_t *)mqttBrokerInstance->next;
    }
}

lwm2m_object_t * get_mqtt_broker_object(const char* uri,
                                        const char* clientId,
                                        const char* username,
                                        const char* password)
{
    lwm2m_object_t * mqttBrokerObj;

    mqttBrokerObj = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));

    if (NULL != mqttBrokerObj)
    {
        mqtt_broker_instance_t * mqttBrokerInstance;

        memset(mqttBrokerObj, 0, sizeof(lwm2m_object_t));

        // Use LwM2M MQTT Broker object ID 18830
        mqttBrokerObj->objID = 18830;

        // Manually create an hardcoded MQTT broker instance
        mqttBrokerInstance = (mqtt_broker_instance_t *)lwm2m_malloc(sizeof(mqtt_broker_instance_t));
        if (NULL == mqttBrokerInstance)
        {
            lwm2m_free(mqttBrokerObj);
            return NULL;
        }

        memset(mqttBrokerInstance, 0, sizeof(mqtt_broker_instance_t));
        mqttBrokerInstance->instanceId = 0;
        
        strncpy(mqttBrokerInstance->uri, uri ? uri : DEFAULT_URI, 
                sizeof(mqttBrokerInstance->uri) - 1);
        strncpy(mqttBrokerInstance->clientId, clientId ? clientId : DEFAULT_CLIENT_ID,
                sizeof(mqttBrokerInstance->clientId) - 1);
        
        if (username)
        {
            strncpy(mqttBrokerInstance->username, username, sizeof(mqttBrokerInstance->username) - 1);
        }
        if (password)
        {
            size_t pwdLen = strlen(password);
            if (pwdLen > 0 && pwdLen < sizeof(mqttBrokerInstance->password))
            {
                memcpy(mqttBrokerInstance->password, password, pwdLen);
                mqttBrokerInstance->passwordLen = pwdLen;
            }
        }
        
        mqttBrokerInstance->cleanSession = DEFAULT_CLEAN_SESSION;
        mqttBrokerInstance->keepAlive = DEFAULT_KEEP_ALIVE;
        mqttBrokerInstance->securityMode = DEFAULT_SECURITY_MODE;
        mqttBrokerInstance->certificateUsage = DEFAULT_CERTIFICATE_USAGE;

        mqttBrokerObj->instanceList = LWM2M_LIST_ADD(mqttBrokerObj->instanceList, mqttBrokerInstance);

        mqttBrokerObj->readFunc = prv_mqtt_broker_read;
        mqttBrokerObj->discoverFunc = prv_mqtt_broker_discover;
        mqttBrokerObj->writeFunc = prv_mqtt_broker_write;
        mqttBrokerObj->createFunc = prv_mqtt_broker_create;
        mqttBrokerObj->deleteFunc = prv_mqtt_broker_delete;
        mqttBrokerObj->executeFunc = NULL;
    }

    return mqttBrokerObj;
}

void clean_mqtt_broker_object(lwm2m_object_t * object)
{
    while (object->instanceList != NULL)
    {
        mqtt_broker_instance_t * mqttBrokerInstance = (mqtt_broker_instance_t *)object->instanceList;
        object->instanceList = object->instanceList->next;
        lwm2m_free(mqttBrokerInstance);
    }
}
