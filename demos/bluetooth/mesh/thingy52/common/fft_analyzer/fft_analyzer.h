/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/*
  Copyright (c) 2016 Arduino LLC. All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef FFT_ANALYZER_H__
#define FFT_ANALYZER_H__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/printk.h>

#define ARM_MATH_CM0PLUS
#include <arm_math.h>

/* FFT no error */
#define FFT_NO_ERROR 0
/* FFT error */
#define FFT_ERROR 1

bool fft_analyzer_available();
int fft_analyzer_read(int spectrum[], int size);
int fft_analyzer_configure(int length);
void fft_analyzer_update(const void *buffer, int size);

#endif /* FFT_ANALYZER_H__ */