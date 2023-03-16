/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef TRX_SYSFS_H
#define TRX_SYSFS_H

#include <linux/kobject.h>
#include <linux/sysfs.h> /* sysfs addition*/
#include "fpc.h"
#include "qsfp.h"

extern const char *mod_state_to_str(unsigned short mod_state);
extern const char *dev_state_to_str(unsigned short dev_state);
extern const char *sm_state_to_str(unsigned short sm_state);
extern char* calc_common_temperature(int16_t tempc, char* str);
char* calc_common_svoltage(u16 svolt, char* str);
int qsfp_sysfs_init(struct qsfp *qsfp);
int qsfp_sysfs_exit(struct qsfp *qsfp);
int module_sysfs_init(struct qsfp *qsfp);
int module_sysfs_exit(struct qsfp *qsfp);

#endif
