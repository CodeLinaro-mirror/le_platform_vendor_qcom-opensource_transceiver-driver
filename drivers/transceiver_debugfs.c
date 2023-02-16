/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include "transceiver_debugfs.h"


#if IS_ENABLED(CONFIG_DEBUG_FS)

struct dentry *transceiver_debugfs_dir;

const char *mod_identifier_to_str(u8 spec_id)
{
    switch (spec_id) {
    case 0x00:
    default:
        return "Unknown or unspecified";
    case 0x01:
        return "GBIC";
    case 0x02:
        return "Module/connector soldered to motherboard (using SFF-8472)";
    case 0x03:
        return "SFP/SFP+/SFP28 and later";
    case 0x04:
        return "300 pin XBI";
    case 0x05:
        return "XENPAK";
    case 0x06:
        return "XFP";
    case 0x07:
        return "XFF";
    case 0x08:
        return "XFP-E";
    case 0x09:
        return "XPAK";
    case 0x0A:
        return "X2";
    case 0x0B:
        return "DWDM-SFP/SFP+";
    case 0x0C:
        return "QSFP (INF-8438)";
    case 0x0D:
        return "QSFP+ or SFF-8636 or SFF-8436";
    case 0x0E:
        return "CXP or later";
    case 0x0F:
        return "Shielded Mini Multilane HD 4X";
    case 0x10:
        return "Shielded Mini Multilane HD 8X";
    case 0x11:
        return "QSFP28 or SFF-8636";
    case 0x12:
        return "CXP2 (aka CXP28) or later";
    case 0x13:
        return "CDFP (Style 1/Style2)";
    case 0x14:
        return "Shielded Mini Multilane HD 4X Fanout Cable";
    case 0x15:
        return "Shielded Mini Multilane HD 8X Fanout Cable";
    case 0x16:
        return "CDFP (Style 3)";
    case 0x17:
        return "micro QSFP";
    case 0x18:
        return "QSFP-DD Double Density 8X Pluggable Transceiver (INF-8628)";
    case 0x19:
        return "OSFP 8X Pluggable Transceiver";
    case 0x1A:
        return "SFP-DD Double Density 2X Pluggable Transceiver";
    case 0x1B:
        return "DSFP Dual Small Form Factor Pluggable Transceiver";
    case 0x1C:
        return "x4 MiniLink/OcuLink";
    case 0x1D:
        return "x8 MiniLink";
    case 0x1E:
        return "QSFP+ or CMIS";
    /* 0x1F to 0x7F was Reserved need to be updated in future. */
    case 0x80 ... 0xFF:
        return "Vendor specific";
    }
}

static const char *mod_connector_to_str(u8 mod_connector,char* vendor_name,
                                                char* vendor_data)
{
    switch (mod_connector) {
    case 0x00:
    default:
        return "Unknown";
    case 0x01:
        return "Subscriber Connector";
    case 0x02:
        return "Fibre Channel Style 1 copper connector";
    case 0x03:
        return "Fibre Channel Style 2 copper connector";
    case 0x04:
        return "BNC/TNC (Bayonet/Threaded Neill-Concelman)";
    case 0x05:
        return "Fibre Channel coax headers";
    case 0x06:
        return "Fiber Jack";
    case 0x07:
        return "LC (Lucent Connector)";
    case 0x08:
        return "MT-RJ (Mechanical Transfer - Registered Jack)";
    case 0x09:
        return "MU (Multiple Optical)";
    case 0x0A:
        return "SG";
    case 0x0B:
        return "Optical Pigtail";
    case 0x0C:
        return "MPO 1x12 (Multifiber Parallel Optic)";
    case 0x0D:
        return "MPO 2x16";
    case 0x20:
        return "HSSDC II (High Speed Serial Data Connector)";
    case 0x21:
        return "Copper pigtail";
    case 0x22:
        return "RJ45";
    case 0x23:
        return "No separable connector";
    case 0x24:
        return "MXC 2x16";
    case 0x25:
        return "CS optical connector";
    case 0x26:
        return "SN (previously Mini CS) optical connector";
    case 0x27:
        return "MPO 2x12";
    case 0x28:
        return "MPO 1x16";
    /* 0x29 to 0x7F was Reserved need to be update in future.*/
    case 0x80 ... 0xFF:
        scnprintf(vendor_data,75,"vendor specific {%s}",vendor_name);
        return vendor_data;
    }
}

