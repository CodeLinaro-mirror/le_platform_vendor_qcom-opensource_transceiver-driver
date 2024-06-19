/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */
#include "qsfp.h"
#include "lane.h"
#include "transceiver_debugfs.h"
#include <linux/phylink.h>

static int sff8472_mod_probe(struct qsfp *qsfp)
{
    /* QSFP module inserted - read I2C data */
    struct qsfp_eeprom_id id = {0};
    u8 check;
    int ret;

    /* As there is no field in EEPROM to tell whether flat mem or not
     * so assume it flat mem
     */
    qsfp->module_flat_mem = 1;

    ret = qsfp_read(qsfp, SFF8472_ID, &id, sizeof(id.sff8472));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to read base EEPROM: %d", ret);
        return ret;
    }

    /* Validate the checksum over the base structure */
    check = qsfp_check(&id.sff8472.base, sizeof(id.sff8472.base) - 1);
    if (check != id.sff8472.base.cc_base) {
        TRX_LOG_ERR(qsfp, "EEPROM base structure checksum failure: "
                    "0x%02x != 0x%02x", check, id.sff8472.base.cc_base);
        return -EINVAL;
    }

    /* Validate the checksum over the extented structure */
    check = qsfp_check(&id.sff8472.ext, sizeof(id.sff8472.ext) - 1);
    if (check != id.sff8472.ext.cc_ext) {
        TRX_LOG_ERR(qsfp, "EEPROM extended structure checksum failure: "
                    "0x%02x != 0x%02x", check, id.sff8472.ext.cc_ext);
        return -EINVAL;
    }

    if (id.sff8472.base.phys_ext_id != SFP_PHYS_EXT_ID_SFP) {
        TRX_LOG_ERR(qsfp, "Extended id 0x%X didnot match",
                          id.sff8472.base.phys_ext_id);
        return -E_UNSUPPORTED_SPEC;
    }

    qsfp->id = id;
    qsfp->module_revision = id.sff8472.ext.sff8472_compliance;

    return 0;
}

