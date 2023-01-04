/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */
#include "qsfp.h"
#include "transceiver_debugfs.h"


static int sff8472_mod_probe(struct qsfp *qsfp, bool report)
{
    /* QSFP module inserted - read I2C data */
    struct qsfp_eeprom_id id = {0};
    u8 check;
    int ret;

    qsfp->module_flat_mem = 1;

    ret = qsfp_read(qsfp, SFF8472_ID, &id, sizeof(id.sff8472));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Failed to read base EEPROM: %d\n",
                            __func__, ret);
        return ret;
    }

    /* Validate the checksum over the base structure */
    check = qsfp_check(&id.sff8472.base, sizeof(id.sff8472.base) - 1);
    if (check != id.sff8472.base.cc_base) {
        dev_err(qsfp->dev, "%s: EEPROM base structure checksum failure: "
                "0x%02x != 0x%02x\n", __func__, check, id.sff8472.base.cc_base);
        return -EINVAL;
    }

    /* Validate the checksum over the extented structure */
    check = qsfp_check(&id.sff8472.ext, sizeof(id.sff8472.ext) - 1);
    if (check != id.sff8472.ext.cc_ext) {
        dev_err(qsfp->dev, "%s: EEPROM extended structure checksum failure: "
                "0x%02x != 0x%02x\n", __func__, check, id.sff8472.ext.cc_ext);
        return -EINVAL;
    }

    if (id.sff8472.base.phys_ext_id != SFP_PHYS_EXT_ID_SFP) {
        dev_err(qsfp->dev, "%s: Extended id 0x%X didnot match\n",
                           id.sff8472.base.phys_ext_id, __func__);
        return -E_UNSUPPORTED_SPEC;
    }

    qsfp->id = id;

    /* In case of sff8472 module version never be zero */
    qsfp->module_revision = id.sff8472.ext.sff8472_compliance;

    /* check state as there is no initial interrupt coming for sff8472 in case
     * transceiver inserted during bootup and also needed in case interrupt
     * called before EEPROM read
     */
    mod_delayed_work(system_wq, &qsfp->poll, 0);

    return 0;
}

static int sff8472_check_feature_impl(struct qsfp *qsfp)
{
    u8 features = qsfp->id.sff8472.ext.enhopts;

    if (features & SFF8472_LOS_IMPL) {
        qsfp->features |= QSFP_F_LOS;
    } else {
        dev_warn(qsfp->dev, "%s: TX LOS not implemented\n", __func__);
    }

    if (features & SFF8472_TX_FAULT_IMPL) {
        qsfp->features |= QSFP_F_TX_FAULT;
    } else {
        dev_warn(qsfp->dev, "%s: TX Fault not implemented\n", __func__);
    }

    if (features & SFF8472_TX_DISABLE_IMPL) {
        qsfp->features |= QSFP_F_TX_DISABLE;
    } else {
        dev_warn(qsfp->dev, "%s: TX Disable not implemented\n", __func__);
    }

    return 0;
}

static int sff8472_module_parse_power(struct qsfp *qsfp)
{
    u32 power_mW;
    u8 power_class;

    if (qsfp->id.sff8472.ext.options &
        cpu_to_be16(SFP_OPTIONS_HIGH_POWER_LEVEL4)) {
        power_class = 4;
        power_mW = 2500;
    } else if (qsfp->id.sff8472.ext.options &
               cpu_to_be16(SFP_OPTIONS_HIGH_POWER_LEVEL)) {
        power_class = 3;
        power_mW = 2000;
    } else if (qsfp->id.sff8472.ext.options &
               cpu_to_be16(SFP_OPTIONS_POWER_DECL)) {
        power_class = 2;
        power_mW = 1500;
    } else {
        power_class = 1;
        power_mW = 1000;
    }

    qsfp->module_power_mW = power_mW;
    qsfp->module_power_class = power_class;

    return 0;
}

static void sff8472_disable_redundant_irq(const struct qsfp *qsfp)
{

}

static int sff8472_handle_max_power_exceed(const struct qsfp *qsfp)
{
    return -E_MAX_POWER_EXCEED;
}

static int sff8472_mod_high_power(const struct qsfp *qsfp)
{
    int ret;
    u8 val;

    dev_notice(qsfp->dev, "%s:\n", __func__);

    ret = qsfp_read(qsfp, SFF8472_EXT_MOD_CTRL, &val, sizeof(val));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Failed to read extended module control."
                           " ret %d\n", __func__, ret);
        return ret;
    }

    val |= SFF8472_HIGH_POWER;

    ret = qsfp_write(qsfp, SFF8472_EXT_MOD_CTRL, &val, sizeof(val));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Failed to write extended module control."
                           " ret %d\n", __func__, ret);
        return ret;
    }

    return 0;
}

