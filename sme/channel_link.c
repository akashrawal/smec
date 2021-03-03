/* channel_link.c
 * A link implementation that uses a channel to send and receive messages.
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

struct _SmeChannelLink
{
	SmeLink parent;

	SmeChannel *channel;
	SmeMsgReader *reader;
	SmeMsgWriter *writer;
};

static void sme_channel_link_notify_msg_cb(MmcMsg *msg, void *data)
{
	SmeChannelLink *link = data;

	sme_link_receive((SmeLink*) link, msg);
}

static void sme_channel_link_send(SmeLink *base_type, MmcMsg *msg)
{
	SmeChannelLink *link = (SmeChannelLink*) base_type;

	sme_msg_writer_add_msg(link->writer, msg);
}

static void sme_channel_link_destroy(SmeLink *base_type)
{
	SmeChannelLink *link = (SmeChannelLink*) base_type;

	sme_channel_unref(link->channel);
	sme_msg_reader_unref(link->reader);
	sme_msg_writer_unref(link->writer);

	sme_link_cleanup(base_type);

	mdsl_free(link);
}

SmeChannelLink *sme_channel_link_new(SmeChannel *channel)
{
	SmeChannelLink *link =  mdsl_alloc(sizeof(SmeChannelLink));

	//Initialize parent class
	sme_link_init((SmeLink*) link);
	link->parent.destroy = sme_channel_link_destroy;
	link->parent.send = sme_channel_link_send;

	sme_channel_ref(channel);
	link->channel = channel;
	SmeMsgReaderNotify notify = { sme_channel_link_notify_msg_cb, link};
	link->reader = sme_msg_reader_new(link->channel, notify);
	link->writer = sme_msg_writer_new(link->channel);

	return link;
}
