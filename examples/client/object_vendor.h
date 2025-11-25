
#ifndef OBJECT_VENDOR_H
#define OBJECT_VENDOR_H

#include "liblwm2m.h"

#define VENDOR_OBJECT_ID 10299

typedef struct _vendor_instance_
{
	struct _vendor_instance_ * next;  // matches lwm2m_list_t
	uint16_t instanceId;              // matches lwm2m_list_t
	int64_t vendor_id;                // Resource 0: Vendor ID
	uint32_t mac_oui;                 // Resource 1: MAC OUI (24-bit, 0-16777215)
} vendor_instance_t;

lwm2m_object_t *get_vendor_object(void);

#endif // OBJECT_VENDOR_H
