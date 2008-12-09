/*****************************************************************************\
 *  $Id: ipmiconsole.c,v 1.53.4.3 2008-12-09 18:42:13 chu11 Exp $
 *****************************************************************************
 *  Copyright (C) 2007-2008 Lawrence Livermore National Security, LLC.
 *  Copyright (C) 2006-2007 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Albert Chu <chu11@llnl.gov>
 *  UCRL-CODE-221226
 *  
 *  This file is part of Ipmiconsole, a set of IPMI 2.0 SOL libraries
 *  and utilities.  For details, see http://www.llnl.gov/linux/.
 *  
 *  Ipmiconsole is free software; you can redistribute it and/or modify 
 *  it under the terms of the GNU General Public License as published by the 
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version.
 *  
 *  Ipmiconsole is distributed in the hope that it will be useful, but 
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 *  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License 
 *  for more details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with Ipmiconsole.  If not, see <http://www.gnu.org/licenses/>.
\*****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#if STDC_HEADERS
#include <string.h>
#endif /* STDC_HEADERS */
#include <stdint.h>
#include <sys/select.h>
#include <sys/types.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else  /* !TIME_WITH_SYS_TIME */
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else /* !HAVE_SYS_TIME_H */
#include <time.h>
#endif  /* !HAVE_SYS_TIME_H */
#endif /* !TIME_WITH_SYS_TIME */
#include <sys/resource.h>
#include <signal.h>
#include <termios.h>
#include <assert.h>
#include <errno.h>

#include <ipmiconsole.h>        /* lib ipmiconsole.h */
#include "ipmiconsole_.h"       /* tool ipmiconsole.h */
#include "ipmiconsole-argp.h"

#include "freeipmi-portability.h"
#include "error.h"
#include "secure.h"

static struct termios saved_tty;
static int raw_mode_set = 0;

#define IPMICONSOLE_BUFLEN 4096

static int
_set_mode_raw(void)
{
  struct termios tty;

  if (tcgetattr(STDIN_FILENO, &saved_tty) < 0)
    {
      perror("tcgetattr");
      return -1;
    }

  memcpy(&tty, &saved_tty, sizeof(struct termios));
  tty.c_iflag = 0;
  tty.c_oflag = 0;
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;
  tty.c_cflag &= ~PARENB;
  tty.c_cflag |= CLOCAL;
  tty.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 0;

  if (tcsetattr(STDIN_FILENO, TCSADRAIN, &tty) < 0)
    {
      perror("tcsetattr");
      return -1;
    }

  raw_mode_set++;
  return 0;
}

static int
_reset_mode(void)
{
  if (!raw_mode_set)
    return 0;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty) < 0)
    {
      perror("tcsetattr");
      return -1;
    }
  return 0;
}

