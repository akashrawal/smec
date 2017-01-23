/* msg.h
 * [De]Serialization of messages on streams
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

typedef struct
{
	MmcMsg *msg;
	uint32_t *preamble;
} Job;

mmc_declare_queue(Job, JobQueue, job_queue);

typedef enum
{
	INACTIVE, 
	ACTIVE, 
	DISPOSED
} ObjectState;

//Message writer
struct _SmeMsgWriter
{
	MmcRC parent;

	ObjectState state;
	SmeChannel channel;
	JobQueue job_queue[1];
};

static void sme_msg_writer_set_channel_fn
	(void *source_ptr, SmeChannel channel)
{
	SmeMsgWriter *writer = source_ptr;

	if (writer->state != INACTIVE)
		sme_error("Cannot set channel more than once");

	sme_msg_writer_ref(writer); //Reference is owned by the channel. 
	writer->state = ACTIVE;
	writer->channel = channel;
}

static void sme_msg_writer_unset_channel_fn(void *source_ptr)
{
	SmeMsgWriter *writer = source_ptr;

	if (writer->state == ACTIVE)
	{
		writer->state = DISPOSED;
		sme_msg_writer_unref(writer);
	}
	else if (writer->state == INACTIVE) 
		sme_error("Cannot unset channel when no channel is set");
	else 
		sme_error("Cannot unset channel more than one");
}

static void sme_msg_writer_notify_fn(void *source_ptr, int n_jobs)
{
	SmeMsgWriter *writer = source_ptr;

	while (n_jobs--)
	{
		Job one_job = job_queue_pop(writer->job_queue);	
		mmc_msg_unref(one_job.msg);
		free(one_job.preamble);
	}
}

//
mmc_rc_define(SmeMsgWriter, sme_msg_writer);

static void sme_msg_writer_destroy(SmeMsgWriter *writer)
{
	free(writer);
}

SmeMsgWriter *sme_msg_writer_new()
{
	SmeMsgWriter *writer;

	writer = (SmeMsgWriter *) mmc_alloc(sizeof(SmeMsgWriter));

	mmc_rc_init(writer);
	
	writer->active = 0;
	job_queue_init(writer->job_queue);

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

	Job new_job;
	SscMBlock *iov;

	if (! writer->active)
		sme_error("Associate with channel before adding messages. ");

	//Allocate
	new_job.msg = msg;
	mmc_msg_ref(msg);
	new_job.preamble = (uint32_t *) mmc_alloc
		((len + 1) * sizeof(uint32_t));
	iov = (SscMBlock *) mmc_alloc(sizeof(SscMBlock) * (len + 1));

	//Setup job
	iov[0].mem = new_job.preamble;
	iov[0].len = (len + 1) * sizeof(uint32_t);
	new_job.preamble[0] = ssc_uint32_to_le((uint32_t) len);
	ssc_msg_create_layout(msg, len, new_job.preamble + 1, iov + 1);	

	//Add job
	(* writer->channel.add_job)
		(writer->channel.channel_ptr, iov, len + 1);

	//Free
	free(iov);
}

int sme_msg_writer_get_queue_len(SmeMsgWriter *writer)
{
	return job_queue_size(writer->job_queue);
}

