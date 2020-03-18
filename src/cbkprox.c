
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
	"push {r4, r5, lr}\n" \
	"nop\n" \
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


struct
{
	uint32_t state[3];
	void* callbacks[CONFIG_CBKPROX_OUT_SLOTS];
} out_context;


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
		"ldr   r5, .L%=callback_jump_table_start_addr\n"
		"sub   lr, r5\n"
		"asrs  lr, lr, #1\n"
		"ldr   r5, .L%=out_context_addr\n"
		"mrs   r4, PRIMASK\n"
		"cpsid i\n"
		"str   r4, [r5]\n"
		"str   lr, [r5, #8]\n"
		"ldr   r4, [sp, #8]\n"
		"str   r4, [r5, #4]\n"
		"ldr   lr, [r5, lr]\n"
		"pop   {r4, r5}\n"
		"add   sp, #4\n"
		"push  {lr}\n"
		"ldr   lr, .L%=handler_return_addr\n"
		"pop   {pc}\n"
		".L%=handler_return:"
		"sub   sp, #4\n"
		"ldr   r2, .L%=out_context_addr\n"
		"ldr   r3, [r2, #4]\n"
		"str   r3, [sp]\n"
		"ldr   r3, [r2]\n"
 		"cbnz  r3, .L%=next_instr\n"
 		"cpsie i\n"
		".L%=next_instr:\n"
		"pop   {pc}\n"
		".align 2\n"
		".L%=handler_return_addr:\n"
		".word .L%=handler_return + 1\n"
		".L%=callback_jump_table_start_addr:\n"
		".word callback_jump_table_start - 16\n"
		".L%=out_context_addr:\n"
		".word out_context\n":::
		);
}


uint64_t cbkprox_out_start_handler()
{
	uint64_t result;
	result = (uint64_t)((out_context.state[2] - 12) / 4) | ((uint64_t)out_context.state[1] << 32);
	if (!out_context.state[0]) {
		__asm volatile("cpsie i\n":::"memory");
	} else {
		result |= 0x80000000LL;
	}
	return result;
}

void cbkprox_out_end_handler(uint64_t state)
{
	uint64_t key = state & 0x80000000LL;
	__asm volatile("cpsid i\n"::"r"(key):"memory");
	out_context.state[0] = key;
	out_context.state[1] = (uint32_t)(state >> 32);
}

void* cbkprox_out_get(int index, void *handler)
{
	uint32_t addr;

	if (index >= CONFIG_CBKPROX_OUT_SLOTS || index < 0) {
		return NULL;
	}
	else if (out_context.callbacks[index] == NULL)
	{
		out_context.callbacks[index] = handler;
	}
	else if (out_context.callbacks[index] != handler)
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