const char *mod_link_codes_to_str(unsigned short mod_link_codes)
{
    switch (mod_link_codes) {
    case 0x00:
    default:
        return "Unspecified";
    case 0x01:
        return "100G AOC or 25GAUI C2M AOC";
    case 0x02:
        return "100GBASE-SR4 or 25GBASE-SR";
    case 0x03:
        return "100GBASE-LR4 or 25GBASE-LR";
    case 0x04:
        return "100GBASE-ER4 or 25GBASE-ER";
    case 0x05:
        return "100GBASE-SR10";
    case 0x06:
        return "100G CWDM4";
    case 0x07:
        return "100G PSM4 Parallel SMF";
    case 0x08:
        return "100G ACC or 25GAUI C2M ACC";
    case 0x09:
        return "Obsolete (assigned before 100G CWDM4 MSA required FEC)";
    case 0x0B:
        return "100GBASE-CR4, 25GBASE-CR CA-25G-L or 50GBASE-CR2 with RS FEC";
    case 0x0C:
        return "25GBASE-CR CA-25G-S or 50GBASE-CR2 with BASE-R FEC";
    case 0x0D:
        return "25GBASE-CR CA-25G-N or 50GBASE-CR2 with no FEC";
    case 0x0E:
        return "10 Mb/s Single Pair Ethernet (802.3cg, 1000 m copper)";
    case 0x10:
        return "40GBASE-ER4";
    case 0x11:
        return "4 x 10GBASE-SR";
    case 0x12:
        return "40G PSM4 Parallel SMF";
    case 0x13:
        return "G959.1 profile P1I1-2D1 (10709 MBd, 2km, 1310 nm SM)";
    case 0x14:
        return "G959.1 profile P1S1-2D2 (10709 MBd, 40km, 1550 nm SM)";
    case 0x15:
        return "G959.1 profile P1L1-2D2 (10709 MBd, 80km, 1550 nm SM)";
    case 0x16:
        return "10GBASE-T with SFI electrical interface";
    case 0x17:
        return "100G CLR4";
    case 0x18:
        return "100G AOC or 25GAUI C2M AOC";
    case 0x19:
        return "100G ACC or 25GAUI C2M ACC";
    case 0x1A:
        return "100GE-DWDM2";
    case 0x1B:
        return "100G 1550nm WDM (4 wavelengths)";
    case 0x1C:
        return "10GBASE-T Short Reach (30 meters)";
    case 0x1D:
        return "5GBASE-T";
    case 0x1E:
        return "2.5GBASE-T";
    case 0x1F:
        return "40G SWDM4";
    case 0x20:
        return "100G SWDM4";
    case 0x21:
        return "100G PAM4 BiDi";
    case 0x22:
        return "4WDM-10 MSA";
    case 0x23:
        return "4WDM-20 MSA";
    case 0x24:
        return "4WDM-40 MSA";
    case 0x25:
        return "100GBASE-DR";
    case 0x26:
        return "100G-FR or 100GBASE-FR1";
    case 0x27:
        return "100G-LR or 100GBASE-LR1";
    case 0x28:
        return "100GBASE-SR";
    case 0x29:
        return "100GBASE-SR, 200GBASE-SR2 or 400GBASE-SR4";
    case 0x2A:
        return "100GBASE-FR1";
    case 0x2B:
        return "100GBASE-LR1";
    case 0x2C:
        return "100G-LR1-20 MSA, CAUI-4";
    case 0x2D:
        return "100G-ER1-30 MSA, CAUI-4";
    case 0x2E:
        return "100G-ER1-40 MSA, CAUI-4";
    case 0x2F:
        return "100G-LR1-20 MSA";
    case 0x30:
        return "Active Copper Cable with 50GAUI,BER of 10-6 or below";
    case 0x31:
        return "Active Optical Cable with 50GAUI,BER of 10-6 or below";
    case 0x32:
        return "Active Copper Cable, BER of 2.6x10-4 for ACC, 10-5 for AUI";
    case 0x33:
        return "Active Optical Cable, BER of 2.6x10-4 for ACC, 10-5 for AUI";
    case 0x34:
        return "100G-ER1-30 MSA";
    case 0x35:
        return "100G-ER1-40 MSA";
    case 0x36:
        return "100GBASE-VR, 200GBASE-VR2 or 400GBASE-VR4";
    case 0x37:
        return "10GBASE-BR";
    case 0x38:
        return "25GBASE-BR";
    case 0x39:
        return "50GBASE-BR";
    case 0x3A:
        return "100GBASE-VR";
    /* 0x3B-0x3E Reserved */
    case 0x3F:
        return "100GBASE-CR1, 200GBASE-CR2 or 400GBASE-CR4";
    case 0x40:
        return "50GBASE-CR, 100GBASE-CR2, or 200GBASE-CR4";
    case 0x41:
        return "50GBASE-SR, 100GBASE-SR2, or 200GBASE-SR4";
    case 0x42:
        return "50GBASE-FR or 200GBASE-DR4";
    case 0x4A:
        return "50GBASE-ER";
    case 0x43:
        return "200GBASE-FR4";
    case 0x44:
        return "200G 1550 nm PSM4";
    case 0x45:
        return "50GBASE-LR";
    case 0x46:
        return "200GBASE-LR4";
    case 0x47:
        return "400GBASE-DR4";
    case 0x48:
        return "400GBASE-FR4";
    case 0x49:
        return "400GBASE-LR4-6";
    case 0x4B:
        return "400G-LR4-10";
    case 0x4C:
        return "400GBASE-ZR";
    /*0x4D-0x7E Reserved was Reserved need to be update in future.*/
    case 0x7F:
        return "256GFC-SW4";
    case 0x80:
        return "64GFC";
    case 0x81:
        return "128GFC";
    /*0x82-0xFF Reserved was Reserved need to be update in future.*/
    }
}

char* calc_common_temperature(int16_t tempc, char* str)
{
    int16_t temp_in = 0;
    int32_t temp = 0;

    /* Convert tempc info to cpu byte order */
    temp_in = be16_to_cpu(tempc & 0XFFFF);

    /* (temp_in * 1000) to get the three digit precision
       while converting it to celsius. */
    temp = temp_in * 1000;

    /* Convert the temp to degrees Celsius by dividing it by 256. */
    temp = temp/256;

    /* To avoid printing of the minus(-) symbol in decimal places.
       Multiply temp with -1 for negative values. */
    scnprintf(str,75,"Temperature: %d.%03d °C",temp/1000,
                       (temp < 0 ? (temp * -1) : temp)%1000);
    return str;
}

char* calc_common_svoltage(u16 svolt, char* str)
{
    u16 supply_voltage_t = 0;

    /* convert svolt info to cpu byte order */
    supply_voltage_t = be16_to_cpu(svolt & 0XFFFF);

    /* supply voltage in volts (supply_voltage_t * 100 μV/ 1000000) */
    scnprintf(str,75,"Supply Voltage: %d.%03d V",
         supply_voltage_t/10000, supply_voltage_t%10000);

    return str;
}

inline void spec_info_print(struct seq_file *s, u8 spec_id)
{
    if (spec_id == 0x00) {
        seq_printf(s,"Specification Identifier {0x%02X}\n"
                      "Unknown module\n",spec_id);
    }
    else {
        seq_printf(s,"Specification Identifier {0x%02X}\n"
                      "%s Transceiver module is not supported\n",
                      spec_id,mod_identifier_to_str(spec_id));
    }
}

