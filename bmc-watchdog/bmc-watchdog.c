/*****************************************************************************\
 *  $Id: bmc-watchdog.c,v 1.134 2010-06-28 20:24:30 chu11 Exp $
 *****************************************************************************
 *  Copyright (C) 2007-2012 Lawrence Livermore National Security, LLC.
 *  Copyright (C) 2004-2007 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Albert Chu <chu11@llnl.gov>
 *  UCRL-CODE-155913
 *
 *  This file is part of Bmc-watchdog, a base management controller
 *  (BMC) watchdog timer management tool. For details, see
 *  http://www.llnl.gov/linux/.
 *
 *  Bmc-Watchdog is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 3 of the License, or (at your
 *  option) any later version.
 *
 *  Bmc-Watchdog is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with Bmc-Watchdog.  If not, see <http://www.gnu.org/licenses/>.
\*****************************************************************************/

#if HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#if STDC_HEADERS
#include <string.h>
#include <stdarg.h>
#endif /* STDC_HEADERS */
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */
#include <sys/stat.h>
#include <sys/select.h>
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
#include <assert.h>
#include <errno.h>

#include <freeipmi/freeipmi.h>

#include "bmc-watchdog.h"
#include "bmc-watchdog-argp.h"

#include "freeipmi-portability.h"
#include "error.h"
#include "tool-common.h"

#define BMC_WATCHDOG_ERR_BUFLEN           1024
#define BMC_WATCHDOG_STR_BUFLEN           1024
#define BMC_WATCHDOG_PKT_BUFLEN           1024
#define BMC_WATCHDOG_RESET_PERIOD_DEFAULT   60

#define BMC_WATCHDOG_RETRY_WAIT_TIME_DEFAULT 1
#define BMC_WATCHDOG_RETRY_ATTEMPTS_DEFAULT  5

#define BMC_WATCHDOG_PIDFILE BMC_WATCHDOG_LOCALSTATEDIR "/run/bmc-watchdog.pid"

struct bmc_watchdog_arguments cmd_args;

static ipmi_ctx_t ipmi_ctx = NULL;
static unsigned int retry_wait_time = BMC_WATCHDOG_RETRY_WAIT_TIME_DEFAULT;
static unsigned int retry_attempts = BMC_WATCHDOG_RETRY_ATTEMPTS_DEFAULT;
static char comp_code_errbuf[BMC_WATCHDOG_ERR_BUFLEN];

static int shutdown_flag = 1;

static int logfile_fd = -1;

static void
_syslog (int priority, const char *fmt, ...)
{
  char buffer[BMC_WATCHDOG_ERR_BUFLEN];
  va_list ap;

  assert (fmt);

  if (cmd_args.no_logging)
    return;

  va_start (ap, fmt);
  snprintf (buffer, BMC_WATCHDOG_ERR_BUFLEN, "%s\n", fmt);
  vsyslog (priority, buffer, ap);
  va_end (ap);
}

static void
_bmclog_write (void *buf, size_t count)
{
  ssize_t ret;
  size_t left;
  char *ptr;

  ptr = buf;
  left = count;

  while (left > 0)
    {
      if ((ret = write (logfile_fd, ptr, left)) < 0)
        {
          if (errno == EINTR)
            continue;
          else
            {
              /* The only place we should really need to syslog */
              _syslog (LOG_ERR, "_bmcwrite: write: %s", strerror (errno));
              return;
            }
        }
      ptr += ret;
      left -= ret;
    }

  if (fsync (logfile_fd) < 0)
    _syslog (LOG_ERR, "_bmcwrite: fsync: %s", strerror (errno));
}

static void
_bmclog (const char *fmt, ...)
{
  time_t t;
  struct tm *tm;
  int len;
  char buffer[BMC_WATCHDOG_ERR_BUFLEN];
  char fbuffer[BMC_WATCHDOG_ERR_BUFLEN];
  va_list ap;

  assert (fmt
	  && (cmd_args.no_logging || logfile_fd >= 0));

  if (cmd_args.no_logging)
    return;

  va_start (ap, fmt);
  t = time (NULL);

  if (!(tm = localtime (&t)))
    {
      /* Just use the value from time() */
      snprintf (buffer, BMC_WATCHDOG_ERR_BUFLEN, "%ld: %s\n", t, fmt);
    }
  else
    {
      char tbuffer[BMC_WATCHDOG_ERR_BUFLEN];
      strftime (tbuffer, BMC_WATCHDOG_ERR_BUFLEN, "[%b %d %H:%M:%S]", tm);
      snprintf (buffer, BMC_WATCHDOG_ERR_BUFLEN, "%s: %s\n", tbuffer, fmt);
    }

  len = vsnprintf (fbuffer, BMC_WATCHDOG_ERR_BUFLEN, buffer, ap);

  _bmclog_write (fbuffer, len);

  va_end (ap);
}

static int
_init_ipmi (void)
{
  
  return (0);
}

/* Must be called after cmdline parsed */
static void
_init_bmc_watchdog (void)
{
  unsigned int workaround_flags = 0;
  unsigned int flags = 0;

  if (!ipmi_is_root ())
    err_exit ("Permission denied, must be root.");

  if (!cmd_args.no_logging)
    {
      if ((logfile_fd = open ((cmd_args.logfile) ? cmd_args.logfile : BMC_WATCHDOG_LOGFILE_DEFAULT,
                              O_WRONLY | O_CREAT | O_APPEND,
                              S_IRUSR | S_IWUSR)) < 0)
	err_exit ("Error opening logfile '%s': %s",
		  (cmd_args.logfile) ? cmd_args.logfile : BMC_WATCHDOG_LOGFILE_DEFAULT,
		  strerror (errno));
    }

  if (cmd_args.common.workaround_flags_inband & IPMI_PARSE_WORKAROUND_FLAGS_INBAND_ASSUME_IO_BASE_ADDRESS)
    workaround_flags |= IPMI_WORKAROUND_FLAGS_INBAND_ASSUME_IO_BASE_ADDRESS;
  
  if (cmd_args.common.workaround_flags_inband & IPMI_PARSE_WORKAROUND_FLAGS_INBAND_SPIN_POLL)
    workaround_flags |= IPMI_WORKAROUND_FLAGS_INBAND_SPIN_POLL;
  
  flags = IPMI_FLAGS_NONBLOCKING;
  if (cmd_args.common.debug)
    flags |= IPMI_FLAGS_DEBUG_DUMP; 
  
  if (!(ipmi_ctx = ipmi_ctx_create ()))
    err_exit ("ipmi_ctx_create: %s", strerror (errno));
  
  if (cmd_args.common.driver_type == IPMI_DEVICE_UNKNOWN)
    {
      int ret;
      
      if ((ret = ipmi_ctx_find_inband (ipmi_ctx,
				       NULL,
				       cmd_args.common.disable_auto_probe,
				       cmd_args.common.driver_address,
				       cmd_args.common.register_spacing,
				       cmd_args.common.driver_device,
				       workaround_flags,
				       flags)) < 0)
	err_exit ("ipmi_ctx_find_inband: %s", ipmi_ctx_errormsg (ipmi_ctx));
      
      if (!ret)
	err_exit ("could not find inband device");
    }
  else
    {
      if (ipmi_ctx_open_inband (ipmi_ctx,
				cmd_args.common.driver_type,
				cmd_args.common.disable_auto_probe,
				cmd_args.common.driver_address,
				cmd_args.common.register_spacing,
				cmd_args.common.driver_device,
				workaround_flags,
				flags) < 0)
	err_exit ("ipmi_ctx_open_inband: %s", ipmi_ctx_errormsg (ipmi_ctx));
    }
}

