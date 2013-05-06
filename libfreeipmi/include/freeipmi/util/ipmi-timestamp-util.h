/*
 * Copyright (C) 2003-2013 FreeIPMI Core Team
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#ifndef IPMI_TIMESTAMP_UTIL_H
#define IPMI_TIMESTAMP_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define IPMI_TIMESTAMP_FLAG_DEFAULT         0x00
/* Special string abbreviations
 * "Unspecified" will be abbreviated "Unspec."
 * "Post-Init X s" will be abbreviated to just "PostInit"
 */
#define IPMI_TIMESTAMP_FLAG_ABBREVIATE      0x01

/* Handles special case timestamps in IPMI, most notably unspecified
 * timestamps and timestamps in the range of initialization of the
 * BMC.  In those respective cases, an appropriate string will
 * written into the buffer instead of the actual time/date.
 */

/* format uses format from strftime(), pass NULL for default one */
int ipmi_timestamp_string (uint32_t timestamp,
			   unsigned int flags,
			   const char *format,
			   char *buf,
			   unsigned int buflen);

#ifdef __cplusplus
}
#endif

#endif /* IPMI_TIMESTAMP_UTIL_H */
