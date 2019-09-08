/* test_channel.c
 * Unit test for channel
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

#include <sme/incl.h>
#include <stdio.h>
#include <inttypes.h>

#define assert_equals_int(_a, _b) \
	do {\
		int64_t a = (_a); \
		int64_t b = (_b); \
		if (a != b) \
		{ \
			sme_error("Assertion failed a == b (a = %" PRId64 ", " \
					"b = %" PRId64 ")", \
					a, b); \
		} \
	} while (0)

/*
 * The intention here is to generate programs for all possible sequences of 
 * function calls and then execute programs and verify behavior.
 */

#define MAX_IO 6
#define MAX_DEPTH 5

#define MAX_IOV  MAX_DEPTH

typedef enum
{
	INST_ADD_SEG,
	INST_COMMIT_JOB,
	INST_IO
} Opcode;

typedef struct
{
	int size;
} AddSegData;


typedef struct 
{
	int io_output;
} IOData;

typedef struct
{
	int parent;
	Opcode opcode;
	union {
		AddSegData add_seg_data;
		IOData io_data;
	} payload;
} Instruction;

typedef struct
{
	int idx;
	int io_queued;
	int io_actual;
} Program;

mdsl_declare_array(Instruction, InstructionArray, instruction_array);
mdsl_declare_array(Program, ProgramArray, program_array);
mdsl_declare_array(int64_t, Int64Array, int64_array);

InstructionArray instruction_array[1];
ProgramArray program_array[1];

//Recursive program generator algorithm
static void generate_programs_rec(int parent, int io_avail, int depth_avail,
		int blk_ready, int io_ready, int io_commited)
{
	int i;
	Instruction inst;

	if (depth_avail > 0)
	{
		//add_seg
		memset(&inst, 0, sizeof(inst));
		inst.opcode = INST_ADD_SEG;
		for (i = 0; i <= io_avail; i++)
		{
			inst.parent = parent;
			inst.payload.add_seg_data.size = i;
			instruction_array_append(instruction_array, inst);
			generate_programs_rec
				(instruction_array_size(instruction_array) - 1,
				io_avail - i, depth_avail - 1, 
				blk_ready + 1, io_ready + i, io_commited);			
		}

		//commit_job
		memset(&inst, 0, sizeof(inst));
		inst.opcode = INST_COMMIT_JOB;
		inst.parent = parent;
		instruction_array_append(instruction_array, inst);
		generate_programs_rec
			(instruction_array_size(instruction_array) - 1, 
			io_avail, depth_avail - 1,
			0, 0, io_ready + io_commited);

		//io
		if (blk_ready == 0)
		{
			memset(&inst, 0, sizeof(inst));
			inst.opcode = INST_IO;
			for (i = 0; i <= io_commited; i++)
			{
				inst.parent = parent;
				inst.payload.add_seg_data.size = i;
				instruction_array_append(instruction_array, inst);
				generate_programs_rec
					(instruction_array_size(instruction_array) - 1,
					io_avail, depth_avail - 1, 
					blk_ready, io_ready, io_commited - i);			
			}
		}
	}

	if (parent > 1)
	{
		if (blk_ready == 0)
		{
			Program prg;
			prg.idx = parent;
			prg.io_queued = MAX_IO - io_avail;
			prg.io_actual = MAX_IO - io_avail - io_commited;

			program_array_append(program_array, prg);
		}
	}
}

//Mock job source
typedef struct
{
	SmeChannel channel;
	SmeJobSource iface;
	int active;
	int submitted_jobs;
	int completed_jobs;
	int used_blk;
	SscMBlock blk[MAX_IOV];

	int jobsizes[MAX_DEPTH + 1];
	int iosize;
} MockJobSource;

static void mock_job_source_set_channel(void *jsptr, SmeChannel channel)
{
	MockJobSource *jobsource = (MockJobSource *) jsptr;

	jobsource->channel = channel;
	jobsource->active = 1;
}

static void mock_job_source_unset_channel(void *jsptr)
{
	MockJobSource *jobsource = (MockJobSource *) jsptr;
	jobsource->active = 0;
}

static void mock_job_source_notify(void *jsptr, int n_jobs)
{
	MockJobSource *jobsource = (MockJobSource *) jsptr;

	jobsource->completed_jobs += n_jobs;
}

static void mock_job_source_init(MockJobSource *jobsource)
{
	jobsource->iface.source_ptr = jobsource;
	jobsource->iface.set_channel = mock_job_source_set_channel;
	jobsource->iface.unset_channel = mock_job_source_unset_channel;
	jobsource->iface.notify = mock_job_source_notify;

	jobsource->active = 0;
	jobsource->submitted_jobs = 0;
	jobsource->completed_jobs = 0;
	jobsource->used_blk = 0;

	jobsource->iosize = 0;
	int i;
	for (i = 0; i <= MAX_DEPTH; i++)
		jobsource->jobsizes[i] = 0;
}

static void mock_job_source_add_blk(MockJobSource *jobsource,
		char* mem, int size)
{
	jobsource->blk[jobsource->used_blk].mem = mem;
	jobsource->blk[jobsource->used_blk].len = size;
	jobsource->used_blk++;
	jobsource->jobsizes[jobsource->submitted_jobs] += size;
}

static void mock_job_source_add_job(MockJobSource *jobsource)
{
	SscMBlock *blocks = jobsource->blk;
	int n_blocks = jobsource->used_blk;
	if (! n_blocks)
		blocks = NULL;
	(*jobsource->channel.add_job)(jobsource->channel.channel_ptr,
			blocks, n_blocks);

	jobsource->used_blk = 0;
	jobsource->submitted_jobs++;
}