static int sff8472_mod_low_power(const struct qsfp *qsfp)
{
    int ret;
    u8 val;

    dev_notice(qsfp->dev, "%s:\n", __func__);

    ret = qsfp_read(qsfp, SFF8472_EXT_MOD_CTRL, &val, sizeof(val));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Failed to read extended module control."
                           " ret %d\n", __func__, ret);
        return ret;
    }

    val &= ~SFF8472_HIGH_POWER;

    ret = qsfp_write(qsfp, SFF8472_EXT_MOD_CTRL, &val, sizeof(val));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Failed to write extended module control."
                           " ret %d\n", __func__, ret);
        return ret;
    }

    return 0;
}

static void sff8472_eeprom_print(const struct qsfp *qsfp)
{
    const struct sfp_eeprom_id *id = &qsfp->id.sff8472;
    char date[9];

    dev_notice(qsfp->dev, "%s: phys_id 0x%X phys_ext_id 0x%X connector 0x%X "
    "encoding 0x%X \n", __func__, id->base.phys_id, id->base.phys_ext_id,
    id->base.connector, id->base.encoding);

    date[0] = id->ext.datecode[4];
    date[1] = id->ext.datecode[5];
    date[2] = '-';
    date[3] = id->ext.datecode[2];
    date[4] = id->ext.datecode[3];
    date[5] = '-';
    date[6] = id->ext.datecode[0];
    date[7] = id->ext.datecode[1];
    date[8] = '\0';

    dev_notice(qsfp->dev, "%s: date %s\n", __func__, date);

    dev_notice(qsfp->dev, "%s: vendor name %.*s\n", __func__,
                           (int)sizeof(id->base.vendor_name),
                           id->base.vendor_name);
    dev_notice(qsfp->dev, "%s: vendor pn %.*s\n", __func__,
                           (int)sizeof(id->base.vendor_pn),
                           id->base.vendor_pn);

    dev_notice(qsfp->dev, "%s: opt 0x%X enhopts 0x%X sff8472_compliance "
    "0x%X\n", __func__, id->ext.options, id->ext.enhopts,
    id->ext.sff8472_compliance);

    dev_notice(qsfp->dev, "%s: vendor sn %.*s\n", __func__,
                           (int)sizeof(id->ext.vendor_sn),
                           id->ext.vendor_sn);
}

static u8 sff8472_get_state(struct qsfp *qsfp)
{
    int ret;
    u8 state = 0;
    u8 irq_flag = 0;
    const __be16 los_inverted = cpu_to_be16(SFP_OPTIONS_LOS_INVERTED);
    const __be16 los_normal = cpu_to_be16(SFP_OPTIONS_LOS_NORMAL);
    __be16 los_options;

    ret = qsfp_read(qsfp, SFF8472_STATUS_FLAGS, &irq_flag, sizeof(irq_flag));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Failed to read IRQ status flag. "
                           "ret %d\n", __func__, ret);
        /* Preserve the current state */
        return qsfp->state;
    }

    /* EEPROM not yet read so wont have details about feature
     * and LOS polarity
     * poll not necessary as it is handled in mod probe
     */
    if (qsfp->module_revision == 0) {
        dev_notice(qsfp->dev, "%s: EEPROM not yet read\n", __func__);
        return qsfp->state;
    }

    dev_notice(qsfp->dev, "%s: IRQ status flag: 0x%X\n", __func__, irq_flag);

    los_options = qsfp->id.sff8472.ext.options & (los_inverted | los_normal);

    if (qsfp->features & QSFP_F_LOS) {
        if (los_options == los_normal) {
            if (irq_flag & SFF8472_LOS) {
                state |= QSFP_F_LOS;
                dev_notice(qsfp->dev, "%s: LOS set\n", __func__);
            }
        } else if (los_options == los_inverted) {
            if (!(irq_flag & SFF8472_LOS)) {
                state |= QSFP_F_LOS;
                dev_notice(qsfp->dev, "%s: LOS set (inverted)\n", __func__);
            }
        } else {
            dev_err(qsfp->dev, "%s: LOS Neither normal nor inverted\n",
                               __func__);
        }
    }

    if ((qsfp->features & QSFP_F_TX_FAULT) && (irq_flag & SFF8472_TX_FAULT)) {
        state |= QSFP_F_TX_FAULT;
        dev_notice(qsfp->dev, "%s: TX Fault set\n", __func__);
    }

    return state;
}

