/*******************************************************************************
 *
 * W3C WoT LwM2M Bootstrap Example
 * 
 * This example demonstrates how to use the W3C WoT LwM2M objects for:
 * 1. Bootstrap configuration via LwM2M server
 * 2. Dynamic Thing Description updates
 * 3. Action execution and Event generation
 * 4. Property monitoring and control
 *
 *******************************************************************************/

#include "../../../../main/lwm2m_client.h"
#include "object_wot_thing.h"
#include "object_wot_data_feature.h"
#include "object_wot_action.h"
#include "object_wot_event.h"
#include "wot_bootstrap_config.h"
#include "esp_log.h"

static const char *TAG = "wot_example";

// Example: Creating a temperature sensor Thing via bootstrap
void example_create_temperature_sensor_thing(lwm2m_object_t *thing_obj, 
                                            lwm2m_object_t *data_feature_obj,
                                            lwm2m_object_t *action_obj,
                                            lwm2m_object_t *event_obj)
{
    ESP_LOGI(TAG, "Creating temperature sensor Thing via bootstrap");
    
    // 1. Create Thing Definition
    uint8_t result = wot_thing_add_instance(thing_obj, 0, 
                                           "urn:edgez:esp32:temp-sensor:001",
                                           "ESP32 Temperature Sensor");
    if (result == COAP_201_CREATED) {
        wot_thing_update_description(thing_obj, 0, 
            "ESP32-based temperature sensor with monitoring capabilities");
        wot_thing_update_version(thing_obj, 0, "1.0.0");
        ESP_LOGI(TAG, "Thing Definition created successfully");
    }
    
    // 2. Create Data Feature for temperature reading
    result = wot_data_feature_add_instance(data_feature_obj, 0, "temperature");
    if (result == COAP_201_CREATED) {
        // Link to LwM2M temperature sensor resource (IPSO Temperature Sensor Object)
        wot_data_feature_add_linked_resource(data_feature_obj, 0, "/3303/0/5700");
        wot_data_feature_set_owning_thing(data_feature_obj, 0, WOT_THING_OBJECT_ID, 0);
        ESP_LOGI(TAG, "Temperature data feature created");
    }
    
    // 3. Create Action for sensor calibration
    result = wot_action_add_instance(action_obj, 0, "calibrate_sensor");
    if (result == COAP_201_CREATED) {
        // Simple calibration script
        const char* script = "calibration_offset = input.offset; save_calibration();";
        wot_action_update_script(action_obj, 0, (uint8_t*)script, strlen(script), "text/javascript");
        wot_action_set_owning_thing(action_obj, 0, WOT_THING_OBJECT_ID, 0);
        ESP_LOGI(TAG, "Calibration action created");
    }
    
    // 4. Create Event for temperature alerts
    result = wot_event_add_instance(event_obj, 0, "temperature_alert");
    if (result == COAP_201_CREATED) {
        // Alert generation script
        const char* script = "if (temperature > threshold) { emit('alert', {temp: temperature, time: now()}); }";
        wot_event_update_script(event_obj, 0, (uint8_t*)script, strlen(script), "text/javascript");
        wot_event_set_owning_thing(event_obj, 0, WOT_THING_OBJECT_ID, 0);
        ESP_LOGI(TAG, "Temperature alert event created");
    }
}

// Example: Bootstrap configuration sent from LwM2M server
void example_bootstrap_from_server(lwm2m_context_t *context)
{
    ESP_LOGI(TAG, "Simulating bootstrap configuration from LwM2M server");
    
    /*
     * This demonstrates what a bootstrap server would send to configure
     * the W3C WoT objects. In practice, this would come via LwM2M Write
     * operations during the bootstrap process.
     */
    
    // Bootstrap server would send:
    // 1. Create /26250/0 (Thing Definition)
    // 2. Write /26250/0/0 = "urn:edgez:gateway:device-001"  (Thing Identifier)
    // 3. Write /26250/0/1 = "IoT Gateway Device"           (Title)
    // 4. Write /26250/0/2 = "Multi-protocol IoT gateway"   (Description)
    
    // 5. Create /26251/0 (Data Feature for temperature)
    // 6. Write /26251/0/0 = "temperature"                  (Feature Identifier)
    // 7. Write /26251/0/1/0 = "/3303/0/5700"              (Linked Resource)
    // 8. Write /26251/0/2 = "26250:0"                     (Owning Thing)
    
    // 9. Create /26252/0 (Action for LED control)
    // 10. Write /26252/0/0 = "led_control"                (Action Identifier)
    // 11. Write /26252/0/1 = <script_bytes>               (Script)
    // 12. Write /26252/0/2 = "text/javascript"            (Script Format)
    
    // 13. Create /26253/0 (Event for alarms)
    // 14. Write /26253/0/0 = "alarm"                      (Event Identifier)
    // 15. Write /26253/0/1 = <script_bytes>               (Script)
    
    ESP_LOGI(TAG, "Bootstrap configuration would be applied via LwM2M Write operations");
}

// Example: Triggering an action execution
void example_trigger_action(lwm2m_object_t *action_obj, uint16_t instance_id)
{
    ESP_LOGI(TAG, "Triggering action execution for instance %u", instance_id);
    
    // Server executes action by sending POST to /26252/{instance_id}/1
    // This would trigger the prv_execute callback in the action object
    
    // For demonstration, we can simulate this by calling the internal functions
    const char* action_id = wot_action_get_identifier(action_obj, instance_id);
    if (action_id != NULL) {
        ESP_LOGI(TAG, "Would execute action: %s", action_id);
        
        // In practice, this execution happens automatically when server sends
        // POST /26252/{instance_id}/1
    }
}