static void mock_job_source_inform_io(MockJobSource *jobsource, int size)
{
	jobsource->iosize += size;

	int extra = jobsource->iosize;
	int i;
	for (i = 0; i < jobsource->submitted_jobs; i++)
	{
		if (extra < jobsource->jobsizes[i])
			break;
		extra -= jobsource->jobsizes[i];
	}

	assert_equals_int(i, jobsource->completed_jobs);

	if (i == jobsource->submitted_jobs)
		assert_equals_int(extra, 0);
}

//Mock IO functions
const char *data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

int mock_io_size;
int mock_bytes_read;
int mock_bytes_written;

void mock_io_init()
{
	mock_io_size = -1;
	
	mock_bytes_read = 0;
	mock_bytes_written = 0;
}

ssize_t mock_io_readv(int fd, struct iovec *iov, size_t iov_len)
{
	int i, j, k;
	k = 0;
	for (i = 0; i < iov_len; i++)
	{
		for (j = 0; j < iov[i].iov_len; j++)
		{
			if (k >= mock_io_size)
				break;
			((char *) iov[i].iov_base)[j] = data[mock_bytes_read + k];
			k++;
		}
	}

	mock_bytes_read += k;
	mock_io_size -= k;
	return k;
}

ssize_t mock_io_writev(int fd, struct iovec *iov, size_t iov_len)
{
	int i, j, k;
	k = 0;
	for (i = 0; i < iov_len; i++)
	{
		for (j = 0; j < iov[i].iov_len; j++)
		{
			if (k >= mock_io_size)
				break;
			assert_equals_int(((char *) iov[i].iov_base)[j],
					data[mock_bytes_written + k]);
			k++;
		}
	}

	mock_bytes_written += k;
	mock_io_size -= k;
	return k;
}

int io_used;
char read_buf[MAX_IO];

MockJobSource read_source[1], write_source[1];

SmeFdChannel *channel;


static void start_test_case()
{
	io_used = 0;
	memset(read_buf, 0, MAX_IO);

	mock_io_init();
	mock_job_source_init(read_source);
	mock_job_source_init(write_source);

	channel = sme_fd_channel_new(-1);
	sme_fd_channel_set_write_source(channel, write_source->iface);
	sme_fd_channel_set_read_source(channel, read_source->iface);
}

static void op_add_seg(int size)
{
	//Write: 
	mock_job_source_add_blk(write_source, (char *) data + io_used, size);

	//Read:
	mock_job_source_add_blk(read_source, read_buf + io_used, size);

	//Common
	io_used += size;
}

static void op_commit_job()
{
	//Write:
	mock_job_source_add_job(write_source);

	//Read:
	mock_job_source_add_job(read_source);

}

static void op_io(int size)
{
	//Write:
	mock_io_size = size;
	sme_fd_channel_test_write(channel, mock_io_writev);
	sme_fd_channel_inform_write_completion(channel);
	assert_equals_int(mock_io_size, 0);
	mock_job_source_inform_io(write_source, size);
	

	//Read:
	mock_io_size = size;
	sme_fd_channel_test_read(channel, mock_io_readv);
	sme_fd_channel_inform_read_completion(channel);
	assert_equals_int(mock_io_size, 0);
	mock_job_source_inform_io(read_source, size);
}

static void end_test_case(int io_size)
{
#if 0
	//Write:
	assert_equals_int(write_source->completed_jobs,
			write_source->submitted_jobs);

	//Read:
	assert_equals_int(read_source->completed_jobs,
			read_source->submitted_jobs);
#endif

	int i;

	for (i = 0; i < io_size; i++)
	{
		if (read_buf[i] != data[i])
		{
			sme_error("Assertion failure: read_buf[%d] == data[%d], "
					"lhs = %d, rhs = %d", 
					i, i, (int) read_buf[i], (int) data[i]);
		}
	}

	sme_fd_channel_unref(channel);

	assert_equals_int(write_source->active, 0);
	assert_equals_int(read_source->active, 0);
}

int main()
{
	instruction_array_init(instruction_array);
	program_array_init(program_array);

	generate_programs_rec(-1, MAX_IO, MAX_DEPTH, 0, 0, 0);

	fprintf(stderr, 
			"Program count: %zu\n", program_array_size(program_array));

	int i;
	Int64Array indexes[1];
	for (i = 0; i < program_array_size(program_array); i++)
	{
		int64_array_init(indexes);

		Program prg = program_array->data[i];
		int j;
		for (j = prg.idx; j >= 0; j = instruction_array->data[j].parent)
		{
			int64_array_append(indexes, j);
		}

		//Print program
		/*
		fprintf(stderr, "program[%d]: ", i);
		for (j = int64_array_size(indexes) - 1; j >= 0; j--)
		{
			Instruction *inst = instruction_array->data + indexes->data[j];

			int arg = -1;
			if (inst->opcode == INST_ADD_SEG)
				arg = inst->payload.add_seg_data.size;
			else if (inst->opcode == INST_IO)
				arg = inst->payload.io_data.io_output;

			
			fprintf(stderr, "%d[%d] ", (int) inst->opcode, arg);
		}
		fprintf(stderr, "\n");
		*/

		//Actual testing
		start_test_case();
		for (j = int64_array_size(indexes) - 1; j >= 0; j--)
		{
			Instruction *inst = instruction_array->data + indexes->data[j];

			switch(inst->opcode)
			{
				case INST_ADD_SEG:
					op_add_seg(inst->payload.add_seg_data.size);
					break;
				case INST_COMMIT_JOB:
					op_commit_job();
					break;
				case INST_IO:
					op_io(inst->payload.io_data.io_output);
					break;
			}
		}
		end_test_case(prg.io_actual);

		free(indexes->data);
	}

	free(program_array->data);
	free(instruction_array->data);

	return 0;
}
