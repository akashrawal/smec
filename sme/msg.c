/* msg.h
 * [De]Serialization of messages on streams
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


typedef enum
{
	WRITER_INACTIVE, 
	WRITER_ACTIVE, 
	WRITER_DISPOSED
} WriterState;

//Message writer

typedef struct
{
	MmcMsg *msg;
	uint32_t *preamble;
} WriterJob;

mdsl_declare_queue(WriterJob, WriterJobQueue, writer_job_queue);

struct _SmeMsgWriter
{
	MdslRC parent;

	WriterState state;
	SmeChannel channel;
	WriterJobQueue job_queue[1];
};

//
static void sme_msg_writer_set_channel_fn
	(void *source_ptr, SmeChannel channel)
{
	SmeMsgWriter *writer = source_ptr;

	if (writer->state != WRITER_INACTIVE)
		sme_error("Cannot set channel more than once");

	sme_msg_writer_ref(writer); //Reference is owned by the channel. 
	writer->state = WRITER_ACTIVE;
	writer->channel = channel;
}

static void sme_msg_writer_unset_channel_fn(void *source_ptr)
{
	SmeMsgWriter *writer = source_ptr;

	if (writer->state == WRITER_ACTIVE)
	{
		writer->state = WRITER_DISPOSED;
		sme_msg_writer_unref(writer);
	}
	else if (writer->state == WRITER_INACTIVE) 
		sme_error("Cannot unset channel when no channel is set");
	else 
		sme_error("Cannot unset channel more than one");
}

static void sme_msg_writer_notify_fn(void *source_ptr, int n_jobs)
{
	SmeMsgWriter *writer = source_ptr;

	while (n_jobs--)
	{
		WriterJob one_job = writer_job_queue_pop(writer->job_queue);	
		mmc_msg_unref(one_job.msg);
		free(one_job.preamble);
	}
}

//
mdsl_rc_define(SmeMsgWriter, sme_msg_writer);

static void sme_msg_writer_destroy(SmeMsgWriter *writer)
{
	int i, len;
	WriterJob *jobs;

	//Destroy job queue
	len = writer_job_queue_size(writer->job_queue);
	jobs = writer_job_queue_head(writer->job_queue);
	for (i = 0; i < len; i++)
	{
		mmc_msg_unref(jobs[i].msg);
		free(jobs[i].preamble);
	}
	writer_job_queue_destroy(writer->job_queue);

	free(writer);
}

SmeMsgWriter *sme_msg_writer_new()
{
	SmeMsgWriter *writer;

	writer = (SmeMsgWriter *) mdsl_alloc(sizeof(SmeMsgWriter));

	mdsl_rc_init(writer);
	
	writer->state = WRITER_INACTIVE;
	writer_job_queue_init(writer->job_queue); 

	return writer;
}

SmeJobSource sme_msg_writer_get_source(SmeMsgWriter *writer)
{
	SmeJobSource res;

	res.source_ptr = writer;
	res.set_channel = sme_msg_writer_set_channel_fn;
	res.unset_channel = sme_msg_writer_unset_channel_fn;
	res.notify = sme_msg_writer_notify_fn;

	return res;
}

void sme_msg_writer_add_msg(SmeMsgWriter *writer, MmcMsg *msg)
{
	size_t len = ssc_msg_count(msg);
	size_t n_blocks;

	WriterJob new_job;
	SscMBlock *iov;

	if (writer->state != WRITER_ACTIVE)
		sme_error("Associate with channel before adding messages. ");

	//Create job
	new_job.msg = msg;
	mmc_msg_ref(msg);
	iov = (SscMBlock *) mdsl_alloc(sizeof(SscMBlock) * (len + 1));

	//Add preamble (size + layout)
	new_job.preamble = (uint32_t *) mdsl_alloc
		((len + 1) * sizeof(uint32_t));
	new_job.preamble[0] = ssc_uint32_to_le((uint32_t) len);
	ssc_msg_create_layout(msg, len, new_job.preamble + 1);	
	iov[0].mem = new_job.preamble;
	iov[0].len = (len + 1) * sizeof(uint32_t);

	//Add data
	n_blocks = ssc_msg_get_blocks(msg, len, iov + 1);

	//Add to queue
	writer_job_queue_push(writer->job_queue, new_job);

	//Assign job to channel
	(* writer->channel.add_job)
		(writer->channel.channel_ptr, iov, n_blocks + 1);

	//Free
	free(iov);
}

int sme_msg_writer_get_queue_len(SmeMsgWriter *writer)
{
	return writer_job_queue_size(writer->job_queue);
}

//Message reader

typedef enum
{
	READER_INACTIVE,
	READER_READ_SIZE,
	READER_READ_LAYOUT,
	READER_READ_MSG,
	READER_DISPOSED,
	READER_ERROR
} ReaderState;

mdsl_declare_queue(MmcMsg*, MsgQueue, msg_queue);

struct _SmeMsgReader
{
	MdslRC parent;


	ReaderState state;
	SmeChannel channel;

	MsgQueue msg_queue[1];
	
	uint32_t d;
	void *layout;
	uint32_t layout_size;
	MmcMsg *msg;
};

//
static void sme_msg_reader_goto_read_size(SmeMsgReader *reader)
{
	SmeChannel c = reader->channel;

	SscMBlock iov = {&(reader->d), sizeof(uint32_t)};
	reader->state = READER_READ_SIZE;
	(*c.add_job)(c.channel_ptr, &iov, 1);
}

static void sme_msg_reader_advance(SmeMsgReader *reader)
{
	SmeChannel c = reader->channel;

	if (reader->state == READER_READ_SIZE)
	{
		uint32_t size;
		SscMBlock iov;

		//Fetch data
		size = ssc_uint32_from_le(reader->d);

		//Checks
		if (size == 0)
			goto fail;

		//Allocate memory for layout
		iov.len = size * sizeof(uint32_t);
		iov.mem = mdsl_tryalloc(iov.len);
		if (! iov.mem)
			goto fail;
		
		//State change
		reader->layout = iov.mem;
		reader->layout_size = size;
		reader->state = READER_READ_LAYOUT;

		//Add job
		(*c.add_job)(c.channel_ptr, &iov, 1);
	}
	else if (reader->state == READER_READ_LAYOUT)
	{
		uint32_t *layout;
		uint32_t size;
		uint32_t n_blocks;
		MmcMsg *msg;
		SscMBlock *iov;

		//Fetch data
		layout = (uint32_t *) reader->layout;
		size = reader->layout_size;
		reader->layout = NULL;


		//Allocate  vector
		iov = (SscMBlock *) mdsl_tryalloc(sizeof(SscMBlock) * size);
		if (! iov)
		{
			free(layout);
			goto fail;
		}

		//Allocate message
		msg = ssc_msg_alloc_by_layout(size, layout);
		free(layout); //< Not needed anymore
		if (! msg)
		{
			free(iov);
			goto fail;
		}

		//Fill data into vector
		n_blocks = ssc_msg_get_blocks(msg, size, iov);
		
		if (n_blocks == 0)
		{
			//Add finished message to queue and return 
			msg_queue_push(reader->msg_queue, msg);
			sme_msg_reader_goto_read_size(reader);
		}
		else
		{
			//State change
			reader->msg = msg;
			reader->state = READER_READ_MSG;
			
			//Add job
			(*c.add_job)(c.channel_ptr, iov, n_blocks);
		}

		//Cleanup
		free(iov);
	}
	else if (reader->state == READER_READ_MSG)
	{
		MmcMsg *msg;

		//Fetch data
		msg = reader->msg;
		reader->msg = NULL;

		//Add finished message to queue and return 
		msg_queue_push(reader->msg_queue, msg);
		sme_msg_reader_goto_read_size(reader);
	}

	return;
fail:
	reader->state = READER_ERROR;
}

//
static void sme_msg_reader_set_channel_fn
	(void *source_ptr, SmeChannel channel)
{
	SmeMsgReader *reader = source_ptr;

	if (reader->state != READER_INACTIVE)
		sme_error("Cannot set channel more than once");

	reader->channel = channel;
	sme_msg_reader_ref(reader); //Reference is owned by the channel. 
	sme_msg_reader_goto_read_size(reader);
}

static void sme_msg_reader_unset_channel_fn(void *source_ptr)
{
	SmeMsgReader *reader = source_ptr;

	if (reader->state == READER_INACTIVE) 
		sme_error("Cannot unset channel when no channel is set");
	else if (reader->state == READER_DISPOSED)
		sme_error("Cannot unset channel more than once");
	else
	{
		reader->state = READER_DISPOSED;
		sme_msg_reader_unref(reader);
	}
}

static void sme_msg_reader_notify_fn(void *source_ptr, int n_jobs)
{
	SmeMsgReader *reader = source_ptr;

	while (n_jobs--)
	{
		sme_msg_reader_advance(reader);
	}
}

//
mdsl_rc_define(SmeMsgReader, sme_msg_reader);

static void sme_msg_reader_destroy(SmeMsgReader *reader)
{
	int n_msgs, i;
	MmcMsg **msg_array;
	
	//Destroy message queue
	msg_array = msg_queue_head(reader->msg_queue);
	n_msgs = msg_queue_size(reader->msg_queue);
	for (i = 0; i < n_msgs; i++)
	{
		mmc_msg_unref(msg_array[i]);
	}
	msg_queue_destroy(reader->msg_queue);
	
	if (reader->layout)
		free(reader->layout);
	if (reader->msg)
		mmc_msg_unref(reader->msg);

	free(reader);
}

SmeMsgReader *sme_msg_reader_new()
{
	SmeMsgReader *reader;

	reader = (SmeMsgReader *) mdsl_alloc(sizeof(SmeMsgReader));

	mdsl_rc_init(reader);

	msg_queue_init(reader->msg_queue);
	reader->state = READER_INACTIVE;
	reader->layout = NULL;
	reader->msg = NULL;

	return reader;
}

SmeJobSource sme_msg_reader_get_source(SmeMsgReader *reader)
{
	SmeJobSource res;

	res.source_ptr = reader;
	res.set_channel = sme_msg_reader_set_channel_fn;
	res.unset_channel = sme_msg_reader_unset_channel_fn;
	res.notify = sme_msg_reader_notify_fn;

	return res;
}

int sme_msg_reader_get_queue_len(SmeMsgReader *reader)
{
	int res; 
	res = msg_queue_size(reader->msg_queue);

	if (res == 0 && reader->state == READER_ERROR)
		return -1;

	return res;
}

MmcMsg *sme_msg_reader_pop_msg(SmeMsgReader *reader)
{
	if (msg_queue_size(reader->msg_queue) == 0)
		return NULL;
	return msg_queue_pop(reader->msg_queue);
}