static void
_ipmi_err_exit (char *str)
{
  assert (str);

  if (ipmi_ctx_errnum (ipmi_ctx) == IPMI_ERR_BAD_COMPLETION_CODE)
    err_exit ("%s: %s", str, comp_code_errbuf);
  else
    err_exit ("%s: %s", str, ipmi_ctx_errormsg (ipmi_ctx));
}

/* signal handlers + sleep(3) is a bad idea */
static int
_sleep (unsigned int sleep_len)
{
  struct timeval tv;

  if (!sleep_len)
    return (0);

  tv.tv_sec = sleep_len;
  tv.tv_usec = 0;
  if (select (1, NULL, NULL, NULL, &tv) < 0)
    {
      if (errno != EINTR)
        err_exit ("select: %s", strerror (errno));
    }
  return (0);
}

static void
_fiid_obj_get(fiid_obj_t obj_cmd_rs, const char *field, uint64_t *val)
{
  uint64_t valtemp;

  if (FIID_OBJ_GET (obj_cmd_rs, field, &valtemp) < 0)
    err_exit ("fiid_obj_get: '%s': %s", field, fiid_obj_errormsg (obj_cmd_rs));

  (*val) = valtemp;
}

static int
_cmd (char *str,
      uint8_t netfn,
      uint8_t cmd,
      fiid_obj_t obj_cmd_rq,
      fiid_obj_t obj_cmd_rs)
{
  int retry_count = 0;
  int ret = 0;

  assert (str
          && (netfn == IPMI_NET_FN_APP_RQ || netfn == IPMI_NET_FN_TRANSPORT_RQ)
          && obj_cmd_rq
          && obj_cmd_rs);

  while (1)
    {
      if ((ret = ipmi_cmd (ipmi_ctx,
			   IPMI_BMC_IPMB_LUN_BMC,
			   netfn,
			   obj_cmd_rq,
			   obj_cmd_rs)) < 0)
	{
	  if (ipmi_ctx_errnum (ipmi_ctx) != IPMI_ERR_DRIVER_BUSY
	      && ipmi_ctx_errnum (ipmi_ctx) != IPMI_ERR_BMC_BUSY
	      && ipmi_ctx_errnum (ipmi_ctx) != IPMI_ERR_IPMI_ERROR)
	    {
	      _bmclog ("%s: ipmi_cmd: %s", str, ipmi_ctx_errormsg (ipmi_ctx));

	      if (ipmi_ctx_errnum (ipmi_ctx) == IPMI_ERR_BAD_COMPLETION_CODE)
		{
		  if (ipmi_completion_code_strerror_cmd_r (obj_cmd_rs,
							   netfn,
							   comp_code_errbuf,
							   BMC_WATCHDOG_ERR_BUFLEN) < 0)
		    {
		      uint8_t comp_code;
		      uint64_t val;
		      
		      _bmclog ("ipmi_completion_code_strerror_r: %s", strerror (errno));

		      _fiid_obj_get (obj_cmd_rs, "comp_code", &val);
		      comp_code = val;
		      
		      snprintf (comp_code_errbuf,
				BMC_WATCHDOG_ERR_BUFLEN,
				"Comp Code 0x%X",
				comp_code);
		    }
		}

	      return (-1);
	    }
        }

      if (ret < 0)
        {
          if (retry_count >= retry_attempts)
            {
              _bmclog ("%s: BMC Timeout: retry_wait_time=%d, retry_attempts=%d",
                       str,
                       retry_wait_time,
                       retry_attempts);
              return (-1);
            }

          if (cmd_args.common.debug)
	    fprintf (stderr, "%s: BMC busy\n", str);

          _sleep (retry_wait_time);
          retry_count++;
        }
      else
        break;
    }

  return (0);
}

static int
_reset_watchdog_timer_cmd (void)
{
  fiid_obj_t obj_cmd_rq = NULL;
  fiid_obj_t obj_cmd_rs = NULL;
  int rv = -1;

  if (!(obj_cmd_rq = fiid_obj_create (tmpl_cmd_reset_watchdog_timer_rq)))
    {
      _bmclog ("_reset_watchdog_timer_cmd: fiid_obj_create: %s",
               strerror (errno));
      goto cleanup;
    }

  if (!(obj_cmd_rs = fiid_obj_create (tmpl_cmd_reset_watchdog_timer_rs)))
    {
      _bmclog ("_reset_watchdog_timer_cmd: fiid_obj_create: %s",
               strerror (errno));
      goto cleanup;
    }

  if (fill_cmd_reset_watchdog_timer (obj_cmd_rq) < 0)
    {
      _bmclog ("_reset_watchdog_timer_cmd: "
               "fill_cmd_reset_watchdog_timer: %s", strerror (errno));
      goto cleanup;
    }

  if (_cmd ("Reset Cmd",
	    IPMI_NET_FN_APP_RQ,
	    IPMI_CMD_RESET_WATCHDOG_TIMER,
	    obj_cmd_rq,
	    obj_cmd_rs) < 0)
    goto cleanup;

  rv = 0;
 cleanup:
  fiid_obj_destroy (obj_cmd_rq);
  fiid_obj_destroy (obj_cmd_rs);
  return (rv);
}

