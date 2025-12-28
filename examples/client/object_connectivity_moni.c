/*** Connectivity Monitoring Object (multi-instance) ported from esp32-lwm2m-gateway ***/

#include "liblwm2m.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef ESP_PLATFORM
#include "esp_log.h"
#endif

// Resource Id's:
#define RES_M_NETWORK_BEARER            0
#define RES_M_AVL_NETWORK_BEARER        1
#define RES_M_RADIO_SIGNAL_STRENGTH     2
#define RES_O_LINK_QUALITY              3
#define RES_M_IP_ADDRESSES              4
#define RES_O_ROUTER_IP_ADDRESS         5
#define RES_O_LINK_UTILIZATION          6
#define RES_O_APN                       7
#define RES_O_CELL_ID                   8
#define RES_O_SMNC                      9
#define RES_O_SMCC                      10

#define VALUE_NETWORK_BEARER_GSM    0
#define VALUE_AVL_NETWORK_BEARER_1  0
#define VALUE_AVL_NETWORK_BEARER_2  21
#define VALUE_AVL_NETWORK_BEARER_3  41
#define VALUE_AVL_NETWORK_BEARER_4  42
#define VALUE_AVL_NETWORK_BEARER_5  43
#define VALUE_IP_ADDRESS_1              "192.168.178.101"
#define VALUE_IP_ADDRESS_2              "192.168.178.102"
#define VALUE_ROUTER_IP_ADDRESS_1       "192.168.178.001"
#define VALUE_ROUTER_IP_ADDRESS_2       "192.168.178.002"
#define VALUE_APN_1                     "web.vodafone.de"
#define VALUE_APN_2                     "cda.vodafone.de"
#define VALUE_CELL_ID                   69696969
#define VALUE_RADIO_SIGNAL_STRENGTH     80
#define VALUE_LINK_QUALITY              98
#define VALUE_LINK_UTILIZATION          666
#define VALUE_SMNC                      33
#define VALUE_SMCC                      44

typedef struct _conn_m_instance_t
{
	struct _conn_m_instance_t * next;
	uint16_t shortID;
	char ipAddresses[2][16];
	char routerIpAddresses[2][16];
	long cellId;
	int signalStrength;
	int linkQuality;
	int linkUtilization;
	uint32_t deviceId;
} conn_m_instance_t;

static uint8_t prv_set_value_extended(lwm2m_data_t * dataP, conn_m_instance_t * connDataP)
{
	lwm2m_data_t * subTlvP;
	size_t count;
	size_t i;

	switch (dataP->id)
	{
	case RES_M_NETWORK_BEARER:
		if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
		lwm2m_data_encode_int(VALUE_NETWORK_BEARER_GSM, dataP);
		return COAP_205_CONTENT;

	case RES_M_AVL_NETWORK_BEARER:
	{
		if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE)
		{
			count = dataP->value.asChildren.count;
			subTlvP = dataP->value.asChildren.array;
		}
		else
		{
			count = 1;
			subTlvP = lwm2m_data_new(count);
			for (i = 0; i < count; i++) subTlvP[i].id = i;
			lwm2m_data_encode_instances(subTlvP, count, dataP);
		}

		for (i = 0; i < count; i++)
		{
			switch (subTlvP[i].id)
			{
			case 0:
				lwm2m_data_encode_int(VALUE_AVL_NETWORK_BEARER_1, subTlvP + i);
				break;
			default:
				return COAP_404_NOT_FOUND;
			}
		}
		return COAP_205_CONTENT ;
	}

	case RES_M_RADIO_SIGNAL_STRENGTH:
		if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
		lwm2m_data_encode_int(connDataP->signalStrength, dataP);
		return COAP_205_CONTENT;

	case RES_O_LINK_QUALITY:
		if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
		lwm2m_data_encode_int(connDataP->linkQuality, dataP);
		return COAP_205_CONTENT ;

	case RES_M_IP_ADDRESSES:
	{
		if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE)
		{
			count = dataP->value.asChildren.count;
			subTlvP = dataP->value.asChildren.array;
		}
		else
		{
			count = 1;
			subTlvP = lwm2m_data_new(count);
			for (i = 0; i < count; i++) subTlvP[i].id = i;
			lwm2m_data_encode_instances(subTlvP, count, dataP);
		}

		for (i = 0; i < count; i++)
		{
			switch (subTlvP[i].id)
			{
			case 0:
				lwm2m_data_encode_string(connDataP->ipAddresses[i], subTlvP + i);
				break;
			default:
				return COAP_404_NOT_FOUND;
			}
		}
		return COAP_205_CONTENT ;
	}

	case RES_O_ROUTER_IP_ADDRESS:
	{
		if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE)
		{
			count = dataP->value.asChildren.count;
			subTlvP = dataP->value.asChildren.array;
		}
		else
		{
			count = 1;
			subTlvP = lwm2m_data_new(count);
			for (i = 0; i < count; i++) subTlvP[i].id = i;
			lwm2m_data_encode_instances(subTlvP, count, dataP);
		}

		for (i = 0; i < count; i++)
		{
			switch (subTlvP[i].id)
			{
			case 0:
				lwm2m_data_encode_string(connDataP->routerIpAddresses[i], subTlvP + i);
				break;
			default:
				return COAP_404_NOT_FOUND;
			}
		}
		return COAP_205_CONTENT ;
	}

	case RES_O_LINK_UTILIZATION:
		if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
		lwm2m_data_encode_int(connDataP->linkUtilization, dataP);
		return COAP_205_CONTENT;

	case RES_O_APN:
	{
		if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE)
		{
			count = dataP->value.asChildren.count;
			subTlvP = dataP->value.asChildren.array;
		}
		else
		{
			count = 1;
			subTlvP = lwm2m_data_new(count);
			for (i = 0; i < count; i++) subTlvP[i].id = i;
			lwm2m_data_encode_instances(subTlvP, count, dataP);
		}

		for (i = 0; i < count; i++)
		{
			switch (subTlvP[i].id)
			{
			case 0:
				lwm2m_data_encode_string(VALUE_APN_1, subTlvP + i);
				break;
			default:
				return COAP_404_NOT_FOUND;
			}
		}
		return COAP_205_CONTENT;
	}

	case RES_O_CELL_ID:
		if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
		lwm2m_data_encode_int(connDataP->cellId, dataP);
		return COAP_205_CONTENT ;

	case RES_O_SMNC:
		if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
		lwm2m_data_encode_int(VALUE_SMNC, dataP);
		return COAP_205_CONTENT ;

	case RES_O_SMCC:
		if (dataP->type == LWM2M_TYPE_MULTIPLE_RESOURCE) return COAP_404_NOT_FOUND;
		lwm2m_data_encode_int(VALUE_SMCC, dataP);
		return COAP_205_CONTENT ;

	default:
		return COAP_404_NOT_FOUND ;
	}
}

