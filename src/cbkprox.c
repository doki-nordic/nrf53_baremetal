
#include <stddef.h>
#include <stdint.h>

#include "cbkprox.h"

#if CONFIG_CBKPROX_OUT_SLOTS > 0

#if CONFIG_CBKPROX_OUT_SLOTS > 16383
#error "Too many callback proxy output slots"
#endif

#if __ARM_ARCH != 8 || __ARM_ARCH_ISA_THUMB != 2 || !defined(__GNUC__)
#error "Callback proxy output implemented only for Cortex-M33 and GCC. Set CONFIG_CBKPROX_OUT_SLOTS to 0 to disable them."
#endif

#define TABLE_ENTRY1 \
	"push {r0, r1, r2, r3}\n" \
	"mov r1, lr\n" \
	"bl .L%=callback_jump_table_end\n"

#define TABLE_ENTRY2 TABLE_ENTRY1 TABLE_ENTRY1
#define TABLE_ENTRY4 TABLE_ENTRY2 TABLE_ENTRY2
#define TABLE_ENTRY8 TABLE_ENTRY4 TABLE_ENTRY4
#define TABLE_ENTRY16 TABLE_ENTRY8 TABLE_ENTRY8
#define TABLE_ENTRY32 TABLE_ENTRY16 TABLE_ENTRY16
#define TABLE_ENTRY64 TABLE_ENTRY32 TABLE_ENTRY32
#define TABLE_ENTRY128 TABLE_ENTRY64 TABLE_ENTRY64
#define TABLE_ENTRY256 TABLE_ENTRY128 TABLE_ENTRY128
#define TABLE_ENTRY512 TABLE_ENTRY256 TABLE_ENTRY256
#define TABLE_ENTRY1024 TABLE_ENTRY512 TABLE_ENTRY512
#define TABLE_ENTRY2048 TABLE_ENTRY1024 TABLE_ENTRY1024
#define TABLE_ENTRY4096 TABLE_ENTRY2048 TABLE_ENTRY2048
#define TABLE_ENTRY8192 TABLE_ENTRY4096 TABLE_ENTRY4096


void* out_callbacks[CONFIG_CBKPROX_OUT_SLOTS];


__attribute__((naked))
static void callback_jump_table_start(void)
{
	__asm volatile (
#if CONFIG_CBKPROX_OUT_SLOTS & 1
		TABLE_ENTRY1
#endif
#if CONFIG_CBKPROX_OUT_SLOTS & 2
		TABLE_ENTRY2
#endif
#if CONFIG_CBKPROX_OUT_SLOTS & 4
		TABLE_ENTRY4
#endif
#if CONFIG_CBKPROX_OUT_SLOTS & 8
		TABLE_ENTRY8
#endif
#if CONFIG_CBKPROX_OUT_SLOTS & 16
		TABLE_ENTRY16
#endif
#if CONFIG_CBKPROX_OUT_SLOTS & 32
		TABLE_ENTRY32
#endif
#if CONFIG_CBKPROX_OUT_SLOTS & 64
		TABLE_ENTRY64
#endif
#if CONFIG_CBKPROX_OUT_SLOTS & 128
		TABLE_ENTRY128
#endif
#if CONFIG_CBKPROX_OUT_SLOTS & 256
		TABLE_ENTRY256
#endif
#if CONFIG_CBKPROX_OUT_SLOTS & 512
		TABLE_ENTRY512
#endif
#if CONFIG_CBKPROX_OUT_SLOTS & 1024
		TABLE_ENTRY1024
#endif
#if CONFIG_CBKPROX_OUT_SLOTS & 2048
		TABLE_ENTRY2048
#endif
#if CONFIG_CBKPROX_OUT_SLOTS & 4096
		TABLE_ENTRY4096
#endif
#if CONFIG_CBKPROX_OUT_SLOTS & 8192
		TABLE_ENTRY8192
#endif
		".L%=callback_jump_table_end:\n"
		"mov   r0, lr\n"
		"mov   lr, r1\n"
		"ldr   r1, .L%=callback_jump_table_start_addr\n"
		"sub   r0, r1\n"
		"asrs  r0, r0, #1\n"
		"ldr   r1, .L%=out_callbacks_addr\n"
		"ldr   r1, [r1, r0]\n"
		"asrs  r0, r0, #2\n"
		"bx    r1\n"
		".align 2\n"
		".L%=callback_jump_table_start_addr:\n"
		".word callback_jump_table_start + 8\n"
		".L%=out_callbacks_addr:\n"
		".word out_callbacks\n":::
		);
}

