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

	SmeChannel *channel;
	WriterJobQueue job_queue[1];
};

//

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

	//Unref channel
	sme_channel_unset_write_source(writer->channel);	
	sme_channel_unref(writer->channel);
	writer->channel = NULL;

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

SmeMsgWriter *sme_msg_writer_new(SmeChannel *channel)
{
	SmeMsgWriter *writer;
	SmeJobSource js;

	writer = (SmeMsgWriter *) mdsl_alloc(sizeof(SmeMsgWriter));

	mdsl_rc_init(writer);
	
	writer_job_queue_init(writer->job_queue); 

	sme_channel_ref(channel);
	writer->channel = channel;
	js.source_ptr = writer;
	js.notify = sme_msg_writer_notify_fn;
	sme_channel_set_write_source(channel, js);	

	return writer;
}

void sme_msg_writer_add_msg(SmeMsgWriter *writer, MmcMsg *msg)
{
	size_t len = ssc_msg_count(msg);
	size_t n_blocks;

	WriterJob new_job;
	SscMBlock *iov;

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
	sme_channel_add_write_job(writer->channel, iov, n_blocks + 1);

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


struct _SmeMsgReader
{
	MdslRC parent;


	ReaderState state;
	SmeChannel *channel;

	MmcMsg *msg_buf;

	SmeMsgReaderNotify notify;
	
	uint32_t d;
	void *layout;
	uint32_t layout_size;
	MmcMsg *msg;
};

//
static void sme_msg_reader_goto_read_size(SmeMsgReader *reader)
{
	SscMBlock iov = {&(reader->d), sizeof(uint32_t)};
	reader->state = READER_READ_SIZE;
	sme_channel_add_read_job(reader->channel, &iov, 1);
}

static void sme_msg_reader_advance(SmeMsgReader *reader)
{
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
		sme_channel_add_read_job(reader->channel, &iov, 1);
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
			sme_assert(! reader->msg_buf, "Message buffer overflow");
			reader->msg_buf = msg;
			sme_msg_reader_goto_read_size(reader);
		}
		else
		{
			//State change
			reader->msg = msg;
			reader->state = READER_READ_MSG;
			
			//Add job
			sme_channel_add_read_job(reader->channel, iov, n_blocks);
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
		sme_assert(! reader->msg_buf, "Message buffer overflow");
		reader->msg_buf = msg;
		sme_msg_reader_goto_read_size(reader);
	}

	return;
fail:
	reader->state = READER_ERROR;
}

static void sme_msg_reader_notify_fn(void *source_ptr, int n_jobs)
{
	SmeMsgReader *reader = source_ptr;

	sme_assert(n_jobs == 1, "Unexpected completion of %d jobs", n_jobs);

	sme_msg_reader_advance(reader);

	if (reader->msg_buf)
	{
		MmcMsg *msg_tmp = reader->msg_buf;
		reader->msg_buf = NULL;

		(* reader->notify.call)(msg_tmp, reader->notify.data);
	}
}

//
mdsl_rc_define(SmeMsgReader, sme_msg_reader);

static void sme_msg_reader_destroy(SmeMsgReader *reader)
{
	//Unref the channel
	sme_channel_unset_read_source(reader->channel);	
	sme_channel_unref(reader->channel);
	reader->channel = NULL;

	//Destroy message in the buffer pointer
	if (reader->msg_buf)
		mmc_msg_unref(reader->msg_buf);
	reader->msg_buf = NULL;
	
	if (reader->layout)
		free(reader->layout);
	if (reader->msg)
		mmc_msg_unref(reader->msg);

	free(reader);
}

SmeMsgReader *sme_msg_reader_new
	(SmeChannel *channel, SmeMsgReaderNotify notify)
{
	SmeJobSource js;
	SmeMsgReader *reader;

	reader = (SmeMsgReader *) mdsl_alloc(sizeof(SmeMsgReader));

	mdsl_rc_init(reader);

	reader->msg_buf = NULL;
	reader->layout = NULL;
	reader->msg = NULL;
	reader->notify = notify;

	sme_channel_ref(channel);
	reader->channel = channel;

	js.source_ptr = reader;
	js.notify = sme_msg_reader_notify_fn;
	sme_channel_set_read_source(reader->channel, js);	

	sme_msg_reader_goto_read_size(reader);

	return reader;
}