static int
_set_watchdog_timer_cmd (uint8_t timer_use,
                         uint8_t stop_timer,
                         uint8_t log,
                         uint8_t timeout_action,
                         uint8_t pre_timeout_interrupt,
                         uint8_t pre_timeout_interval,
                         uint8_t timer_use_expiration_flag_bios_frb2,
                         uint8_t timer_use_expiration_flag_bios_post,
                         uint8_t timer_use_expiration_flag_os_load,
                         uint8_t timer_use_expiration_flag_sms_os,
                         uint8_t timer_use_expiration_flag_oem,
                         uint16_t initial_countdown_seconds)
{
  fiid_obj_t obj_cmd_rq = NULL;
  fiid_obj_t obj_cmd_rs = NULL;
  uint16_t initial_countdown_chunks;
  int rv = -1;

  /* IPMI specifies timeout in 100 millisecond chunks */
  initial_countdown_chunks = initial_countdown_seconds * 10;

  if (!(obj_cmd_rq = fiid_obj_create (tmpl_cmd_set_watchdog_timer_rq)))
    {
      _bmclog ("_set_watchdog_timer_cmd: fiid_obj_create: %s",
               strerror (errno));
      goto cleanup;
    }

  if (!(obj_cmd_rs = fiid_obj_create (tmpl_cmd_set_watchdog_timer_rs)))
    {
      _bmclog ("_set_watchdog_timer_cmd: fiid_obj_create: %s",
               strerror (errno));
      goto cleanup;
    }

  if (fill_cmd_set_watchdog_timer (timer_use,
                                   stop_timer,
                                   log,
                                   timeout_action,
                                   pre_timeout_interrupt,
                                   pre_timeout_interval,
                                   timer_use_expiration_flag_bios_frb2,
                                   timer_use_expiration_flag_bios_post,
                                   timer_use_expiration_flag_os_load,
                                   timer_use_expiration_flag_sms_os,
                                   timer_use_expiration_flag_oem,
                                   initial_countdown_chunks,
                                   obj_cmd_rq) < 0)
    {
      _bmclog ("_set_watchdog_timer_cmd: fill_cmd_set_watchdog_timer: %s",
               strerror (errno));
      goto cleanup;
    }

  if (_cmd ("Set Cmd",
	    IPMI_NET_FN_APP_RQ,
	    IPMI_CMD_SET_WATCHDOG_TIMER,
	    obj_cmd_rq,
	    obj_cmd_rs) < 0)
    goto cleanup;

  rv = 0;
 cleanup:
  fiid_obj_destroy (obj_cmd_rq);
  fiid_obj_destroy (obj_cmd_rs);
  return (rv);
}

static int
_get_watchdog_timer_cmd (uint8_t *timer_use,
                         uint8_t *timer_state,
                         uint8_t *log,
                         uint8_t *timeout_action,
                         uint8_t *pre_timeout_interrupt,
                         uint8_t *pre_timeout_interval,
                         uint8_t *timer_use_expiration_flag_bios_frb2,
                         uint8_t *timer_use_expiration_flag_bios_post,
                         uint8_t *timer_use_expiration_flag_os_load,
                         uint8_t *timer_use_expiration_flag_sms_os,
                         uint8_t *timer_use_expiration_flag_oem,
                         uint16_t *initial_countdown_seconds,
                         uint16_t *present_countdown_seconds)
{
  fiid_obj_t obj_cmd_rq = NULL;
  fiid_obj_t obj_cmd_rs = NULL;
  uint64_t val;
  int rv = -1;

  if (!(obj_cmd_rq = fiid_obj_create (tmpl_cmd_get_watchdog_timer_rq)))
    {
      _bmclog ("_get_watchdog_timer_cmd: fiid_obj_create: %s",
               strerror (errno));
      goto cleanup;
    }

  if (!(obj_cmd_rs = fiid_obj_create (tmpl_cmd_get_watchdog_timer_rs)))
    {
      _bmclog ("_get_watchdog_timer_cmd: fiid_obj_create: %s",
               strerror (errno));
      goto cleanup;
    }

  if (fill_cmd_get_watchdog_timer (obj_cmd_rq) < 0)
    {
      _bmclog ("_get_watchdog_timer_cmd: fill_cmd_get_watchdog_timer: %s",
               strerror (errno));
      goto cleanup;
    }

  if (_cmd ("Get Cmd",
	    IPMI_NET_FN_APP_RQ,
	    IPMI_CMD_GET_WATCHDOG_TIMER,
	    obj_cmd_rq,
	    obj_cmd_rs) < 0)
    goto cleanup;

  if (timer_use)
    {
      _fiid_obj_get (obj_cmd_rs, "timer_use", &val);
      (*timer_use) = val;
    }

  if (timer_state)
    {
      _fiid_obj_get (obj_cmd_rs, "timer_state", &val);
      (*timer_state) = val;
    }

  if (log)
    {
      _fiid_obj_get (obj_cmd_rs, "log", &val);
      (*log) = val;
    }

  if (timeout_action)
    {
      _fiid_obj_get (obj_cmd_rs, "timeout_action", &val);
      (*timeout_action) = val;
    }

  if (pre_timeout_interrupt)
    {
      _fiid_obj_get (obj_cmd_rs, "pre_timeout_interrupt", &val);
      (*pre_timeout_interrupt) = val;
    }

  if (pre_timeout_interval)
    {
      _fiid_obj_get (obj_cmd_rs, "pre_timeout_interval", &val);
      (*pre_timeout_interval) = val;
    }

  if (timer_use_expiration_flag_bios_frb2)
    {
      _fiid_obj_get (obj_cmd_rs, "timer_use_expiration_flag.bios_frb2", &val);
      (*timer_use_expiration_flag_bios_frb2) = val;
    }

  if (timer_use_expiration_flag_bios_post)
    {
      _fiid_obj_get (obj_cmd_rs, "timer_use_expiration_flag.bios_post", &val);
      (*timer_use_expiration_flag_bios_post) = val;
    }

  if (timer_use_expiration_flag_os_load)
    {
      _fiid_obj_get (obj_cmd_rs, "timer_use_expiration_flag.os_load", &val);
      (*timer_use_expiration_flag_os_load) = val;
    }

  if (timer_use_expiration_flag_sms_os)
    {
      _fiid_obj_get (obj_cmd_rs, "timer_use_expiration_flag.sms_os", &val);
      (*timer_use_expiration_flag_sms_os) = val;
    }

  if (timer_use_expiration_flag_oem)
    {
      _fiid_obj_get (obj_cmd_rs, "timer_use_expiration_flag.oem", &val);
      (*timer_use_expiration_flag_oem) = val;
    }

  if (initial_countdown_seconds)
    {
      _fiid_obj_get (obj_cmd_rs, "initial_countdown_value", &val);
      (*initial_countdown_seconds) = val / 10;
    }

  if (present_countdown_seconds)
    {
      _fiid_obj_get (obj_cmd_rs, "present_countdown_value", &val);
      (*present_countdown_seconds) = val / 10;
    }

  rv = 0;
 cleanup:
  fiid_obj_destroy (obj_cmd_rq);
  fiid_obj_destroy (obj_cmd_rs);
  return (rv);
}

