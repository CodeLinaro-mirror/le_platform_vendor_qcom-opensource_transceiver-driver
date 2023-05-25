/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef TRANSCEIVER_DEBUG_FS_H
#define TRANSCEIVER_DEBUG_FS_H

#include "fpc.h"
#include "qsfp.h"

#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif

#define SIM_REQ_MAX (40)
#define SIM_READ_BUF_MAX (500)

#define SIM_INSERT "insert\n"
#define SIM_REMOVE "remove\n"
#define SIM_LOS "los\n"
#define SIM_LOS_RECOVERY "los_recovery\n"
#define SIM_TX_FAULT "tx_fault\n"
#define SIM_TX_FAULT_RECOVERY "tx_fault_recovery\n"
#define SIM_CLEAR "clear\n"

void transceiver_debugfs_init(void);
void transceiver_debugfs_exit(void);
void fpc_debugfs_init(struct fpc *fpc);
void fpc_debugfs_exit(struct fpc *fpc);
void qsfp_debugfs_exit(struct qsfp *qsfp);
void qsfp_debugfs_init(struct qsfp *qsfp);

extern const char *sff8636_mod_revision_to_str(u8 mod_rev_value);
extern const char *sff8636_mod_encoding_to_str(u8 mod_encoding);
extern const char *cmis_revision_to_str(u8 mod_rev_value, char *revStr);
int create_common_debugfs_files(struct qsfp *qsfp);
void module_debugfs_exit(struct qsfp *qsfp);
int module_debugfs_init(struct qsfp *qsfp);
inline void spec_info_print(struct seq_file *s, u8 spec_id);
extern void fpc_reset_qsfp(const struct qsfp *qsfp);
const char *sff8472_mod_revision_to_str(u8 mod_rev_value);

extern void qsfp_sm_link_update_txf_los_status(struct qsfp *qsfp);
#endif