// Example: Generating an event
void example_generate_event(lwm2m_object_t *event_obj, uint16_t instance_id)
{
    ESP_LOGI(TAG, "Generating event for instance %u", instance_id);
    
    // Events can be triggered in multiple ways:
    // 1. Server executes event script via POST /26253/{instance_id}/1
    // 2. Device triggers event based on conditions
    // 3. Timer-based event generation
    
    uint8_t result = wot_event_trigger(event_obj, instance_id);
    if (result == COAP_204_CHANGED) {
        ESP_LOGI(TAG, "Event triggered successfully");
    } else {
        ESP_LOGW(TAG, "Failed to trigger event: %d", result);
    }
}

// Example: Monitoring data features
void example_monitor_data_features(lwm2m_object_t *data_feature_obj)
{
    ESP_LOGI(TAG, "Monitoring data features");
    
    // Data features link WoT properties to LwM2M resources
    // When LwM2M resources change, corresponding WoT properties should be updated
    
    // For instance, if /3303/0/5700 (temperature) changes:
    // 1. The change is detected by LwM2M resource monitoring
    // 2. WoT Data Feature /26251/0 represents this property
    // 3. Any subscribed WoT clients would be notified
    
    int resource_count = wot_data_feature_get_linked_resource_count(data_feature_obj, 0);
    ESP_LOGI(TAG, "Data feature 0 has %d linked resources", resource_count);
    
    for (int i = 0; i < resource_count; i++) {
        const char* resource_path = wot_data_feature_get_linked_resource(data_feature_obj, 0, i);
        if (resource_path != NULL) {
            ESP_LOGI(TAG, "  Linked resource %d: %s", i, resource_path);
        }
    }
}

// Example: Complete bootstrap workflow
void example_complete_bootstrap_workflow(void)
{
    ESP_LOGI(TAG, "=== W3C WoT Bootstrap Workflow Example ===");
    
    // 1. Initialize WoT objects
    wot_objects_t* objects = wot_bootstrap_init_objects();
    if (objects == NULL) {
        ESP_LOGE(TAG, "Failed to initialize WoT objects");
        return;
    }
    
    // 2. Create default configuration
    wot_bootstrap_config_t* config = wot_bootstrap_create_default_config();
    if (config == NULL) {
        ESP_LOGE(TAG, "Failed to create bootstrap configuration");
        wot_bootstrap_free_objects(objects);
        return;
    }
    
    // 3. Apply bootstrap configuration
    uint8_t result = wot_bootstrap_apply_config(objects, config);
    if (result == COAP_204_CHANGED) {
        ESP_LOGI(TAG, "Bootstrap configuration applied successfully");
        
        // 4. Display configured objects
        display_wot_thing_object(objects->wot_thing_obj);
        display_wot_data_feature_object(objects->wot_data_feature_obj);
        display_wot_action_object(objects->wot_action_obj);
        display_wot_event_object(objects->wot_event_obj);
        
        // 5. Demonstrate functionality
        example_monitor_data_features(objects->wot_data_feature_obj);
        example_trigger_action(objects->wot_action_obj, 0);
        example_generate_event(objects->wot_event_obj, 0);
        
    } else {
        ESP_LOGE(TAG, "Failed to apply bootstrap configuration: %d", result);
    }
    
    // 6. Cleanup
    wot_bootstrap_free_config(config);
    wot_bootstrap_free_objects(objects);
    
    ESP_LOGI(TAG, "=== Bootstrap Workflow Complete ===");
}

// Bootstrap message format examples (for reference)
void example_bootstrap_message_formats(void)
{
    ESP_LOGI(TAG, "=== Bootstrap Message Format Examples ===");
    
    // Example bootstrap messages that would be sent by LwM2M server:
    
    ESP_LOGI(TAG, "1. Create Thing Definition:");
    ESP_LOGI(TAG, "   POST /26250/0");
    ESP_LOGI(TAG, "   Body: {0:'urn:device:001', 1:'My Device', 2:'IoT Device'}");
    
    ESP_LOGI(TAG, "2. Create Data Feature:");
    ESP_LOGI(TAG, "   POST /26251/0");
    ESP_LOGI(TAG, "   Body: {0:'temperature', 1:['/3303/0/5700'], 2:'26250:0'}");
    
    ESP_LOGI(TAG, "3. Create Action:");
    ESP_LOGI(TAG, "   POST /26252/0");
    ESP_LOGI(TAG, "   Body: {0:'led_control', 1:<script>, 2:'text/javascript', 3:'26250:0'}");
    
    ESP_LOGI(TAG, "4. Create Event:");
    ESP_LOGI(TAG, "   POST /26253/0");
    ESP_LOGI(TAG, "   Body: {0:'alarm', 1:<script>, 2:'text/javascript', 3:'26250:0'}");
    
    ESP_LOGI(TAG, "5. Execute Action:");
    ESP_LOGI(TAG, "   POST /26252/0/1");
    ESP_LOGI(TAG, "   Body: <optional parameters>");
    
    ESP_LOGI(TAG, "6. Trigger Event:");
    ESP_LOGI(TAG, "   POST /26253/0/1");
    ESP_LOGI(TAG, "   Body: <optional parameters>");
}

// Function to call from main application
void wot_bootstrap_example_run(void)
{
    example_bootstrap_message_formats();
    example_complete_bootstrap_workflow();
}