void transceiver_debugfs_init(void)
{
    transceiver_debugfs_dir = debugfs_create_dir("transceiver_module", NULL);
    if (IS_ERR(transceiver_debugfs_dir)) {
        pr_err("%s: debugfs_create_dir fail, error (%ld)\n",__func__,
                                   PTR_ERR(transceiver_debugfs_dir));
       return;
    }
}

void transceiver_debugfs_exit(void)
{
    if (IS_ERR(transceiver_debugfs_dir)) {
        pr_err("%s: debugfs_create_dir fail, error (%ld)\n",__func__,
                                  PTR_ERR(transceiver_debugfs_dir));
        return;
    }
    debugfs_remove_recursive(transceiver_debugfs_dir);
}

static int fpc_debug_i2c_address_show(struct seq_file *s, void *data)
{
    struct fpc *fpc = s->private;

    seq_printf(s, "0x%X\n",fpc->i2c_address);
    return 0;
}
DEFINE_SHOW_ATTRIBUTE(fpc_debug_i2c_address);

static int fpc_debug_i2c_adapter_show(struct seq_file *s, void *data)
{
    struct fpc *fpc = s->private;

    seq_printf(s, "i2c-%d\n",fpc->i2c->nr);
    return 0;
}
DEFINE_SHOW_ATTRIBUTE(fpc_debug_i2c_adapter);


void fpc_debugfs_init(struct fpc *fpc)
{
    struct dentry *file;
    char fpc_devname[20] = {};
    char fpc_devsubname[10] = {};
    strlcpy(fpc_devname,dev_name(fpc->dev),sizeof(fpc_devname));

    if (IS_ERR(transceiver_debugfs_dir)) {
        dev_err(fpc->dev,"%s:transceiver debugfs_create_dir fail, error %ld\n",
                         __func__,PTR_ERR(transceiver_debugfs_dir));
        return;
    }

    if (strncmp(fpc_devname,"soc:",4) == 0 ) {
        strlcpy(fpc_devsubname,&fpc_devname[4],sizeof(fpc_devsubname));
    }
    else {
        strlcpy(fpc_devsubname, fpc_devname,sizeof(fpc_devsubname));
    }

    dev_notice(fpc->dev,"%s: dev %s , sub %s\n", __func__, dev_name(fpc->dev),
                                            fpc_devsubname);

    fpc->debugfs_dir = debugfs_create_dir(fpc_devsubname,
                             transceiver_debugfs_dir);
    if (IS_ERR(fpc->debugfs_dir)) {
        dev_err(fpc->dev,"%s: fpc debugfs_create_dir fail, error %ld\n",
                         __func__,PTR_ERR(fpc->debugfs_dir));
        return;
    }

    file = debugfs_create_file("i2c_address", 0600, fpc->debugfs_dir, fpc,
                        &fpc_debug_i2c_address_fops);
    if (!file || IS_ERR(file)) {
        dev_err(fpc->dev,"%s: fpc i2c address debugfs_create_file fail,"
                       " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(fpc->debugfs_dir);
        return;
    }

    file = debugfs_create_file("i2c_adapter", 0600, fpc->debugfs_dir, fpc,
                        &fpc_debug_i2c_adapter_fops);
    if (!file || IS_ERR(file)) {
        dev_err(fpc->dev,"%s: fpc i2c adapter debugfs_create_file fail,"
                       " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(fpc->debugfs_dir);
    }
}

void fpc_debugfs_exit(struct fpc *fpc)
{
    if (IS_ERR(fpc->debugfs_dir) || IS_ERR(transceiver_debugfs_dir)) {
        dev_err(fpc->dev,"%s: debugfs_create_dir fail, error (%ld %ld)\n",
                               __func__,PTR_ERR(transceiver_debugfs_dir),
                               PTR_ERR(fpc->debugfs_dir));
        return;
    }
    debugfs_remove_recursive(fpc->debugfs_dir);
}

static int qsfp_debug_qsfp_state_info_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;

    seq_printf(s, "Port number: %u\n", qsfp->port_num);
    seq_printf(s, "Module state: %s\n",
                  mod_state_to_str(qsfp->sm_mod_state));

    seq_printf(s, "Device state: %s\n",
                  dev_state_to_str(qsfp->sm_dev_state));
    seq_printf(s, "Main state: %s\n",
                  sm_state_to_str(qsfp->sm_state));

    seq_printf(s, "Module probe attempts: %d %d\n",
                  R_PROBE_RETRY_INIT - qsfp->sm_mod_tries_init,
                  R_PROBE_RETRY_SLOW - qsfp->sm_mod_tries);
    seq_printf(s, "Fault recovery remaining retries: %d\n",
                  qsfp->sm_fault_retries);
    seq_printf(s, "Module present: %d\n", !!(qsfp->state & QSFP_F_PRESENT));
    seq_printf(s, "LOS: %d\n", !!(qsfp->state & QSFP_F_LOS));
    seq_printf(s, "TX Fault: %d\n", !!(qsfp->state & QSFP_F_TX_FAULT));
    seq_printf(s, "TX Disable: %d\n", !!(qsfp->state & QSFP_F_TX_DISABLE));
    seq_printf(s, "Poll status: %s\n", qsfp->need_poll ? "Yes" : "No");
    seq_printf(s, "Features: %s %s %s\n", qsfp->features & QSFP_F_LOS ? "LOS":"",
               qsfp->features & QSFP_F_TX_FAULT ? "TX_FAULT":"",
               qsfp->features & QSFP_F_TX_DISABLE ? "TX_DISABLE":"");

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_qsfp_state_info);

static int qsfp_debug_qsfp_i2c_address_info_show(struct seq_file *s,
                               void *data)
{
    struct qsfp *qsfp = s->private;
    seq_printf(s, "I2C device0 Address: 0x%X\n",qsfp->i2c_address_dev0);
    seq_printf(s, "I2C device1 Address: 0x%X\n",qsfp->i2c_address_dev1);

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_qsfp_i2c_address_info);

static int qsfp_debug_qsfp_port_num_info_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    seq_printf(s, "0x%X\n",qsfp->port_num);

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_qsfp_port_num_info);

