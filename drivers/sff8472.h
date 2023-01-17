/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */
#ifndef LINUX_SFF8472_H
#define LINUX_SFF8472_H

enum {
    SFF8472_ID                   = QSFP_ADDR(0, 0,   0),
    SFF8472_EXT                  = QSFP_ADDR(0, 0,  64),

    /* Device 1 registers */
    SFF8472_STATUS_FLAGS         = QSFP_ADDR(1, 0, 110),
    SFF8472_EXT_MOD_CTRL         = QSFP_ADDR(1, 0, 118),

};

#define SFP_OPTIONS_HIGH_POWER_LEVEL4 (BIT(14))
#define SFF8472_HIGH_POWER (BIT(0))
#define SFF8472_TX_DISABLE (BIT(6))
#define SFF8472_LOS_IMPL (BIT(4))
#define SFF8472_TX_FAULT_IMPL (BIT(5))
#define SFF8472_TX_DISABLE_IMPL (BIT(6))

#endif
