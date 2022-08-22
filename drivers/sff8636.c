/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Code is derived from http://git.armlinux.org.uk/cgit/linux-arm.git/
 * tree/drivers/net/phy/qsfp.c?h=cex7
 */
#include "qsfp.h"

static int sff8636_mod_probe(struct qsfp *qsfp, bool report)
{
    /* QSFP module inserted - read I2C data */
    struct sff8636_id_stat id_stat;
    struct qsfp_eeprom_id id;
    u8 check;
    int ret;

    ret = qsfp_read(qsfp, 0, &id_stat, sizeof(id_stat));
    if (ret < 0) {
        if (report)
            dev_err(qsfp->dev, "%s: Failed to read EEPROM: %d\n",
                                __func__, ret);
        return -EAGAIN;
    }

    dev_notice(qsfp->dev, "%s: id_stat id %X rev %X status %X\n", __func__,
               id_stat.phys_id, id_stat.rev_spec, id_stat.status);

    // Early setup - we need to know if this module has a page register
    qsfp->module_flat_mem = id_stat.status & SFF8636_STAT_FLAT_MEM;
    qsfp->module_revision = id_stat.rev_spec;

    ret = qsfp_read(qsfp, SFF8636_ID, &id, sizeof(id.sff8636));
    if (ret < 0) {
        if (report)
            dev_err(qsfp->dev, "%s: Failed to read EEPROM: %d\n",
                                __func__, ret);
        return -EAGAIN;
    }

    if (id.sff8636.base.phys_id != id_stat.phys_id) {
        dev_err(qsfp->dev, "%s: QSFP phys_id mismatch: "
                            "0x%02x != 0x%02x\n", __func__,
                            id_stat.phys_id, id.sff8636.base.phys_id);
        return -EINVAL;
    }

    // Validate the checksum over the base structure
    check = qsfp_check(&id.sff8636.base, sizeof(id.sff8636.base) - 1);
    if (check != id.sff8636.base.cc_base) {
        dev_err(qsfp->dev,
                "%s: EEPROM base structure checksum failure: \
                0x%02x != 0x%02x\n", __func__, check, id.sff8636.base.cc_base);
        return -EINVAL;
    }

    // Validate the checksum over the extended structure
    check = qsfp_check(&id.sff8636.ext, sizeof(id.sff8636.ext) - 1);
    if (check != id.sff8636.ext.cc_ext) {
        dev_err(qsfp->dev,
                "%s: EEPROM extended structure checksum failure: \
                0x%02x != 0x%02x\n",__func__, check, id.sff8636.ext.cc_ext);
        memset(&id.sff8636.ext, 0, sizeof(id.sff8636.ext));
    }

    qsfp->id = id;

    return 0;
}

static int sff8636_check_feature_impl(const struct qsfp *qsfp)
{
    if (!(qsfp->id.sff8636.ext.options[2] & SFF8636_TX_LOS_IMPLEMENTED)) {
        dev_err(qsfp->dev, "%s: TX LOS not implemented\n", __func__);
        return -E_NOT_IMPL;
    }

    if (!(qsfp->id.sff8636.ext.options[2] & SFF8636_TX_FAULT_IMPLEMENTED)) {
        dev_err(qsfp->dev, "%s: TX Fault not implemented\n", __func__);
        return -E_NOT_IMPL;
    }

    if (!(qsfp->id.sff8636.ext.options[2] & SFF8636_TX_DISABLE_IMPLEMENTED)) {
        dev_err(qsfp->dev, "%s: TX Disable not implemented\n", __func__);
        return -E_NOT_IMPL;
    }

    return 0;
}

static int sff8636_module_parse_power(struct qsfp *qsfp)
{
    u32 power_mW, power_class;
    u8 pwr, mask;
    int ret;

    if (qsfp->module_revision >= SFF8636_REV_8636_2_8 &&
        qsfp->id.sff8636.base.phys_ext_id & BIT(5)) {

        ret = qsfp_read(qsfp, SFF8636_CLS8_MAX_POWER, &pwr,
                        sizeof(pwr));
        if (ret < 0)
            return ret;

        power_class = 8;
        power_mW = pwr * 100;
    } else {
        if (qsfp->id.sff8636.base.phys_id == SFF8024_ID_QSFP_8438)
            mask = 0xc0;
        else
            mask = 0xc3;

        switch (qsfp->id.sff8636.base.phys_ext_id & mask) {
        default:
            power_mW = 1500;
            power_class = 1;
            break;
        case 0x40:
            power_mW = 2000;
            power_class = 2;
            break;
        case 0x80:
            power_mW = 2500;
            power_class = 3;
            break;
        case 0xc0:
            power_mW = 3500;
            power_class = 4;
            break;
        case 0xc1:
            power_mW = 4000;
            power_class = 5;
            break;
        case 0xc2:
            power_mW = 4500;
            power_class = 6;
            break;
        case 0xc3:
            power_mW = 5000;
            power_class = 7;
            break;
        }
    }

    qsfp->module_power_mW = power_mW;
    qsfp->module_power_class = power_class;

    return 0;
}

