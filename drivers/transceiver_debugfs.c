/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include "transceiver_debugfs.h"


#if IS_ENABLED(CONFIG_DEBUG_FS)

struct dentry *transceiver_debugfs_dir;

static const char *mod_identifier_to_str(u8 spec_id)
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

static const char *mod_link_codes_to_str(unsigned short mod_link_codes)
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

static int qsfp_debug_qsfp_power_info_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
        seq_printf(s, "QSFP module is not attached\n");
        return 0;
    }

    seq_printf(s, "Maximum Power (mW) : %u\n",qsfp->max_power_mW);
    seq_printf(s, "Module Power (mW) : %u\n",qsfp->module_power_mW);

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_qsfp_power_info);

static int qsfp_debug_qsfp_port_num_info_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    seq_printf(s, "0x%X\n",qsfp->port_num);

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_qsfp_port_num_info);

static int qsfp_debug_qsfp_module_power_class_info_show(struct seq_file *s,
                               void *data)
{
    struct qsfp *qsfp = s->private;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
        seq_printf(s, "QSFP module is not attached\n");
        return 0;
    }

    seq_printf(s, "0x%X\n",qsfp->module_power_class);

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_qsfp_module_power_class_info);

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
    struct sff8636_eeprom_id *id;
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
        seq_printf(s, "{0x%X} %s\n",qsfp->module_revision,
                      sff8636_mod_revision_to_str(qsfp->module_revision));
        break;
    default:
        if (*spec_id == 0x00) {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "Unknown module\n",*spec_id);
        }
        else {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                        "%s Transceiver module is not supported\n",
                        *spec_id,mod_identifier_to_str(*spec_id));
        }
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_revision_info);

static int qsfp_debug_phys_ext_id_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;
    u8 powerclassl,powerclassh;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
        seq_printf(s, "QSFP module is not attached\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        powerclassl = id->base.phys_ext_id >> 6;
        powerclassh = id->base.phys_ext_id & 0x03;
        seq_printf(s, "Extended Identifier: 0x%X\n",id->base.phys_ext_id);
        seq_printf(s, "BIT[7-6]: ");
        switch (powerclassl) {
        case 0x00:
            seq_printf(s, "Power Class 1 (1.5 W max)\n");
            break;
        case 0x01:
            seq_printf(s, "Power Class 2 (2.0 W max)\n");
            break;
        case 0x02:
            seq_printf(s, "Power Class 3 (2.5 W max)\n");
            break;
        case 0x03:
            seq_printf(s, "Power Class 4 (3.5 W max) and Power Classes 5,"
                          " 6 or 7\n");
            break;
        }
        id->base.phys_ext_id&BIT(5)?seq_printf(s, "  BIT[5]: Power Class 8 "
                                    "implemented\n"):
                  seq_printf(s, "  BIT[5]: Power Class 8 not implemented\n");
        id->base.phys_ext_id&BIT(4)?seq_printf(s, "  BIT[4]: CLEI code present"
                                " in Page 02h\n"):
                  seq_printf(s, "  BIT[4]: No CLEI code present in"
                                " Page 02h\n");
        id->base.phys_ext_id&BIT(3)?seq_printf(s, "  BIT[3]: CDR present"
                                " in Tx\n"):
                  seq_printf(s, "  BIT[3]: No CDR in Tx\n");
        id->base.phys_ext_id&BIT(2)?seq_printf(s, "  BIT[2]: CDR present in"
                                " Rx\n"):
                  seq_printf(s, "  BIT[2]: No CDR in Rx\n");
        seq_printf(s, "BIT[1-0]: ");
        switch (powerclassh) {
        case 0x00:
            seq_printf(s, "Power Classes 1 to 4\n");
            break;
        case 0x01:
            seq_printf(s, "Power Class 5 (4.0 W max)\n");
            break;
        case 0x02:
            seq_printf(s, "Power Class 6 (4.5 W max)\n");
            break;
        case 0x03:
            seq_printf(s, "Power Class 7 (5.0 W max)\n");
            break;
        }
        break;
    default:
        if (*spec_id == 0x00) {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "Unknown module\n",*spec_id);
        }
        else {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                        "%s Transceiver module is not supported\n",
                        *spec_id,mod_identifier_to_str(*spec_id));
        }
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_phys_ext_id);