static int sff8472_update_features_supported(struct qsfp *qsfp)
{
    struct qsfp_support *support = &qsfp->support;
    const struct sfp_eeprom_ext *ext = &qsfp->id.sff8472.ext;
    const __be16 lol_impl = cpu_to_be16(SFP_OPTIONS_RETIMER);
    u8 enh_flag_adv = 0;
    int ret;

    if (ext->enhopts & SFP_ENHOPTS_SOFT_RX_LOS) {
        support->rx_los = 1;
    }

    if (ext->enhopts & SFP_ENHOPTS_SOFT_TX_FAULT) {
        support->tx_fault = 1;
    }

    if (ext->enhopts & SFP_ENHOPTS_SOFT_TX_DISABLE) {
        support->tx_disable = 1;
    }

    if (ext->options & lol_impl) {
        support->rx_cdr_lol = 1;
        support->tx_cdr_lol = 1;
    }

    if (ext->enhopts & SFP_ENHOPTS_ALARMWARN) {
        support->temp_flags = 1;
        support->volt_flags = 1;
        support->rx_power_flags = 1;
        support->tx_power_flags = 1;
        support->tx_bias_flags = 1;
    }

    if (ext->enhopts & SFF8472_SOFT_RATE_SELECT_IMPL) {
        support->rate_select = 1;
    }

    /* TX LOS not supported by sff8472 */
    support->tx_los = 0;

    ret = qsfp_read(qsfp, SFF8472_ENH_FLAGS_ADV, &enh_flag_adv,
                    sizeof(enh_flag_adv));
    if (ret < 0) {
        /* For DAC it fails */
        if (!qsfp->id.sff8472.base.sfp_ct_passive) {
            TRX_LOG_ERR(qsfp, "Failed to read TX adap eq in fail. ret %d", ret);
            return ret;
        }
    } else if (enh_flag_adv & SFF8472_TX_ADAP_EQ_IN_FAIL_IMPL) {
        support->tx_adap_eq_in_fail = 1;
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

static int sff8472_disable_redundant_irq(struct qsfp *qsfp)
{
    /* sff8472 doesnot support disabling any interrupts and there are
     * no redundant interrupts as well to disable them
     */
    return 0;
}

static int sff8472_handle_max_power_exceed(struct qsfp *qsfp)
{
    return -E_MAX_POWER_EXCEED;
}

static int sff8472_set_rate_select(struct qsfp *qsfp, bool enable)
{
    int ret;
    u8 ctrl = 0;
    u8 rate_id = 0;
    bool rx_rs0 = false;
    bool tx_rs1 = false;

    ret = qsfp_read(qsfp, SFF8472_RATE_ID, &rate_id, sizeof(rate_id));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to read rate identifier register. "
                          "ret %d", ret);
        return ret;
    } else {
        TRX_LOG_INFO(qsfp, "Rate identifier 0x%X", rate_id);
    }

    switch (rate_id) {
    case 0x02:
    case 0x08:
        rx_rs0 = true;
        break;

    case 0x04:
        tx_rs1 = true;
        break;

    case 0x01:
    case 0x06:
    case 0x0A:
    case 0x0C:
    case 0x0E:
    case 0x10:
        rx_rs0 = true;
        tx_rs1 = true;
        break;

    default:
        rx_rs0 = false;
        tx_rs1 = false;
        TRX_LOG_INFO(qsfp, "Unspecified or Reserved Rate identifier 0x%X", rate_id);
        break;
    }

    /* FINISAR FTLF1436W5BTV RS0 and RS1 can control based on rate select bit
     * A0H bit 5 of byte 65 is set to 1 */
    if ((qsfp->id.sff8472.ext.options & cpu_to_be16(SFP_OPTIONS_RATE_SELECT)) &&
        (rate_id == 0x00))
    {
        rx_rs0 = true;
        tx_rs1 = true;
    }

    if (rx_rs0) {
        ret = qsfp_read(qsfp, SFF8472_STATUS_CTRL, &ctrl, sizeof(ctrl));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Failed to read status/control register. "
                              "ret %d", ret);
            return ret;
        }

        if(enable)
        {
            if (!(ctrl & SFF8472_RX_RATE_SELECT))
            {
                if (rate_id == 0x0E) {
                    TRX_LOG_WARN(qsfp, "RX Rate select not set by default");
                }

                ctrl |= SFF8472_RX_RATE_SELECT;

                ret = qsfp_write(qsfp, SFF8472_STATUS_CTRL, &ctrl, sizeof(ctrl));
                if (ret < 0) {
                    TRX_LOG_ERR(qsfp, "Failed to write status/control register. "
                                      "ret %d", ret);
                    return ret;
                }
            }
            else
            {
                TRX_LOG_WARN(qsfp, "RX Rate select was already set: 0x%X", ctrl);
            }
        }
        else
        {
            if (ctrl & SFF8472_TX_RATE_SELECT)
            {
                ctrl &= (~SFF8472_RX_RATE_SELECT);

                ret = qsfp_write(qsfp, SFF8472_STATUS_CTRL, &ctrl, sizeof(ctrl));
                if (ret < 0)
                {
                    TRX_LOG_ERR(qsfp, "Failed to write status/control register. "
                                      "ret %d", ret);
                    return ret;
                }
            }
            else
            {
                TRX_LOG_WARN(qsfp, "RX Rate select was already Reset: 0x%X", ctrl);
            }
        }
    }

    if (tx_rs1) {
        ctrl = 0;
        ret = qsfp_read(qsfp, SFF8472_EXT_MOD_CTRL, &ctrl, sizeof(ctrl));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Failed to read extended status/control register. "
                              "ret %d", ret);
            return ret;
        }

        if(enable)
        {
            if (!(ctrl & SFF8472_TX_RATE_SELECT))
            {
                if (rate_id == 0x0E) {
                    TRX_LOG_WARN(qsfp, "TX Rate select not set by default");
                }

                ctrl |= SFF8472_TX_RATE_SELECT;

                ret = qsfp_write(qsfp, SFF8472_EXT_MOD_CTRL, &ctrl, sizeof(ctrl));
                if (ret < 0) {
                    TRX_LOG_ERR(qsfp, "Failed to write extended status/control register. "
                                      "ret %d", ret);
                    return ret;
                }
            }
            else
            {
                TRX_LOG_WARN(qsfp, "TX Rate select was already set: 0x%X", ctrl);
            }
        }
        else
        {
            if (ctrl & SFF8472_TX_RATE_SELECT)
            {

                ctrl &= (~SFF8472_TX_RATE_SELECT);

                ret = qsfp_write(qsfp, SFF8472_EXT_MOD_CTRL, &ctrl, sizeof(ctrl));
                if (ret < 0) {
                    TRX_LOG_ERR(qsfp, "Failed to write extended status/control register. "
                                      "ret %d", ret);
                    return ret;
                }
            }
            else
            {
                TRX_LOG_WARN(qsfp, "TX Rate select was already Reset: 0x%X", ctrl);
            }
        }
    }

    return 0;
}