/* returns -1 on error, 0 on not found, 1 on found */
static int
_get_channel_number (uint8_t *channel_number)
{
  fiid_obj_t dev_id_obj_cmd_rq = NULL;
  fiid_obj_t dev_id_obj_cmd_rs = NULL;
  fiid_obj_t channel_info_obj_cmd_rq = NULL;
  fiid_obj_t channel_info_obj_cmd_rs = NULL;
  uint32_t manufacturer_id;
  uint16_t product_id;
  unsigned int i;
  uint64_t val;
  int rv = -1;

  assert (channel_number);

  if (!(dev_id_obj_cmd_rq = fiid_obj_create (tmpl_cmd_get_device_id_rq)))
    {
      _bmclog ("fiid_obj_create: %s", strerror (errno));
      goto cleanup;
    }

  if (!(dev_id_obj_cmd_rs = fiid_obj_create (tmpl_cmd_get_device_id_rs)))
    {
      _bmclog ("fiid_obj_create: %s", strerror (errno));
      goto cleanup;
    }

  if (fill_cmd_get_device_id (dev_id_obj_cmd_rq) < 0)
    {
      _bmclog ("fill_cmd_get_device_id: %s", strerror (errno));
      goto cleanup;
    }

  if (_cmd ("Get Device Id Cmd",
	    IPMI_NET_FN_APP_RQ,
	    IPMI_CMD_GET_DEVICE_ID,
	    dev_id_obj_cmd_rq,
	    dev_id_obj_cmd_rs) < 0)
    _ipmi_err_exit ("Get Device Id Error");
  
  _fiid_obj_get (dev_id_obj_cmd_rs, "manufacturer_id.id", &val);
  manufacturer_id = val;
  
  _fiid_obj_get (dev_id_obj_cmd_rs, "product_id", &val);
  product_id = val;

  switch (manufacturer_id)
    {
    case IPMI_IANA_ENTERPRISE_ID_INTEL:
    case 0xB000157: /* Intel */
      switch (product_id)
        {
        case 0x1B:
	  (*channel_number) = 7;
	  rv = 1;
          goto cleanup;
        }
    }

  if (!(channel_info_obj_cmd_rq = fiid_obj_create (tmpl_cmd_get_channel_info_rq)))
    {
      _bmclog ("fiid_obj_create: %s", strerror (errno));
      goto cleanup;
    }

  if (!(channel_info_obj_cmd_rs = fiid_obj_create (tmpl_cmd_get_channel_info_rs)))
    {
      _bmclog ("fiid_obj_create: %s", strerror (errno));
      goto cleanup;
    }

  /* Channel numbers range from 0 - 7 */
  for (i = 0; i < 8; i++)
    {
      uint8_t channel_medium_type;

      if (fill_cmd_get_channel_info (i, channel_info_obj_cmd_rq) < 0)
        {
          _bmclog ("fill_cmd_get_channel_info: %s", strerror (errno));
          continue;
        }

      if (_cmd ("Get Channel Info Cmd",
                IPMI_NET_FN_APP_RQ,
                IPMI_CMD_GET_CHANNEL_INFO_COMMAND,
                channel_info_obj_cmd_rq,
                channel_info_obj_cmd_rs) < 0)
        continue;

      _fiid_obj_get (channel_info_obj_cmd_rs, "channel_medium_type", &val);
      channel_medium_type = val;

      if (channel_medium_type == IPMI_CHANNEL_MEDIUM_TYPE_LAN_802_3)
        {
          _fiid_obj_get (channel_info_obj_cmd_rs, "actual_channel_number", &val);
          (*channel_number) = val;
          rv = 1;
          goto cleanup;
        }
    }

  rv = 0;
 cleanup:
  fiid_obj_destroy (dev_id_obj_cmd_rq);
  fiid_obj_destroy (dev_id_obj_cmd_rs);
  fiid_obj_destroy (channel_info_obj_cmd_rq);
  fiid_obj_destroy (channel_info_obj_cmd_rs);
  return (rv);
}

/* returns -1 on error, 0 can't configure, 1 on configured */
static int
_suspend_bmc_arps_cmd (uint8_t gratuitous_arp,
                       uint8_t arp_response)
{
  fiid_obj_t obj_cmd_rq = NULL;
  fiid_obj_t obj_cmd_rs = NULL;
  uint8_t channel_number = 0;
  int ret, rv = -1;

  if (!(obj_cmd_rq = fiid_obj_create (tmpl_cmd_suspend_bmc_arps_rq)))
    {
      _bmclog ("_suspend_bmc_arps: fiid_obj_create: %s",
               strerror (errno));
      goto cleanup;
    }

  if (!(obj_cmd_rs = fiid_obj_create (tmpl_cmd_suspend_bmc_arps_rs)))
    {
      _bmclog ("_suspend_bmc_arps: fiid_obj_create: %s",
               strerror (errno));
      goto cleanup;
    }

  if ((ret = _get_channel_number (&channel_number)) < 0)
    goto cleanup;

  if (!ret)
    {
      rv = 0;
      goto cleanup;
    }

  if (fill_cmd_suspend_bmc_arps (channel_number,
                                 gratuitous_arp,
                                 arp_response,
                                 obj_cmd_rq) < 0)
    {
      _bmclog ("fill_cmd_suspend_bmc_arps: %s",
               strerror (errno));
      goto cleanup;
    }

  if (_cmd ("Suspend Cmd",
	    IPMI_NET_FN_TRANSPORT_RQ,
	    IPMI_CMD_SUSPEND_BMC_ARPS,
	    obj_cmd_rq,
	    obj_cmd_rs) < 0)
    goto cleanup;

  rv = 1;
 cleanup:
  fiid_obj_destroy (obj_cmd_rq);
  fiid_obj_destroy (obj_cmd_rs);
  return (rv);
}

static void
_set_cmd (void)
{
  uint8_t timer_use, stop_timer, timer_state, log, timeout_action,
    pre_timeout_interrupt, pre_timeout_interval;
  uint16_t initial_countdown_seconds;

  if (_get_watchdog_timer_cmd (&timer_use,
			       &timer_state,
			       &log,
			       &timeout_action,
			       &pre_timeout_interrupt,
			       &pre_timeout_interval,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       &initial_countdown_seconds,
			       NULL) < 0)
    _ipmi_err_exit ("Get Watchdog Timer Error");

  if ((!timer_state && cmd_args.start_if_stopped)
      || (timer_state && cmd_args.reset_if_running))
    {
      if (_reset_watchdog_timer_cmd () < 0)
        _ipmi_err_exit ("Reset Watchdog Timer Error");
    }

  timer_use = (cmd_args.timer_use) ? cmd_args.timer_use_arg : timer_use;
  stop_timer = (cmd_args.stop_timer) ? cmd_args.stop_timer_arg : timer_state;
  log = (cmd_args.log) ? cmd_args.log_arg : log;
  timeout_action = (cmd_args.timeout_action) ?
    cmd_args.timeout_action_arg : timeout_action;
  pre_timeout_interrupt = (cmd_args.pre_timeout_interrupt) ?
    cmd_args.pre_timeout_interrupt_arg : pre_timeout_interrupt;
  pre_timeout_interval = (cmd_args.pre_timeout_interval) ?
    cmd_args.pre_timeout_interval_arg : pre_timeout_interval;
  initial_countdown_seconds = (cmd_args.initial_countdown_seconds) ?
    cmd_args.initial_countdown_seconds_arg : initial_countdown_seconds;

  if ((pre_timeout_interrupt != IPMI_BMC_WATCHDOG_TIMER_PRE_TIMEOUT_INTERRUPT_NONE)
      && (pre_timeout_interval > initial_countdown_seconds))
    err_exit ("pre-timeout interval greater than initial countdown seconds");

  if (_set_watchdog_timer_cmd (timer_use,
			       stop_timer,
			       log,
			       timeout_action,
			       pre_timeout_interrupt,
			       pre_timeout_interval,
			       (cmd_args.clear_bios_frb2) ? 1 : 0,
			       (cmd_args.clear_bios_post) ? 1 : 0,
			       (cmd_args.clear_os_load) ? 1 : 0,
			       (cmd_args.clear_sms_os) ? 1 : 0,
			       (cmd_args.clear_oem) ? 1 : 0,
			       initial_countdown_seconds) < 0)
    _ipmi_err_exit ("Set Watchdog Timer Error");

  if (cmd_args.start_after_set || cmd_args.reset_after_set)
    {
      if (_get_watchdog_timer_cmd (NULL,
				   &timer_state,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   NULL) < 0)
        _ipmi_err_exit ("Get Watchdog Timer Error");

      if ((!timer_state && cmd_args.start_after_set)
          || (timer_state && cmd_args.reset_after_set))
        {
          if (_reset_watchdog_timer_cmd () < 0)
            _ipmi_err_exit ("Reset Watchdog Timer Error");
        }
    }

  return;
}