static int qsfp_debug_qsfp_module_identifier_info_show(struct seq_file *s,
                               void *data)
{
    struct qsfp *qsfp = s->private;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the module is attached before processing
       showing module identifier type even module was not
       probed to handle un implemented module types       */
    if (qsfp->sm_mod_state < QSFP_MOD_ERROR) {
        seq_printf(s, "QSFP module is not attached\n");
        return 0;
    }

    seq_printf(s, "{0x%X} %s\n", *spec_id, mod_identifier_to_str(*spec_id));

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_qsfp_module_identifier_info);

static int qsfp_debug_revision_info_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    char revSpec[75];
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
        seq_printf(s, "QSFP module is not attached\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        seq_printf(s, "{0x%X} %s\n",qsfp->module_revision,
                      sff8636_mod_revision_to_str(qsfp->module_revision));
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        seq_printf(s, "{0x%X} %s\n",qsfp->module_revision,
                      cmis_revision_to_str(qsfp->module_revision, revSpec));
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_revision_info);

static int qsfp_debug_connector_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id = (u8*)&qsfp->id;
    char vendor_data[75] ={};

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
        seq_printf(s, "QSFP module is not attached\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        seq_printf(s, "{0x%X} %s\n",id->base.connector,
            mod_connector_to_str(id->base.connector,id->base.vendor_name,
                                     vendor_data));
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;
        seq_printf(s, "{0x%X} %s\n",cmis_id->base.connector,
            mod_connector_to_str(cmis_id->base.connector,
                                 cmis_id->base.vendor_name,
                                 vendor_data));
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_connector);

static int qsfp_debug_vendor_info_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
        seq_printf(s, "QSFP module is not attached\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        seq_printf(s, "vendor name: %.*s\n",
                      (int)sizeof(id->base.vendor_name),
                      id->base.vendor_name);
        seq_printf(s, "vendor pn: %.*s\n",
                      (int)sizeof(id->base.vendor_pn),
                      id->base.vendor_pn);
        seq_printf(s, "vendor rev: %.*s\n",
                      (int)sizeof(id->base.vendor_rev),
                      id->base.vendor_rev);
        seq_printf(s, "vendor sn: %.*s\n",
                      (int)sizeof(id->ext.vendor_sn),
                      id->ext.vendor_sn);
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;
        seq_printf(s, "vendor name: %.*s\n",
                      (int)sizeof(cmis_id->base.vendor_name),
                      cmis_id->base.vendor_name);
        seq_printf(s, "vendor pn: %.*s\n",
                      (int)sizeof(cmis_id->base.vendor_pn),
                      cmis_id->base.vendor_pn);
        seq_printf(s, "vendor rev: %.*s\n",
                      (int)sizeof(cmis_id->base.vendor_rev),
                      cmis_id->base.vendor_rev);
        seq_printf(s, "vendor sn: %.*s\n",
                      (int)sizeof(cmis_id->base.vendor_sn),
                      cmis_id->base.vendor_sn);
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_vendor_info);

/*
* Information about the Qsfp device temperature is being exported to Sysfs.
*/
static int qsfp_debug_device_temperature_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id = (u8*)&qsfp->id;
    char temperature_data[75] = {0};
    int16_t tempc = 0;
    int ret;

    /* Ensure that the device is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
        seq_printf(s, "QSFP module is not attached\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h Bytes 22-23 */
        ret = qsfp_read(qsfp, SFF8636_TEMPERATURE, &tempc,
                               sizeof(tempc));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        seq_printf(s, "%s\n", calc_common_temperature(tempc,
                                         temperature_data));
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            seq_printf(s, "TRX temperature measurement not supported\n");
            return 0;
        }

        /*Support advertised in page 01h:159.0 */
        if (!cmis_id->ext.temp_mon_sup) {
            seq_printf(s, "TRX temperature measurement not supported\n");
            return 0;
        }

        /* Page 00h Bytes 14-15 */
        ret = qsfp_read(qsfp, CMIS_MOD_TEMPMON, &tempc,
                               sizeof(tempc));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        seq_printf(s, "%s\n", calc_common_temperature(tempc,
                                         temperature_data));
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_temperature);

/*
* Information about the Qsfp device supply voltage is being exported to Sysfs.
*/
static int qsfp_debug_device_Supply_Voltage_show(struct seq_file *s,
                                                    void *data)
{
    struct qsfp *qsfp = s->private;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id = (u8*)&qsfp->id;
    char voltage_data[75] = {};
    u16 supply_voltage_t = 0;
    int ret;

    /* Ensure that the device is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
        seq_printf(s, "QSFP module is not attached\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h Bytes 26-27 */
        ret = qsfp_read(qsfp, SFF8636_SUPPLY_VOLTAGE, &supply_voltage_t,
                              sizeof(supply_voltage_t));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        seq_printf(s, "%s\n", calc_common_svoltage(supply_voltage_t,
                                                     voltage_data));
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            seq_printf(s, "TRX supply voltage measurement not supported\n");
            return 0;
        }

        /*Support advertised in page 01h:159.1 */
        if (!cmis_id->ext.volt_mon_sup) {
            seq_printf(s, "TRX supply voltage measurement not supported\n");
            return 0;
        }

        /* Page 00h Bytes 16-17 */
        ret = qsfp_read(qsfp, CMIS_MOD_VCCMON, &supply_voltage_t,
                              sizeof(supply_voltage_t));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        seq_printf(s, "%s\n", calc_common_svoltage(supply_voltage_t,
                                                     voltage_data));

        break;
    default:
        spec_info_print(s, *spec_id);
    }
    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_Supply_Voltage);

