// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * http://git.armlinux.org.uk/cgit/linux-arm.git/tree/drivers/
 * net/phy/sfp.c?h=cex7
 *
 */
#ifndef LINUX_LANE_H
#define LINUX_LANE_H

#include "sfp.h"

#define LANE_COMPATIBLE "sff,lane"

struct lane {
    struct device *dev;
    struct sfp_bus *sfp_bus;
    /* Stores presence RX LOS TX Fault TX Disable status */
    u8 status;
    u8 lane_num;
    u8 sm_mod_state;
    u8 sm_dev_state;
    u8 sm_link_state;
    struct qsfp *qsfp;
};

extern const char *mod_state_to_str(u8 mod_state);
extern const char *dev_state_to_str(u8 dev_state);
extern const char *event_to_str(u8 event);
extern const char *link_state_to_str(u8 link_state);

extern void qsfp_attach(const struct lane *lane);
extern void qsfp_detach(const struct lane *lane);
extern void qsfp_start(const struct lane *lane);
extern void qsfp_stop(const struct lane *lane);

extern int qsfp_module_info(struct sfp *sfp, struct ethtool_modinfo *modinfo);
extern int qsfp_module_eeprom(struct sfp *sfp, struct ethtool_eeprom *ee,
                              u8 *data);
extern int qsfp_module_eeprom_by_page(struct sfp *sfp,
                     const struct ethtool_module_eeprom *page,
                     struct netlink_ext_ack *extack);

#endif