static int
_stdin(ipmiconsole_ctx_t c,
       char escape_char,
       int fd, 
       char *buf,
       unsigned int buflen)
{
  static int last_char_escape = 0;
  char tbuf[IPMICONSOLE_BUFLEN];
  unsigned int tbuflen = 0;
  ssize_t n;
  int i;

  assert(c);
  assert(fd);
  assert(buf);
  assert(buflen);

  for (i = 0; i < buflen; i++)
    {
      if (last_char_escape)
	{
	  last_char_escape = 0;
	  if (buf[i] == '?')
	    {
	      printf("&? - this menu\r\n");
	      printf("&. - exit\r\n");
	      printf("&B - generate break\r\n");
	      printf("&D - send DEL character\r\n");
	      printf("&& - & character\r\n");
	    }
	  else if (buf[i] == '.')
	    {
	      if (tbuflen)
		{
		  n = write(fd, tbuf, tbuflen);
		  
		  /* Clear out data */
		  secure_memset(tbuf, '\0', IPMICONSOLE_BUFLEN);
		  
		  if (n < 0)
		    {
		      perror("write");
		      return -1;
		    }
		  if (n != tbuflen)
		    {
		      perror("write");
		      return -1;
		    }
		}
	      
	      /* b/c we're exitting */
	      return -1;
	    }
	  else if (buf[i] == 'B')
	    {
	      if (tbuflen)
		{
		  n = write(fd, tbuf, tbuflen);

		  /* Clear out data */
                  secure_memset(tbuf, '\0', IPMICONSOLE_BUFLEN);
		  
		  if (n < 0)
		    {
		      perror("write");
		      return -1;
		    }
		  
		  if (n != tbuflen)
		    {
		      perror("write");
		      return -1;
		    }
		}
	      tbuflen = 0;
	      
	      printf("[generate break]\r\n");
	      if (ipmiconsole_ctx_generate_break(c) < 0)
		{
		  fprintf(stderr, 
                          "ipmiconsole_ctx_generate_break: %s\r\n",
                          ipmiconsole_ctx_strerror(ipmiconsole_ctx_errnum(c)));
		  return -1;
		}
	    }
	  else if (buf[i] == 'D')
	    {
	      /* achu: Some keywords don't send DEL when you press
		 delete, they send some other funky crap. */
	      tbuf[tbuflen++] = 0x7F;
	    }
	  else if (buf[i] == escape_char)
	    tbuf[tbuflen++] = escape_char;
	  else
	    {
	      tbuf[tbuflen++] = escape_char;
	      tbuf[tbuflen++] = buf[i];
	    }
	}
      else if (buf[i] == escape_char)
        last_char_escape = 1;
      else
	tbuf[tbuflen++] = buf[i];
    }
  
  if (tbuflen)
    {
      n = write(fd, tbuf, tbuflen);
      
      /* Clear out data */
      secure_memset(tbuf, '\0', IPMICONSOLE_BUFLEN);

      if (n < 0)
	{
	  if (errno != EPIPE)
	    perror("write");
	  else
            {
              if (ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_SOL_STOLEN)
                printf("\r\n[%s]\r\n", 
                       ipmiconsole_ctx_strerror(ipmiconsole_ctx_errnum(c)));
              else
                printf("\r\n[error received]: %s\r\n", 
                       ipmiconsole_ctx_strerror(ipmiconsole_ctx_errnum(c)));
            }
	  return -1;
	}

      if (n != tbuflen)
	{
	  perror("write");
	  return -1;
	}
    }

  return 0;
}