void* cbkprox_out_get(int index, void *handler)
{
	uint32_t addr;

	if (index >= CONFIG_CBKPROX_OUT_SLOTS || index < 0) {
		return NULL;
	}
	else if (out_callbacks[index] == NULL)
	{
		out_callbacks[index] = handler;
	}
	else if (out_callbacks[index] != handler)
	{
		return NULL;
	}

	addr = (uint32_t)(void*)&callback_jump_table_start;
	addr += 8 * index;

	return (void*)addr;
}


#else


uint64_t cbkprox_out_start_handler()
{
	return 0;
}

void cbkprox_out_end_handler(uint64_t state)
{
}

void* cbkprox_out_get(int index, void *handler)
{
	return NULL;
}


#endif /* CONFIG_CBKPROX_OUT_SLOTS > 0 */


#if CONFIG_CBKPROX_IN_SLOTS > 0


static struct
{
	intptr_t callback;
	void *handler;
	uint16_t gt;
	uint16_t lt;
} in_slots[CONFIG_CBKPROX_IN_SLOTS];

static uint32_t next_free_in_slot = 0;


int cbkprox_in_set(void *handler, void *callback)
{
	int index = 0;
	uint16_t *attach_to = NULL;
	uintptr_t callback_int = (uintptr_t)callback;

#ifdef CONFIG_CBKPROX_IN_LOCK
	CONFIG_CBKPROX_IN_LOCK;
#endif

	if (next_free_in_slot == 0) {
		attach_to = &in_slots[0].lt;
	} else {
		do {
			if (callback_int == in_slots[index].callback) {
				attach_to = NULL;
				break;
			} else if (callback_int < in_slots[index].callback) {
				attach_to = &in_slots[index].lt;
			} else {
				attach_to = &in_slots[index].gt;
			}
			index = *attach_to;
		} while (index > 0);
	}

	if (attach_to != NULL) {
		if (next_free_in_slot >= CONFIG_CBKPROX_IN_SLOTS) {
			index = -1;
		} else {
			index = next_free_in_slot;
			next_free_in_slot++;
			*attach_to = index;
			in_slots[index].callback = callback_int;
			in_slots[index].handler = handler;
			in_slots[index].gt = 0;
			in_slots[index].lt = 0;
		}
	} else if (in_slots[index].handler != handler) {
		index = -1;
	}

#ifdef CONFIG_CBKPROX_IN_UNLOCK
	CONFIG_CBKPROX_IN_UNLOCK;
#endif

	return index;
}

void *cbkprox_in_get(int index, void **callback)
{
	if (index >= next_free_in_slot || index < 0) {
		return NULL;
	}
	*callback = (void*)in_slots[index].callback;
	return in_slots[index].handler;
}

#else

int cbkprox_in_set(void *handler, void *callback)
{
	return -1;
}

void *cbkprox_in_get(int index, void **callback)
{
	return NULL;
}

#endif /* CONFIG_CBKPROX_IN_SLOTS > 0 */


void test5i(int a, int b, int c, int d, uint64_t x) {
	__asm__ volatile (
		"nop\nnop\nnop\nnop\nnop\n"::"r"(a), "r"(b), "r"(c), "r"(d), "r"((uint32_t)x)
	);
}