static int qsfp_debug_connector_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
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
    default:
        if (*spec_id == 0x00) {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "Unknown module\n",*spec_id);
        }
        else {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                        "%s Transceiver module is not supported\n",
                        *spec_id,mod_identifier_to_str(*spec_id));
        }
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_connector);

static int qsfp_debug_encoding_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
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
        seq_printf(s, "{0x%X} %s\n",id->base.encoding,
                     sff8636_mod_encoding_to_str(id->base.encoding));
        break;
    default:
        if (*spec_id == 0x00) {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "Unknown module\n",*spec_id);
        }
        else {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                        "%s Transceiver module is not supported\n",
                        *spec_id,mod_identifier_to_str(*spec_id));
        }
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_encoding);

static int qsfp_debug_vendor_info_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
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
    default:
        if (*spec_id == 0x00) {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "Unknown module\n",*spec_id);
        }
        else {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                        "%s Transceiver module is not supported\n",
                        *spec_id,mod_identifier_to_str(*spec_id));
        }
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_vendor_info);

static int qsfp_debug_link_codes_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
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
        seq_printf(s, "{0x%X} %s\n",id->ext.link_codes,
                     mod_link_codes_to_str(id->ext.link_codes));
        break;
    default:
        if (*spec_id == 0x00) {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "Unknown module\n",*spec_id);
        }
        else {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                        "%s Transceiver module is not supported\n",
                        *spec_id,mod_identifier_to_str(*spec_id));
        }
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_link_codes);

static int qsfp_debug_max_case_temp_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
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
        seq_printf(s, "0x%X\n",id->base.max_case_temp);
        break;
    default:
        if (*spec_id == 0x00) {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "Unknown module\n",*spec_id);
        }
        else {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                        "%s Transceiver module is not supported\n",
                        *spec_id,mod_identifier_to_str(*spec_id));
        }
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_max_case_temp);

static int qsfp_debug_device_tech_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;
    u8 dtechl,dtechr;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
          seq_printf(s, "QSFP module is not attached\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        dtechl = id->base.device_tech >> 4;
        dtechr = id->base.device_tech & 0x0F;
        seq_printf(s, "Device Technology: 0x%X\n",id->base.device_tech);
        seq_printf(s, "Transmitter technology BIT[7-4]: ");
        switch (dtechl) {
        case 0x00:
            seq_printf(s, "850 nm VCSEL\n");
            break;
        case 0x01:
            seq_printf(s, "1310 nm VCSEL\n");
            break;
        case 0x02:
            seq_printf(s, "1550 nm VCSEL\n");
            break;
        case 0x03:
            seq_printf(s, "1310 nm FP\n");
            break;
        case 0x04:
            seq_printf(s, "1310 nm DFB\n");
            break;
        case 0x05:
            seq_printf(s, "1550 nm DFB\n");
            break;
        case 0x06:
            seq_printf(s, "1310 nm EML\n");
            break;
        case 0x07:
            seq_printf(s, "1550 nm EML\n");
            break;
        case 0x08:
            seq_printf(s, "Other / Undefined\n");
            break;
        case 0x09:
            seq_printf(s, "1490 nm DFB\n");
            break;
        case 0x0A:
            seq_printf(s, "Copper cable unequalized\n");
            break;
        case 0x0B:
            seq_printf(s, "Copper cable passive equalized\n");
            break;
        case 0x0C:
            seq_printf(s, "Copper cable, near and far end limiting active"
                           "equalizers\n");
            break;
        case 0x0D:
            seq_printf(s, "Copper cable, far end limiting active"
                          " equalizers\n");
            break;
        case 0x0E:
            seq_printf(s, "Copper cable, near end limiting active"
                          " equalizers\n");
            break;
        case 0x0F:
            seq_printf(s, "Copper cable, linear active equalizers\n");
            break;
        }
        dtechr&BIT(3)?seq_printf(s, "BIT[3]: Active wavelength control\n"):
                       seq_printf(s, "BIT[3]: No wavelength control\n");
        dtechr&BIT(2)?seq_printf(s, "BIT[2]: Cooled transmitter\n"):
                       seq_printf(s, "BIT[2]: Uncooled transmitter device\n");
        dtechr&BIT(1)?seq_printf(s, "BIT[1]: APD detector\n"):
                       seq_printf(s, "BIT[1]: Pin detector\n");
        dtechr&BIT(0)?seq_printf(s, "BIT[0]: Transmitter tunable\n"):
                         seq_printf(s, "BIT[0]: Transmitter not tunable\n");
        break;
    default:
        if (*spec_id == 0x00) {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "Unknown module\n",*spec_id);
        }
        else {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                        "%s Transceiver module is not supported\n",
                        *spec_id,mod_identifier_to_str(*spec_id));
        }
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_tech);

