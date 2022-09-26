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

    dev_notice(qsfp->dev, "%s: id_stat id 0x%X rev 0x%X flat_mem 0x%X\n", __func__,
               id_stat.phys_id, id_stat.rev_spec, id_stat.flat_mem);

    // Early setup - we need to know if this module has a page register
    qsfp->module_flat_mem = id_stat.flat_mem;
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
    if (!qsfp->id.sff8636.ext.tx_los_impl)
        dev_warn(qsfp->dev, "%s: TX LOS not implemented\n", __func__);

    if (!qsfp->id.sff8636.ext.tx_fault_impl)
        dev_warn(qsfp->dev, "%s: TX Fault not implemented\n", __func__);

    if (!qsfp->id.sff8636.ext.tx_dis_impl)
        dev_warn(qsfp->dev, "%s: TX Disable not implemented\n", __func__);

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
    /* Enable only TX/RX LOS and TX Fault intterupts */
    u8 buf1[] = {0x00, /* TX LOS , RX LOS enable */
                 0xF0, /* TX Fault enable */
                 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    u8 buf2[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    ret = qsfp_write(qsfp, SFF8636_INTERRUPT_MASK,
                     buf1, sizeof(buf1));
    if (ret < 0)
        dev_err(qsfp->dev, "%s: Failed to mask redundant interrupts. "
                           "ret %d\n", __func__, ret);

    ret = qsfp_write(qsfp, SFF8636_CHANNEL_INTERRUPT_MASK,
                     buf2, sizeof(buf2));
    if (ret < 0)
        dev_err(qsfp->dev, "%s: Failed to mask redundant channel interrupts."
                           " ret %d\n", __func__, ret);

}

static int sff8636_handle_max_power_exceed(const struct qsfp *qsfp)
{
    u8 val = SFF8636_POWER_CLASS_1TO4;

    return qsfp_write(qsfp, SFF8636_POWER_ENABLE, &val, sizeof(val));
}

static int sff8636_mod_high_power(const struct qsfp *qsfp)
{
    u8 val = SFF8636_POWER_CLASS_HIGH;

    /* As power class 1 is the lowest we can not push module for
     * further high power class
     */
    if (qsfp->module_power_class == 1)
        return 0;

    return qsfp_write(qsfp, SFF8636_POWER_ENABLE, &val, sizeof(val));
}

static int sff8636_mod_low_power(const struct qsfp *qsfp)
{
    u8 val = SFF8636_POWER_CLASS_LOW;

    /* As power class 1 is the lowest we can not push module for
     * further low power class
     */
    if (qsfp->module_power_class == 1)
        return 0;

    return qsfp_write(qsfp, SFF8636_POWER_ENABLE, &val, sizeof(val));
}

static void sff8636_eeprom_print(const struct qsfp *qsfp)
{
    const struct sff8636_eeprom_id *id = &qsfp->id.sff8636;
    char date[9];

    dev_notice(qsfp->dev, "%s: phys_id 0x%X  phys_ext_id 0x%X  connector 0x%X\n",
    __func__, id->base.phys_id, id->base.phys_ext_id, id->base.connector);

    dev_notice(qsfp->dev, "%s: ecom_extended 0x%X   e10g_base_lrm 0x%X   "
    "e10g_base_lr 0x%X  e10g_base_sr 0x%X  e40g_base_cr4 0x%X  e40g_base_sr4 0x%X   "
    "e40g_base_lr4 0x%X   e40g_active 0x%X\n", __func__, id->base.ecom_extended,
    id->base.e10g_base_lrm, id->base.e10g_base_lr, id->base.e10g_base_sr,
    id->base.e40g_base_cr4, id->base.e40g_base_sr4, id->base.e40g_base_lr4,
    id->base.e40g_active);

    dev_notice(qsfp->dev, "%s: reserved_2 0x%X  "
    "sonet_oc48_smf_long_reach 0x%X  sonet_oc48_smf_intermediate_reach 0x%X"
    "   sonet_oc48_short_reach 0x%X\n", __func__, id->base.reserved_2,
    id->base.sonet_oc48_smf_long_reach,
    id->base.sonet_oc48_smf_intermediate_reach,
    id->base.sonet_oc48_short_reach);

    dev_notice(qsfp->dev, "%s: sas_24gbps 0x%X  sas_12gbps 0x%X  "
    "sas_6gbps 0x%X  sas_3gbps 0x%X   reserved_3 0x%X\n", __func__,
    id->base.sas_24gbps, id->base.sas_12gbps,id->base.sas_6gbps,
    id->base.sas_3gbps, id->base.reserved_3);

    dev_notice(qsfp->dev, "%s: reserved_4 0x%X   e1000_base_t 0x%X   "
    "e1000_base_cx 0x%X   e1000_base_lx 0x%X   e1000_base_sx 0x%X\n", __func__,
    id->base.reserved_4, id->base.e1000_base_t, id->base.e1000_base_cx,
    id->base.e1000_base_lx, id->base.e1000_base_sx);

    dev_notice(qsfp->dev, "%s: fc_ll_v 0x%X  fc_ll_s 0x%X  fc_ll_i 0x%X"
    "  fc_ll_l 0x%X  fc_ll_m 0x%X   reserved_5 0x%X   fc_tech_lc 0x%X   "
    "fc_tech_electrical_inter_enclosure 0x%X\n", __func__, id->base.fc_ll_v,
    id->base.fc_ll_s, id->base.fc_ll_i, id->base.fc_ll_l, id->base.fc_ll_m,
    id->base.reserved_5, id->base.fc_tech_lc,
    id->base.fc_tech_electrical_inter_enclosure);

    dev_notice(qsfp->dev, "%s: electrical_intra_enclosure 0x%X   "
    "longwave_laser_wo_ofc 0x%X   longwave_laser_w_ofc 0x%X   "
    "longwave_laser 0x%X  reserved_6 %X\n", __func__,
    id->base.electrical_intra_enclosure,
    id->base.longwave_laser_wo_ofc, id->base.longwave_laser_w_ofc,
    id->base.longwave_laser, id->base.reserved_6);

    dev_notice(qsfp->dev, "%s: fc_media_tw 0x%X   fc_media_tp 0x%X   "
    "fc_media_mi 0x%X   fc_media_tv 0x%X   fc_media_m6 0x%X   "
    "fc_media_m5 0x%X   fc_media_om3 0x%X   fc_media_sm 0x%X\n", __func__,
    id->base.fc_media_tw, id->base.fc_media_tp, id->base.fc_media_mi,
    id->base.fc_media_tv, id->base.fc_media_m6, id->base.fc_media_m5,
    id->base.fc_media_om3, id->base.fc_media_sm);

    dev_notice(qsfp->dev, "%s: fc_speed_1200 0x%X   fc_speed_800 0x%X   "
    "fc_speed_1600 0x%X   fc_speed_400 0x%X   fc_speed_3200 0x%X   "
    "fc_speed_200 0x%X   fc_extended 0x%X   fc_speed_100 0x%X\n", __func__,
    id->base.fc_speed_1200, id->base.fc_speed_800, id->base.fc_speed_1600,
    id->base.fc_speed_400, id->base.fc_speed_3200, id->base.fc_speed_200,
    id->base.fc_extended, id->base.fc_speed_100);

    dev_notice(qsfp->dev, "%s: encoding 0x%X   br_nominal 0x%X   "
    "ext_ratesel_spec 0x%X\n", __func__, id->base.encoding,
    id->base.br_nominal, id->base.ext_ratesel_spec);

    dev_notice(qsfp->dev, "%s: length[]: 0x%X 0x%X 0x%X 0x%X 0x%X\n", __func__,
    id->base.length[4], id->base.length[3], id->base.length[2],
    id->base.length[1], id->base.length[0]);

    dev_notice(qsfp->dev, "%s: device_tech 0x%X\n", __func__,
                           id->base.device_tech);

    dev_notice(qsfp->dev, "%s: vendor name %.*s\n", __func__,
                           (int)sizeof(id->base.vendor_name),
                           id->base.vendor_name);

    dev_notice(qsfp->dev, "%s: ext_module 0x%X\n", __func__,
                           id->base.ext_module);

    dev_notice(qsfp->dev, "%s: vendor_oui[]: 0x%X 0x%X 0x%X\n", __func__,
    id->base.vendor_oui[2], id->base.vendor_oui[1], id->base.vendor_oui[0]);

    dev_notice(qsfp->dev, "%s: vendor pn %.*s\n", __func__,
               (int)sizeof(id->base.vendor_pn), id->base.vendor_pn);

    dev_notice(qsfp->dev, "%s: vendor rev %.*s\n", __func__,
               (int)sizeof(id->base.vendor_rev), id->base.vendor_rev);

    dev_notice(qsfp->dev, "%s: wavelength 0x%X   wavelength_tolerance 0x%X   "
    "max_case_temp 0x%X   cc_base 0x%X\n", __func__, id->base.wavelength,
    id->base.wavelength_tolerance, id->base.max_case_temp,
    id->base.cc_base);

    dev_notice(qsfp->dev, "%s: link_codes 0x%X\n", __func__,
                           id->ext.link_codes);

    dev_notice(qsfp->dev, "%s: lpmode_gpio 0x%X  intl_gpio 0x%X "
    "tx_adap_eq_freeze 0x%X  tx_eq_auto_adap 0x%X  tx_eq_prg 0x%X  "
    "rx_emp_prg 0x%X  rx_amp_prg 0x%X\n", __func__, id->ext.lpmode_gpio,
    id->ext.intl_gpio, id->ext.tx_adap_eq_freeze, id->ext.tx_eq_auto_adap,
    id->ext.tx_eq_prg, id->ext.rx_emp_prg, id->ext.rx_amp_prg);

    dev_notice(qsfp->dev, "%s: tx_cdr_ctrl_impl 0x%X  rx_cdr_ctrl_impl 0x%X  "
    "tx_cdr_lol_impl 0x%X  rx_cdr_lol_impl 0x%X  rx_squelch_dis_impl 0x%X  "
    "rx_output_dis_impl 0x%X  tx_squelch_dis_impl 0x%X  "
    "tx_squelch_impl 0x%X\n", __func__, id->ext.tx_cdr_ctrl_impl,
    id->ext.rx_cdr_ctrl_impl, id->ext.tx_cdr_lol_impl, id->ext.rx_cdr_lol_impl,
    id->ext.rx_squelch_dis_impl, id->ext.rx_output_dis_impl,
    id->ext.tx_squelch_dis_impl, id->ext.tx_squelch_impl);

    dev_notice(qsfp->dev, "%s: page2 0x%X  page1 0x%X  rate_select_impl 0x%X  "
    "tx_dis_impl 0x%X  tx_fault_impl 0x%X  tx_squelch_oma_impl 0x%X  "
    "tx_los_impl 0x%X  page20_21 0x%X\n", __func__, id->ext.page2,
    id->ext.page1, id->ext.rate_select_impl, id->ext.tx_dis_impl,
    id->ext.tx_fault_impl, id->ext.tx_squelch_oma_impl, id->ext.tx_los_impl,
    id->ext.page20_21);

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
    dev_notice(qsfp->dev, "%s: diagmon 0x%X   enh_options 0x%X   "
    "baud_rate_nominal 0x%X    cc_ext 0x%X\n", __func__, id->ext.diagmon,
    id->ext.enh_options, id->ext.baud_rate_nominal, id->ext.cc_ext);

}

static u8 sff8636_soft_get_state(struct qsfp *qsfp)
{
    int ret;
    u8 state = 0;
    struct sff8636_irq_status irq_status = {0};

    ret = qsfp_read(qsfp, SFF8636_IRQ_FLAGS, &irq_status,
                    sizeof(irq_status));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Failed to read QSFP IRQ status. "
                           "ret %d\n", __func__, ret);
        /* Preserve the current state */
        return qsfp->state;
    }

    dev_notice(qsfp->dev, "%s: IntL 0x%X\n", __func__, irq_status.intl);

    if (irq_status.los)
        state |= QSFP_F_LOS;

    if (irq_status.tx_fault)
        state |= QSFP_F_TX_FAULT;

    dev_notice(qsfp->dev, "%s: IRQ status dump: LOS 0x%X TX Fault 0x%X "
    "eq 0x%X LOL 0x%X Init 0x%X ready 0x%X Temp 0x%X VCC 0x%X Vendor 0x%X "
    "RX12_Power 0x%X RX34_Power 0x%X TX12_bias 0x%X TX34_bias 0x%X "
    "TX12_pow 0x%X TX34_pow 0x%X Vendor 0x%X 0x%X 0x%X\n", __func__,
    irq_status.los, irq_status.tx_fault, irq_status.tx_adap_eq_fault,
    irq_status.lol, irq_status.init_complete, irq_status.tc_ready,
    irq_status.temp_alarm, irq_status.volt_alarm, irq_status.vendor_specific1,
    irq_status.rx12_pow_alarm, irq_status.rx34_pow_alarm,
    irq_status.tx12_bias_alarm, irq_status.tx34_bias_alarm,
    irq_status.tx12_pow_alarm, irq_status.tx34_pow_alarm,
    irq_status.vendor_specific2[0], irq_status.vendor_specific2[1],
    irq_status.vendor_specific2[2]);

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