static char *
_log_str (uint8_t log)
{
  switch (log)
    {
    case IPMI_BMC_WATCHDOG_TIMER_LOG_ENABLE:
      return "Enabled";
    case IPMI_BMC_WATCHDOG_TIMER_LOG_DISABLE:
      return "Disabled";
    default:
      return "Internal Error, Unknown Log Value";
    }

  return (NULL);		/* NOT REACHED */
}

static char *
_timer_state_str (uint8_t timer_state)
{
  switch (timer_state)
    {
    case IPMI_BMC_WATCHDOG_TIMER_TIMER_STATE_RUNNING:
      return "Running";
    case IPMI_BMC_WATCHDOG_TIMER_TIMER_STATE_STOPPED:
      return "Stopped";
    default:
      return "Internal Error, Unknown Stop Timer Value";
    }

  return (NULL);		/* NOT REACHED */
}

static char *
_timer_use_str (uint8_t timer_use)
{
  switch (timer_use)
    {
    case IPMI_BMC_WATCHDOG_TIMER_TIMER_USE_BIOS_FRB2:
      return "BIOS FRB2";
    case IPMI_BMC_WATCHDOG_TIMER_TIMER_USE_BIOS_POST:
      return "BIOS POST";
    case IPMI_BMC_WATCHDOG_TIMER_TIMER_USE_OS_LOAD:
      return "OS LOAD";
    case IPMI_BMC_WATCHDOG_TIMER_TIMER_USE_SMS_OS:
      return "SMS/OS";
    case IPMI_BMC_WATCHDOG_TIMER_TIMER_USE_OEM:
      return "OEM";
    default:
      return "Reserved";
    }

  return (NULL);		/* NOT REACHED */
}

static char *
_pre_timeout_interrupt_str (uint8_t pre_timeout_interrupt)
{
  switch (pre_timeout_interrupt)
    {
    case IPMI_BMC_WATCHDOG_TIMER_PRE_TIMEOUT_INTERRUPT_NONE:
      return "None";
    case IPMI_BMC_WATCHDOG_TIMER_PRE_TIMEOUT_INTERRUPT_SMI:
      return "SMI";
    case IPMI_BMC_WATCHDOG_TIMER_PRE_TIMEOUT_INTERRUPT_NMI:
      return "NMI / Diagnostic Interrupt";
    case IPMI_BMC_WATCHDOG_TIMER_PRE_TIMEOUT_INTERRUPT_MESSAGING_INTERRUPT:
      return "Messaging Interrupt";
    default:
      return "Reserved";
    }
  
  return (NULL);		/* NOT REACHED */
}

static char *
_timeout_action_str (uint8_t timeout_action)
{
  switch (timeout_action)
    {
    case IPMI_BMC_WATCHDOG_TIMER_TIMEOUT_ACTION_NO_ACTION:
      return "None";
    case IPMI_BMC_WATCHDOG_TIMER_TIMEOUT_ACTION_HARD_RESET:
      return "Hard Reset";
    case IPMI_BMC_WATCHDOG_TIMER_TIMEOUT_ACTION_POWER_DOWN:
      return "Power Down";
    case IPMI_BMC_WATCHDOG_TIMER_TIMEOUT_ACTION_POWER_CYCLE:
      return "Power Cycle";
    default:
      return "Reserved";
    }

  return (NULL);		/* NOT REACHED */
}

static void
_get_cmd (void)
{
  uint8_t timer_use, timer_state, log, timeout_action, pre_timeout_interrupt,
    pre_timeout_interval, timer_use_expiration_flag_bios_frb2,
    timer_use_expiration_flag_bios_post, timer_use_expiration_flag_os_load,
    timer_use_expiration_flag_sms_os, timer_use_expiration_flag_oem;
  uint16_t initial_countdown_seconds, present_countdown_seconds;

  if (_get_watchdog_timer_cmd (&timer_use,
			       &timer_state,
			       &log,
			       &timeout_action,
			       &pre_timeout_interrupt,
			       &pre_timeout_interval,
			       &timer_use_expiration_flag_bios_frb2,
			       &timer_use_expiration_flag_bios_post,
			       &timer_use_expiration_flag_os_load,
			       &timer_use_expiration_flag_sms_os,
			       &timer_use_expiration_flag_oem,
			       &initial_countdown_seconds,
			       &present_countdown_seconds) < 0)
    _ipmi_err_exit ("Get Watchdog Timer Error");

  printf ("Timer Use:                   %s\n",
          _timer_use_str (timer_use));
  printf ("Timer:                       %s\n",
          _timer_state_str (timer_state));
  printf ("Logging:                     %s\n",
          _log_str (log));
  printf ("Timeout Action:              %s\n",
          _timeout_action_str (timeout_action));
  printf ("Pre-Timeout Interrupt:       %s\n",
          _pre_timeout_interrupt_str (pre_timeout_interrupt));
  printf ("Pre-Timeout Interval:        %d seconds\n",
          pre_timeout_interval);
  printf ("Timer Use BIOS FRB2 Flag:    %s\n",
          (timer_use_expiration_flag_bios_frb2) ? "Set" : "Clear");
  printf ("Timer Use BIOS POST Flag:    %s\n",
          (timer_use_expiration_flag_bios_post) ? "Set" : "Clear");
  printf ("Timer Use BIOS OS Load Flag: %s\n",
          (timer_use_expiration_flag_os_load) ? "Set" : "Clear");
  printf ("Timer Use BIOS SMS/OS Flag:  %s\n",
          (timer_use_expiration_flag_sms_os) ? "Set" : "Clear");
  printf ("Timer Use BIOS OEM Flag:     %s\n",
          (timer_use_expiration_flag_oem) ? "Set" : "Clear");
  printf ("Initial Countdown:           %d seconds\n",
          initial_countdown_seconds);
  printf ("Current Countdown:           %d seconds\n",
          present_countdown_seconds);
}