/*
* Information about the Qsfp Channel Monitor value of Rx Power
* is being exported to Sysfs.
*/
static int qsfp_debug_device_rx_power_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id = (u8*)&qsfp->id;
    char rx_power_data[500] ={};
    u16 rx_power_t[4] = {};
    u8  rx_power[8] = {0};
    u16 cmis_rx_power_t[8] = {0};
    u8  cmis_rx_power[16] = {0};
    int ret;

    /* Ensure that the device is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
        seq_printf(s, "QSFP module is not attached\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h Bytes 34-41 */
        ret = qsfp_read(qsfp, SFF8636_RX_POWER, rx_power,
                              sizeof(rx_power));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        rx_power_t[0] = (( rx_power[0] << 8) | rx_power[1]);
        rx_power_t[1] = (( rx_power[2] << 8) | rx_power[3]);
        rx_power_t[2] = (( rx_power[4] << 8) | rx_power[5]);
        rx_power_t[3] = (( rx_power[6] << 8) | rx_power[7]);

        /* rx power in milliWatts (rx_power_t * 0.1 μW/ 1000) */
        scnprintf(rx_power_data,500,"Rx Power Lane1: %d.%03d mW\n"
                                    "Rx Power Lane2: %d.%03d mW\n"
                                    "Rx Power Lane3: %d.%03d mW\n"
                                    "Rx Power Lane4: %d.%03d mW",
                          rx_power_t[0]/10000,rx_power_t[0]%10000,
                          rx_power_t[1]/10000,rx_power_t[1]%10000,
                          rx_power_t[2]/10000,rx_power_t[2]%10000,
                          rx_power_t[3]/10000,rx_power_t[3]%10000);
        seq_printf(s, "%s\n", rx_power_data);
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            seq_printf(s, "TRX optical rx power measurement not supported\n");
            return 0;
        }

        /*Support advertised in page 01h:160.2 */
        if(!cmis_id->ext.rx_optical_pow_mon_sup ) {
            seq_printf(s, "TRX optical rx power measurement not supported\n");
            return 0;
        }

        /* Page 11h Bytes 186-201 */
        ret = qsfp_read(qsfp, CMIS_RX_POWER, cmis_rx_power,
                              sizeof(cmis_rx_power));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        cmis_rx_power_t[0] = (( cmis_rx_power[0] << 8) | cmis_rx_power[1]);
        cmis_rx_power_t[1] = (( cmis_rx_power[2] << 8) | cmis_rx_power[3]);
        cmis_rx_power_t[2] = (( cmis_rx_power[4] << 8) | cmis_rx_power[5]);
        cmis_rx_power_t[3] = (( cmis_rx_power[6] << 8) | cmis_rx_power[7]);
        cmis_rx_power_t[4] = (( cmis_rx_power[8] << 8) | cmis_rx_power[9]);
        cmis_rx_power_t[5] = (( cmis_rx_power[10] << 8) | cmis_rx_power[11]);
        cmis_rx_power_t[6] = (( cmis_rx_power[12] << 8) | cmis_rx_power[13]);
        cmis_rx_power_t[7] = (( cmis_rx_power[14] << 8) | cmis_rx_power[15]);

        /* rx power in milliWatts (rx_power_t * 0.1 μW/ 1000) */
        scnprintf(rx_power_data,500,"Rx Power Lane1: %d.%03d mW\n"
                                    "Rx Power Lane2: %d.%03d mW\n"
                                    "Rx Power Lane3: %d.%03d mW\n"
                                    "Rx Power Lane4: %d.%03d mW\n"
                                    "Rx Power Lane5: %d.%03d mW\n"
                                    "Rx Power Lane6: %d.%03d mW\n"
                                    "Rx Power Lane7: %d.%03d mW\n"
                                    "Rx Power Lane8: %d.%03d mW",
                          cmis_rx_power_t[0]/10000,cmis_rx_power_t[0]%10000,
                          cmis_rx_power_t[1]/10000,cmis_rx_power_t[1]%10000,
                          cmis_rx_power_t[2]/10000,cmis_rx_power_t[2]%10000,
                          cmis_rx_power_t[3]/10000,cmis_rx_power_t[3]%10000,
                          cmis_rx_power_t[4]/10000,cmis_rx_power_t[4]%10000,
                          cmis_rx_power_t[5]/10000,cmis_rx_power_t[5]%10000,
                          cmis_rx_power_t[6]/10000,cmis_rx_power_t[6]%10000,
                          cmis_rx_power_t[7]/10000,cmis_rx_power_t[7]%10000);
        seq_printf(s, "%s\n", rx_power_data);
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_rx_power);

