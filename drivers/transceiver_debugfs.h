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
#define SIM_READ_BUF_MAX (800)

#define SIM_INSERT "insert\n"
#define SIM_REMOVE "remove\n"

#define SIM_COPY_FLAGS "copy_flags\n"
#define SIM_FLAGS_CHANGE "flags_change\n"
#define SIM_CLEAR "clear\n"

#define SIM_TEMP_HIGH_ALARM "temp_high_alarm\n"
#define SIM_TEMP_HIGH_ALARM_RECOVERY "temp_high_alarm_recovery\n"

#define SIM_TEMP_LOW_ALARM "temp_low_alarm\n"
#define SIM_TEMP_LOW_ALARM_RECOVERY "temp_low_alarm_recovery\n"

#define SIM_TEMP_HIGH_WARN "temp_high_warn\n"
#define SIM_TEMP_HIGH_WARN_RECOVERY "temp_high_warn_recovery\n"

#define SIM_TEMP_LOW_WARN "temp_low_warn\n"
#define SIM_TEMP_LOW_WARN_RECOVERY "temp_low_warn_recovery\n"

#define SIM_VOLT_HIGH_ALARM "volt_high_alarm\n"
#define SIM_VOLT_HIGH_ALARM_RECOVERY "volt_high_alarm_recovery\n"

#define SIM_VOLT_LOW_ALARM "volt_low_alarm\n"
#define SIM_VOLT_LOW_ALARM_RECOVERY "volt_low_alarm_recovery\n"

#define SIM_VOLT_HIGH_WARN "volt_high_warn\n"
#define SIM_VOLT_HIGH_WARN_RECOVERY "volt_high_warn_recovery\n"

#define SIM_VOLT_LOW_WARN "volt_low_warn\n"
#define SIM_VOLT_LOW_WARN_RECOVERY "volt_low_warn_recovery\n"

/* Lane flags */
#define SIM_RX_LOS "rx_los"
#define SIM_TX_FAULT "tx_fault"
#define SIM_TX_LOS "tx_los"

#define SIM_RX_CDR_LOL "rx_cdr_lol"
#define SIM_TX_CDR_LOL "tx_cdr_lol"
#define SIM_TX_ADAP_EQ_IN_FAIL "tx_adap_eq_in_fail"

#define SIM_RX_POWER_HIGH_ALARM "rx_power_high_alarm"
#define SIM_RX_POWER_LOW_ALARM "rx_power_low_alarm"
#define SIM_RX_POWER_HIGH_WARN "rx_power_high_warn"
#define SIM_RX_POWER_LOW_WARN "rx_power_low_warn"

#define SIM_TX_POWER_HIGH_ALARM "tx_power_high_alarm"
#define SIM_TX_POWER_LOW_ALARM "tx_power_low_alarm"
#define SIM_TX_POWER_HIGH_WARN "tx_power_high_warn"
#define SIM_TX_POWER_LOW_WARN "tx_power_low_warn"

#define SIM_TX_BIAS_HIGH_ALARM "tx_bias_high_alarm"
#define SIM_TX_BIAS_LOW_ALARM "tx_bias_low_alarm"
#define SIM_TX_BIAS_HIGH_WARN "tx_bias_high_warn"
#define SIM_TX_BIAS_LOW_WARN "tx_bias_low_warn"

#define SIM_RECOVERY "_recovery"

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
extern void qsfp_stop_poll(struct qsfp *qsfp);
extern void qsfp_fill_features_str(const struct qsfp *qsfp, char *buf, int len);
extern bool qsfp_atleast_one_flag_supported(const struct qsfp *qsfp);

#endif