const char *sff8636_mod_revision_to_str(u8 mod_rev_value)
{
    switch (mod_rev_value) {
    case 0x00:
    default:
        return "Revision not specified";
    case 0x01:
        return "SFF-8436 Rev 4.8 or earlier";
    case 0x02:
        return "revision 4.8 or earlier of SFF-8436";
    case 0x03:
        return "SFF-8636 Rev 1.3 or earlier";
    case 0x04:
        return "SFF-8636 Rev 1.4";
    case 0x05:
        return "SFF-8636 Rev 1.5";
    case 0x06:
        return "SFF-8636 Rev 2.0";
    case 0x07:
        return "SFF-8636 Rev 2.5, 2.6 and 2.7";
    case 0x08:
        return "SFF-8636 Rev 2.8, 2.9 and 2.10";
    case 0x09 ... 0xFF:
        return "Reserved need to be update in future";
    }
}

const char *sff8636_mod_encoding_to_str(u8 mod_encoding)
{
    switch (mod_encoding) {
    case 0x00:
    default:
        return "Unspecified";
    case 0x01:
        return "8B/10B";
    case 0x02:
        return "4B/5B";
    case 0x03:
        return "NRZ";
    case 0x04:
        return "SONET Scrambled";
    case 0x05:
        return "64B/66B";
    case 0x06:
        return "Manchester";
    case 0x07:
        return "256B/257B";
    case 0x08:
        return "PAM4";
    case 0x09 ... 0xFF:
        return "Reserved need to be update in future";
    }
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
