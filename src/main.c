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

void sample_function_callback_handler(uint32_t param1)
{
	// start handler and get callback slot index
	uint64_t state = cbkprox_out_start_handler();
	uint32_t callback_slot = state & 0xFFFF;
	// encode parameters
	uint32_t buffer[3];
	buffer[0] = ID_CALL_CALLBACK;
	buffer[1] = callback_slot;
	buffer[2] = param1;
	// send command to app core
	send_command(buffer);
	// exit handler
	cbkprox_out_end_handler(state);
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


int main()
{

	NRF_P0_S->PIN_CNF[28] = 1;
	NRF_P0_S->PIN_CNF[29] = 1;
	NRF_P0_S->PIN_CNF[30] = 1;
	NRF_P0_S->PIN_CNF[31] = 1;

	test3();
	//test2();

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