static void
_reset_cmd (void)
{
  if (_reset_watchdog_timer_cmd () < 0)
    _ipmi_err_exit ("Reset Watchdog Timer Error");
}

static void
_start_cmd (void)
{
  uint8_t timer_state;

  if (_get_watchdog_timer_cmd (NULL,
			       &timer_state,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL) < 0)
    _ipmi_err_exit ("Get Watchdog Timer Error");

  if (!timer_state)
    {
      if (_reset_watchdog_timer_cmd () < 0)
        _ipmi_err_exit ("Reset Watchdog Timer Error");
    }


  if (cmd_args.gratuitous_arp || cmd_args.arp_response)
    {
      uint8_t gratuitous_arp, arp_response;
      int ret;

      if (cmd_args.gratuitous_arp)
        gratuitous_arp = cmd_args.gratuitous_arp_arg;
      else
        gratuitous_arp = IPMI_BMC_GENERATED_GRATUITOUS_ARP_DO_NOT_SUSPEND;

      if (cmd_args.arp_response)
        arp_response = cmd_args.arp_response_arg;
      else
        arp_response = IPMI_BMC_GENERATED_ARP_RESPONSE_DO_NOT_SUSPEND;

      if ((ret = _suspend_bmc_arps_cmd (gratuitous_arp,
					arp_response)) < 0)
        _ipmi_err_exit ("Suspend BMC ARPs Error");

      if (!ret)
	err_exit ("cannot suspend BMC ARPs"); 
    }
}

static void
_stop_cmd (void)
{
  uint8_t timer_use, log, timeout_action, pre_timeout_interrupt,
    pre_timeout_interval;
  uint16_t initial_countdown_seconds;

  if (_get_watchdog_timer_cmd (&timer_use,
			       NULL,
			       &log,
			       &timeout_action,
			       &pre_timeout_interrupt,
			       &pre_timeout_interval,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       &initial_countdown_seconds,
			       NULL) < 0)
    _ipmi_err_exit ("Get Watchdog Timer Error");

  if (_set_watchdog_timer_cmd (timer_use,
			       IPMI_BMC_WATCHDOG_TIMER_STOP_TIMER_ENABLE,
			       log,
			       timeout_action,
			       pre_timeout_interrupt,
			       pre_timeout_interval,
			       0,
			       0,
			       0,
			       0,
			       0,
			       initial_countdown_seconds) < 0)
    _ipmi_err_exit ("Set Watchdog Timer Error");
}

static void
_clear_cmd (void)
{
  uint8_t timer_use;

  /* Timer use cannot be NONE, so use whatever was there before */

  if (_get_watchdog_timer_cmd (&timer_use,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL,
			       NULL) < 0)
    _ipmi_err_exit ("Get Watchdog Timer Error");

  if (_set_watchdog_timer_cmd (timer_use,
			       IPMI_BMC_WATCHDOG_TIMER_STOP_TIMER_ENABLE,
			       IPMI_BMC_WATCHDOG_TIMER_LOG_DISABLE,
			       IPMI_BMC_WATCHDOG_TIMER_TIMEOUT_ACTION_NO_ACTION,
			       IPMI_BMC_WATCHDOG_TIMER_PRE_TIMEOUT_INTERRUPT_NONE,
			       0,
			       0,
			       0,
			       0,
			       0,
			       0,
			       0) < 0)
    _ipmi_err_exit ("Set Watchdog Timer Error");
}

static void
_daemon_init (const char *progname)
{
  assert (progname);

  /* Based on code in Unix network programming by R. Stevens */

  /* Run in foreground if debugging */
  if (!cmd_args.common.debug)
    {
      unsigned int i;
      pid_t pid;
      int fds[2];

      if ( pipe(fds) < 0 )
        err_exit ("pipe: %s", strerror (errno));

      if ((pid = fork ()) < 0)
        err_exit ("fork: %s", strerror (errno));
      if (pid)
        {
          /* parent terminates */
          char buf;
          read(fds[0], &buf, 1);
          close(fds[1]);
          close(fds[0]);
          exit (0);
        }

      setsid ();

      if (signal (SIGHUP, SIG_IGN) == SIG_ERR)
        err_exit ("signal: %s", strerror (errno));

      if ((pid = fork ()) < 0)
        err_exit ("fork: %s", strerror (errno));
      if (pid) {
	FILE *pidfile;

	/* Do not want pidfile writable to group/other */
	umask(022);

	(void) unlink (BMC_WATCHDOG_PIDFILE);

	if ( (pidfile = fopen(BMC_WATCHDOG_PIDFILE, "w")) == NULL )
	  err_exit ("fopen: %s", strerror (errno));

        /* write the 2nd child PID to the pidfile */
        fprintf(pidfile, "%u\n", pid);
        fclose(pidfile);

        exit (0);                   /* 1st child terminates */
      }

      if (chdir ("/") < 0)
        err_exit ("chdir: %s", strerror (errno));

      umask (0);

      write(fds[1], "a", 1);
      close(fds[1]);
      close(fds[0]);
      for (i = 0; i < 64; i++)
        close (i);
    }

  /* move error outs to syslog from stderr */

  err_set_flags (ERROR_SYSLOG);

  openlog (progname, LOG_ODELAY | LOG_PID, LOG_DAEMON);

  _init_bmc_watchdog ();
}

static void
_daemon_cmd_error_maybe_exit (char *str)
{
  assert (str);

  if (ipmi_ctx_errnum (ipmi_ctx) == IPMI_ERR_BAD_COMPLETION_CODE)
    {
      _bmclog ("%s: %s", str, comp_code_errbuf);
      return;
    }
  
  if (ipmi_ctx_errnum (ipmi_ctx) != IPMI_ERR_DRIVER_BUSY
      && ipmi_ctx_errnum (ipmi_ctx) != IPMI_ERR_BMC_BUSY
      && ipmi_ctx_errnum (ipmi_ctx) != IPMI_ERR_IPMI_ERROR)
    {
      _bmclog ("%s: %s", str, ipmi_ctx_errormsg (ipmi_ctx));
      exit (1);
    }
}

