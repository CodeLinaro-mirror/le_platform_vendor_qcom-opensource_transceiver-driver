/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */
#ifndef LINUX_SFF8472_H
#define LINUX_SFF8472_H

enum {
    SFF8472_ID                   = QSFP_ADDR(0, 0,   0),
    SFF8472_RATE_ID              = QSFP_ADDR(0, 0,  13),

    /* Device 1 registers */
    SFF8472_DDM_TH               = QSFP_ADDR(1, 0, 0),
    SFF8472_ENH_FLAGS_ADV        = QSFP_ADDR(1, 0, 58),
    SFF8472_TXI_EXT              = QSFP_ADDR(1, 0, 76),
    SFF8472_TXPWR_EXT            = QSFP_ADDR(1, 0, 80),
    SFF8472_TEMP_EXT             = QSFP_ADDR(1, 0, 84),
    SFF8472_VCC_EXT              = QSFP_ADDR(1, 0, 88),
    SFF8472_TEMP                 = QSFP_ADDR(1, 0, 96),
    SFF8472_VCC                  = QSFP_ADDR(1, 0, 98),
    SFF8472_TX_BIAS              = QSFP_ADDR(1, 0, 100),
    SFF8472_TX_POWER             = QSFP_ADDR(1, 0, 102),
    SFF8472_RX_POWER             = QSFP_ADDR(1, 0, 104),
    SFF8472_LASER_TEMP_WL        = QSFP_ADDR(1, 0, 106),
    SFF8472_STATUS_CTRL          = QSFP_ADDR(1, 0, 110),
    SFF8472_ALARM_WARN           = QSFP_ADDR(1, 0, 112),
    SFF8472_EXT_MOD_CTRL         = QSFP_ADDR(1, 0, 118),
    SFF8472_CDR_LOL              = QSFP_ADDR(1, 0, 119),

};

enum {
    SFF8472_REV_9_3         = 1,
    SFF8472_REV_9_5         = 2,
    SFF8472_REV_10_2        = 3,
    SFF8472_REV_10_4        = 4,
    SFF8472_REV_11_0        = 5,
    SFF8472_REV_11_3        = 6,
    SFF8472_REV_11_4        = 7,
    SFF8472_REV_12_3        = 8,
    SFF8472_REV_12_4        = 9,
};

struct sff8472_rxlos_txf {
    /* Device 1 */
    /* byte 110 */
    u8 data_not_ready:1;
    u8 rx_los:1;
    u8 tx_fault:1;
    u8 rate_select:3;
    u8 soft_tx_disble_select:1;
    u8 tx_disable_state:1;
}__packed;

struct sff8472_alarm_warn {
    /* Device 1 */
    /* byte 112 */
    u8 tx_power_low_alarm:1;
    u8 tx_power_high_alarm:1;
    u8 tx_bias_low_alarm:1;
    u8 tx_bias_high_alarm:1;
    u8 volt_low_alarm:1;
    u8 volt_high_alarm:1;
    u8 temp_low_alarm:1;
    u8 temp_high_alarm:1;

    /* byte 113 */
    u8 reserved:2;
    u8 tec_cur_alarm:2;
    u8 laser_temp_low_alarm:1;
    u8 laser_temp_high_alarm:1;
    u8 rx_power_low_alarm:1;
    u8 rx_power_high_alarm:1;

    /* byte 114 */
    u8 tx_input_eq_ctrl_rate;
    u8 rx_output_emp_ctrl_rate;

    /* byte 116 */
    u8 tx_power_low_warn:1;
    u8 tx_power_high_warn:1;
    u8 tx_bias_low_warn:1;
    u8 tx_bias_high_warn:1;
    u8 volt_low_warn:1;
    u8 volt_high_warn:1;
    u8 temp_low_warn:1;
    u8 temp_high_warn:1;

    /* byte 117 */
    u8 reserved1:2;
    u8 tec_cur_warn:2;
    u8 laser_temp_low_warn:1;
    u8 laser_temp_high_warn:1;
    u8 rx_power_low_warn:1;
    u8 rx_power_high_warn:1;
}__packed;

struct sff8472_tx_adp_eq_in_fail {
    /* Device 1 */
    /* byte 118 */
    u8 power_select:1;
    u8 power_state:1;
    u8 power4_enable:1;
    u8 soft_tx_rate_select:1;
    u8 tx_adap_eq_in_fail:1;
    u8 reserved:3;
}__packed;

struct sff8472_cdr_lol {
    /* Device 1 */
    /* byte 119 */
    u8 rx_cdr_lol:1;
    u8 tx_cdr_lol:1;
    u8 gfc64:1;
    u8 rx_pam4:1;
    u8 tx_pam4:1;
    u8 reserved1:3;
}__packed;

struct sff8472_temp_diag {
    u16 cal_t_slope;
    int16_t cal_t_offset;
}__packed;


struct sff8472_vcc_diag {
    u16 cal_v_slope;
    int16_t cal_v_offset;
}__packed;


struct sff8472_txpwr_diag {
    u16 cal_txpwr_slope;
    u16 cal_txpwr_offset;
}__packed;


struct sff8472_txi_diag {
    u16 cal_txi_slope;
    u16 cal_txi_offset;
}__packed;

struct sff8472_ddm_thresholds {
    u16 temp_high_alarm;
    __be16 temp_low_alarm;
    u16 temp_high_warn;
    __be16 temp_low_warn;
    u16 volt_high_alarm;
    u16 volt_low_alarm;
    u16 volt_high_warn;
    u16 volt_low_warn;
    u16 bias_high_alarm;
    u16 bias_low_alarm;
    u16 bias_high_warn;
    u16 bias_low_warn;
    u16 txpwr_high_alarm;
    u16 txpwr_low_alarm;
    u16 txpwr_high_warn;
    u16 txpwr_low_warn;
    u16 rxpwr_high_alarm;
    u16 rxpwr_low_alarm;
    u16 rxpwr_high_warn;
    u16 rxpwr_low_warn;
    u16 laser_temp_high_alarm;
    u16 laser_temp_low_alarm;
    u16 laser_temp_high_warn;
    u16 laser_temp_low_warn;
    u16 tec_cur_high_alarm;
    u16 tec_cur_low_alarm;
    u16 tec_cur_high_warn;
    u16 tec_cur_low_warn;
}__packed;

#define SFP_OPTIONS_HIGH_POWER_LEVEL4 (BIT(14))
#define SFF8472_HIGH_POWER (BIT(0))
#define SFF8472_TX_DISABLE (BIT(6))

#define SFF8472_SOFT_RATE_SELECT_IMPL (BIT(3)|BIT(1))
#define SFF8472_TX_ADAP_EQ_IN_FAIL_IMPL (BIT(0))

#define SFF8472_DIAGMON_EXT_CAL  (BIT(4))
#define SFF8472_DIAGMON_INT_CAL  (BIT(5))
#define SFF8472_DIAGMON_DDM      (BIT(6))
#define SFF8472_RX_RATE_SELECT   (BIT(3))
#define SFF8472_TX_RATE_SELECT   (BIT(3))

#endif
