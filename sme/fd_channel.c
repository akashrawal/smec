 /* fd_channel.c
 * Manages vectored IO over a file descriptor.
 * 
 * Copyright 2015-2020 Akash Rawal
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


//TODO: Fix IOV_MAX stuff
#include <limits.h> 
#include <unistd.h>

//Declarations
mdsl_declare_queue(struct iovec, IovQueue, iov_queue);
mdsl_declare_queue(int, IntQueue, int_queue);

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
	SmeChannel parent;
	int fd;

	ChannelLane write[1], read[1];
	size_t iov_max;
};

//Channel lane
static void channel_lane_init(ChannelLane *lane)
{
	lane->enabled = 0;
}

static void channel_lane_add_job
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

static void channel_lane_enable
	(ChannelLane *lane, SmeJobSource source)
{
	if (lane->enabled)
	{
		sme_error("Cannot set source more than once");	
	}

	lane->source = source;
	lane->enabled = 1;
	lane->n_pending_compl = 0;
	iov_queue_init(lane->iov);
	int_queue_init(lane->compl);
}

static void channel_lane_disable(ChannelLane *lane)
{
	if (lane->enabled)
	{
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
		dv[i].iov_base = MDSL_PTR_ADD(dv[i].iov_base, n_bytes);
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

	//Inform completions
	if (job > 0)
		(* lane->source.notify)
			(lane->source.source_ptr, job);
}

#define channel_lane_io(lane, fd, fn, iov_max, res) \
do { \
	if (! lane->enabled) \
		sme_error("No job source"); \
	size_t size = iov_queue_size(lane->iov); \
	if (size > iov_max) \
		size = iov_max; \
	res = fn(fd, iov_queue_head(lane->iov), size); \
	if (res >= 0) \
		channel_lane_pop_bytes(lane, res); \
} while (0)

static int channel_lane_get_queue_len(ChannelLane *lane)
{
	return int_queue_size(lane->compl);
}

//Virtual function implementations
static void sme_fd_channel_destroy(SmeChannel *base_type)
{
	SmeFdChannel *channel = (SmeFdChannel *) base_type;

	channel_lane_disable(channel->write);
	channel_lane_disable(channel->read);

	free(channel);
}

static void sme_fd_channel_set_write_source
	(SmeChannel *base_type, SmeJobSource source)
{
	SmeFdChannel *channel = (SmeFdChannel *) base_type;

	channel_lane_enable(channel->write, source);
}

static void sme_fd_channel_unset_write_source(SmeChannel *base_type)
{
	SmeFdChannel *channel = (SmeFdChannel *) base_type;

	channel_lane_disable(channel->write);
}

static void sme_fd_channel_add_write_job
	(SmeChannel *base_type, SscMBlock *blocks, size_t n_blocks)
{
	SmeFdChannel *channel = (SmeFdChannel *) base_type;

	channel_lane_add_job(channel->write, blocks, n_blocks);	
}

static ssize_t sme_fd_channel_write(SmeChannel *base_type)
{
	SmeFdChannel *channel = (SmeFdChannel *) base_type;
	ssize_t res;
	channel_lane_io
		(channel->write, channel->fd, writev, channel->iov_max, res);
	return res;
}


static void sme_fd_channel_set_read_source
	(SmeChannel *base_type, SmeJobSource source)
{
	SmeFdChannel *channel = (SmeFdChannel *) base_type;
	channel_lane_enable(channel->read, source);
}

static void sme_fd_channel_unset_read_source(SmeChannel *base_type)
{
	SmeFdChannel *channel = (SmeFdChannel *) base_type;
	channel_lane_disable(channel->read);
}

static void sme_fd_channel_add_read_job
	(SmeChannel *base_type, SscMBlock *blocks, size_t n_blocks)
{
	SmeFdChannel *channel = (SmeFdChannel *) base_type;

	channel_lane_add_job(channel->read, blocks, n_blocks);	
}

static ssize_t sme_fd_channel_read(SmeChannel *base_type)
{
	SmeFdChannel *channel = (SmeFdChannel *) base_type;
	ssize_t res;
	channel_lane_io
		(channel->read, channel->fd, readv, channel->iov_max, res);
	return res;
}

//Writing
int sme_fd_channel_get_write_queue_len(SmeFdChannel *channel)
{
	return channel_lane_get_queue_len(channel->write);
}

//Reading
int sme_fd_channel_get_read_queue_len(SmeFdChannel *channel)
{
	return channel_lane_get_queue_len(channel->read);
}

SmeFdChannel *sme_fd_channel_new(int fd)
{
	SmeFdChannel *channel;

	channel = (SmeFdChannel *) mdsl_alloc(sizeof(SmeFdChannel));

	sme_channel_init((SmeChannel*) channel);

	channel->fd = fd;

	//IOV_MAX restriction?
#ifdef IOV_MAX
	channel->iov_max = IOV_MAX;
#else
	channel->iov_max = sysconf(_SC_IOV_MAX);
#endif

	//Initialize lanes
	channel_lane_init(channel->write);
	channel_lane_init(channel->read);
	
	//Setup virtual functions
	channel->parent.destroy = sme_fd_channel_destroy;
	channel->parent.set_read_source = sme_fd_channel_set_read_source;
	channel->parent.unset_read_source = sme_fd_channel_unset_read_source;
	channel->parent.set_write_source = sme_fd_channel_set_write_source;
	channel->parent.unset_write_source = sme_fd_channel_unset_write_source;
	channel->parent.add_read_job = sme_fd_channel_add_read_job;
	channel->parent.add_write_job = sme_fd_channel_add_write_job;
	channel->parent.read = sme_fd_channel_read;
	channel->parent.write = sme_fd_channel_write;

	return channel;
}

//Testing
ssize_t sme_fd_channel_test_write(SmeFdChannel *channel, SmeVectorIOFn fn)
{
	ssize_t res;
	channel_lane_io
		(channel->write, channel->fd, (*fn), channel->iov_max, res);
	return res;
}

ssize_t sme_fd_channel_test_read(SmeFdChannel *channel, SmeVectorIOFn fn)
{
	ssize_t res;
	channel_lane_io
		(channel->read, channel->fd, (*fn), channel->iov_max, res);
	return res;
}
