

#include "object_vendor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "liblwm2m.h"

#include "esp_log.h"

#define PRV_TLV_BUFFER_SIZE 128

// Resource IDs
#define RID_PUBLIC_KEY 0

// Support up to 10 vendor data items
#define MAX_VENDOR_DATA 10
static vendor_data_t g_vendor_data[MAX_VENDOR_DATA];
static int g_vendor_data_count = 0;

static const char *TAG = "object_vendor";

// Correct callback signatures for wakaama
static uint8_t prv_read(lwm2m_context_t *contextP, uint32_t instanceId, int *numDataP, lwm2m_data_t **dataArrayP, lwm2m_object_t *objectP) {
    for (int i = 0; i < *numDataP; i++) {
        switch ((*dataArrayP)[i].id) {
            case RID_PUBLIC_KEY:

                break;
            default:
                return COAP_404_NOT_FOUND;
        }
    }
    return COAP_205_CONTENT;
}

static uint8_t prv_write(lwm2m_context_t *contextP, uint32_t instanceId, int numData, lwm2m_data_t *dataArray, lwm2m_object_t *objectP, lwm2m_write_type_t writeType) {
    for (int i = 0; i < numData; i++) {
        switch (dataArray[i].id) {
            case RID_PUBLIC_KEY: {

                break;
            }
            default:
                return COAP_404_NOT_FOUND;
        }
    }
    return COAP_204_CHANGED;
}

static uint8_t prv_execute(lwm2m_context_t *contextP, uint32_t instanceId, uint16_t resourceId, uint8_t *buffer, int length, lwm2m_object_t *objectP) {
    // No executable resources for now
    ESP_LOGI(TAG, "Execute called: instanceId=%d, resourceId=%d", instanceId, resourceId);
    return COAP_404_NOT_FOUND;
}
static uint8_t prv_create(lwm2m_context_t *contextP, uint32_t instanceId, int numData, lwm2m_data_t *dataArray, lwm2m_object_t *objectP) {
    ESP_LOGI(TAG, "Create called: instanceId=%d", instanceId);
    for (int i = 0; i < numData; i++) {
        switch (dataArray[i].id) {
            case RID_PUBLIC_KEY: {
                if (dataArray[i].type == LWM2M_TYPE_STRING && dataArray[i].value.asBuffer.length < sizeof(g_vendor_data[0].public_key)) {
                    if (g_vendor_data_count < MAX_VENDOR_DATA) {
                        memcpy(g_vendor_data[g_vendor_data_count].public_key, dataArray[i].value.asBuffer.buffer, dataArray[i].value.asBuffer.length);
                        g_vendor_data[g_vendor_data_count].public_key[dataArray[i].value.asBuffer.length] = '\0';
                        // Optionally set other fields, e.g., id
                        g_vendor_data[g_vendor_data_count].instance_id = instanceId;
                        ESP_LOGI(TAG, "Create public_key[%d]: %s", g_vendor_data_count, g_vendor_data[g_vendor_data_count].public_key);
                        g_vendor_data_count++;
                    } else {
                        ESP_LOGW(TAG, "Max vendor data reached");
                        return COAP_500_INTERNAL_SERVER_ERROR;
                    }
                }
                break;
            }
            default:
                return COAP_404_NOT_FOUND;
        }
    }
    // Optionally add instance to objectP->instanceList if needed
    return COAP_201_CREATED;
}

lwm2m_object_t *get_vendor_object(void) {
    lwm2m_object_t *obj = (lwm2m_object_t *)malloc(sizeof(lwm2m_object_t));
    if (obj == NULL) return NULL;
    memset(obj, 0, sizeof(lwm2m_object_t));
    obj->objID = VENDOR_OBJECT_ID;
    obj->instanceList = NULL;
    obj->readFunc = prv_read;
    obj->writeFunc = prv_write;
    obj->executeFunc = prv_execute;
    obj->createFunc = prv_create;
    // Add more initialization as needed
    return obj;
}