int
main(int argc, char **argv)
{
  struct ipmiconsole_arguments cmd_args;
  struct ipmiconsole_ipmi_config ipmi_config;
  struct ipmiconsole_protocol_config protocol_config;
  struct ipmiconsole_engine_config engine_config;
  ipmiconsole_ctx_t c = NULL;
  int debug_flags = 0;
  int fd = -1;

  err_init(argv[0]);
  err_set_flags(ERROR_STDOUT);

  ipmiconsole_argp_parse(argc, argv, &cmd_args);

  if (cmd_args.common.debug)
    {
#ifndef NDEBUG
      if (cmd_args.debugfile)
	debug_flags |= IPMICONSOLE_DEBUG_FILE;
      else
	debug_flags |= IPMICONSOLE_DEBUG_STDERR;
#else
      debug_flags |= IPMICONSOLE_DEBUG_STDERR;
#endif /* NDEBUG */  
      debug_flags |= IPMICONSOLE_DEBUG_IPMI_PACKETS;
    }

  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    {
      /* Argh, it doesn't return an errno, oh well */
      perror("signal");
      exit(1);
    }

  if (ipmiconsole_engine_init(1, debug_flags) < 0)
    {
      perror("ipmiconsole_setup");
      exit(1);
    }

  /* convert config information to ipmiconsole configuration */

  memset(&ipmi_config, '\0', sizeof(struct ipmiconsole_ipmi_config));
  ipmi_config.username = cmd_args.common.username;
  ipmi_config.password = cmd_args.common.password;
  ipmi_config.k_g = cmd_args.common.k_g;
  ipmi_config.k_g_len = cmd_args.common.k_g_len;

  if (cmd_args.common.privilege_level == IPMI_PRIVILEGE_LEVEL_USER)
    ipmi_config.privilege_level = IPMICONSOLE_PRIVILEGE_USER;
  else if (cmd_args.common.privilege_level == IPMI_PRIVILEGE_LEVEL_OPERATOR)
    ipmi_config.privilege_level = IPMICONSOLE_PRIVILEGE_OPERATOR;
  else if (cmd_args.common.privilege_level == IPMI_PRIVILEGE_LEVEL_ADMIN)
    ipmi_config.privilege_level = IPMICONSOLE_PRIVILEGE_ADMIN;
  else
    err_exit("Config Error: Invalid privilege level");

  ipmi_config.cipher_suite_id = cmd_args.common.cipher_suite_id;

  if (cmd_args.common.workaround_flags & IPMI_TOOL_WORKAROUND_FLAGS_AUTHENTICATION_CAPABILITIES)
    ipmi_config.workaround_flags |= IPMICONSOLE_WORKAROUND_AUTHENTICATION_CAPABILITIES;
  if (cmd_args.common.workaround_flags & IPMI_TOOL_WORKAROUND_FLAGS_IGNORE_SOL_PAYLOAD_SIZE)
    ipmi_config.workaround_flags |= IPMICONSOLE_WORKAROUND_IGNORE_SOL_PAYLOAD_SIZE;
  if (cmd_args.common.workaround_flags & IPMI_TOOL_WORKAROUND_FLAGS_IGNORE_SOL_PORT)
    ipmi_config.workaround_flags |= IPMICONSOLE_WORKAROUND_IGNORE_SOL_PORT;
  if (cmd_args.common.workaround_flags & IPMI_TOOL_WORKAROUND_FLAGS_INTEL_2_0_SESSION)
    ipmi_config.workaround_flags |= IPMICONSOLE_WORKAROUND_INTEL_2_0_SESSION;
  if (cmd_args.common.workaround_flags & IPMI_TOOL_WORKAROUND_FLAGS_SUPERMICRO_2_0_SESSION)
    ipmi_config.workaround_flags |= IPMICONSOLE_WORKAROUND_SUPERMICRO_2_0_SESSION;
  if (cmd_args.common.workaround_flags & IPMI_TOOL_WORKAROUND_FLAGS_SUN_2_0_SESSION)
    ipmi_config.workaround_flags |= IPMICONSOLE_WORKAROUND_SUN_2_0_SESSION;

  memset(&protocol_config, '\0', sizeof(struct ipmiconsole_protocol_config));
  protocol_config.session_timeout_len = cmd_args.common.session_timeout; 
  protocol_config.retransmission_timeout_len = cmd_args.common.retransmission_timeout; 
  protocol_config.retransmission_backoff_count = -1; 
  protocol_config.keepalive_timeout_len = -1; 
  protocol_config.retransmission_keepalive_timeout_len = -1; 
  protocol_config.acceptable_packet_errors_count = -1; 
  protocol_config.maximum_retransmission_count = -1; 

  memset(&engine_config, '\0', sizeof(struct ipmiconsole_engine_config));
  engine_config.engine_flags = 0;
  if (cmd_args.lock_memory)
    engine_config.engine_flags |= IPMICONSOLE_ENGINE_LOCK_MEMORY;
  engine_config.behavior_flags = 0;
  if (cmd_args.dont_steal)
    engine_config.behavior_flags |= IPMICONSOLE_BEHAVIOR_ERROR_ON_SOL_INUSE;
  if (cmd_args.deactivate)
    engine_config.behavior_flags |= IPMICONSOLE_BEHAVIOR_DEACTIVATE_ONLY;
  engine_config.debug_flags = debug_flags;

  if (!(c = ipmiconsole_ctx_create(cmd_args.common.hostname,
				   &ipmi_config,
				   &protocol_config,
                                   &engine_config)))
    {
      perror("ipmiconsole_ctx_create");
      goto cleanup;
    }

  if (ipmiconsole_engine_submit_block(c) < 0)
    {
      if (ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_IPMI_2_0_UNAVAILABLE
          || ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_CIPHER_SUITE_ID_UNAVAILABLE
          || ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_HOSTNAME_INVALID
          || ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_USERNAME_INVALID
          || ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_PASSWORD_INVALID
          || ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_K_G_INVALID
          || ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_PRIVILEGE_LEVEL_INSUFFICIENT
          || ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_PRIVILEGE_LEVEL_CANNOT_BE_OBTAINED
          || ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_SOL_UNAVAILABLE
          || ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_SOL_INUSE
          || ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_SOL_REQUIRES_ENCRYPTION
          || ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_SOL_REQUIRES_NO_ENCRYPTION
          || ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_BMC_BUSY
          || ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_BMC_ERROR
          || ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_CONNECTION_TIMEOUT
          || ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_SESSION_TIMEOUT
          || ipmiconsole_ctx_errnum(c) ==  IPMICONSOLE_ERR_EXCESS_RETRANSMISSIONS_SENT
          || ipmiconsole_ctx_errnum(c) ==  IPMICONSOLE_ERR_EXCESS_ERRORS_RECEIVED)
        printf("[error received]: %s\n", ipmiconsole_ctx_strerror(ipmiconsole_ctx_errnum(c)));
      else
        fprintf(stderr, "ipmiconsole_submit_block: %s\r\n", ipmiconsole_ctx_strerror(ipmiconsole_ctx_errnum(c)));
      goto cleanup;
    }

  if (cmd_args.deactivate)
    goto cleanup;

  if ((fd = ipmiconsole_ctx_fd(c)) < 0)
    {
      fprintf(stderr, "ipmiconsole_ctx_fd: %s\r\n", ipmiconsole_ctx_strerror(ipmiconsole_ctx_errnum(c)));
      goto cleanup;
    }

#ifndef NDEBUG
  if (!cmd_args.noraw)
    {
      if (_set_mode_raw() < 0)
	goto cleanup;
    }
#else /* !NDEBUG */
   if (_set_mode_raw() < 0)
     goto cleanup;
#endif /* !NDEBUG */

  printf("[SOL established]\r\n");

  while (1)
    {
      char buf[IPMICONSOLE_BUFLEN];
      struct timeval tv;
      ssize_t n;
      fd_set rds;

      FD_ZERO(&rds);
      FD_SET(fd, &rds);
      FD_SET(STDIN_FILENO, &rds);
      
      tv.tv_sec = 0;
      tv.tv_usec = 250000;

      if (select(fd + 1, &rds, NULL, NULL, &tv) < 0)
        {
          perror("select");
          goto cleanup;
        }

      if (FD_ISSET(STDIN_FILENO, &rds))
	{
          if ((n = read(STDIN_FILENO, buf, IPMICONSOLE_BUFLEN)) < 0)
            {
              perror("read");
              goto cleanup;
            }

	  if (!n)
	    goto cleanup;

	  if (_stdin(c, 
                     cmd_args.escape_char,
                     fd, 
                     buf,
                     n) < 0)
	    goto cleanup;
	}

      if (FD_ISSET(fd, &rds))
        {
          if ((n = read(fd, buf, IPMICONSOLE_BUFLEN)) < 0)
            {
              perror("read");
              goto cleanup;
            }
          
	  if (n)
            {
	      if (write(STDOUT_FILENO, buf, n) != n)
		{
		  perror("write");
		  goto cleanup;
		}
	    }
          else 
	    {
	      /* b/c we're exitting */
              if (ipmiconsole_ctx_errnum(c) == IPMICONSOLE_ERR_SOL_STOLEN)
                printf("\r\n[%s]\r\n", 
                       ipmiconsole_ctx_strerror(ipmiconsole_ctx_errnum(c)));
              else
                printf("\r\n[error received]: %s\r\n", 
                       ipmiconsole_ctx_strerror(ipmiconsole_ctx_errnum(c)));
	      goto cleanup;
	    }

        }

      /* Clear out data */
      secure_memset(buf, '\0', IPMICONSOLE_BUFLEN);
    }

 cleanup:
  if (fd >= 0)
    {
      printf("\r\n[closing the connection]\r\n");
      close(fd);
    }
  ipmiconsole_ctx_destroy(c);
  ipmiconsole_engine_teardown(1);

#ifndef NDEBUG
  if (!cmd_args.noraw)
    _reset_mode();
#else /* !NDEBUG */
  _reset_mode();
#endif /* !NDEBUG */

  return 0;
}