static int qsfp_debug_device_diagmon_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
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
        seq_printf(s, "Diagnostic Monitoring Type: 0x%X\n",id->ext.diagmon);
        id->ext.diagmon&BIT(5)?seq_printf(s, "BIT[5]: Temperature monitoring"
                                             " implemented\n"):
            seq_printf(s, "BIT[5]: Temperature monitoring Not implemented"
                              " or pre-Rev 2.8\n");
        id->ext.diagmon&BIT(4)?seq_printf(s, "BIT[4]: Supply voltage"
                                             " monitoring implemented\n"):
            seq_printf(s, "BIT[4]: Supply voltage monitoring Not implemented"
                          " or pre-Rev 2.8\n");
        id->ext.diagmon&BIT(3)?seq_printf(s, "BIT[3]: Received power "
                               "measurements type is Average Power\n"):
            seq_printf(s, "BIT[3]: Received power measurements type is OMA\n");
        id->ext.diagmon&BIT(2)?seq_printf(s, "BIT[2]: Transmitter power "
                                             "measurement Supported\n"):
            seq_printf(s, "BIT[2]: Transmitter power measurement "
                                               "Not supported\n");
        break;
    default:
        if (*spec_id == 0x00) {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "Unknown module\n",*spec_id);
        }
        else {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                        "%s Transceiver module is not supported\n",
                        *spec_id,mod_identifier_to_str(*spec_id));
        }
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_diagmon);

/*
* Information about the Qsfp status indicators (Page 00h, Byte 2) is being
* exported to Sysfs.
*/
static int qsfp_debug_status_indicators_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;
    u8 status;
    int ret;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
        seq_printf(s, "QSFP module is not attached\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        ret = qsfp_read(qsfp, SFF8636_IRQ_FLAGS, &status,
                         sizeof(status));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        seq_printf(s, "Status Indicators: 0x%X\n",status);
        status&BIT(2)?seq_printf(s, "BIT[2]: Flat upper memory\n"):
                      seq_printf(s, "BIT[2]: Paged upper memory\n");
        status&BIT(1)?seq_printf(s, "BIT[1]: IntL not asserted\n"):
                      seq_printf(s, "BIT[1]: IntL asserted\n");
        status&BIT(0)?seq_printf(s, "BIT[0]: Free side does not yet have"
                                    " valid monitor data due to device reset\n"
                                    "or power up reset or prior to a valid "
                                    "suite of monitor readings\n"):
                      seq_printf(s, "BIT[0]: Free side have valid "
                                    "monitor data like temperature & supply voltage\n");
        break;
    default:
        if (*spec_id == 0x00) {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "Unknown module\n",*spec_id);
        }
        else {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "%s Transceiver module not supported\n",
                          *spec_id,mod_identifier_to_str(*spec_id));
        }
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_status_indicators);

/*
* Information about the Qsfp device temperature (Page 00h Bytes 22-23)
* is being exported to Sysfs.
*/
static int qsfp_debug_device_temperature_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    u8 *spec_id = (u8*)&qsfp->id;
    char temperature_data[75] = {};
    int16_t tempc;
    int32_t temp;
    int ret;

    /* Ensure that the device is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
        seq_printf(s, "QSFP module is not attached\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        ret = qsfp_read(qsfp, SFF8636_TEMPERATURE, &tempc,
                               sizeof(tempc));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        /* Convert tempc info to cpu byte order */
        tempc = be16_to_cpu(tempc & 0XFFFF);

        /* (tempc * 1000) to get the three digit precision
           while converting it to celsius. */
        temp = tempc * 1000;

        /* Convert the temp to degrees Celsius by dividing it by 256. */
        temp = temp/256;

        /* To avoid printing of the minus(-) symbol in decimal places.
           Multiply temp with -1 for negative values. */
        scnprintf(temperature_data,75,"Temperature: %d.%03d °C",temp/1000,
                                    (temp < 0 ? (temp * -1) : temp)%1000);
        seq_printf(s, "%s\n", temperature_data);
        break;
    default:
        if (*spec_id == 0x00) {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "Unknown module\n",*spec_id);
        }
        else {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "%s Transceiver module is not supported\n",
                          *spec_id,mod_identifier_to_str(*spec_id));
       }
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_temperature);

