

#include "object_vendor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "liblwm2m.h"

#include "esp_log.h"

#define PRV_TLV_BUFFER_SIZE 128

// Resource IDs
#define RID_VENDOR_ID 0
#define RID_MAC_OUI 1

// Support up to 10 vendor data items
#define MAX_VENDOR_DATA 10
static vendor_data_t g_vendor_data[MAX_VENDOR_DATA];
static int g_vendor_data_count = 0;

static const char *TAG = "object_vendor";

// Correct callback signatures for wakaama
static uint8_t prv_read(lwm2m_context_t *contextP, uint16_t instanceId, int *numDataP, lwm2m_data_t **dataArrayP, lwm2m_object_t *objectP) {
    ESP_LOGI(TAG, "[prv_read] ENTER: instanceId=%d, numData=%d", instanceId, *numDataP);
    
    // Find the vendor data for this instance
    int data_index = -1;
    for (int i = 0; i < g_vendor_data_count; i++) {
        if (g_vendor_data[i].instance_id == instanceId) {
            data_index = i;
            break;
        }
    }
    
    if (data_index == -1) {
        ESP_LOGE(TAG, "[prv_read] Instance %d not found (total instances: %d)", instanceId, g_vendor_data_count);
        return COAP_404_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "[prv_read] Found instance %d at index %d", instanceId, data_index);
    
    for (int i = 0; i < *numDataP; i++) {
        ESP_LOGI(TAG, "[prv_read] Processing resource[%d]: id=%d", i, (*dataArrayP)[i].id);
        switch ((*dataArrayP)[i].id) {
            case RID_VENDOR_ID:
                lwm2m_data_encode_int(g_vendor_data[data_index].vendor_id, &(*dataArrayP)[i]);
                ESP_LOGI(TAG, "[prv_read] Read vendor_id: %lld", g_vendor_data[data_index].vendor_id);
                break;
            case RID_MAC_OUI:
                lwm2m_data_encode_int(g_vendor_data[data_index].mac_oui, &(*dataArrayP)[i]);
                ESP_LOGI(TAG, "[prv_read] Read mac_oui: %u", g_vendor_data[data_index].mac_oui);
                break;
            default:
                ESP_LOGE(TAG, "[prv_read] Unknown resource ID: %d", (*dataArrayP)[i].id);
                return COAP_404_NOT_FOUND;
        }
    }
    ESP_LOGI(TAG, "[prv_read] EXIT: SUCCESS (COAP_205_CONTENT)");
    return COAP_205_CONTENT;
}

static uint8_t prv_write(lwm2m_context_t *contextP, uint16_t instanceId, int numData, lwm2m_data_t *dataArray, lwm2m_object_t *objectP, lwm2m_write_type_t writeType) {
    ESP_LOGW(TAG, "[prv_write] ENTER: instanceId=%d, numData=%d, writeType=%d", instanceId, numData, writeType);
    for (int i = 0; i < numData; i++) {
        ESP_LOGW(TAG, "[prv_write] Resource[%d]: id=%d, type=%d", i, dataArray[i].id, dataArray[i].type);
    }
    // All resources are read-only, reject writes
    ESP_LOGW(TAG, "[prv_write] EXIT: Write not allowed on read-only vendor object (COAP_405)");
    return COAP_405_METHOD_NOT_ALLOWED;
}

