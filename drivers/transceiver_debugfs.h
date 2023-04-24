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
#define SIM_READ_BUF_MAX (400)

#define SIM_INSERT "insert\n"
#define SIM_REMOVE "remove\n"
#define SIM_FAR_END_INSERT "far_end_insert\n"
#define SIM_FAR_END_REMOVE "far_end_remove\n"
#define SIM_TX_FAULT "tx_fault\n"
#define SIM_TX_FAULT_RECOVER "tx_fault_recover\n"
#define SIM_CLEAR "clear_all_simulation\n"

void transceiver_debugfs_init(void);
void transceiver_debugfs_exit(void);
void fpc_debugfs_init(struct fpc *fpc);
void fpc_debugfs_exit(struct fpc *fpc);
void qsfp_debugfs_exit(struct qsfp *qsfp);
void qsfp_debugfs_init(struct qsfp *qsfp);
extern const char *mod_state_to_str(unsigned short mod_state);
extern const char *dev_state_to_str(unsigned short dev_state);
extern const char *sm_state_to_str(unsigned short sm_state);
extern const char *sff8636_mod_revision_to_str(u8 mod_rev_value);
extern const char *sff8636_mod_encoding_to_str(u8 mod_encoding);
extern const char *cmis_revision_to_str(u8 mod_rev_value, char *revStr);
int create_common_debugfs_files(struct qsfp *qsfp);
void module_debugfs_exit(struct qsfp *qsfp);
int module_debugfs_init(struct qsfp *qsfp);
inline void spec_info_print(struct seq_file *s, u8 spec_id);
extern void fpc_reset_qsfp(const struct qsfp *qsfp);
const char *sff8472_mod_revision_to_str(u8 mod_rev_value);

#endif