/*
* Information about the Qsfp device supply voltage (Page 00h Bytes 26-27)
* is being exported to Sysfs.
*/
static int qsfp_debug_device_Supply_Voltage_show(struct seq_file *s,
                                                    void *data)
{
    struct qsfp *qsfp = s->private;
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
        ret = qsfp_read(qsfp, SFF8636_SUPPLY_VOLTAGE, &supply_voltage_t,
                              sizeof(supply_voltage_t));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        /* convert supply_voltage_t info to cpu byte order */
        supply_voltage_t = be16_to_cpu(supply_voltage_t & 0XFFFF);

        /* supply voltage in volts (supply_voltage_t * 100 μV/ 1000000) */
        scnprintf(voltage_data,75,"Supply Voltage: %d.%03d V",
                supply_voltage_t/10000, supply_voltage_t%10000);
        seq_printf(s, "%s\n", voltage_data);
        break;
    default:
        if (*spec_id == 0x00) {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "Unknown module\n",*spec_id);
        }
        else {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "%s Transceiver module is not supported\n",
                          *spec_id,mod_identifier_to_str(*spec_id));
        }
    }
    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_Supply_Voltage);

/*
* Information about the Qsfp Channel Monitor value of Rx Power
* (Page 00h Bytes 34-41) is being exported to Sysfs.
*/
static int qsfp_debug_device_rx_power_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    u8 *spec_id = (u8*)&qsfp->id;
    char rx_power_data[250] ={};
    u16 rx_power_t[4] = {};
    u8  rx_power[8] = {0};
    int ret;

    /* Ensure that the device is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
        seq_printf(s, "QSFP module is not attached\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
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
        scnprintf(rx_power_data,250,"Rx Power Lane1: %d.%03d mW\n"
                                    "Rx Power Lane2: %d.%03d mW\n"
                                    "Rx Power Lane3: %d.%03d mW\n"
                                    "Rx Power Lane4: %d.%03d mW",
                          rx_power_t[0]/10000,rx_power_t[0]%10000,
                          rx_power_t[1]/10000,rx_power_t[1]%10000,
                          rx_power_t[2]/10000,rx_power_t[2]%10000,
                          rx_power_t[3]/10000,rx_power_t[3]%10000);
        seq_printf(s, "%s\n", rx_power_data);
        break;
    default:
        if (*spec_id == 0x00) {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "Unknown module\n",*spec_id);
        }
        else {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "%s Transceiver module is not supported\n",
                          *spec_id,mod_identifier_to_str(*spec_id));
        }
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_rx_power);

/*
* Information about the Qsfp Channel Monitor value of Tx Bias Current
* (Page 00h Bytes 42-49) is being exported to Sysfs.
*/
static int qsfp_debug_device_tx_bias_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    u8 *spec_id = (u8*)&qsfp->id;
    char tx_bias_current_data[250] = {};
    u32 tx_bias_current_t[4] = {};
    u16 tx_bias_current = 0;
    u8 tx_bias[8] = {0};
    int ret;

    /* Ensure that the device is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
        seq_printf(s, "QSFP module is not attached\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        ret = qsfp_read(qsfp, SFF8636_TX_BIAS, tx_bias,
                              sizeof(tx_bias));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }
        tx_bias_current = (( tx_bias[0] << 8) | tx_bias[1]);
        /* tx_bias_current in  Micro Amp */
        tx_bias_current_t[0] = tx_bias_current * 2;

        tx_bias_current  = 0;
        tx_bias_current = (( tx_bias[2] << 8) | tx_bias[3]);
        /* tx_bias_current in  Micro Amp */
        tx_bias_current_t[1] = tx_bias_current * 2;

        tx_bias_current  = 0;
        tx_bias_current = (( tx_bias[4] << 8) | tx_bias[5]);
        /* tx_bias_current in  Micro Amp */
        tx_bias_current_t[2] = tx_bias_current * 2;

        tx_bias_current  = 0;
        tx_bias_current = (( tx_bias[6] << 8) | tx_bias[7]);
        /* tx_bias_current in  Micro Amp */
        tx_bias_current_t[3] = tx_bias_current * 2;

        scnprintf(tx_bias_current_data,250,"Tx Bias Current Lane1: %d.%03d"
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
    default:
        if (*spec_id == 0x00) {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "Unknown module\n",*spec_id);
        }
        else {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "%s Transceiver module is not supported\n",
                          *spec_id,mod_identifier_to_str(*spec_id));
        }
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_tx_bias);

