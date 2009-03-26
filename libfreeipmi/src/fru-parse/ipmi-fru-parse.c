/*
  Copyright (C) 2003-2009 FreeIPMI Core Team

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#if STDC_HEADERS
#include <string.h>
#endif /* STDC_HEADERS */
#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <assert.h>
#include <errno.h>

#include "freeipmi/fru-parse/ipmi-fru-parse.h"

#include "freeipmi/debug/ipmi-debug.h"
#include "freeipmi/fiid/fiid.h"

#include "ipmi-fru-parse-defs.h"
#include "ipmi-fru-parse-trace.h"
#include "ipmi-fru-parse-util.h"

#include "libcommon/ipmi-fiid-util.h"

#include "freeipmi-portability.h"
#include "debug-util.h"

static char *ipmi_fru_parse_errmsgs[] =
  {
    "success",
    "context null",
    "context invalid",
    "invalid parameters",
    "out of memory",
    "internal system error",
    "internal error",
    "errnum out of range",
    NULL
  };

ipmi_fru_parse_ctx_t
ipmi_fru_parse_ctx_create (void)
{
  struct ipmi_fru_parse_ctx *ctx = NULL;

  if (!(ctx = (ipmi_fru_parse_ctx_t)malloc (sizeof (struct ipmi_fru_parse_ctx))))
    {
      ERRNO_TRACE (errno);
      return (NULL);
    }
  memset (ctx, '\0', sizeof (struct ipmi_fru_parse_ctx));
  ctx->magic = IPMI_FRU_PARSE_CTX_MAGIC;
  ctx->flags = IPMI_FRU_PARSE_FLAGS_DEFAULT;

  return (ctx);
}

void
ipmi_fru_parse_ctx_destroy (ipmi_fru_parse_ctx_t ctx)
{
  if (!ctx || ctx->magic != IPMI_FRU_PARSE_CTX_MAGIC)
    return;

  ctx->magic = ~IPMI_FRU_PARSE_CTX_MAGIC;
  free (ctx);
}

int
ipmi_fru_parse_ctx_errnum (ipmi_fru_parse_ctx_t ctx)
{
  if (!ctx)
    return (IPMI_FRU_PARSE_ERR_CONTEXT_NULL);
  else if (ctx->magic != IPMI_FRU_PARSE_CTX_MAGIC)
    return (IPMI_FRU_PARSE_ERR_CONTEXT_INVALID);
  else
    return (ctx->errnum);
}

char *
ipmi_fru_parse_ctx_strerror (int errnum)
{
  if (errnum >= IPMI_FRU_PARSE_ERR_SUCCESS && errnum <= IPMI_FRU_PARSE_ERR_ERRNUMRANGE)
    return (ipmi_fru_parse_errmsgs[errnum]);
  else
    return (ipmi_fru_parse_errmsgs[IPMI_FRU_PARSE_ERR_ERRNUMRANGE]);
}

char *
ipmi_fru_parse_ctx_errormsg (ipmi_fru_parse_ctx_t ctx)
{
  return (ipmi_fru_parse_ctx_strerror (ipmi_fru_parse_ctx_errnum (ctx)));
}

int
ipmi_fru_parse_ctx_get_flags (ipmi_fru_parse_ctx_t ctx, unsigned int *flags)
{
  if (!ctx || ctx->magic != IPMI_FRU_PARSE_CTX_MAGIC)
    {
      ERR_TRACE (ipmi_fru_parse_ctx_errormsg (ctx), ipmi_fru_parse_ctx_errnum (ctx));
      return (-1);
    }

  if (!flags)
    {
      FRU_PARSE_SET_ERRNUM (ctx, IPMI_FRU_PARSE_ERR_PARAMETERS);
      return (-1);
    }

  *flags = ctx->flags;
  return (0);
}

int
ipmi_fru_parse_ctx_set_flags (ipmi_fru_parse_ctx_t ctx, unsigned int flags)
{
  if (!ctx || ctx->magic != IPMI_FRU_PARSE_CTX_MAGIC)
    {
      ERR_TRACE (ipmi_fru_parse_ctx_errormsg (ctx), ipmi_fru_parse_ctx_errnum (ctx));
      return (-1);
    }

  if (flags & ~IPMI_FRU_PARSE_FLAGS_MASK)
    {
      FRU_PARSE_SET_ERRNUM (ctx, IPMI_FRU_PARSE_ERR_PARAMETERS);
      return (-1);
    }

  ctx->flags = flags;
  return (0);
}
