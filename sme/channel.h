 /* channel.h
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

//Function prototype for assigning jobs
typedef void (*SmeChannelJobFn)
	(void *channel, struct iovec *blocks, size_t n_blocks);

//Function prototype for notifying job completion
typedef void (*SmeChannelCompletionFn)(void *receiver, int n_jobs);

//Implementation for file descriptor backend using libevent
typedef struct _SmeFdChannel SmeFdChannel;

mmc_rc_declare(SmeFdChannel, sme_fd_channel);

SmeFdChannel *sme_fd_channel_new(int fd);


//Writing
void sme_fd_channel_set_write_receiver
	(SmeFdChannel *channel, void *receiver, SmeChannelCompletionFn comp_fn);

void sme_fd_channel_add_write_job
	(SmeFdChannel *channel, struct iovec *blocks, size_t n_blocks);

ssize_t sme_fd_channel_write(SmeFdChannel *channel);

void sme_fd_channel_inform_write_completion(SmeFdChannel *channel);

//Reading
void sme_fd_channel_set_read_receiver
	(SmeFdChannel *channel, void *receiver, SmeChannelCompletionFn comp_fn);

void sme_fd_channel_add_read_job
	(SmeFdChannel *channel, struct iovec *blocks, size_t n_blocks);

ssize_t sme_fd_channel_read(SmeFdChannel *channel);

void sme_fd_channel_inform_read_completion(SmeFdChannel *channel);