static int sff8472_mod_high_power(struct qsfp *qsfp)
{
    int ret;
    u8 val = 0;

    /* As power class 1 is the highest we can not push module for
     * further high power class
     */
    if (qsfp->module_power_class == 1) {
        return 0;
    }

    ret = qsfp_read(qsfp, SFF8472_EXT_MOD_CTRL, &val, sizeof(val));
    if (ret < 0) {
        return ret;
    }

    val |= SFF8472_HIGH_POWER;

    return qsfp_write(qsfp, SFF8472_EXT_MOD_CTRL, &val, sizeof(val));
}

static int sff8472_mod_low_power(struct qsfp *qsfp)
{
    int ret;
    u8 val = 0;

    /* As power class 1 is the highest we can not push module for
     * further high power class
     */
    if (qsfp->module_power_class == 1) {
        return 0;
    }

    ret = qsfp_read(qsfp, SFF8472_EXT_MOD_CTRL, &val, sizeof(val));
    if (ret < 0) {
        return ret;
    }

    val &= ~SFF8472_HIGH_POWER;

    return qsfp_write(qsfp, SFF8472_EXT_MOD_CTRL, &val, sizeof(val));
}

static void sff8472_eeprom_print(struct qsfp *qsfp)
{
    const struct sfp_eeprom_id *id = &qsfp->id.sff8472;
    char date[9];

    TRX_LOG_INFO(qsfp, "phys_id 0x%X phys_ext_id 0x%X connector 0x%X "
    "encoding 0x%X ", id->base.phys_id, id->base.phys_ext_id,
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

    TRX_LOG_INFO(qsfp, "date %s", date);

    TRX_LOG_INFO(qsfp, "vendor name %.*s",
                       (int)sizeof(id->base.vendor_name),
                       id->base.vendor_name);
    TRX_LOG_INFO(qsfp, "vendor pn %.*s",
                       (int)sizeof(id->base.vendor_pn),
                       id->base.vendor_pn);

    TRX_LOG_INFO(qsfp, "opt 0x%X enhopts 0x%X sff8472_compliance "
    "0x%X", id->ext.options, id->ext.enhopts,
    id->ext.sff8472_compliance);

    TRX_LOG_INFO(qsfp, "vendor sn %.*s",
                       (int)sizeof(id->ext.vendor_sn),
                       id->ext.vendor_sn);

    TRX_QXDM_LOG_INFO(qsfp, "Port-%u: SFP Vendor %.*s PN %.*s SN %.*s",
    qsfp->port_num, (int)sizeof(id->base.vendor_name), id->base.vendor_name,
    (int)sizeof(id->base.vendor_pn), id->base.vendor_pn,
    (int)sizeof(id->ext.vendor_sn), id->ext.vendor_sn);
}

static void sff8472_update_flags(struct qsfp *qsfp)
{
    int ret;
    bool err = false;
    struct sff8472_rxlos_txf rt = {0};
    struct sff8472_alarm_warn aw = {0};
    struct sff8472_cdr_lol lol = {0};
    struct sff8472_tx_adp_eq_in_fail taeif = {0};
    const __be16 rx_los_inverted = cpu_to_be16(SFP_OPTIONS_LOS_INVERTED);
    const __be16 rx_los_normal = cpu_to_be16(SFP_OPTIONS_LOS_NORMAL);
    __be16 rx_los_options;
    struct qsfp_flags *flags = &qsfp->flags;
    struct qsfp_support *support = &qsfp->support;

    /* RX LOS and TX Fault set in the same register */
    if (support->rx_los || support->tx_fault) {
        ret = qsfp_read(qsfp, SFF8472_STATUS_CTRL, &rt, sizeof(rt));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Failed to read RX los and TX fault. ret %d", ret);
            err = true;
        } else {
            rx_los_options = qsfp->id.sff8472.ext.options & (rx_los_inverted | rx_los_normal);

            if (rx_los_options == rx_los_normal) {
                flags->rx_los = rt.rx_los;
            } else if (rx_los_options == rx_los_inverted) {
                flags->rx_los = ~rt.rx_los;
            }

            flags->tx_fault = rt.tx_fault;
        }
    }

    /* RX and TX CDR LOL set in the same register */
    if (support->rx_cdr_lol || support->tx_cdr_lol) {
        ret = qsfp_read(qsfp, SFF8472_CDR_LOL, &lol, sizeof(lol));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Failed to read cdr lol register. ret %d", ret);
            err = true;
        } else {
            flags->rx_cdr_lol = lol.rx_cdr_lol;
            flags->tx_cdr_lol = lol.tx_cdr_lol;
        }
    }

    if (support->tx_adap_eq_in_fail) {
        ret = qsfp_read(qsfp, SFF8472_EXT_MOD_CTRL, &taeif, sizeof(taeif));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Failed to read TX adap eq fail register. ret %d", ret);
            err = true;
        } else {
            flags->tx_adap_eq_in_fail = taeif.tx_adap_eq_in_fail;
        }
    }

    if (qsfp->support.temp_flags || qsfp->support.volt_flags ||
        qsfp->support.rx_power_flags || qsfp->support.tx_power_flags ||
        qsfp->support.tx_bias_flags) {
        ret = qsfp_read(qsfp, SFF8472_ALARM_WARN, &aw, sizeof(aw));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Failed to read alarm warning register. ret %d", ret);
            err = true;
        } else {
            flags->temp_high_alarm = aw.temp_high_alarm | aw.laser_temp_high_alarm;
            flags->temp_low_alarm = aw.temp_low_alarm | aw.laser_temp_low_alarm;
            flags->temp_high_warn = aw.temp_high_warn | aw.laser_temp_high_warn;
            flags->temp_low_warn = aw.temp_low_warn | aw.laser_temp_low_warn;

            flags->volt_high_alarm = aw.volt_high_alarm;
            flags->volt_low_alarm = aw.volt_low_alarm;
            flags->volt_high_warn = aw.volt_high_warn;
            flags->volt_low_warn = aw.volt_low_warn;

            flags->rx1_power_high_alarm = aw.rx_power_high_alarm;
            flags->rx1_power_low_alarm = aw.rx_power_low_alarm;
            flags->rx1_power_high_warn = aw.rx_power_high_warn;
            flags->rx1_power_low_warn = aw.rx_power_low_warn;

            flags->tx1_power_high_alarm = aw.tx_power_high_alarm;
            flags->tx1_power_low_alarm = aw.tx_power_low_alarm;
            flags->tx1_power_high_warn = aw.tx_power_high_warn;
            flags->tx1_power_low_warn = aw.tx_power_low_warn;

            flags->tx1_bias_high_alarm = aw.tx_bias_high_alarm;
            flags->tx1_bias_low_alarm = aw.tx_bias_low_alarm;
            flags->tx1_bias_high_warn = aw.tx_bias_high_warn;
            flags->tx1_bias_low_warn = aw.tx_bias_low_warn;
        }
    }

    /* As SFF8472 completely rely on interrupt and polling used one time only
     * after probe, simulation and read fail.
     */
    if (err) {
        qsfp->need_poll = true;
    } else {
        qsfp->need_poll = false;
    }
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

