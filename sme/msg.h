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

//Interface channel should present to job source
typedef struct 
{
	void *channel_ptr;
	void (*add_job)(void *channel_ptr, SscMBlock *blocks, size_t n_blocks);
} SmeChannel;

//Interface a job source should present to channel.
typedef struct
{
	void *source_ptr;

	void (*set_channel)(void *source_ptr, SmeChannel channel);
	void (*unset_channel)(void *source_ptr);
	void (*notify)(void *source_ptr, int n_jobs);
} SmeJobSource;

//Uses protocol as described in ssc/msg.h

//Message writer
typedef struct _SmeMsgWriter SmeMsgWriter;

mmc_rc_declare(SmeMsgWriter, sme_msg_writer);

SmeMsgWriter *sme_msg_writer_new();

SmeJobSource sme_msg_writer_get_source(SmeMsgWriter *writer);

void sme_msg_writer_add_msg(SmeMsgWriter *writer, MmcMsg *msg);

int sme_msg_writer_get_queue_len(SmeMsgWriter *writer);


//Message reader
typedef struct _SmeMsgReader SmeMsgReader;

mmc_rc_declare(SmeMsgReader, sme_msg_reader);

SmeMsgReader *sme_msg_reader_new();

SmeJobSource sme_msg_reader_get_source(SmeMsgReader *reader);

int sme_msg_reader_get_queue_len(SmeMsgReader *reader);

MmcMsg *sme_msg_reader_pop_msg(SmeMsgReader *reader);
