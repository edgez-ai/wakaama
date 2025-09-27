
#ifndef OBJECT_VENDOR_H
#define OBJECT_VENDOR_H

#include "liblwm2m.h"

#define VENDOR_OBJECT_ID 9999

typedef struct {
	uint16_t instance_id;
	char public_key[128]; // Adjust size as needed
} vendor_data_t;

lwm2m_object_t *get_vendor_object(void);

#endif // OBJECT_VENDOR_H
