/* link.h
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

typedef struct _SmeLink SmeLink;


//Link base class
struct _SmeLink
{
	MdslRC parent;

	//TODO: Address space

	//Virtual functions
	void (*destroy) (SmeLink *link);
	void (*send) (SmeLink *link, MmcMsg *msg);
};

mdsl_rc_declare(SmeLink, sme_link);

static inline void sme_link_send(SmeLink *link, MmcMsg *msg)
{
	(* link->send)(link, msg);
}

void sme_link_receive(SmeLink *link, MmcMsg *msg);

void sme_link_cleanup(SmeLink *link);

void sme_link_init(SmeLink *link);


