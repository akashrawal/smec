/* test_msg.c
 * Unit test for msg.c
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

#include <sme/sme.h>
#include <stdio.h>
#include <inttypes.h>

#define MAX_DATA 26
#define MAX_IO 100
#define MAX_SUBMSG 2
#define MAX_DEPTH 3
#define MAX_SEG 100

#define assert_equals_int(a, b) \
	do {\
		int64_t _a = (a); \
		int64_t _b = (b); \
		if (_a != _b) \
		{ \
			sme_error("Assertion failed a == b (a = %" PRId64 ", " \
					"b = %" PRId64 ")", \
					_a, _b); \
		} \
	} while (0)

void assert_equals_msg(MmcMsg* a, MmcMsg* b)
{
	assert_equals_int(a->mem_len, b->mem_len);
	if (a->mem_len > 0)
	{
		if (memcmp(a->mem, b->mem, a->mem_len) != 0)
			sme_error("Unequal memory blocks");
	}

	int i;
	assert_equals_int(a->submsgs_len, b->submsgs_len);
	for (i = 0; i < a->submsgs_len; i++)
	{
		assert_equals_msg(a->submsgs[i], b->submsgs[i]);
	}
}

//Mock loopback channel
typedef struct
{
	SscMBlock block;
	int compl;
} ReadSeg;

typedef struct 
{
	int rmark, wmark;
	char data[MAX_IO];

	int rcomp, wcomp;

	ReadSeg read_segs[MAX_SEG];
	int n_read_segs;

	SmeChannel w_if, r_if;
	SmeJobSource w_js, r_js;
} LoopChannel;

void loop_channel_add_write_job(void *ptr, SscMBlock *blocks, size_t n_blocks)
{
	LoopChannel *ch = (LoopChannel*) ptr;	

	int i, j;
	for (i = 0; i < n_blocks; i++)
	{
		if (ch->wmark + blocks[i].len > MAX_IO)
		{
			sme_error("Assertion fail: ch->wmark + blocks[%d].len <= MAX_IO, "
					"ch->wmark = %d, blocks[i].len = %d, n_blocks=%zu", 
					i, (int) ch->wmark, (int) blocks[i].len,
					n_blocks);
		}

		for (j = 0; j < blocks[i].len; j++)
		{
			ch->data[ch->wmark++] = ((char*) blocks[i].mem)[j];
		}
	}

	ch->wcomp++;
}

void loop_channel_add_read_job(void *ptr, SscMBlock *blocks, size_t n_blocks)
{
	LoopChannel *ch = (LoopChannel*) ptr;	

	int i;
	ReadSeg* jobseg = ch->read_segs + ch->n_read_segs;
	for (i = 0; i < n_blocks; i++)
	{
		jobseg[i].block = blocks[i];
		jobseg[i].compl = 0;


	}
	jobseg[n_blocks].compl = 1;
	ch->n_read_segs += n_blocks + 1;
}

void loop_channel_do_read(LoopChannel *ch)
{
	int i, j;
	for (i = 0; i < ch->n_read_segs; i++)
	{
		if (ch->read_segs[i].compl)
		{
			ch->rcomp ++;
			continue;
		}

		if (ch->rmark + ch->read_segs[i].block.len > ch->wmark)
		{
			sme_error("Assertion fail: "
					"ch->rmark + blocks[%d].len <= ch->wmark, "
					"ch->rmark = %d, ch->read_segs[i].block.len = %d, "
					"ch->wmark = %d", 
					i, (int) ch->rmark, (int) ch->read_segs[i].block.len, 
					(int) ch->wmark);
		}

		for (j = 0; j < ch->read_segs[i].block.len; j++)
		{
			((char*) ch->read_segs[i].block.mem)[j] = ch->data[ch->rmark++];
		}
	}

	ch->n_read_segs = 0;

}

void loop_channel_notify(LoopChannel *ch)
{
	if (ch->wcomp)
	{
		(* ch->w_js.notify)(ch->w_js.source_ptr, ch->wcomp);
		ch->wcomp = 0;
	}
	if (ch->rcomp)
	{
		(* ch->r_js.notify)(ch->r_js.source_ptr, ch->rcomp);
		ch->rcomp = 0;
	}
}

void loop_channel_init(LoopChannel *ch, SmeJobSource w_js, SmeJobSource r_js)
{
	ch->rmark = ch->wmark = 0;
	ch->rcomp = ch->wcomp = 0;

	ch->n_read_segs = 0;

	ch->w_js = w_js;	
	ch->w_if.channel_ptr = ch;
	ch->w_if.add_job = loop_channel_add_write_job;
	(* ch->w_js.set_channel)(ch->w_js.source_ptr, ch->w_if);

	ch->r_js = r_js;	
	ch->r_if.channel_ptr = ch;
	ch->r_if.add_job = loop_channel_add_read_job;
	(* ch->r_js.set_channel)(ch->r_js.source_ptr, ch->r_if);
}

void loop_channel_destroy(LoopChannel *ch)
{
	(*ch->w_js.unset_channel)(ch->w_js.source_ptr);
	(*ch->r_js.unset_channel)(ch->r_js.source_ptr);
}

//Test notify function
typedef struct {
	SmeMsgReaderNotify parent;
	MmcMsg *buf;
} TestReaderNotify;

void test_reader_notify_call(MmcMsg *msg, void *data)
{
	TestReaderNotify *self = data;

	sme_assert(!self->buf, "Capacity exceeded");
	self->buf = msg;
}

void test_reader_notify_init(TestReaderNotify *self)
{
	self->parent.call = test_reader_notify_call;
	self->parent.data = self;
	self->buf = NULL;
}

//Test case function
void testcase(MmcMsg *msg)
{
	TestReaderNotify recvd[1];
	test_reader_notify_init(recvd);

	SmeMsgWriter *writer = sme_msg_writer_new();
	SmeMsgReader *reader = sme_msg_reader_new(recvd->parent);

	LoopChannel channel[1];

	loop_channel_init(channel, 
			sme_msg_writer_get_source(writer),
			sme_msg_reader_get_source(reader));

	sme_msg_writer_add_msg(writer, msg);
	loop_channel_notify(channel);
	assert_equals_int(sme_msg_writer_get_queue_len(writer), 0);

	int i;
	for (i = 0; i < 15; i++)
	{
		if (recvd->buf)
			break;

		loop_channel_do_read(channel);
		loop_channel_notify(channel);
	}
	if (i == 15)
		sme_error("Cannot finish reading message");

	assert_equals_msg(msg, recvd->buf);

	mmc_msg_unref(recvd->buf);

	loop_channel_destroy(channel);

	sme_msg_reader_unref(reader);
	sme_msg_writer_unref(writer);
}

//Message generator

typedef struct 
{
	int n_bytes;
	int n_submsg;
	int submsg_id[MAX_SUBMSG];
} MsgSpec;

mdsl_declare_array(MsgSpec, MsgSpecArray, msg_spec_array);

MsgSpecArray msg_spec_array[1];

void create_msg_spec_primary(int max_bytes)
{
	int i;
	MsgSpec el;
	memset(&el, 0, sizeof(el));

	el.n_submsg = 0;
	for (i = 0; i <= max_bytes; i++)
	{
		el.n_bytes = i;
		msg_spec_array_append(msg_spec_array, el);
	}
}

int create_msg_spec_secondary(int choice_start, int max_bytes)
{
	int i, j, k;
	MsgSpec el;
	memset(&el, 0, sizeof(el));

	int submsg_choice = msg_spec_array_size(msg_spec_array);

	//Refill the array
	for (i = 0; i <= max_bytes; i++)
	{
		el.n_bytes = i;
		el.n_submsg = 0;
		msg_spec_array_append(msg_spec_array, el);

		for (j = choice_start; j < submsg_choice; j++)
		{
			el.n_submsg = 1;
			el.submsg_id[0] = j;
			msg_spec_array_append(msg_spec_array, el);

			el.n_submsg = 2;
			for (k = choice_start; k < submsg_choice; k++)
			{
				el.submsg_id[1] = k;
				msg_spec_array_append(msg_spec_array, el);
			}
		}
	}

	return submsg_choice;
}

const char *data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
MmcMsg* create_msg_rec(int offset, int* byte_offset)
{
	MsgSpec el = msg_spec_array->data[offset];

	MmcMsg* res = mmc_msg_newa(el.n_bytes, el.n_submsg);

	memcpy(res->mem, data + (*byte_offset), el.n_bytes);
	(*byte_offset) += el.n_bytes;
	if (*byte_offset > MAX_DATA)
	{
		sme_error("Assertion failure: *byte_offset <= MAX_DATA, "
				"*byte_offset= %d", *byte_offset);
	}

	int i;
	for (i = 0; i < el.n_submsg; i++)
	{
		res->submsgs[i] = create_msg_rec(el.submsg_id[i], byte_offset);
	}

	return res;
}

MmcMsg* create_msg(int offset)
{
	int byte_offset = 0;
	return create_msg_rec(offset, &byte_offset);
}

int main()
{
	msg_spec_array_init(msg_spec_array);

	create_msg_spec_primary(2);
	int choice_start = create_msg_spec_secondary(0, 2);
	choice_start = create_msg_spec_secondary(choice_start, 2);

	printf("choice_start = %d\n", choice_start);
	printf("Message count: %d\n", 
			(int) (msg_spec_array_size(msg_spec_array) - choice_start));

	int i;
	for (i = choice_start; i < msg_spec_array_size(msg_spec_array); i++)
	{
		MmcMsg *msg = create_msg(i);

		testcase(msg);

		mmc_msg_unref(msg);
	}

	free(msg_spec_array->data);

	return 0;
}
