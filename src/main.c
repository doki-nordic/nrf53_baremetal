/*
 * Copyright (c) 2020 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#include "nrf.h"

#include "cbkprox.h"

/* ====================== common parts for example */

char tr_buf[16 * 1024];
char *tr = tr_buf;

typedef void (*example_callback_decoder_t)(void* callback, uint32_t *buffer);

typedef void (*example_callback_t)(uint32_t some_param);

#define ID_CALL_CALLBACK 0
void call_callback_on_app_core(uint32_t *buffer);
#define ID_SAMPLE_FUNCTION 1
void sample_function_on_net_core(uint32_t *buffer);

void send_command(uint32_t* buffer)
{
	switch (buffer[0])
	{
	case ID_CALL_CALLBACK:
		call_callback_on_app_core(buffer + 1);
		break;
	case ID_SAMPLE_FUNCTION:
		sample_function_on_net_core(buffer + 1);
		break;
	}
}

// target API function on net core. Can be called remotely by ser_sample_function on app core
void sample_function(uint32_t some_parameter, example_callback_t callback)
{
	tr += sprintf(tr, "sample_function executed with %d\n", (int)some_parameter);
	tr += sprintf(tr, "executing callback with %d\n", (int)some_parameter + 10);
	callback(some_parameter + 10);
}

/* ====================== net core example */

void sample_function_callback_handler(uint32_t callback_slot, int rsv1, int rsv2, int rsv3, uint32_t param1)
{
	// encode parameters
	uint32_t buffer[3];
	buffer[0] = ID_CALL_CALLBACK;
	buffer[1] = callback_slot;
	buffer[2] = param1;
	// send command to app core
	send_command(buffer);
}

void sample_function_on_net_core(uint32_t *buffer)
{
	// decode parameters
	uint32_t some_parameter = buffer[0];
	uint32_t callback_slot = buffer[1];
	// callback cannot be called directly, because it is on other core, so
	// use proxy callback with handler that will encode end send it to the
	// other core.
	example_callback_t callback = cbkprox_out_get(callback_slot, sample_function_callback_handler);
	sample_function(some_parameter, callback);
}

/* ====================== app core example */

void example_callback_decoder(void* callback, uint32_t *buffer)
{
	// decode parameters
	uint32_t param1 = buffer[0];
	// cast callback to correct type
	example_callback_t callback_func = (example_callback_t)callback;
	// execute callback
	callback_func(param1);
}

void ser_sample_function(uint32_t some_parameter, example_callback_t callback)
{
	uint32_t buffer[3];
	// request in slot
	uint32_t callback_slot = cbkprox_in_set(example_callback_decoder, callback);
	// encode parameters
	buffer[0] = ID_SAMPLE_FUNCTION;
	buffer[1] = some_parameter;
	buffer[2] = callback_slot;
	// send command
	send_command(buffer);
}

void call_callback_on_app_core(uint32_t *buffer)
{
	void *callback;
	// decode callback slot index
	uint32_t callback_slot = buffer[0];
	// get decoder (handler) and callback pointer from slot
	example_callback_decoder_t decoder = (example_callback_decoder_t)cbkprox_in_get(callback_slot, &callback);
	// call callback decoder to decode parameters and execute callback
	decoder(callback, buffer + 1);
}


void test3_callback1(uint32_t some_param)
{
	tr += sprintf(tr, "test3_callback1 executed with %d\n", (int)some_param);
}


void test3_callback2(uint32_t some_param)
{
	tr += sprintf(tr, "test3_callback2 executed with %d\n", (int)some_param);
}


void test3()
{
	tr += sprintf(tr, "calling ser_sample_function with 1 and test3_callback1\n");
	ser_sample_function(1, test3_callback1);
	tr += sprintf(tr, "calling ser_sample_function with 2 and test3_callback2\n");
	ser_sample_function(2, test3_callback2);
	tr += sprintf(tr, "calling ser_sample_function with 3 and test3_callback1\n");
	ser_sample_function(3, test3_callback1);
	tr += sprintf(tr, "calling ser_sample_function with 4 and test3_callback2\n");
	ser_sample_function(4, test3_callback2);
}

const volatile char strtt[] __attribute__((section(".datatext"))) = "qwertyuofksdjfhsdfsdf";

__attribute__((noinline))
void __attribute__((section(".datatext.testRAM"))) testRAM()
{
	NRF_P0_S->PIN_CNF[28] = strtt[3];
}

static inline void* _LONG_JUMP_helper(void* p) {
	__asm__ volatile ("":"+r"(p));
	return p;
}

#define LONG_JUMP(func) ((typeof(&(func)))_LONG_JUMP_helper(&(func)))


void func3(int dummy, ...);

#include <stdarg.h>

void func4(int dummy, ...)
{
	va_list vl;
	va_start(vl, dummy);
	NRF_P0_S->PIN_CNF[28] = va_arg(vl, int32_t);
	va_end(vl);
}

void funcccccc(void)
{
	func3(0, 1, 2, 3, 4, 5, 6);
}

typedef struct S_
{
	uint32_t big[5];
} S;


#define paramsA int r0, int r1, int r2, int r3
#define paramsB S p0, int p1

#define argA 0, 0, 0, 0
#define argB s, 0x11

void test4_handler(paramsA, paramsB)
{
}

typedef void (*test4_t)(paramsB);

void test4_test1(paramsB)
{
}

void test4_test2(paramsA, paramsB)
{
}

void test5i(int a, int b, int c, int d);

void test5(int a, int b, int c, int d) {
	test5i(a, b, c, d);
	a += 10;
	b ++;
	//x = x + c + d;
	test5i(a, b, c, d);
}

int main()
{

	NRF_P0_S->PIN_CNF[28] = 1;
	NRF_P0_S->PIN_CNF[29] = 1;
	NRF_P0_S->PIN_CNF[30] = 1;
	NRF_P0_S->PIN_CNF[31] = 1;
	NRF_P0_S->PIN_CNF[31] = (uint32_t)&test5;
/*
	test4_t f = (test4_t)cbkprox_out_get(0, test4_handler);

	S s;
	s.big[0] = 0x30;
	s.big[1] = 0x31;
	s.big[2] = 0x32;
	s.big[3] = 0x33;
	s.big[4] = 0x34;

	test4_test1(argB);
	test4_test2(argA, argB);
	f(argB);*/

	test3();
//	test2();
	//LONG_JUMP(testRAM)();
	//LONG_JUMP(testRAM)();

	while(1)
	{
		static volatile int x;
		NRF_P0_S->OUTSET = (1 << 28);
		NRF_P0_S->OUTCLR = (1 << 29);
		NRF_P0_S->OUTCLR = (1 << 30);
		NRF_P0_S->OUTSET = (1 << 31);
		for (x = 0; x < 500000; x++);
		NRF_P0_S->OUTCLR = (1 << 28);
		NRF_P0_S->OUTSET = (1 << 29);
		NRF_P0_S->OUTSET = (1 << 30);
		NRF_P0_S->OUTCLR = (1 << 31);
		for (x = 0; x < 500000; x++);
	}
}

/*

solution for out slots on x86 and x86_64

each entry:
	CALL callback_jump_table_end

push minimum what needed
automic inc
if non-zero before:
	push all what cannon change
	wait for access (zero value) with sleep/yeyld
	pop what pushed before
read index from stack and save to state
read return from stack and save to state
write new return to stack
load callback pointer
write it to stack
pop what was pushed at the beginning
"ret" to jump to handler
push minimum what needed
read return from state
write return to stack
pop what was pushed before
"ret" to return to caller


*/