static u8 sff8472_get_connector_type(struct qsfp *qsfp)
{
    return qsfp->id.sff8472.base.connector;
}

static int sff8472_get_lane_speed(struct qsfp *qsfp,
                                  trx_speed_mask *speed_mask)
{
    phy_interface_t sfp_interface = PHY_INTERFACE_MODE_NA;
    __ETHTOOL_DECLARE_LINK_MODE_MASK(sfp_supported) = { 0, };

    const struct sfp_eeprom_id *id = &qsfp->id.sff8472;
    trx_lane_speed lane_speed = TRX_LANE_SPEED_UNKNOWN;

    if (!qsfp->lane[0]) {
        TRX_LOG_ERR(qsfp, "Unable to get the lane");
        return -EINVAL;
    }

    sfp_parse_support(qsfp->lane[0]->sfp_bus, &qsfp->id.sff8472,
                      sfp_supported);

    /* sfp_parse_support() from upstream wont consider SFF8024_ECC_100G_25GAUI_C2M_AOC
     * so added extra check for it.
     */
    if (phylink_test(sfp_supported, 25000baseCR_Full) ||
        phylink_test(sfp_supported, 25000baseKR_Full) ||
        phylink_test(sfp_supported, 25000baseSR_Full) ||
        (qsfp->id.sff8472.base.extended_cc == SFF8024_ECC_100G_25GAUI_C2M_AOC) ||
        (qsfp->id.sff8472.base.extended_cc == SFF8024_ECC_100GBASE_LR4_25GBASE_LR) ||
        (qsfp->id.sff8472.base.extended_cc == SFF8024_ECC_100GBASE_ER4_25GBASE_ER))
    {
        lane_speed |= TRX_LANE_SPEED_25G;
    }

    if (phylink_test(sfp_supported, 10000baseCR_Full) ||
        phylink_test(sfp_supported, 10000baseSR_Full) ||
        phylink_test(sfp_supported, 10000baseLR_Full) ||
        phylink_test(sfp_supported, 10000baseLRM_Full) ||
        phylink_test(sfp_supported, 10000baseER_Full) ||
        phylink_test(sfp_supported, 10000baseT_Full)) {
        lane_speed |= TRX_LANE_SPEED_10G;
    }

    if (lane_speed == TRX_LANE_SPEED_UNKNOWN) {
        sfp_interface = sfp_select_interface(qsfp->lane[0]->sfp_bus, sfp_supported);
        if ((sfp_interface == PHY_INTERFACE_MODE_5GBASER) ||
            (sfp_interface == PHY_INTERFACE_MODE_2500BASEX) ||
            (sfp_interface == PHY_INTERFACE_MODE_SGMII) ||
            (sfp_interface == PHY_INTERFACE_MODE_1000BASEX) ||
            (sfp_interface == PHY_INTERFACE_MODE_100BASEX)) {

            TRX_LOG_ERR(qsfp, "Unsupported SFP interface: 0x%X", sfp_interface);
            return -EINVAL;
        } else {
            TRX_LOG_ERR(qsfp, "Unable to get the lane speed. 0x%X", sfp_interface);
            return -EINVAL;
        }
    }

    /* Handle the Dual speed support transceiver that are not
     * Advertised through EEPROM */
     if ((strncmp(id->base.vendor_pn,"SFP-10/25GSR-85 ",16) == 0) ||
          (strncmp(id->base.vendor_pn,"FTLF1436W5BTV",13) == 0) ||
          (strncmp(id->base.vendor_pn,"M14MK",5) == 0))
     {
         lane_speed |= TRX_LANE_SPEED_10G;
     }

    *speed_mask = (trx_speed_mask)lane_speed;
    return 0;
}

