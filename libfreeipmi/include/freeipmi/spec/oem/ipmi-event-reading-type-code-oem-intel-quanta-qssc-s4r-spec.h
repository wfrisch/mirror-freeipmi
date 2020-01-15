/*
 * Copyright (C) 2003-2015 FreeIPMI Core Team
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

#ifndef IPMI_EVENT_READING_TYPE_CODE_OEM_INTEL_QUANTA_QSSC_S4R_SPEC_H
#define IPMI_EVENT_READING_TYPE_CODE_OEM_INTEL_QUANTA_QSSC_S4R_SPEC_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Quanta QSSC-S4R/Appro GB812X-CN
 * (Quanta motherboard contains Intel manufacturer ID)
 */
/* achu: Similar to above, but some events changed and some new ones,
 * so new macros to differentiate
 */
/* achu: not official names, named based on use context */
#define IPMI_EVENT_READING_TYPE_CODE_OEM_INTEL_QUANTA_QSSC_S4R_PCIE_FATAL_SENSOR       0x70
#define IPMI_EVENT_READING_TYPE_CODE_OEM_INTEL_QUANTA_QSSC_S4R_PCIE_CORRECTABLE_SENSOR 0x71
#define IPMI_EVENT_READING_TYPE_CODE_OEM_INTEL_QUANTA_QSSC_S4R_QPI_CORRECTABLE_SENSOR  0x72
#define IPMI_EVENT_READING_TYPE_CODE_OEM_INTEL_QUANTA_QSSC_S4R_QPI_NON_FATAL_SENSOR    0x73
#define IPMI_EVENT_READING_TYPE_CODE_OEM_INTEL_QUANTA_QSSC_S4R_QPI_FATAL_SENSOR        0x74
#define IPMI_EVENT_READING_TYPE_CODE_OEM_INTEL_QUANTA_QSSC_S4R_CORRECTABLE_ERROR       0x76
#define IPMI_EVENT_READING_TYPE_CODE_OEM_INTEL_QUANTA_QSSC_S4R_UNCORRECTABLE_ERROR     0x77

#ifdef __cplusplus
}
#endif

#endif /* IPMI_EVENT_READING_TYPE_CODE_OEM_INTEL_QUANTA_QSSC_S4R_SPEC_H */