static void
_daemon_setup (void)
{
  uint8_t timer_use, timer_state, log, timeout_action, pre_timeout_interrupt,
    pre_timeout_interval;
  uint32_t reset_period = BMC_WATCHDOG_RESET_PERIOD_DEFAULT;
  uint16_t initial_countdown_seconds;

  while (1)
    {
      if (_get_watchdog_timer_cmd (&timer_use,
				   &timer_state,
				   &log,
				   &timeout_action,
				   &pre_timeout_interrupt,
				   &pre_timeout_interval,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   &initial_countdown_seconds,
				   NULL) < 0)
        {
          _daemon_cmd_error_maybe_exit ("Get Watchdog Timer");
	  _sleep (BMC_WATCHDOG_RETRY_WAIT_TIME_DEFAULT);
          continue;
        }
      break;
    }

  if (timer_state == IPMI_BMC_WATCHDOG_TIMER_TIMER_STATE_RUNNING)
    {
      _bmclog ("Error: watchdog timer must be stopped before running daemon");
      exit (1);
    }

  timer_use = (cmd_args.timer_use) ? cmd_args.timer_use_arg : timer_use;
  log = (cmd_args.log) ? cmd_args.log_arg : log;
  timeout_action = (cmd_args.timeout_action) ?
    cmd_args.timeout_action_arg : timeout_action;
  pre_timeout_interrupt = (cmd_args.pre_timeout_interrupt) ?
    cmd_args.pre_timeout_interrupt_arg : pre_timeout_interrupt;
  pre_timeout_interval = (cmd_args.pre_timeout_interval) ?
    cmd_args.pre_timeout_interval_arg : pre_timeout_interval;
  initial_countdown_seconds = (cmd_args.initial_countdown_seconds) ?
    cmd_args.initial_countdown_seconds_arg : initial_countdown_seconds;

  if ((pre_timeout_interrupt != IPMI_BMC_WATCHDOG_TIMER_PRE_TIMEOUT_INTERRUPT_NONE)
      && (pre_timeout_interval > initial_countdown_seconds))
    {
      _bmclog ("Error: pre-timeout interval greater than initial countdown seconds");
      exit (1);
    }
  if (cmd_args.reset_period)
    reset_period = cmd_args.reset_period_arg;
  if (reset_period > initial_countdown_seconds)
    {
      _bmclog ("Error: reset-period interval greater than initial countdown seconds");
      exit (1);
    }

  while (1)
    {
      if (_set_watchdog_timer_cmd (timer_use,
				   IPMI_BMC_WATCHDOG_TIMER_STOP_TIMER_ENABLE,
				   log,
				   timeout_action,
				   pre_timeout_interrupt,
				   pre_timeout_interval,
				   (cmd_args.clear_bios_frb2) ? 1 : 0,
				   (cmd_args.clear_bios_post) ? 1 : 0,
				   (cmd_args.clear_os_load) ? 1 : 0,
				   (cmd_args.clear_sms_os) ? 1 : 0,
				   (cmd_args.clear_oem) ? 1 : 0,
				   initial_countdown_seconds) < 0)
        {
          _daemon_cmd_error_maybe_exit ("Set Watchdog Timer");
	  _sleep (BMC_WATCHDOG_RETRY_WAIT_TIME_DEFAULT);
          continue;
        }
      break;
    }

  /* Must start watchdog timer before entering loop */
  while (1)
    {
      if (_reset_watchdog_timer_cmd () < 0)
        {
          _daemon_cmd_error_maybe_exit ("Reset Watchdog Timer");
	  _sleep (BMC_WATCHDOG_RETRY_WAIT_TIME_DEFAULT);
          continue;
        }
      break;
    }

  if (cmd_args.gratuitous_arp || cmd_args.arp_response)
    {
      uint8_t gratuitous_arp, arp_response;

      if (cmd_args.gratuitous_arp)
        gratuitous_arp = cmd_args.gratuitous_arp_arg;
      else
        gratuitous_arp = IPMI_BMC_GENERATED_GRATUITOUS_ARP_DO_NOT_SUSPEND;

      if (cmd_args.arp_response)
        arp_response = cmd_args.gratuitous_arp_arg;
      else
        arp_response = IPMI_BMC_GENERATED_ARP_RESPONSE_DO_NOT_SUSPEND;

      while (1)
        {
	  int ret;

          if ((ret = _suspend_bmc_arps_cmd (gratuitous_arp,
                                            arp_response)) < 0)
            {
              _daemon_cmd_error_maybe_exit ("Suspend BMC ARPs");
	      _sleep (BMC_WATCHDOG_RETRY_WAIT_TIME_DEFAULT);
              continue;
            }

	  if (!ret)
	    _bmclog ("cannot suspend BMC ARPs"); 

          break;
        }
    }

  return;
}

static void
_signal_handler (int sig)
{
  if (!cmd_args.common.debug)
    (void) unlink (BMC_WATCHDOG_PIDFILE);
  shutdown_flag = 0;
}

static void
_daemon_cmd_error_noexit (char *str)
{
  assert (str);

  if (ipmi_ctx_errnum (ipmi_ctx) == IPMI_ERR_BAD_COMPLETION_CODE)
    {
      _bmclog ("%s: %s", str, comp_code_errbuf);
      return;
    }
  
  if (ipmi_ctx_errnum (ipmi_ctx) != IPMI_ERR_DRIVER_BUSY
      && ipmi_ctx_errnum (ipmi_ctx) != IPMI_ERR_BMC_BUSY
      && ipmi_ctx_errnum (ipmi_ctx) != IPMI_ERR_IPMI_ERROR)
    _bmclog ("%s: %s", str, ipmi_ctx_errormsg (ipmi_ctx));
}