static uint8_t prv_execute(lwm2m_context_t *contextP, uint16_t instanceId, uint16_t resourceId, uint8_t *buffer, int length, lwm2m_object_t *objectP) {
    ESP_LOGI(TAG, "[prv_execute] ENTER: instanceId=%d, resourceId=%d, bufferLen=%d", instanceId, resourceId, length);
    if (buffer && length > 0) {
        ESP_LOGI(TAG, "[prv_execute] Buffer content (first %d bytes)", length > 32 ? 32 : length);
    }
    // No executable resources for now
    ESP_LOGI(TAG, "[prv_execute] EXIT: No executable resources (COAP_404)");
    return COAP_404_NOT_FOUND;
}
static uint8_t prv_create(lwm2m_context_t *contextP, uint16_t instanceId, int numData, lwm2m_data_t *dataArray, lwm2m_object_t *objectP) {
    ESP_LOGI(TAG, "[prv_create] ENTER: instanceId=%d, numData=%d, current_count=%d/%d", 
             instanceId, numData, g_vendor_data_count, MAX_VENDOR_DATA);
    
    // Log all incoming data
    for (int i = 0; i < numData; i++) {
        ESP_LOGI(TAG, "[prv_create] Input[%d]: resourceId=%d, type=%d", i, dataArray[i].id, dataArray[i].type);
    }
    
    // Check if instance already exists
    for (int i = 0; i < g_vendor_data_count; i++) {
        if (g_vendor_data[i].instance_id == instanceId) {
            ESP_LOGW(TAG, "[prv_create] Instance %d already exists at index %d - returning COAP_400", instanceId, i);
            return COAP_400_BAD_REQUEST;
        }
    }
    
    if (g_vendor_data_count >= MAX_VENDOR_DATA) {
        ESP_LOGE(TAG, "[prv_create] EXIT: Max vendor data reached (COAP_500)");
        return COAP_500_INTERNAL_SERVER_ERROR;
    }
    
    // Initialize the new vendor data entry
    int data_index = g_vendor_data_count;
    g_vendor_data[data_index].instance_id = instanceId;
    g_vendor_data[data_index].vendor_id = 0;
    g_vendor_data[data_index].mac_oui = 0;
    
    // Parse the creation data
    for (int i = 0; i < numData; i++) {
        switch (dataArray[i].id) {
            case RID_VENDOR_ID: {
                int64_t value;
                if (lwm2m_data_decode_int(&dataArray[i], &value) == 1) {
                    g_vendor_data[data_index].vendor_id = value;
                    ESP_LOGI(TAG, "[prv_create] Set vendor_id[%d]: %lld", data_index, value);
                } else {
                    ESP_LOGE(TAG, "[prv_create] Failed to decode vendor_id (type=%d)", dataArray[i].type);
                    return COAP_400_BAD_REQUEST;
                }
                break;
            }
            case RID_MAC_OUI: {
                int64_t value;
                if (lwm2m_data_decode_int(&dataArray[i], &value) == 1) {
                    if (value < 0 || value > 16777215) {
                        ESP_LOGE(TAG, "[prv_create] MAC OUI out of range: %lld (must be 0-16777215)", value);
                        return COAP_400_BAD_REQUEST;
                    }
                    g_vendor_data[data_index].mac_oui = (uint32_t)value;
                    ESP_LOGI(TAG, "[prv_create] Set mac_oui[%d]: %u (0x%06X)", data_index, g_vendor_data[data_index].mac_oui, g_vendor_data[data_index].mac_oui);
                } else {
                    ESP_LOGE(TAG, "[prv_create] Failed to decode mac_oui (type=%d)", dataArray[i].type);
                    return COAP_400_BAD_REQUEST;
                }
                break;
            }
            default:
                ESP_LOGE(TAG, "[prv_create] Unknown resource ID: %d", dataArray[i].id);
                return COAP_404_NOT_FOUND;
        }
    }
    
    g_vendor_data_count++;
    ESP_LOGI(TAG, "[prv_create] EXIT: SUCCESS - Created instance %d (new count: %d, vendor_id=%lld, mac_oui=0x%06X)", 
             instanceId, g_vendor_data_count, 
             g_vendor_data[data_index].vendor_id, 
             g_vendor_data[data_index].mac_oui);
    return COAP_201_CREATED;
}

lwm2m_object_t *get_vendor_object(void) {
    ESP_LOGI(TAG, "[get_vendor_object] ENTER: Creating vendor object %d", VENDOR_OBJECT_ID);
    
    lwm2m_object_t *obj = (lwm2m_object_t *)malloc(sizeof(lwm2m_object_t));
    if (obj == NULL) {
        ESP_LOGE(TAG, "[get_vendor_object] EXIT: Failed to allocate memory");
        return NULL;
    }
    
    memset(obj, 0, sizeof(lwm2m_object_t));
    obj->objID = VENDOR_OBJECT_ID;
    obj->instanceList = NULL;
    obj->readFunc = prv_read;
    obj->writeFunc = prv_write;
    obj->executeFunc = prv_execute;
    obj->createFunc = prv_create;
    
    ESP_LOGI(TAG, "[get_vendor_object] EXIT: Vendor object created successfully (objID=%d, callbacks set)", VENDOR_OBJECT_ID);
    ESP_LOGI(TAG, "[get_vendor_object] Current vendor data count: %d/%d", g_vendor_data_count, MAX_VENDOR_DATA);
    
    return obj;
}
