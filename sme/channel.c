 /* channel.c
 * Fully managed interface to perform vectored IO over stream-like backends
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

//Declarations
mmc_declare_queue(struct iovec, IovQueue, iov_queue);
mmc_declare_queue(int, IntQueue, int_queue);

typedef struct 
{
	int enabled;
	IovQueue iov[1];
	IntQueue compl[1];
	int n_pending_compl;
	SmeJobSource source;
} ChannelLane;

struct _SmeFdChannel
{
	MmcRC parent;
	int fd;

	ChannelLane write[1], read[1];
};

//Channel lane
static void channel_lane_init(ChannelLane *lane)
{
	lane->enabled = 0;
}

static void channel_lane_add_job_fn
	(void *ptr, SscMBlock *blocks, size_t n_blocks)
{
	ChannelLane *lane = ptr;
	
	struct iovec *allocd;
	int i;

	allocd = iov_queue_alloc_n(lane->iov, n_blocks);
	for (i = 0; i < n_blocks; i++)
	{
		allocd[i].iov_base = blocks[i].mem;
		allocd[i].iov_len  = blocks[i].len;
	}

	int_queue_push(lane->compl, (int) n_blocks);
}

static void channel_lane_set_source
	(ChannelLane *lane, SmeJobSource source)
{
	SmeChannel vtable = {lane, channel_lane_add_job_fn};

	if (lane->enabled)
	{
		sme_error("Cannot set source more than once");	
	}

	lane->source = source;
	lane->enabled = 1;
	lane->n_pending_compl = 0;
	iov_queue_init(lane->iov);
	int_queue_init(lane->compl);

	if (source.set_channel)
		(*source.set_channel)(source.source_ptr, vtable); 
}

static void channel_lane_disable(ChannelLane *lane)
{
	if (lane->enabled)
	{
		if (lane->source.unset_channel)
			(*lane->source.unset_channel)(lane->source.source_ptr);
		lane->enabled = 0;
		iov_queue_destroy(lane->iov);
		int_queue_destroy(lane->compl);
	}
}

static void channel_lane_pop_bytes(ChannelLane *lane, size_t n_bytes)
{
	struct iovec *dv;
	int *head;
	int i;
	int len, cnt, blk, job;
	
	//Adjust iov, remove completed blocks
	dv = iov_queue_head(lane->iov);
	len = iov_queue_size(lane->iov);
	for (i = 0; i < len ? (n_bytes >= dv[i].iov_len) : 0; i++)
	{
		n_bytes -= dv[i].iov_len;
	}
	if (i < len)
	{
		dv[i].iov_base = MMC_PTR_ADD(dv[i].iov_base, n_bytes);
		dv[i].iov_len -= n_bytes;
	}
	blk = i;
	iov_queue_pop_n(lane->iov, blk);

	//Remove finished jobs
	head = int_queue_head(lane->compl);	
	len = int_queue_size(lane->compl);
	cnt = blk;
	for (i = 0; i < len ? head[i] <= cnt : 0; i++)
	{
		cnt -= head[i];
	}
	if (i < len)
	{
		head[i] -= cnt;
	}
	job = i;
	int_queue_pop_n(lane->compl, job);

	//Add finished jobs to pending completions
	lane->n_pending_compl += job;
}

#define channel_lane_io(lane, fd, fn, res) \
do { \
	if (! lane->enabled) \
		sme_error("No job source"); \
	res = fn(fd, iov_queue_head(lane->iov), iov_queue_size(lane->iov)); \
	if (res > 0) \
		channel_lane_pop_bytes(lane, res); \
} while (0)

static void channel_lane_inform_completion(ChannelLane *lane)
{
	if (lane->n_pending_compl)
	{
		if (lane->source.notify)
			(* lane->source.notify)
				(lane->source.source_ptr, lane->n_pending_compl);
		lane->n_pending_compl = 0;
	}
}

//Channel functions
mmc_rc_define(SmeFdChannel, sme_fd_channel);

SmeFdChannel *sme_fd_channel_new(int fd)
{
	SmeFdChannel *channel;

	channel = (SmeFdChannel *) mmc_alloc(sizeof(SmeFdChannel));

	mmc_rc_init(channel);

	channel->fd = fd;

	channel_lane_init(channel->write);
	channel_lane_init(channel->read);

	return channel;
}

static void sme_fd_channel_destroy(SmeFdChannel *channel)
{
	channel_lane_disable(channel->write);
	channel_lane_disable(channel->read);
}

//Writing
void sme_fd_channel_set_write_source
	(SmeFdChannel *channel, SmeJobSource source)
{
	channel_lane_set_source(channel->write, source);
}

ssize_t sme_fd_channel_write(SmeFdChannel *channel)
{
	ssize_t res;
	channel_lane_io(channel->write, channel->fd, writev, res);
	return res;
}

void sme_fd_channel_inform_write_completion(SmeFdChannel *channel)
{
	channel_lane_inform_completion(channel->write);
}

//Reading
void sme_fd_channel_set_read_source
	(SmeFdChannel *channel, SmeJobSource source)
{
	channel_lane_set_source(channel->read, source);
}

ssize_t sme_fd_channel_read(SmeFdChannel *channel)
{
	ssize_t res;
	channel_lane_io(channel->read, channel->fd, readv, res);
	return res;
}

void sme_fd_channel_inform_read_completion(SmeFdChannel *channel)
{
	channel_lane_inform_completion(channel->read);
}