/*
* Information about the Qsfp Channel Monitor value of Tx Power
* (Page 00h Bytes 50-57) is being exported to Sysfs.
*/
static int qsfp_debug_device_tx_power_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;
    char tx_power_data[250] ={};
    u16 tx_power_t[4] = {};
    u8  tx_power[8] = {0};
    u8 diagmon;
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
        scnprintf(tx_power_data,250,"Tx Power Lane1: %d.%03d mW\n"
                                    "Tx Power Lane2: %d.%03d mW\n"
                                    "Tx Power Lane3: %d.%03d mW\n"
                                    "Tx Power Lane4: %d.%03d mW",
                          tx_power_t[0]/10000,tx_power_t[0]%10000,
                          tx_power_t[1]/10000,tx_power_t[1]%10000,
                          tx_power_t[2]/10000,tx_power_t[2]%10000,
                          tx_power_t[3]/10000,tx_power_t[3]%10000);
        seq_printf(s, "%s\n", tx_power_data);
        break;
    default:
        if (*spec_id == 0x00) {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "Unknown module\n",*spec_id);
        }
        else {
            seq_printf(s, "Specification Identifier {0x%02X}\n"
                          "%s Transceiver module is not supported\n",
                          *spec_id,mod_identifier_to_str(*spec_id));
        }
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_tx_power);

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

    file = debugfs_create_file("power_info", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_qsfp_power_info_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp power_info debugfs_create_file fail,"
                           "error %ld\n", __func__,PTR_ERR(file));
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

    file = debugfs_create_file("module_power_class", 0600, qsfp->debugfs_dir,
                        qsfp, &qsfp_debug_qsfp_module_power_class_info_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp module_power_class debugfs_create_file"
                           "fail, error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("module_identifier", 0600, qsfp->debugfs_dir,
                   qsfp, &qsfp_debug_qsfp_module_identifier_info_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp module_identifier debugfs_create_file"
                          " fail, error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("module_revision", 0600, qsfp->debugfs_dir,
                                  qsfp, &qsfp_debug_revision_info_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp module_revision debugfs_create_file fail,"
                           "error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
     }

    file = debugfs_create_file("phys_ext_id", 0600, qsfp->debugfs_dir, qsfp,
                    &qsfp_debug_phys_ext_id_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp phys_ext_id debugfs_create_file fail,"
                           "error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("connector", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_connector_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp phys_ext_id debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("encoding", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_encoding_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp encoding debugfs_create_file fail,"
                           "error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("link_codes", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_link_codes_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp link_codes debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("vendor_info", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_vendor_info_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp vendor_info debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("max_case_temp", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_max_case_temp_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp max_case_temp debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("device_tech", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_device_tech_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp device_tech debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
     }

    file = debugfs_create_file("diagmon", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_device_diagmon_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp diagmon debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("status_indicators", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_status_indicators_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp status_indicators debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("temperature", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_device_temperature_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp temperature debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("supply_voltage", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_device_Supply_Voltage_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp supply_voltage debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("rx_power", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_device_rx_power_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp rx_power debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("tx_bias_current", 0600, qsfp->debugfs_dir,
                        qsfp, &qsfp_debug_device_tx_bias_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp tx_bias debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
        return;
    }

    file = debugfs_create_file("tx_power", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_device_tx_power_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp tx_power debugfs_create_file fail,"
                           " error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->debugfs_dir);
    }
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
