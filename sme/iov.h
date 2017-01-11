 /* iov.h
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


typedef struct _SmeIov SmeIov;

mmc_declare_queue(struct iovec, SmeIovecQueue, sme_iovec_queue);

struct _SmeIov
{
	MmcRC parent;
	
	SmeIovecQueue blocks[1];
};


//Operations that fetch data
static inline struct iovec *sme_iov_get_vector(SmeIov *iov)
{
	return sme_iovec_queue_head(iov->blocks);
}
static inline size_t sme_iov_get_len(SmeIov *iov)
{
	return sme_iovec_queue_size(iov->blocks);
}

//Operations that modify
struct iovec *sme_iov_push_blocks(SmeIov *iov, size_t n_blocks);
//	< Atomicity of above operation is debated
void sme_iov_pop_blocks(SmeIov *iov, size_t n_blocks);
size_t sme_iov_pop_bytes(SmeIov *iov, size_t n_bytes);
					   
//The usual
SmeIov *sme_iov_new();
mmc_rc_declare(SmeIov, sme_iov);

//next: Sending mechanism

