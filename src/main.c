/*
 * Copyright (c) 2020 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include "nrf.h"

int main()
{
	NRF_P0_S->PIN_CNF[28] = 1;
	NRF_P0_S->PIN_CNF[29] = 1;
	NRF_P0_S->PIN_CNF[30] = 1;
	NRF_P0_S->PIN_CNF[31] = 1;

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

