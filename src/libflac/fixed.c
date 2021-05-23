/* libFLAC - Free Lossless Audio Codec library
 * Copyright (C) 2000-2009  Josh Coalson
 * Copyright (C) 2011-2016  Xiph.Org Foundation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of the Xiph.org Foundation nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <math.h>
#include <string.h>
#include "share/compat.h"
#include "private/bitmath.h"
#include "private/fixed.h"
#include "private/macros.h"

#ifdef local_abs
#undef local_abs
#endif
#define local_abs(x) ((uint32_t)((x)<0? -(x) : (x)))

void FLAC__fixed_compute_residual(const FLAC__int32 data[], uint32_t data_len, uint32_t order, FLAC__int32 residual[])
{
	const int idata_len = (int)data_len;
	int i;

	switch(order) {
		case 0:
			memcpy(residual, data, sizeof(residual[0])*data_len);
			break;
		case 1:
			for(i = 0; i < idata_len; i++)
				residual[i] = data[i] - data[i-1];
			break;
		case 2:
			for(i = 0; i < idata_len; i++)
				residual[i] = data[i] - 2*data[i-1] + data[i-2];
			break;
		case 3:
			for(i = 0; i < idata_len; i++)
				residual[i] = data[i] - 3*data[i-1] + 3*data[i-2] - data[i-3];
			break;
		case 4:
			for(i = 0; i < idata_len; i++)
				residual[i] = data[i] - 4*data[i-1] + 6*data[i-2] - 4*data[i-3] + data[i-4];
			break;
		default: break;
	}
}

void FLAC__fixed_restore_signal(const FLAC__int32 residual[], uint32_t data_len, uint32_t order, FLAC__int32 data[])
{
	int i, idata_len = (int)data_len;

	switch(order) {
		case 0:
			memcpy(data, residual, sizeof(residual[0])*data_len);
			break;
		case 1:
			for(i = 0; i < idata_len; i++)
				data[i] = residual[i] + data[i-1];
			break;
		case 2:
			for(i = 0; i < idata_len; i++)
				data[i] = residual[i] + 2*data[i-1] - data[i-2];
			break;
		case 3:
			for(i = 0; i < idata_len; i++)
				data[i] = residual[i] + 3*data[i-1] - 3*data[i-2] + data[i-3];
			break;
		case 4:
			for(i = 0; i < idata_len; i++)
				data[i] = residual[i] + 4*data[i-1] - 6*data[i-2] + 4*data[i-3] - data[i-4];
			break;
		default: break;
	}
}
