/* incl.h
 * Includes
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

//TODO: Decide the includes in API headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <mmc/mmc.h>
#include <ssc/ssc.h>

//Include all modules in dependency-based order

#include "msg.h"
#include "channel.h"

#define sme_error(...) mmc_context_error("SME", __VA_ARGS__)
#define sme_warn(...) mmc_context_warn("SME", __VA_ARGS__) 
	//
