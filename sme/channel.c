/* channel.c
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

#include "incl.h"
#include <string.h>

//Reference counting

mdsl_rc_define(SmeChannel, sme_channel);

void sme_channel_cleanup(SmeChannel *channel)
{

}

void sme_channel_init(SmeChannel* channel)
{
	memset(channel, 0, sizeof(SmeChannel));
	mdsl_rc_init(channel);

	channel->destroy = sme_channel_cleanup;
}

static void sme_channel_destroy(SmeChannel* channel)
{
	(* channel->destroy)(channel);
}