static int sff8472_module_info(struct qsfp *qsfp,
                               struct ethtool_modinfo *modinfo)
{
    if (qsfp->id.sff8472.ext.sff8472_compliance &&
        !(qsfp->id.sff8472.ext.diagmon & SFP_DIAGMON_ADDRMODE)) {
        modinfo->type = ETH_MODULE_SFF_8472;
        modinfo->eeprom_len = ETH_MODULE_SFF_8472_LEN;
    } else {
        modinfo->type = ETH_MODULE_SFF_8079;
        modinfo->eeprom_len = ETH_MODULE_SFF_8079_LEN;
    }

    return 0;
}

static u8 sff8472_get_connector_type(const struct qsfp *qsfp)
{
    return qsfp->id.sff8472.base.connector;
}

static int sff8472_get_lane_speed(const struct qsfp *qsfp,
                                  trx_lane_speed* lane_speed)
{
    *lane_speed = TRX_LANE_SPEED_10G;
    dev_notice(qsfp->dev, "%s: Lane speed: 0x%X \n", __func__, *lane_speed);

    return 0;
}

/*
 * Function to return the QSFP identifier value.
 */
static u8 sff8472_get_transceiver_type(const struct qsfp *qsfp)
{
    return qsfp->id.sff8472.base.phys_id;
}

static int sff8472_get_lanes_presence(const struct qsfp *qsfp,
                                      trx_lane_cfg* laneinfo)
{
    *laneinfo = 0x1;

    dev_notice(qsfp->dev, "%s: Lane info: 0x%X\n", __func__, *laneinfo);

    return 0;
}


static int sff8472_get_breakout_config(const struct qsfp *qsfp,
                                       trx_breakout_cfg* bout_config)
{
    *bout_config = TRX_FAR_END_NOT_MANAGED;
    return 0;
}

static void sff8472_tx_enable(const struct qsfp *qsfp)
{
    int ret;
    u8 ctrl;

    ret = qsfp_read(qsfp, SFF8472_STATUS_FLAGS, &ctrl,
                    sizeof(ctrl));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Failed to read control register. "
                           "ret %d\n", __func__, ret);
        return;
    }

    dev_notice(qsfp->dev, "%s: Ctrl: 0x%X\n", __func__, ctrl);
    ctrl &= (~SFF8472_TX_DISABLE);

    ret = qsfp_write(qsfp, SFF8472_STATUS_FLAGS, &ctrl,
                    sizeof(ctrl));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Failed to write control register. "
                           "ret %d\n", __func__, ret);
    }

}

static void sff8472_tx_disable(const struct qsfp *qsfp)
{
    int ret;
    u8 ctrl;

    ret = qsfp_read(qsfp, SFF8472_STATUS_FLAGS, &ctrl,
                    sizeof(ctrl));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Failed to read control register. "
                           "ret %d\n", __func__, ret);
        return;
    }

    dev_notice(qsfp->dev, "%s: Ctrl: 0x%X\n", __func__, ctrl);
    ctrl |= SFF8472_TX_DISABLE;

    ret = qsfp_write(qsfp, SFF8472_STATUS_FLAGS, &ctrl,
                    sizeof(ctrl));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Failed to write control register. "
                           "ret %d\n", __func__, ret);
    }

}

unsigned long sff8472_irq_delay(const struct qsfp *qsfp)
{
    return msecs_to_jiffies(100);
}

int sff8472_create_debugfs_files(struct qsfp *qsfp) {
    int ret;

    ret = module_debugfs_init(qsfp);
    if (ret != 0)
    {
        dev_err(qsfp->dev, "%s: qsfp module_spec_info debugfs dir fail \n",
                           __func__);
    }
    /* Need to implement debugfs support for SFF-8472 */
    return 0;
}

struct qsfp_spec_ops sff8472_spec_ops = {
    .mod_probe = sff8472_mod_probe,
    .disable_redundant_irq = sff8472_disable_redundant_irq,
    .get_state = sff8472_get_state,
    .tx_enable = sff8472_tx_enable,
    .tx_disable = sff8472_tx_disable,
    .check_features_impl = sff8472_check_feature_impl,
    .module_parse_power = sff8472_module_parse_power,
    .handle_max_power_exceed = sff8472_handle_max_power_exceed,
    .mod_high_power = sff8472_mod_high_power,
    .mod_low_power = sff8472_mod_low_power,
    .eeprom_print = sff8472_eeprom_print,
    .module_info = sff8472_module_info,
    .get_connector_type = sff8472_get_connector_type,
    .get_lane_speed = sff8472_get_lane_speed,
    .get_transceiver_type = sff8472_get_transceiver_type,
    .get_lanes_presence = sff8472_get_lanes_presence,
    .get_breakout_config = sff8472_get_breakout_config,
    .irq_delay = sff8472_irq_delay,
    .create_debugfs = sff8472_create_debugfs_files,
};
