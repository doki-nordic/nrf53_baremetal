#ifndef CBKPROX_H
#define CBKPROX_H

#include <stdint.h>

#ifndef CONFIG_CBKPROX_SLOTS
#define CONFIG_CBKPROX_SLOTS 16
#endif

uint64_t cbkprox_start_handler();
void cbkprox_end_handler(uint64_t state);
void* cbkprox_alloc(uint32_t index, void *handler);

#endif
