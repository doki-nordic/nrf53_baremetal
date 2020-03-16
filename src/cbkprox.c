
#include <stddef.h>
#include <stdint.h>

#include "cbkprox.h"

#if CONFIG_CBKPROX_SLOTS > 16383
#error "Too many cbkprox slots"
#endif

#define TABLE_ENTRY1 \
	"push {r4, r5, lr}\n" \
	"nop\n" \
	"bl callback_jump_table_end\n"

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

struct callback_context
{
	uint32_t state[4];
	void* callbacks[CONFIG_CBKPROX_SLOTS];
};

static struct callback_context callback_context;

__attribute__((naked))
void callback_jump_table_start(void)
{
	__asm volatile (
#if CONFIG_CBKPROX_SLOTS & 1
		TABLE_ENTRY1
#endif
#if CONFIG_CBKPROX_SLOTS & 2
		TABLE_ENTRY2
#endif
#if CONFIG_CBKPROX_SLOTS & 4
		TABLE_ENTRY4
#endif
#if CONFIG_CBKPROX_SLOTS & 8
		TABLE_ENTRY8
#endif
#if CONFIG_CBKPROX_SLOTS & 16
		TABLE_ENTRY16
#endif
#if CONFIG_CBKPROX_SLOTS & 32
		TABLE_ENTRY32
#endif
#if CONFIG_CBKPROX_SLOTS & 64
		TABLE_ENTRY64
#endif
#if CONFIG_CBKPROX_SLOTS & 128
		TABLE_ENTRY128
#endif
#if CONFIG_CBKPROX_SLOTS & 256
		TABLE_ENTRY256
#endif
#if CONFIG_CBKPROX_SLOTS & 512
		TABLE_ENTRY512
#endif
#if CONFIG_CBKPROX_SLOTS & 1024
		TABLE_ENTRY1024
#endif
#if CONFIG_CBKPROX_SLOTS & 2048
		TABLE_ENTRY2048
#endif
#if CONFIG_CBKPROX_SLOTS & 4096
		TABLE_ENTRY4096
#endif
#if CONFIG_CBKPROX_SLOTS & 8192
		TABLE_ENTRY8192
#endif
		"callback_jump_table_end:\n"
		"ldr   r5, .L%=callback_jump_table_start_addr\n"
		"sub   lr, r5\n"
		"asrs  lr, lr, #1\n"
		"ldr   r5, .L%=callback_context_addr\n"
		"mrs   r4, PRIMASK\n"
		"cpsid i\n"
		"str   r4, [r5]\n"
		"str   lr, [r5, #4]\n"
		"ldr   r4, [sp, #4]\n"
		"str   r4, [r5, #8]\n"
		"ldr   r4, [sp, #8]\n"
		"str   r4, [r5, #12]\n"
		"ldr   r5, [r5, lr]\n"
		"add   sp, #8\n"
		"pop   {r4}\n"
		"blx   r5\n"
		"sub   sp, #8\n"
		"ldr   r5, .L%=callback_context_addr\n"
		"ldr   r3, [r5, #8]\n"
		"str   r3, [sp, #4]\n"
		"ldr   r3, [r5, #12]\n"
		"str   r3, [sp, #8]\n"
		"ldr   r3, [r5]\n"
 		"cbnz  r3, .L%=next_instr\n"
 		"cpsie i\n"
		".L%=next_instr:\n"
		"pop   {r5, pc}\n"
		".align 1\n"
		".L%=callback_jump_table_start_addr:\n"
		".word callback_jump_table_start - 24\n"
		".L%=callback_context_addr:\n"
		".word callback_context\n":::
		);
}


uint64_t cbkprox_start_handler(uint32_t *index)
{
	uint32_t index_value;
	uint64_t result;
	result = (uint64_t)callback_context.state[2] | ((uint64_t)callback_context.state[3] << 32);
	index_value = callback_context.state[1];
	if (!callback_context.state[0]) {
		__asm volatile("cpsie i\n":::"memory");
	} else {
		index_value |= 0x80000000;
	}
	*index = index_value;
	return result;
}

void cbkprox_end_handler(uint64_t state, uint32_t index)
{
	index &= 0x80000000;
	__asm volatile("cpsid i\n"::"r"(index):"memory");
	callback_context.state[0] = index;
	callback_context.state[2] = (uint32_t)state;
	callback_context.state[3] = (uint32_t)(state >> 32);
}

void* cbkprox_alloc(uint32_t index, void *handler)
{
	uint32_t addr;

	if (index >= CONFIG_CBKPROX_SLOTS) {
		return NULL;
	}
	else if (callback_context.callbacks[index] == NULL)
	{
		callback_context.callbacks[index] = handler;
	}
	else if (callback_context.callbacks[index] != handler)
	{
		return NULL;
	}

	addr = (uint32_t)(void*)&callback_jump_table_start;
	addr += 8 * index;

	return (void*)addr;
}