/*
* Information about the Qsfp Channel Monitor value of Tx Bias Current
* is being exported to Sysfs.
*/
static int qsfp_debug_device_tx_bias_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id = (u8*)&qsfp->id;
    char tx_bias_current_data[500] = {};
    u32 tx_bias_current_t[4] = {};
    u16 tx_bias_current = 0;
    u8 tx_bias[8] = {0};
    u8 cmis_tx_bias[16] = {0};
    u32 cmis_tx_bias_current_t[8] = {0};
    u8 cmis_tx_bias_multiplier = 1;
    int ret;

    /* Ensure that the device is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
        seq_printf(s, "QSFP module is not attached\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h Bytes 42-49 */
        ret = qsfp_read(qsfp, SFF8636_TX_BIAS, tx_bias,
                              sizeof(tx_bias));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }
        tx_bias_current = (( tx_bias[0] << 8) | tx_bias[1]);
        /* tx_bias_current in  Micro Amp */
        tx_bias_current_t[0] = tx_bias_current * 2;

        tx_bias_current = (( tx_bias[2] << 8) | tx_bias[3]);
        /* tx_bias_current in  Micro Amp */
        tx_bias_current_t[1] = tx_bias_current * 2;

        tx_bias_current = (( tx_bias[4] << 8) | tx_bias[5]);
        /* tx_bias_current in  Micro Amp */
        tx_bias_current_t[2] = tx_bias_current * 2;

        tx_bias_current = (( tx_bias[6] << 8) | tx_bias[7]);
        /* tx_bias_current in  Micro Amp */
        tx_bias_current_t[3] = tx_bias_current * 2;

        scnprintf(tx_bias_current_data,500,"Tx Bias Current Lane1: %d.%03d"
                                           " mA\n"
                                        "Tx Bias Current Lane2: %d.%03d mA\n"
                                        "Tx Bias Current Lane3: %d.%03d mA\n"
                                        "Tx Bias Current Lane4: %d.%03d mA",
                          tx_bias_current_t[0]/1000,tx_bias_current_t[0]%1000,
                          tx_bias_current_t[1]/1000,tx_bias_current_t[1]%1000,
                          tx_bias_current_t[2]/1000,tx_bias_current_t[2]%1000,
                          tx_bias_current_t[3]/1000,tx_bias_current_t[3]%1000);
        seq_printf(s, "%s\n", tx_bias_current_data);
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            seq_printf(s, "TRX tx bias current measurement not supported\n");
            return 0;
        }

        /*Support advertised in page 01h:160.0 */
        if(!cmis_id->ext.tx_bias_mon_sup ) {
            seq_printf(s, "TRX tx bias current measurement not supported\n");
            return 0;
        }

        switch(cmis_id->ext.tx_bias_cur_scal) {
        case 0x00:
            /* multiply x1 */
            cmis_tx_bias_multiplier = 1;
            break;
        case 0x01:
            /* multiply x2 */
            cmis_tx_bias_multiplier = 2;
            break;
        case 0x02:
            /* multiply x4 */
            cmis_tx_bias_multiplier = 4;
            break;
        /* reserved case not expected from OIF-CMIS-05.2 Rev */
        case 0x03:
            seq_printf(s, "TRX tx bias current multiplier was reserved "
                          "not expected for OIF-CMIS-05.2\n");
            return 0;
        }

        /* Page 11h Bytes 170-185 */
        ret = qsfp_read(qsfp, CMIS_TX_BIAS, cmis_tx_bias,
                              sizeof(cmis_tx_bias));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }
        tx_bias_current = (( cmis_tx_bias[0] << 8) | cmis_tx_bias[1]);
        /* tx_bias_current in  Micro Amp */
        cmis_tx_bias_current_t[0] = tx_bias_current * 2
                                     * cmis_tx_bias_multiplier;

        tx_bias_current = (( cmis_tx_bias[2] << 8) | cmis_tx_bias[3]);
        /* tx_bias_current in  Micro Amp */
        cmis_tx_bias_current_t[1] = tx_bias_current * 2
                                      * cmis_tx_bias_multiplier;

        tx_bias_current = (( cmis_tx_bias[4] << 8) | cmis_tx_bias[5]);
        /* tx_bias_current in  Micro Amp */
        cmis_tx_bias_current_t[2] = tx_bias_current * 2
                                      * cmis_tx_bias_multiplier;

        tx_bias_current = (( cmis_tx_bias[6] << 8) | cmis_tx_bias[7]);
        /* tx_bias_current in  Micro Amp */
        cmis_tx_bias_current_t[3] = tx_bias_current * 2
                                       * cmis_tx_bias_multiplier;

        tx_bias_current = (( cmis_tx_bias[8] << 8) | cmis_tx_bias[9]);
        /* tx_bias_current in  Micro Amp */
        cmis_tx_bias_current_t[4] = tx_bias_current * 2
                                       * cmis_tx_bias_multiplier;

        tx_bias_current = (( cmis_tx_bias[10] << 8) | cmis_tx_bias[11]);
        /* tx_bias_current in  Micro Amp */
        cmis_tx_bias_current_t[5] = tx_bias_current * 2
                                       * cmis_tx_bias_multiplier;

        tx_bias_current = (( cmis_tx_bias[12] << 8) | cmis_tx_bias[13]);
        /* tx_bias_current in  Micro Amp */
        cmis_tx_bias_current_t[6] = tx_bias_current * 2
                                       * cmis_tx_bias_multiplier;

        tx_bias_current = (( cmis_tx_bias[14] << 8) | cmis_tx_bias[15]);
        /* tx_bias_current in  Micro Amp */
        cmis_tx_bias_current_t[7] = tx_bias_current * 2
                                       * cmis_tx_bias_multiplier;


        scnprintf(tx_bias_current_data,500,"Tx Bias Current Lane1: %d.%03d"
                                           " mA\n"
                                        "Tx Bias Current Lane2: %d.%03d mA\n"
                                        "Tx Bias Current Lane3: %d.%03d mA\n"
                                        "Tx Bias Current Lane4: %d.%03d mA\n"
                                        "Tx Bias Current Lane5: %d.%03d mA\n"
                                        "Tx Bias Current Lane6: %d.%03d mA\n"
                                        "Tx Bias Current Lane7: %d.%03d mA\n"
                                        "Tx Bias Current Lane8: %d.%03d mA",
                cmis_tx_bias_current_t[0]/1000,cmis_tx_bias_current_t[0]%1000,
                cmis_tx_bias_current_t[1]/1000,cmis_tx_bias_current_t[1]%1000,
                cmis_tx_bias_current_t[2]/1000,cmis_tx_bias_current_t[2]%1000,
                cmis_tx_bias_current_t[3]/1000,cmis_tx_bias_current_t[3]%1000,
                cmis_tx_bias_current_t[4]/1000,cmis_tx_bias_current_t[4]%1000,
                cmis_tx_bias_current_t[5]/1000,cmis_tx_bias_current_t[5]%1000,
                cmis_tx_bias_current_t[6]/1000,cmis_tx_bias_current_t[6]%1000,
                cmis_tx_bias_current_t[7]/1000,cmis_tx_bias_current_t[7]%1000);
        seq_printf(s, "%s\n", tx_bias_current_data);
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_tx_bias);