/*
 * Disable the interrupts (Temperature, Voltage alarms, vendor specific)
 * which are not handled by this driver
 */
static void sff8636_disable_redundant_irq(const struct qsfp *qsfp)
{
    int ret;
    u8 buf[] = {0xFF, 0xFF, 0xFF, 0xFF};

    ret = qsfp_write(qsfp, SFF8636_TEMPERATURE_INTERRUPT_MASK,
                     buf, sizeof(buf));
    if (ret < 0)
        dev_err(qsfp->dev, "%s: Failed to mask redundant interrupts. "
                           "ret %d\n", __func__, ret);
}

static int sff8636_handle_max_power_exceed(const struct qsfp *qsfp)
{
    u8 val = SFF8636_POWER_CLASS_1TO4;

    return qsfp_write(qsfp, SFF8636_POWER_ENABLE, &val, sizeof(val));
}

static int sff8636_mod_high_power(const struct qsfp *qsfp)
{
    u8 val = SFF8636_POWER_CLASS_HIGH;

    return qsfp_write(qsfp, SFF8636_POWER_ENABLE, &val, sizeof(val));
}

static int sff8636_mod_low_power(const struct qsfp *qsfp)
{
    u8 val = SFF8636_POWER_CLASS_LOW;

    return qsfp_write(qsfp, SFF8636_POWER_ENABLE, &val, sizeof(val));
}

