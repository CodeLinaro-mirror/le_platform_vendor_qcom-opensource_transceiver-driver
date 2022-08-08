/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef TRANSCEIVER_DEBUG_FS_H
#define TRANSCEIVER_DEBUG_FS_H

#include "fpc.h"
#include "qsfp.h"

#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif

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

#endif