/*
 * Function to return the QSFP identifier value.
 */
static u8 sff8472_get_transceiver_type(struct qsfp *qsfp)
{
    return qsfp->id.sff8472.base.phys_id;
}

/*
 * Function to return the link length range.
 */
static trx_link_length_range sff8472_get_link_length_range(struct qsfp *qsfp)
{
    return qsfp_link_code_to_link_length_range(qsfp->id.sff8472.base.extended_cc);
}

static int sff8472_get_lanes_presence(struct qsfp *qsfp,
                                      trx_lane_cfg* laneinfo)
{
    /* Only one lane supported by sfp */
    *laneinfo = 0x1;

    return 0;
}


static int sff8472_get_breakout_config(struct qsfp *qsfp,
                                       trx_breakout_cfg* bout_config)
{
    *bout_config = TRX_FAR_END_NOT_MANAGED;
    return 0;
}

static int sff8472_lane_tx_enable(struct lane *lane)
{
    int ret;
    u8 ctrl = 0;

    if (lane->lane_num != 0) {
        return -EINVAL;
    }

    ret = qsfp_read(lane->qsfp, SFF8472_STATUS_CTRL, &ctrl, sizeof(ctrl));
    if (ret < 0) {
        return ret;
    }

    if (!(ctrl & SFF8472_TX_DISABLE)) {
        return 0;
    }

    ctrl &= (~SFF8472_TX_DISABLE);

    return qsfp_write(lane->qsfp, SFF8472_STATUS_CTRL, &ctrl, sizeof(ctrl));
}