static void sff8636_eeprom_print(const struct qsfp *qsfp)
{
    const struct sff8636_eeprom_id *id = &qsfp->id.sff8636;
    char date[9];

    dev_notice(qsfp->dev, "%s: phys_id %X  phys_ext_id %X  connector %X\n",
    __func__, id->base.phys_id, id->base.phys_ext_id, id->base.connector);

    dev_notice(qsfp->dev, "%s: ecom_extended %X   e10g_base_lrm %X   "
    "e10g_base_lr %X  e10g_base_sr %X  e40g_base_cr4 %X  e40g_base_sr4 %X   "
    "e40g_base_lr4 %X   e40g_active %X\n", __func__, id->base.ecom_extended,
    id->base.e10g_base_lrm, id->base.e10g_base_lr, id->base.e10g_base_sr,
    id->base.e40g_base_cr4, id->base.e40g_base_sr4, id->base.e40g_base_lr4,
    id->base.e40g_active);

    dev_notice(qsfp->dev, "%s: reserved_2 %X  sonet_oc48_smf_long_reach %X  "
    "sonet_oc48_smf_intermediate_reach %X   sonet_oc48_short_reach %X\n",
    __func__, id->base.reserved_2, id->base.sonet_oc48_smf_long_reach,
    id->base.sonet_oc48_smf_intermediate_reach,
    id->base.sonet_oc48_short_reach);

    dev_notice(qsfp->dev, "%s: sas_24gbps %X  sas_12gbps %X  sas_6gbps %X "
    "sas_3gbps %X   reserved_3 %X\n", __func__, id->base.sas_24gbps,
    id->base.sas_12gbps,id->base.sas_6gbps, id->base.sas_3gbps,
    id->base.reserved_3);

    dev_notice(qsfp->dev, "%s: reserved_4 %X   e1000_base_t %X   "
    "e1000_base_cx %X   e1000_base_lx %X   e1000_base_sx %X\n", __func__,
    id->base.reserved_4, id->base.e1000_base_t, id->base.e1000_base_cx,
    id->base.e1000_base_lx, id->base.e1000_base_sx);

    dev_notice(qsfp->dev, "%s: fc_ll_v %X  fc_ll_s %X  fc_ll_i %X  fc_ll_l %X"
    "  fc_ll_m %X   reserved_5 %X   fc_tech_lc %X   "
    "fc_tech_electrical_inter_enclosure %X\n", __func__, id->base.fc_ll_v,
    id->base.fc_ll_s, id->base.fc_ll_i, id->base.fc_ll_l, id->base.fc_ll_m,
    id->base.reserved_5, id->base.fc_tech_lc,
    id->base.fc_tech_electrical_inter_enclosure);

    dev_notice(qsfp->dev, "%s: electrical_intra_enclosure %X   "
    "longwave_laser_wo_ofc %X   longwave_laser_w_ofc %X   longwave_laser %X  "
    "reserved_6 %X\n", __func__, id->base.electrical_intra_enclosure,
    id->base.longwave_laser_wo_ofc, id->base.longwave_laser_w_ofc,
    id->base.longwave_laser, id->base.reserved_6);

    dev_notice(qsfp->dev, "%s: fc_media_tw %X   fc_media_tp %X   "
    "fc_media_mi %X   fc_media_tv %X   fc_media_m6 %X   fc_media_m5 %X   "
    "fc_media_om3 %X   fc_media_sm %X\n", __func__, id->base.fc_media_tw,
    id->base.fc_media_tp, id->base.fc_media_mi, id->base.fc_media_tv,
    id->base.fc_media_m6, id->base.fc_media_m5, id->base.fc_media_om3,
    id->base.fc_media_sm);

    dev_notice(qsfp->dev, "%s: fc_speed_1200 %X   fc_speed_800 %X   "
    "fc_speed_1600 %X   fc_speed_400 %X   fc_speed_3200 %X   fc_speed_200 %X "
    "  fc_extended %X   fc_speed_100 %X\n", __func__, id->base.fc_speed_1200,
    id->base.fc_speed_800, id->base.fc_speed_1600, id->base.fc_speed_400,
    id->base.fc_speed_3200, id->base.fc_speed_200, id->base.fc_extended,
    id->base.fc_speed_100);

    dev_notice(qsfp->dev, "%s: encoding %X   br_nominal %X   "
    "ext_ratesel_spec %X\n", __func__, id->base.encoding,
    id->base.br_nominal, id->base.ext_ratesel_spec);

    dev_notice(qsfp->dev, "%s: length[]: %X %X %X %X %X\n", __func__,
    id->base.length[4], id->base.length[3], id->base.length[2],
    id->base.length[1], id->base.length[0]);

    dev_notice(qsfp->dev, "%s: device_tech %X\n", __func__,
                           id->base.device_tech);

    dev_notice(qsfp->dev, "%s: vendor name %.*s\n", __func__,
                           (int)sizeof(id->base.vendor_name),
                           id->base.vendor_name);

    dev_notice(qsfp->dev, "%s: ext_module %X\n", __func__,
                           id->base.ext_module);

    dev_notice(qsfp->dev, "%s: vendor_oui[]: %X %X %X\n", __func__,
    id->base.vendor_oui[2], id->base.vendor_oui[1], id->base.vendor_oui[0]);

    dev_notice(qsfp->dev, "%s: vendor pn %.*s\n", __func__,
               (int)sizeof(id->base.vendor_pn), id->base.vendor_pn);

    dev_notice(qsfp->dev, "%s: vendor rev %.*s\n", __func__,
               (int)sizeof(id->base.vendor_rev), id->base.vendor_rev);

    dev_notice(qsfp->dev, "%s: wavelength %X   wavelength_tolerance %X   "
    "max_case_temp %X   cc_base %X\n", __func__, id->base.wavelength,
    id->base.wavelength_tolerance, id->base.max_case_temp,
    id->base.cc_base);

    dev_notice(qsfp->dev, "%s: link_codes %X\n", __func__,
                           id->ext.link_codes);

    dev_notice(qsfp->dev, "%s: options[]: %X %X %X\n", __func__,
               id->ext.options[2], id->ext.options[1], id->ext.options[0]);

    dev_notice(qsfp->dev, "%s: vendor sn %.*s\n", __func__,
               (int)sizeof(id->ext.vendor_sn), id->ext.vendor_sn);

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
    dev_notice(qsfp->dev, "%s: diagmon %X   enh_options %X   "
    "baud_rate_nominal %X    cc_ext %X\n", __func__, id->ext.diagmon,
    id->ext.enh_options, id->ext.baud_rate_nominal, id->ext.cc_ext);

}

