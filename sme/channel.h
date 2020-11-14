/* iface.h
 * Interface between message [de]serializers and channel handlers
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