static void
_daemon_cmd (const char *progname)
{
  uint32_t reset_period = BMC_WATCHDOG_RESET_PERIOD_DEFAULT;
  uint8_t timer_use, timer_state, log, timeout_action, pre_timeout_interrupt,
    pre_timeout_interval;
  uint16_t initial_countdown_seconds;
  uint16_t previous_present_countdown_seconds = 0;
  uint16_t present_countdown_seconds;

  _daemon_init (progname);

  _daemon_setup ();

  if (cmd_args.reset_period)
    reset_period = cmd_args.reset_period_arg;

  if (signal (SIGTERM, _signal_handler) == SIG_ERR)
    {
      _bmclog ("signal: %s", strerror (errno));
      exit (1);
    }
  if (signal (SIGINT, _signal_handler) == SIG_ERR)
    {
      _bmclog ("signal: %s", strerror (errno));
      exit (1);
    }
  if (signal (SIGQUIT, _signal_handler) == SIG_ERR)
    {
      _bmclog ("signal: %s", strerror (errno));
      exit (1);
    }

  _bmclog ("starting bmc-watchdog daemon");

  retry_wait_time = BMC_WATCHDOG_RETRY_WAIT_TIME_DEFAULT;
  retry_attempts = BMC_WATCHDOG_RETRY_ATTEMPTS_DEFAULT;

  if ((retry_wait_time * retry_attempts) > reset_period)
    {
      retry_wait_time = 0;
      retry_attempts = 0;
    }
  else if (reset_period > retry_wait_time
           && reset_period < (retry_wait_time * retry_attempts))
    retry_attempts = reset_period/retry_wait_time;

  /* IPMI Workaround
   *
   * Discovered on Sun x4100M2 and x4200M2
   *
   * If implementing the IGNORE_STATE_FLAG workaround flag below, we
   * need to sleep a little bit to make sure the BMC timer has really
   * started.
   *
   * From 27.7 "Internal delays in the BMC may require software to
   * delay up to 100 ms before seeing the countdown value change and
   * be reflected in the Get Watchdog Timer command".
   */
  if (cmd_args.common.section_specific_workaround_flags & IPMI_PARSE_SECTION_SPECIFIC_WORKAROUND_FLAGS_IGNORE_STATE_FLAG)
    _sleep (1);

  while (shutdown_flag)
    {
      struct timeval start_tv, end_tv;
      int timeval_bad = 0;

      if (gettimeofday (&start_tv, NULL) < 0)
        {
          _bmclog ("gettimeofday: %s", strerror (errno));
          timeval_bad++;
        }

      if (_get_watchdog_timer_cmd (NULL,
				   &timer_state,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   &present_countdown_seconds) < 0)
        {
          _daemon_cmd_error_noexit ("Get Watchdog Timer");
          goto sleep_now;
        }

      /* IPMI Workaround
       *
       * Discovered on Sun x4100M2 and x4200M2
       *
       * On some BMCs, the timer state flag is not functional.  Therefore,
       * to have an operational BMC watchdog, it must function without it.
       * We instead look to see if the timer is changing.
       */
      if (cmd_args.common.section_specific_workaround_flags & IPMI_PARSE_SECTION_SPECIFIC_WORKAROUND_FLAGS_IGNORE_STATE_FLAG)
        {
          if (previous_present_countdown_seconds == present_countdown_seconds)
            {
              _bmclog ("timer stopped by another process");
              goto cleanup;
            }
          previous_present_countdown_seconds = present_countdown_seconds;
        }
      else
        {
          if (timer_state == IPMI_BMC_WATCHDOG_TIMER_TIMER_STATE_STOPPED)
            {
              _bmclog ("timer stopped by another process");
              goto cleanup;
            }
        }

      if (_reset_watchdog_timer_cmd () < 0)
        {
          _daemon_cmd_error_noexit ("Reset Watchdog Timer");
          goto sleep_now;
        }

      /* IPMI Workaround
       *
       * Discovered on Sun x4100M2 and x4200M2
       *
       * If implementing the IGNORE_STATE_FLAG workaround flag above,
       * we need to reset the previous_present_countdown_seconds to
       * what it is after the timer reset.
       */
      if (cmd_args.common.section_specific_workaround_flags & IPMI_PARSE_SECTION_SPECIFIC_WORKAROUND_FLAGS_IGNORE_STATE_FLAG)
        {
          /* From 27.7 "Internal delays in the BMC may require software to
           * delay up to 100 ms before seeing the countdown value change and
           * be reflected in the Get Watchdog Timer command".
           */
          _sleep (1);

          if (_get_watchdog_timer_cmd (NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       &present_countdown_seconds) < 0)
            {
	      _daemon_cmd_error_noexit ("Get Watchdog Timer");
              goto sleep_now;
            }
          
          previous_present_countdown_seconds = present_countdown_seconds;
        }

    sleep_now:
      if (gettimeofday (&end_tv, NULL) < 0)
        {
          _bmclog ("gettimeofday: %s", strerror (errno));
          timeval_bad++;
        }

      if (timeval_bad)
        _sleep (reset_period);
      else
        {
          uint32_t adjusted_period = reset_period;

          /* Ignore micro secs, just seconds is good enough */
          if ((end_tv.tv_sec - start_tv.tv_sec) < adjusted_period)
            adjusted_period -= (end_tv.tv_sec - start_tv.tv_sec);

          _sleep (adjusted_period);
        }
    }

  /* Need to stop the timer, don't want it to keep on going.  Don't
   * give up until its shut off.
   */

  /* set back to defaults, no reset-period adjustment anymore */
  retry_wait_time = BMC_WATCHDOG_RETRY_WAIT_TIME_DEFAULT;
  retry_attempts = BMC_WATCHDOG_RETRY_ATTEMPTS_DEFAULT;

  while (1)
    {
      if (_get_watchdog_timer_cmd (&timer_use,
				   NULL,
				   &log,
				   &timeout_action,
				   &pre_timeout_interrupt,
				   &pre_timeout_interval,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   NULL,
				   &initial_countdown_seconds,
				   NULL) < 0)
        {
          _daemon_cmd_error_noexit ("Get Watchdog Timer");
	  _sleep (BMC_WATCHDOG_RETRY_WAIT_TIME_DEFAULT);
          continue;
        }
      break;
    }

  while (1)
    {
      if (_set_watchdog_timer_cmd (timer_use,
				   IPMI_BMC_WATCHDOG_TIMER_STOP_TIMER_ENABLE,
				   log,
				   timeout_action,
				   pre_timeout_interrupt,
				   pre_timeout_interval,
				   0,
				   0,
				   0,
				   0,
				   0,
				   initial_countdown_seconds) < 0)
        {
          _daemon_cmd_error_noexit ("Set Watchdog Timer");
	  _sleep (BMC_WATCHDOG_RETRY_WAIT_TIME_DEFAULT);
          continue;
        }
      break;
    }
  
 cleanup:
  _bmclog ("stopping bmc-watchdog daemon");
}

int
main (int argc, char **argv)
{
  err_init (argv[0]);
  err_set_flags (ERROR_STDERR);

  ipmi_disable_coredump ();

  bmc_watchdog_argp_parse (argc, argv, &cmd_args);

  /* Early initialization.  Daemon must do all initialization in
   * daemon_init() b/c daemon_init() needs to close all formerly open
   * file descriptors.
   */
  if (!cmd_args.daemon)
    _init_bmc_watchdog ();

  if (cmd_args.set)
    _set_cmd ();
  else if (cmd_args.get)
    _get_cmd ();
  else if (cmd_args.reset)
    _reset_cmd ();
  else if (cmd_args.start)
    _start_cmd ();
  else if (cmd_args.stop)
    _stop_cmd ();
  else if (cmd_args.clear)
    _clear_cmd ();
  else if (cmd_args.daemon)
    _daemon_cmd (argv[0]);
  else
    err_exit ("internal error, command not set");

  ipmi_ctx_close (ipmi_ctx);
  ipmi_ctx_destroy (ipmi_ctx);
  if (logfile_fd >= 0)
    {
      if (close (logfile_fd) < 0)
        err_exit ("close: %s", strerror (errno));
    }
  closelog ();
  exit (0);
}
