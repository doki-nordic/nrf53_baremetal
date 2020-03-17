
#include <stddef.h>
#include <stdint.h>

#include "cbkprox.h"

#if CONFIG_CBKPROX_SLOTS > 16383
#error "Too many cbkprox slots"
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

struct callback_context
{
	uint32_t state[3]; /* 0 - previous PRIMASK value, 1 - return address, 2 - index of the callback slot */
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
		".L%=callback_jump_table_end:\n"
		"ldr   r5, .L%=callback_jump_table_start_addr\n"
		"sub   lr, r5\n"
		"asrs  lr, lr, #1\n"
		"ldr   r5, .L%=callback_context_addr\n"
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
		"ldr   r2, .L%=callback_context_addr\n"
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
		".L%=callback_context_addr:\n"
		".word callback_context\n":::
		);
}


uint64_t cbkprox_start_handler()
{
	uint64_t result;
	result = (uint64_t)((callback_context.state[2] - 12) / 4) | ((uint64_t)callback_context.state[1] << 32);
	if (!callback_context.state[0]) {
		__asm volatile("cpsie i\n":::"memory");
	} else {
		result |= 0x80000000LL;
	}
	return result;
}

void cbkprox_end_handler(uint64_t state)
{
	uint64_t key = state & 0x80000000LL;
	__asm volatile("cpsid i\n"::"r"(key):"memory");
	callback_context.state[0] = key;
	callback_context.state[1] = (uint32_t)(state >> 32);
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

/*

solution for x86 i x86_64

each entry:
	CALL callback_jump_table_end

push minimum what needed
automic inc
if non-zero before:
	push all what cannon change
	wait for access (zero value) with sleep/yeyld
	pop what pushed before
pop index and save to state
pop return and save to state
push new return
load callback pointer
push to stack
"ret" to jump to handler
read return from state
push return to stack
"ret" to return to caller


*/