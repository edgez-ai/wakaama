/*******************************************************************************
 *
 * Copyright (c) 2013, 2014, 2015 Intel Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v20.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    David Navarro, Intel Corporation - initial API and implementation
 *******************************************************************************/

#include <liblwm2m.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include <stdint.h>
#endif

#ifdef ESP_PLATFORM
#define LWM2M_LARGE_POOL_SLOTS 2
#define LWM2M_LARGE_POOL_SIZE  (32 * 1024)

static EXT_RAM_BSS_ATTR uint8_t s_lwm2m_large_pool[LWM2M_LARGE_POOL_SLOTS][LWM2M_LARGE_POOL_SIZE];
static uint8_t s_lwm2m_large_pool_used[LWM2M_LARGE_POOL_SLOTS] = {0};

static void *lwm2m_large_pool_alloc(size_t s)
{
    if (s == 0 || s > LWM2M_LARGE_POOL_SIZE) {
        return NULL;
    }

    for (size_t i = 0; i < LWM2M_LARGE_POOL_SLOTS; i++) {
        if (!s_lwm2m_large_pool_used[i]) {
            s_lwm2m_large_pool_used[i] = 1;
            return (void *)s_lwm2m_large_pool[i];
        }
    }

    return NULL;
}

static int lwm2m_large_pool_free(void *p)
{
    if (p == NULL) {
        return 0;
    }

    for (size_t i = 0; i < LWM2M_LARGE_POOL_SLOTS; i++) {
        if (p == (void *)s_lwm2m_large_pool[i]) {
            s_lwm2m_large_pool_used[i] = 0;
            return 1;
        }
    }

    return 0;
}
#endif


void * lwm2m_malloc(size_t s)
{
#ifdef ESP_PLATFORM
    if (s >= 4096) {
        void *pool_ptr = lwm2m_large_pool_alloc(s);
        if (pool_ptr != NULL) {
            return pool_ptr;
        }
    }

    if (s >= 4096) {
        void *psram_ptr = heap_caps_malloc(s, MALLOC_CAP_SPIRAM);
        if (psram_ptr != NULL) {
            return psram_ptr;
        }
    }

    void *heap8_ptr = heap_caps_malloc(s, MALLOC_CAP_8BIT);
    if (heap8_ptr != NULL) {
        return heap8_ptr;
    }
#endif
    void *fallback = malloc(s);
    return fallback;
}

void lwm2m_free(void * p)
{
#ifdef ESP_PLATFORM
    if (lwm2m_large_pool_free(p)) {
        return;
    }
#endif
    free(p);
}

char * lwm2m_strdup(const char * str)
{
    if (!str) {
      return NULL;
    }

    const int len = strlen(str) + 1;
    char * const buf = lwm2m_malloc(len);

    if (buf) {
      memset(buf, 0, len);
      memcpy(buf, str, len - 1);
    }

    return buf;
}


int lwm2m_strncmp(const char * s1,
                     const char * s2,
                     size_t n)
{
    return strncmp(s1, s2, n);
}

int lwm2m_strcasecmp(const char * str1, const char * str2) {
    return strcasecmp(str1, str2);
}

time_t lwm2m_gettime(void)
{
    return time(NULL);
}

int lwm2m_seed(void) {
    /*
     * Return a seed for random number generation, the seed must be a
     * different number at every boot and unpredictable, time(NULL) may not be
     * a reliable source as a seed.
     * See: https://github.com/eclipse/wakaama/pull/711
     */
    return time(NULL);
}

void lwm2m_printf(const char * format, ...)
{
    va_list ap;

    va_start(ap, format);

    vfprintf(stderr, format, ap);

    va_end(ap);
}