static u8 sff8636_soft_get_state(struct qsfp *qsfp)
{
    int ret;
    u8 state = 0;
    u8 irq_status[SFF8636_INTERRUPT_FLAG_SIZE] = {0};

    ret = qsfp_read(qsfp, SFF8636_IRQ_FLAGS, irq_status,
                    sizeof(irq_status));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Failed to read QSFP IRQ status. "
                           "ret %d\n", __func__, ret);
        /* Preserve the current state */
        return qsfp->state;
    }

    if (irq_status[0])
        dev_notice(qsfp->dev, "%s: LOS state %X trigger %X\n",__func__,
                              qsfp->los_state, irq_status[0]);

    /* Either TX or RX LOS in any one of 4 lines
     * Assuming interrupt coming for LOS recovery as well
     * los_state stores RX TX LOS for 4 lanes
     * NoLOS -> 0 -> NoLOS
     * NoLOS -> 1 -> LOS
     * LOS   -> 0 -> LOS
     * LOS   -> 1 -> NoLOS
     */
    qsfp->los_state ^= irq_status[0];

    if (qsfp->los_state)
        state |= QSFP_F_LOS;

    if (irq_status[1] & 0xF)
        dev_notice(qsfp->dev, "%s: TX Fault state %X trigger %X\n", __func__,
                              qsfp->tx_fault_state, irq_status[1] & 0xF);

    /* TX fault in any one of 4 lines */
    qsfp->tx_fault_state ^= (irq_status[1] & 0xF);

    if (qsfp->tx_fault_state)
        state |= QSFP_F_TX_FAULT;

    dev_notice(qsfp->dev, "%s: IRQ status dump: LOS %X TX Fault %X LOL %X "
    "Init Temp %X VCC %X Vendor %X Power %X %X %X %X %X %X Reserved %X %X "
    "%X %X Vendor %X %X %X\n", __func__, irq_status[0], irq_status[1],
    irq_status[2], irq_status[3], irq_status[4], irq_status[5], irq_status[6],
    irq_status[7], irq_status[8], irq_status[9], irq_status[10],
    irq_status[11], irq_status[12], irq_status[13], irq_status[14],
    irq_status[15], irq_status[16], irq_status[17], irq_status[18]);

    return state;
}

static void sff8636_tx_disable(const struct qsfp *qsfp)
{
    u8 status;
    int ret;

    status = 0xF;

    ret = qsfp_write(qsfp, SFF8636_TX_DISABLE, &status,
                     sizeof(status));
    if (ret < 0)
        dev_err(qsfp->dev, "%s: TX disable failed. ret %d\n", __func__, ret);

}

static void sff8636_tx_enable(const struct qsfp *qsfp)
{
    u8 status;
    int ret;

    status = 0;

    ret = qsfp_write(qsfp, SFF8636_TX_DISABLE, &status,
                     sizeof(status));
    if (ret < 0)
        dev_err(qsfp->dev, "%s: TX enable failed. ret %d\n", __func__, ret);

}

static int sff8636_module_info(struct qsfp *qsfp, struct ethtool_modinfo *modinfo)
{
    switch (qsfp->id.sff8636.base.phys_id) {
    case ETH_MODULE_SFF_8636:
        modinfo->type = ETH_MODULE_SFF_8636;
        modinfo->eeprom_len = ETH_MODULE_SFF_8636_LEN;
        break;

    case ETH_MODULE_SFF_8436:
        modinfo->type = ETH_MODULE_SFF_8436;
        modinfo->eeprom_len = ETH_MODULE_SFF_8436_LEN;
        break;

    default:
        modinfo->type = 0;
        modinfo->eeprom_len = 0;
        break;
    }

    return 0;
}

struct qsfp_spec_ops sff8636_spec_ops = {
    .mod_probe = sff8636_mod_probe,
    .disable_redundant_irq = sff8636_disable_redundant_irq,
    .soft_get_state = sff8636_soft_get_state,
    .tx_enable = sff8636_tx_enable,
    .tx_disable = sff8636_tx_disable,
    .check_features_impl = sff8636_check_feature_impl,
    .module_parse_power = sff8636_module_parse_power,
    .handle_max_power_exceed = sff8636_handle_max_power_exceed,
    .mod_high_power = sff8636_mod_high_power,
    .mod_low_power = sff8636_mod_low_power,
    .eeprom_print = sff8636_eeprom_print,
    .module_info = sff8636_module_info,
};
