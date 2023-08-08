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

extern const char *mod_state_to_str(u8 mod_state);
extern const char *dev_state_to_str(u8 dev_state);
extern const char *link_state_to_str(u8 sm_state);
extern char* calc_common_temperature(int16_t tempc, char* str);
extern char* calc_external_calib_temperature(struct qsfp*,int16_t, char*);
extern char* calc_external_calib_svoltage(struct qsfp*, u16, char*);
char* calc_common_svoltage(u16 svolt, char* str);
extern char* calc_external_calib_txi(struct qsfp*, u16, char*);
extern char* calc_external_calib_txpwr(struct qsfp*, u16, char*);
int qsfp_sysfs_init(struct qsfp *qsfp);
int qsfp_sysfs_exit(struct qsfp *qsfp);
int module_sysfs_init(struct qsfp *qsfp);
int module_sysfs_exit(struct qsfp *qsfp);
extern void qsfp_fill_features_str(const struct qsfp *qsfp, char *buf, int len);

#endif