static uint8_t prv_read(lwm2m_context_t *contextP,
						uint16_t instanceId,
						int * numDataP,
						lwm2m_data_t ** dataArrayP,
						lwm2m_object_t * objectP)
{
	uint8_t result;
	int i;
	conn_m_instance_t * targetP;

	(void)contextP;

	targetP = (conn_m_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
	if (NULL == targetP)
	{
#ifdef ESP_PLATFORM
		ESP_LOGW("CONN_MONI", "Instance %u not found", instanceId);
#else
		printf("CONN_MONI: Instance %u not found\n", instanceId);
#endif
		return COAP_404_NOT_FOUND;
	}

	if (*numDataP == 0)
	{
		uint16_t resList[] = {
				RES_M_NETWORK_BEARER,
				RES_M_AVL_NETWORK_BEARER,
				RES_M_RADIO_SIGNAL_STRENGTH,
				RES_O_LINK_QUALITY,
				RES_M_IP_ADDRESSES,
				RES_O_ROUTER_IP_ADDRESS,
				RES_O_LINK_UTILIZATION,
				RES_O_APN,
				RES_O_CELL_ID,
				RES_O_SMNC,
				RES_O_SMCC
		};
		int nbRes = sizeof(resList) / sizeof(uint16_t);

		*dataArrayP = lwm2m_data_new(nbRes);
		if (*dataArrayP == NULL)
			return COAP_500_INTERNAL_SERVER_ERROR ;
		*numDataP = nbRes;
		for (i = 0; i < nbRes; i++)
		{
			(*dataArrayP)[i].id = resList[i];
		}
	}

	i = 0;
	do
	{
		result = prv_set_value_extended((*dataArrayP) + i, targetP);
		i++;
	} while (i < *numDataP && result == COAP_205_CONTENT );

	return result;
}

lwm2m_object_t * get_object_conn_m(void)
{
	lwm2m_object_t * connObj;
	conn_m_instance_t * instanceP;

	connObj = (lwm2m_object_t *) lwm2m_malloc(sizeof(lwm2m_object_t));

	if (NULL != connObj)
	{
		memset(connObj, 0, sizeof(lwm2m_object_t));
		connObj->objID = LWM2M_CONN_MONITOR_OBJECT_ID;
		connObj->instanceList = NULL;
		connObj->readFunc = prv_read;
		connObj->executeFunc = NULL;
		connObj->userData = NULL;

		instanceP = (conn_m_instance_t *)lwm2m_malloc(sizeof(conn_m_instance_t));
		if (NULL != instanceP)
		{
			memset(instanceP, 0, sizeof(conn_m_instance_t));
			instanceP->shortID = 0;
			instanceP->deviceId = 0;
			instanceP->cellId = VALUE_CELL_ID;
			instanceP->signalStrength = VALUE_RADIO_SIGNAL_STRENGTH;
			instanceP->linkQuality = VALUE_LINK_QUALITY;
			instanceP->linkUtilization = VALUE_LINK_UTILIZATION;
			strcpy(instanceP->ipAddresses[0], VALUE_IP_ADDRESS_1);
			strcpy(instanceP->ipAddresses[1], VALUE_IP_ADDRESS_2);
			strcpy(instanceP->routerIpAddresses[0], VALUE_ROUTER_IP_ADDRESS_1);
			strcpy(instanceP->routerIpAddresses[1], VALUE_ROUTER_IP_ADDRESS_2);
			connObj->instanceList = LWM2M_LIST_ADD(connObj->instanceList, instanceP);
		}
		else
		{
			lwm2m_free(connObj);
			return NULL;
		}
	}
	return connObj;
}

void free_object_conn_m(lwm2m_object_t * objectP)
{
	conn_m_instance_t * instanceP = (conn_m_instance_t *)objectP->instanceList;
	while (instanceP != NULL)
	{
		conn_m_instance_t * nextP = instanceP->next;
		lwm2m_free(instanceP);
		instanceP = nextP;
	}
	lwm2m_free(objectP->userData);
	lwm2m_free(objectP);
}

uint8_t connectivity_moni_change(lwm2m_data_t * dataArray,
								 lwm2m_object_t * objectP)
{
	return COAP_405_METHOD_NOT_ALLOWED;
}

uint8_t connectivity_moni_add_instance(lwm2m_object_t * objectP, uint16_t instanceId, uint32_t deviceId)
{
	conn_m_instance_t * instanceP;

	if (NULL == objectP)
	{
		return COAP_500_INTERNAL_SERVER_ERROR;
	}

	instanceP = (conn_m_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
	if (NULL != instanceP)
	{
		if (instanceP->deviceId == deviceId) {
			return COAP_204_CHANGED;
		}
		return COAP_406_NOT_ACCEPTABLE;
	}

	instanceP = (conn_m_instance_t *)lwm2m_malloc(sizeof(conn_m_instance_t));
	if (NULL == instanceP)
	{
		return COAP_500_INTERNAL_SERVER_ERROR;
	}

	memset(instanceP, 0, sizeof(conn_m_instance_t));
	instanceP->shortID = instanceId;
	instanceP->deviceId = deviceId;
	instanceP->cellId = VALUE_CELL_ID;
	instanceP->signalStrength = VALUE_RADIO_SIGNAL_STRENGTH;
	instanceP->linkQuality = VALUE_LINK_QUALITY;
	instanceP->linkUtilization = VALUE_LINK_UTILIZATION;
	strcpy(instanceP->ipAddresses[0], VALUE_IP_ADDRESS_1);
	strcpy(instanceP->ipAddresses[1], VALUE_IP_ADDRESS_2);
	strcpy(instanceP->routerIpAddresses[0], VALUE_ROUTER_IP_ADDRESS_1);
	strcpy(instanceP->routerIpAddresses[1], VALUE_ROUTER_IP_ADDRESS_2);

	objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, instanceP);
	return COAP_201_CREATED;
}

uint8_t connectivity_moni_remove_instance(lwm2m_object_t * objectP, uint16_t instanceId)
{
	conn_m_instance_t * instanceP;

	if (NULL == objectP)
	{
		return COAP_500_INTERNAL_SERVER_ERROR;
	}

	instanceP = (conn_m_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
	if (NULL == instanceP)
	{
		return COAP_404_NOT_FOUND;
	}

	objectP->instanceList = LWM2M_LIST_RM(objectP->instanceList, instanceId, NULL);
	lwm2m_free(instanceP);

	return COAP_202_DELETED;
}

uint8_t connectivity_moni_update_rssi(lwm2m_object_t * objectP, uint16_t instanceId, int rssi)
{
	conn_m_instance_t * instanceP;

	if (NULL == objectP)
	{
		return COAP_500_INTERNAL_SERVER_ERROR;
	}

	instanceP = (conn_m_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
	if (NULL == instanceP)
	{
		return COAP_404_NOT_FOUND;
	}

	instanceP->signalStrength = rssi;
	return COAP_204_CHANGED;
}

uint8_t connectivity_moni_update_link_quality(lwm2m_object_t * objectP, uint16_t instanceId, int linkQuality)
{
	conn_m_instance_t * instanceP;

	if (NULL == objectP)
	{
		return COAP_500_INTERNAL_SERVER_ERROR;
	}

	instanceP = (conn_m_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
	if (NULL == instanceP)
	{
		return COAP_404_NOT_FOUND;
	}

	instanceP->linkQuality = linkQuality;
	return COAP_204_CHANGED;
}

void connectivity_moni_debug_instances(lwm2m_object_t * objectP)
{
	if (NULL == objectP) {
		return;
	}
	conn_m_instance_t * instanceP = (conn_m_instance_t *)objectP->instanceList;
	while (instanceP != NULL) {
		instanceP = instanceP->next;
	}
}