/*
* Information about the Qsfp Channel Monitor value of Tx Power
* is being exported to Sysfs.
*/
static int qsfp_debug_device_tx_power_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id = (u8*)&qsfp->id;
    char tx_power_data[500] = {0};
    u16 tx_power_t[4] = {0};
    u8  tx_power[8] = {0};
    u8 diagmon;
    u8  cmis_tx_power[16] = {0};
    u16 cmis_tx_power_t[8] = {0};
    int ret;

    /* Ensure that the device is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
        seq_printf(s, "QSFP module is not attached\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        diagmon = id->ext.diagmon;
        if(!(diagmon & BIT(2))) {
            seq_printf(s, "Transmitter power measurement not supported\n");
            return 0;
        }
        /* Page 00h Bytes 50-57 */
        ret = qsfp_read(qsfp, SFF8636_TX_POWER, tx_power,
                              sizeof(tx_power));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        tx_power_t[0] = (( tx_power[0] << 8) | tx_power[1]);
        tx_power_t[1] = (( tx_power[2] << 8) | tx_power[3]);
        tx_power_t[2] = (( tx_power[4] << 8) | tx_power[5]);
        tx_power_t[3] = (( tx_power[6] << 8) | tx_power[7]);

        /* tx power in milliWatts (tx_power_t * 0.1 μW/ 1000) */
        scnprintf(tx_power_data,500,"Tx Power Lane1: %d.%03d mW\n"
                                    "Tx Power Lane2: %d.%03d mW\n"
                                    "Tx Power Lane3: %d.%03d mW\n"
                                    "Tx Power Lane4: %d.%03d mW",
                          tx_power_t[0]/10000,tx_power_t[0]%10000,
                          tx_power_t[1]/10000,tx_power_t[1]%10000,
                          tx_power_t[2]/10000,tx_power_t[2]%10000,
                          tx_power_t[3]/10000,tx_power_t[3]%10000);
        seq_printf(s, "%s\n", tx_power_data);
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            seq_printf(s, "Transmitter power measurement not supported\n");
            return 0;
        }

        /*Support advertised in page 01h:160.1 */
        if(!cmis_id->ext.tx_optical_pow_mon_sup) {
            seq_printf(s, "Transmitter power measurement not supported\n");
            return 0;
        }

        /* Page 11h Bytes 154-169 */
        ret = qsfp_read(qsfp, CMIS_TX_POWER, cmis_tx_power,
                              sizeof(cmis_tx_power));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        cmis_tx_power_t[0] = (( cmis_tx_power[0] << 8) | cmis_tx_power[1]);
        cmis_tx_power_t[1] = (( cmis_tx_power[2] << 8) | cmis_tx_power[3]);
        cmis_tx_power_t[2] = (( cmis_tx_power[4] << 8) | cmis_tx_power[5]);
        cmis_tx_power_t[3] = (( cmis_tx_power[6] << 8) | cmis_tx_power[7]);
        cmis_tx_power_t[4] = (( cmis_tx_power[9] << 8) | cmis_tx_power[9]);
        cmis_tx_power_t[5] = (( cmis_tx_power[10] << 8) | cmis_tx_power[11]);
        cmis_tx_power_t[6] = (( cmis_tx_power[12] << 8) | cmis_tx_power[13]);
        cmis_tx_power_t[7] = (( cmis_tx_power[14] << 8) | cmis_tx_power[15]);

        /* tx power in milliWatts (tx_power_t * 0.1 μW/ 1000) */
        scnprintf(tx_power_data,500,"Tx Power Lane1: %d.%03d mW\n"
                                    "Tx Power Lane2: %d.%03d mW\n"
                                    "Tx Power Lane3: %d.%03d mW\n"
                                    "Tx Power Lane4: %d.%03d mW\n"
                                    "Tx Power Lane5: %d.%03d mW\n"
                                    "Tx Power Lane6: %d.%03d mW\n"
                                    "Tx Power Lane7: %d.%03d mW\n"
                                    "Tx Power Lane8: %d.%03d mW",
                          cmis_tx_power_t[0]/10000,cmis_tx_power_t[0]%10000,
                          cmis_tx_power_t[1]/10000,cmis_tx_power_t[1]%10000,
                          cmis_tx_power_t[2]/10000,cmis_tx_power_t[2]%10000,
                          cmis_tx_power_t[3]/10000,cmis_tx_power_t[3]%10000,
                          cmis_tx_power_t[4]/10000,cmis_tx_power_t[4]%10000,
                          cmis_tx_power_t[5]/10000,cmis_tx_power_t[5]%10000,
                          cmis_tx_power_t[6]/10000,cmis_tx_power_t[6]%10000,
                          cmis_tx_power_t[7]/10000,cmis_tx_power_t[7]%10000);
        seq_printf(s, "%s\n", tx_power_data);

        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_tx_power);

int create_common_debugfs_files(struct qsfp *qsfp)
{
    struct dentry *file;

    file = debugfs_create_file("module_revision", 0600,
                                   qsfp->module_debugfs_dir,
                                   qsfp, &qsfp_debug_revision_info_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp module_revision debugfs_create_file fail,"
                           "error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return -1;
     }

    file = debugfs_create_file("connector", 0600, qsfp->module_debugfs_dir,
                               qsfp, &qsfp_debug_connector_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp phys_ext_id debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return -1;
    }

    file = debugfs_create_file("vendor_info", 0600, qsfp->module_debugfs_dir,
                        qsfp, &qsfp_debug_vendor_info_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp vendor_info debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return -1;
    }

    file = debugfs_create_file("temperature", 0600, qsfp->module_debugfs_dir,
                        qsfp, &qsfp_debug_device_temperature_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp temperature debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return -1;
    }

    file = debugfs_create_file("supply_voltage", 0600,
                        qsfp->module_debugfs_dir, qsfp,
                        &qsfp_debug_device_Supply_Voltage_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp supply_voltage debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return -1;
    }

    file = debugfs_create_file("rx_power", 0600, qsfp->module_debugfs_dir,
                        qsfp, &qsfp_debug_device_rx_power_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp rx_power debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return -1;
    }

    file = debugfs_create_file("tx_bias_current", 0600,
                        qsfp->module_debugfs_dir,
                        qsfp, &qsfp_debug_device_tx_bias_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp tx_bias debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return -1;
    }

    file = debugfs_create_file("tx_power", 0600, qsfp->module_debugfs_dir,
                        qsfp, &qsfp_debug_device_tx_power_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp tx_power debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return -1;
    }

    return 0;
}

