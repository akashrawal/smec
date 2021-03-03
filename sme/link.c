/* link.c
 * Abstract link for sending and receiving messages.
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

mdsl_rc_define(SmeLink, sme_link);

void sme_link_receive(SmeLink *link, MmcMsg *msg)
{
	//TODO: implementation

	mmc_msg_unref(msg);
}

void sme_link_cleanup(SmeLink *link)
{

}

static void sme_link_destroy(SmeLink *link)
{
	(* link->destroy)(link);
}

void sme_link_init(SmeLink *link)
{
	mdsl_rc_init(link);

	link->destroy = sme_link_cleanup;
}

