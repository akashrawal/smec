/* error.c
 * Error domain mechanism
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


//Error domain functions
char* sme_error_domain_strerror_as(SmeErrorDomain *domain, int code)
{
	return (* domain->strerror_as)(domain, code);
}

char* sme_error_domain_strerror_a(SmeErrorDomain *domain, int code)
{
	char* res = (* domain->strerror_as)(domain, code);
	if (! res)
	{
		sme_error("Error domain %s: Allocation failed while "
				"getting message for code %d", domain->domain_name, code);
	}
	return res;
}

int sme_error_domain_write
	(SmeErrorDomain *domain, int code, FILE *stream);
void sme_error_domain_perror
	(SmeErrorDomain *domain, int code, const char* message);