int module_debugfs_init(struct qsfp *qsfp)
{
    if (IS_ERR(qsfp->debugfs_dir) || IS_ERR(qsfp->fpc->debugfs_dir) ||
        IS_ERR(transceiver_debugfs_dir)) {
        dev_err(qsfp->dev, "%s: debugfs_create_dir fail, "
                            "error (%ld %ld %ld)\n",
                            __func__,PTR_ERR(transceiver_debugfs_dir),
                            PTR_ERR(qsfp->fpc->debugfs_dir),
                            PTR_ERR(qsfp->debugfs_dir));
        return -1;
    }
    qsfp->module_debugfs_dir = debugfs_create_dir("module_spec_info",
                                                  qsfp->debugfs_dir);
    if (IS_ERR(qsfp->module_debugfs_dir)) {
        dev_err(qsfp->dev, "%s: qsfp module_spec_info debugfs_create_dir"
                           "fail, error %ld\n",
                           __func__,PTR_ERR(qsfp->module_debugfs_dir));
        return -1;
    }

    return 0;
}

void module_debugfs_exit(struct qsfp *qsfp)
{
    if (IS_ERR(qsfp->module_debugfs_dir) || IS_ERR(qsfp->debugfs_dir) ||
        IS_ERR(qsfp->fpc->debugfs_dir) ||
        IS_ERR(transceiver_debugfs_dir)) {
        dev_err(qsfp->dev,"%s: debugfs_create_dir fail, error "
                          "(%ld %ld %ld %ld)\n",
                        __func__,PTR_ERR(transceiver_debugfs_dir),
                        PTR_ERR(qsfp->fpc->debugfs_dir),
                        PTR_ERR(qsfp->debugfs_dir),
                        PTR_ERR(qsfp->module_debugfs_dir));
        return;
    }
    else
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
}

void qsfp_debugfs_init(struct qsfp *qsfp)
{
    struct dentry *file;
    char qsfp_devname[20] = {};
    char qsfp_devsubname[10] = {};
    strlcpy(qsfp_devname,dev_name(qsfp->dev),sizeof(qsfp_devname));

    if (IS_ERR(qsfp->fpc->debugfs_dir) || IS_ERR(transceiver_debugfs_dir)) {
        dev_err(qsfp->dev, "%s: debugfs_create_dir fail, error (%ld %ld)\n",
                           __func__,PTR_ERR(transceiver_debugfs_dir),
                           PTR_ERR(qsfp->fpc->debugfs_dir));
        return;
    }

    if (strncmp(qsfp_devname,"soc:",4) == 0) {
        strlcpy(qsfp_devsubname,&qsfp_devname[4],sizeof(qsfp_devsubname));
    }
    else {
        strlcpy(qsfp_devsubname,qsfp_devname,sizeof(qsfp_devsubname));
    }

    dev_notice(qsfp->dev,"%s: dev %s , sub %s\n", __func__,
                                       dev_name(qsfp->dev),
                                       qsfp_devsubname);

    qsfp->debugfs_dir = debugfs_create_dir(qsfp_devsubname,
                         qsfp->fpc->debugfs_dir);
    if (IS_ERR(qsfp->debugfs_dir)) {
        dev_err(qsfp->dev, "%s: qsfp debugfs_create_dir fail, error %ld\n",
                           __func__,PTR_ERR(qsfp->debugfs_dir));
        return;
    }

    file = debugfs_create_file("state_info", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_qsfp_state_info_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp state_info debugfs_create_file fail,"
                             " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("i2c_address_info", 0600, qsfp->debugfs_dir,
                        qsfp, &qsfp_debug_qsfp_i2c_address_info_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp i2c_address_info debugfs_create_file "
                             "fail, error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("port_num_info", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_qsfp_port_num_info_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp port_num_info debugfs_create_file fail,"
                           "error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("module_identifier", 0600, qsfp->debugfs_dir,
                   qsfp, &qsfp_debug_qsfp_module_identifier_info_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp module_identifier debugfs_create_file"
                          " fail, error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
    }
    return;

}

void qsfp_debugfs_exit(struct qsfp *qsfp)
{
    if (IS_ERR(qsfp->debugfs_dir) || IS_ERR(qsfp->fpc->debugfs_dir) ||
        IS_ERR(transceiver_debugfs_dir)) {
        dev_err(qsfp->dev,"%s: debugfs_create_dir fail, error (%ld %ld %ld)\n",
                        __func__,PTR_ERR(transceiver_debugfs_dir),
                        PTR_ERR(qsfp->fpc->debugfs_dir),
                        PTR_ERR(qsfp->debugfs_dir));
        return;
    }
    debugfs_remove_recursive(qsfp->debugfs_dir);
}
#else
void transceiver_debugfs_init(void)
{
}

void transceiver_debugfs_exit(void)
{
}

void fpc_debugfs_init(struct fpc *fpc)
{
}

void fpc_debugfs_exit(struct fpc *fpc)
{
}

void qsfp_debugfs_init(struct qsfp *qsfp)
{
}

void qsfp_debugfs_exit(struct qsfp *qsfp)
{
}
#endif
