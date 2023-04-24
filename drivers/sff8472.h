/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */
#ifndef LINUX_SFF8472_H
#define LINUX_SFF8472_H

enum {
    SFF8472_ID                   = QSFP_ADDR(0, 0,   0),

    /* Device 1 registers */
    SFF8472_TEMP                 = QSFP_ADDR(1, 0, 96),
    SFF8472_VCC                  = QSFP_ADDR(1, 0, 98),
    SFF8472_TX_BIAS              = QSFP_ADDR(1, 0, 100),
    SFF8472_TX_POWER             = QSFP_ADDR(1, 0, 102),
    SFF8472_RX_POWER             = QSFP_ADDR(1, 0, 104),

    SFF8472_TXI_EXT              = QSFP_ADDR(1, 0, 76),
    SFF8472_TXPWR_EXT            = QSFP_ADDR(1, 0, 80),
    SFF8472_TEMP_EXT             = QSFP_ADDR(1, 0, 84),
    SFF8472_VCC_EXT              = QSFP_ADDR(1, 0, 88),

    SFF8472_STATUS_FLAGS         = QSFP_ADDR(1, 0, 110),
    SFF8472_EXT_MOD_CTRL         = QSFP_ADDR(1, 0, 118),

};

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

#define SFP_OPTIONS_HIGH_POWER_LEVEL4 (BIT(14))
#define SFF8472_HIGH_POWER (BIT(0))
#define SFF8472_TX_DISABLE (BIT(6))
#define SFF8472_LOS_IMPL (BIT(4))
#define SFF8472_TX_FAULT_IMPL (BIT(5))
#define SFF8472_TX_DISABLE_IMPL (BIT(6))
#define SFF8472_LOS (BIT(1))
#define SFF8472_TX_FAULT (BIT(2))
#define SFF8472_DIAGMON_EXT_CAL  (BIT(4))
#define SFF8472_DIAGMON_INT_CAL  (BIT(5))
#define SFF8472_DIAGMON_DDM      (BIT(6))

#endif
