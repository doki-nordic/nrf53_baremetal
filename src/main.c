/*
 * Copyright (c) 2020 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <stddef.h>

#include "nrf.h"

#include "cbkprox.h"

typedef int (*call_t)(int);

int callback_handler(int a)
{
	uint64_t state = cbkprox_start_handler();
	volatile int x = 0;
	x = x;
	cbkprox_end_handler(state);
	return 123;
}

void test()
{
	call_t call0 = (call_t)cbkprox_alloc(0, (void *)callback_handler);
	call_t call1 = (call_t)cbkprox_alloc(5, (void *)callback_handler);
	int r;
	volatile int x;
	r = call0(234);
	x = r;
	x = x;
	r = call1(234);
	x = r;
	x = x;
}

volatile int xxx;

__attribute__ ((__used__))
__attribute__ ((__noinline__))
void *test2()
{
	xxx = sin(12);
	return __builtin_return_address(0);
}

int main()
{

	NRF_P0_S->PIN_CNF[28] = 1;
	NRF_P0_S->PIN_CNF[29] = 1;
	NRF_P0_S->PIN_CNF[30] = 1;
	NRF_P0_S->PIN_CNF[31] = 1;

	test();
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

