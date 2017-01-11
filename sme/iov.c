/* iov.c
 * Managed data structure for vectored IO
 * 
 * Copyright 2015 Akash Rawal
 * This file is part of Modular Middleware.
 * 
 * Modular Middleware is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Modular Middleware is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Modular Middleware.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "incl.h"


struct iovec *sme_iov_push_blocks(SmeIov *iov, size_t n_blocks)
{
	struct iovec *res;

	res = sme_iovec_queue_alloc_n(iov->blocks, n_blocks);

	return res;
}

void sme_iov_pop_blocks(SmeIov *iov, size_t n_blocks)
{
	sme_iovec_queue_pop_n(iov->blocks, n_blocks);
}

size_t sme_iov_pop_bytes(SmeIov *iov, size_t n_bytes)
{
	struct iovec *dv;
	int i;
	int len;
	
	dv = sme_iovec_queue_head(iov->blocks);
	len = sme_iovec_queue_size(iov->blocks);
	for (i = 0; i < len ? (n_bytes >= dv[i].iov_len) : 0; i++)
	{
		n_bytes -= dv[i].iov_len;
	}
	
	if (i < len)
	{
		dv[i].iov_base = MMC_PTR_ADD(dv[i].iov_base, n_bytes);
		dv[i].iov_len -= n_bytes;
	}
	
	sme_iov_pop_blocks(iov, i);

	return i;
}

SmeIov *sme_iov_new()
{
	SmeIov *iov;

	iov = (SmeIov *) mmc_alloc(sizeof(SmeIov));

	mmc_rc_init(iov);
	sme_iovec_queue_init(iov->blocks);

	return iov;
}

mmc_rc_define(SmeIov, sme_iov);

static void sme_iov_destroy(SmeIov *iov)
{
	sme_iovec_queue_destroy(iov->blocks);
}

