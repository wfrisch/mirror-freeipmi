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

#ifndef _IPMI_SERIAL_MODEM_PARAMETER_SPEC_H
#define _IPMI_SERIAL_MODEM_PARAMETER_SPEC_H

#ifdef __cplusplus
extern "C" {
#endif

#define IPMI_SERIAL_MODEM_PARAMETER_SET_IN_PROGRESS                       0
#define IPMI_SERIAL_MODEM_PARAMETER_AUTH_TYPE_SUPPORT                     1
#define IPMI_SERIAL_MODEM_PARAMETER_AUTH_TYPE_ENABLES                     2
#define IPMI_SERIAL_MODEM_PARAMETER_CONNECTION_MODE                       3
#define IPMI_SERIAL_MODEM_PARAMETER_SESSION_INACTIVITY_TIMEOUT            4
#define IPMI_SERIAL_MODEM_PARAMETER_CHANNEL_CALLBACK_CONTROL              5
#define IPMI_SERIAL_MODEM_PARAMETER_SESSION_TERMINATION                   6
#define IPMI_SERIAL_MODEM_PARAMETER_IPMI_MESSAGING_COMM_SETTINGS          7
#define IPMI_SERIAL_MODEM_PARAMETER_MUX_SWITCH_CONTROL                    8
#define IPMI_SERIAL_MODEM_PARAMETER_MODEM_RING_TIME                       9
#define IPMI_SERIAL_MODEM_PARAMETER_MODEM_INIT_STRING                     10
#define IPMI_SERIAL_MODEM_PARAMETER_MODEM_ESCAPE_SEQUENCE                 11
#define IPMI_SERIAL_MODEM_PARAMETER_MODEM_HANG_UP_SEQUENCE                12
#define IPMI_SERIAL_MODEM_PARAMETER_MODEM_DIAL_COMMAND                    13
#define IPMI_SERIAL_MODEM_PARAMETER_PAGE_BLACKOUT_INTERVAL                14
#define IPMI_SERIAL_MODEM_PARAMETER_COMMUNITY_STRING                      15
#define IPMI_SERIAL_MODEM_PARAMETER_NO_OF_ALERT_DESTINATIONS              16
#define IPMI_SERIAL_MODEM_PARAMETER_DESTINATION_INFO                      17
#define IPMI_SERIAL_MODEM_PARAMETER_CALL_RETRY_INTERVAL                   18
#define IPMI_SERIAL_MODEM_PARAMETER_DESTINATION_COMM_SETTINGS             19
#define IPMI_SERIAL_MODEM_PARAMETER_NO_OF_DIAL_STRINGS                    20
#define IPMI_SERIAL_MODEM_PARAMETER_DESTINATION_DIAL_STRINGS              21
#define IPMI_SERIAL_MODEM_PARAMETER_NO_OF_ALERT_DESTINATION_IP_ADDRS      22
#define IPMI_SERIAL_MODEM_PARAMETER_DESTINATION_IP_ADDRS                  23
#define IPMI_SERIAL_MODEM_PARAMETER_NO_OF_TAP_ACCOUNTS                    24
#define IPMI_SERIAL_MODEM_PARAMETER_TAP_ACCOUNT                           25
#define IPMI_SERIAL_MODEM_PARAMETER_TAP_PASSWORDS                         26
#define IPMI_SERIAL_MODEM_PARAMETER_TAP_PAGER_ID_STRINGS                  27
#define IPMI_SERIAL_MODEM_PARAMETER_TAP_SERVICE_SETTINGS                  28
#define IPMI_SERIAL_MODEM_PARAMETER_TERMINAL_MODE_CONF                    29
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_PROTOCOL_OPTIONS                  30
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_PRIMARY_RMCP_PORT_NO              31
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_SECONDARY_RMCP_PORT_NO            32
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_LINK_AUTH                         33
#define IPMI_SERIAL_MODEM_PARAMETER_CHAP_NAME                             34
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_ACCM                              35
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_SNOOP_ACCM                        36
#define IPMI_SERIAL_MODEM_PARAMETER_NO_OF_PPP_ACCOUNT                     37
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_ACCOUNT_DIAL_STRING_SELECTOR      38
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_ACCOUNT_BMC_IP_ADDRESSES          39
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_ACCOUNT_USER_NAMES                40
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_ACCOUNT_USER_DOMAINS              41
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_ACCOUNT_USER_PASSWORDS            42
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_ACCOUNT_AUTH_SETTINGS             43
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_ACCOUNT_CONNECTION_HOLD_TIMES     44
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_UDP_PROXY_IP_HEADER_DATA          45
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_UDP_PROXY_TRANSMIT_BUFFER_SIZE    46
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_UDP_PROXY_RECEIVE_BUFFER_SIZE     47
#define IPMI_SERIAL_MODEM_PARAMETER_PPP_REMOTE_CONSOLE_IP_ADDRESS         48
#define IPMI_SERIAL_MODEM_PARAMETER_OEM_MIN                               192
#define IPMI_SERIAL_MODEM_PARAMETER_OEM_MAX                               255

/* To avoid gcc warnings, add +1 in comparison */
#define IPMI_SERIAL_PARAMETER_SELECTOR_VALID(__parameter_selector)              \
  ((((__parameter_selector) + 1) > (IPMI_SERIAL_MODEM_PARAMETER_SET_IN_PROGRESS + 1) \
    && (__parameter_selector) <= IPMI_SERIAL_MODEM_PARAMETER_PPP_REMOTE_CONSOLE_IP_ADDRESS) ? 1 : 0)

/* To avoid gcc warnings, subtract -1 in comparison */
#define IPMI_SERIAL_PARAMETER_SELECTOR_IS_OEM(__parameter_selector) \
  (((__parameter_selector) >= IPMI_SERIAL_MODEM_PARAMETER_OEM_MIN \
    && ((__parameter_selector) - 1) <= (IPMI_SERIAL_MODEM_PARAMETER_OEM_MAX - 1)) ? 1 : 0)

#ifdef __cplusplus
}
#endif

#endif
