 /* fd_channel.h
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


//Implementation for file descriptor backend using libev
typedef struct _SmeFdChannel SmeFdChannel;


SmeFdChannel *sme_fd_channel_new(int fd);


//Writing

int sme_fd_channel_get_write_queue_len(SmeFdChannel *channel);

//Reading

int sme_fd_channel_get_read_queue_len(SmeFdChannel *channel);

//Testing
#ifndef SME_PUBLIC_HEADER

typedef ssize_t (*SmeVectorIOFn)(int fd, struct iovec *iov, size_t iov_len);

ssize_t sme_fd_channel_test_write(SmeFdChannel *channel, SmeVectorIOFn fn);

ssize_t sme_fd_channel_test_read(SmeFdChannel *channel, SmeVectorIOFn fn);


#endif //SME_PUBLIC_HEADER