static int sff8472_mod_tx_disable(struct qsfp *qsfp)
{
    int ret;
    u8 ctrl = 0;

    ret = qsfp_read(qsfp, SFF8472_STATUS_CTRL, &ctrl, sizeof(ctrl));
    if (ret < 0) {
        return ret;
    }

    if (ctrl & SFF8472_TX_DISABLE) {
        return 0;
    }

    ctrl |= SFF8472_TX_DISABLE;

    return qsfp_write(qsfp, SFF8472_STATUS_CTRL, &ctrl, sizeof(ctrl));
}

static int sff8472_lane_tx_disable(struct lane *lane)
{
    if (lane->lane_num == 0) {
        return sff8472_mod_tx_disable(lane->qsfp);
    } else {
        return -EINVAL;
    }
}

unsigned long sff8472_irq_delay(struct qsfp *qsfp)
{
    return msecs_to_jiffies(100);
}

const struct qsfp_spec_ops sff8472_spec_ops = {
    .mod_probe = sff8472_mod_probe,
    .disable_redundant_irq = sff8472_disable_redundant_irq,
    .update_flags = sff8472_update_flags,
    .mod_tx_disable = sff8472_mod_tx_disable,
    .lane_tx_enable = sff8472_lane_tx_enable,
    .lane_tx_disable = sff8472_lane_tx_disable,
    .update_features_supported = sff8472_update_features_supported,
    .module_parse_power = sff8472_module_parse_power,
    .handle_max_power_exceed = sff8472_handle_max_power_exceed,
    .mod_high_power = sff8472_mod_high_power,
    .mod_low_power = sff8472_mod_low_power,
    .eeprom_print = sff8472_eeprom_print,
    .module_info = sff8472_module_info,
    .get_connector_type = sff8472_get_connector_type,
    .get_lane_speed = sff8472_get_lane_speed,
    .get_transceiver_type = sff8472_get_transceiver_type,
    .get_link_length_range = sff8472_get_link_length_range,
    .get_lanes_presence = sff8472_get_lanes_presence,
    .get_breakout_config = sff8472_get_breakout_config,
    .set_rate_select = sff8472_set_rate_select,
    .irq_delay = sff8472_irq_delay,
    .create_debugfs = sff8472_create_debugfs_files,
    .spec_sensor_sysfs_init = sff8472_create_sysfs_files,
    .spec_sensor_sysfs_exit = sff8472_remove_sysfs_files,
};
