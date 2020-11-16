/* channel.h
 * Abstract channel for IO in form of sequence of bytes.
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


typedef struct _SmeChannel SmeChannel;

//Interface a job source should present to channel.
typedef struct
{
	void *source_ptr;

	void (*notify)(void *source_ptr, int n_jobs);
} SmeJobSource;


//Channel base class
struct _SmeChannel
{
	MdslRC parent;

	//Virtual functions
	void (*destroy) (SmeChannel *channel);
	void (*set_read_source)
		(SmeChannel *channel, SmeJobSource source);
	void (*unset_read_source) (SmeChannel *channel);
	void (*set_write_source)
		(SmeChannel *channel, SmeJobSource source);
	void (*unset_write_source) (SmeChannel *channel);
	void (*add_read_job)
		(SmeChannel *channel, SscMBlock *blocks, size_t n_blocks);
	void (*add_write_job)
		(SmeChannel *channel, SscMBlock *blocks, size_t n_blocks);
	ssize_t (*read) (SmeChannel *channel);
	ssize_t (*write) (SmeChannel *channel);
};

mdsl_rc_declare(SmeChannel, sme_channel);

static inline void sme_channel_set_read_source
	(SmeChannel *channel, SmeJobSource source)
{
	(* channel->set_read_source)(channel, source);
}

static inline void sme_channel_unset_read_source (SmeChannel *channel)
{
	(* channel->unset_read_source)(channel);
}

static inline void sme_channel_set_write_source
	(SmeChannel *channel, SmeJobSource source)
{
	(* channel->set_write_source)(channel, source);
}

static inline void sme_channel_unset_write_source (SmeChannel *channel)
{
	(* channel->unset_write_source)(channel);
}

static inline void sme_channel_add_read_job
	(SmeChannel *channel, SscMBlock *blocks, size_t n_blocks)
{
	(* channel->add_read_job)(channel, blocks, n_blocks);
}

static inline void sme_channel_add_write_job
	(SmeChannel *channel, SscMBlock *blocks, size_t n_blocks)
{
	(* channel->add_write_job)(channel, blocks, n_blocks);
}

static inline ssize_t sme_channel_read (SmeChannel *channel)
{
	return (* channel->read)(channel);
}

static inline ssize_t sme_channel_write (SmeChannel *channel)
{
	return (* channel->write)(channel);
}

void sme_channel_init(SmeChannel *channel);
