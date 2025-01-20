/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Code is derived from http://git.armlinux.org.uk/cgit/linux-arm.git/
 * tree/drivers/net/phy/sfp.h?h=cex7
 *
 */
#ifndef LINUX_QSFP_H
#define LINUX_QSFP_H

#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/rtnetlink.h>
#include <linux/of_platform.h>
#include <linux/ipc_logging.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>


#define QSFP_ADDR(device, page, addr) ((device) << 16 | (page) << 8 | (addr))

#include "sfp.h"
#include "sff8636.h"
#include "cmis.h"
#include "sff8472.h"
#include "transceiver_api.h"

#define QSFP_COMPATIBLE "sff,qsfp"

#define QSFP_PAGE_OFFSET (0x7F)
#define QSFP_LED1 (BIT(0))
#define QSFP_LED2 (BIT(1))
#define QSFP_LED_ON  (true)
#define QSFP_LED_OFF (false)

#define MAX_LANES 8

#define QSFP_TEMP_HIGH_ALARM (BIT(0))
#define QSFP_TEMP_LOW_ALARM (BIT(1))
#define QSFP_TEMP_HIGH_WARN (BIT(2))
#define QSFP_TEMP_LOW_WARN (BIT(3))

#define QSFP_VOLT_HIGH_ALARM (BIT(0))
#define QSFP_VOLT_LOW_ALARM (BIT(1))
#define QSFP_VOLT_HIGH_WARN (BIT(2))
#define QSFP_VOLT_LOW_WARN (BIT(3))

#define QSFP_EVENT_TEMP_HIGH_ALARM  "EVENT=TEMPERATURE_HIGH_ALARM"
#define QSFP_EVENT_TEMP_LOW_ALARM   "EVENT=TEMPERATURE_LOW_ALARM"
#define QSFP_EVENT_TEMP_HIGH_WARN   "EVENT=TEMPERATURE_HIGH_WARN"
#define QSFP_EVENT_TEMP_LOW_WARN    "EVENT=TEMPERATURE_LOW_WARN"

#define QSFP_EVENT_TEMP_HIGH_ALARM_RECOVERY  "EVENT=TEMPERATURE_HIGH_ALARM_RECOVERY"
#define QSFP_EVENT_TEMP_LOW_ALARM_RECOVERY   "EVENT=TEMPERATURE_LOW_ALARM_RECOVERY"
#define QSFP_EVENT_TEMP_HIGH_WARN_RECOVERY   "EVENT=TEMPERATURE_HIGH_WARN_RECOVERY"
#define QSFP_EVENT_TEMP_LOW_WARN_RECOVERY    "EVENT=TEMPERATURE_LOW_WARN_RECOVERY"

#define QSFP_EVENT_VOLT_HIGH_ALARM  "EVENT=VOLTAGE_HIGH_ALARM"
#define QSFP_EVENT_VOLT_LOW_ALARM   "EVENT=VOLTAGE_LOW_ALARM"
#define QSFP_EVENT_VOLT_HIGH_WARN   "EVENT=VOLTAGE_HIGH_WARN"
#define QSFP_EVENT_VOLT_LOW_WARN    "EVENT=VOLTAGE_LOW_WARN"

#define QSFP_EVENT_VOLT_HIGH_ALARM_RECOVERY  "EVENT=VOLTAGE_HIGH_ALARM_RECOVERY"
#define QSFP_EVENT_VOLT_LOW_ALARM_RECOVERY   "EVENT=VOLTAGE_LOW_ALARM_RECOVERY"
#define QSFP_EVENT_VOLT_HIGH_WARN_RECOVERY   "EVENT=VOLTAGE_HIGH_WARN_RECOVERY"
#define QSFP_EVENT_VOLT_LOW_WARN_RECOVERY    "EVENT=VOLTAGE_LOW_WARN_RECOVERY"

#define QSFP_EVENT_RX_LOS   "EVENT=RX_LOS"
#define QSFP_EVENT_TX_FAULT "EVENT=TX_FAULT"
#define QSFP_EVENT_TX_LOS   "EVENT=TX_LOS"

#define QSFP_EVENT_RX_POWER_HIGH_ALARM  "EVENT=RX_POWER_HIGH_ALARM"
#define QSFP_EVENT_RX_POWER_LOW_ALARM   "EVENT=RX_POWER_LOW_ALARM"
#define QSFP_EVENT_RX_POWER_HIGH_WARN   "EVENT=RX_POWER_HIGH_WARN"
#define QSFP_EVENT_RX_POWER_LOW_WARN    "EVENT=RX_POWER_LOW_WARN"

#define QSFP_EVENT_TX_POWER_HIGH_ALARM  "EVENT=TX_POWER_HIGH_ALARM"
#define QSFP_EVENT_TX_POWER_LOW_ALARM   "EVENT=TX_POWER_LOW_ALARM"
#define QSFP_EVENT_TX_POWER_HIGH_WARN   "EVENT=TX_POWER_HIGH_WARN"
#define QSFP_EVENT_TX_POWER_LOW_WARN    "EVENT=TX_POWER_LOW_WARN"

#define QSFP_EVENT_TX_BIAS_HIGH_ALARM  "EVENT=TX_BIAS_HIGH_ALARM"
#define QSFP_EVENT_TX_BIAS_LOW_ALARM   "EVENT=TX_BIAS_LOW_ALARM"
#define QSFP_EVENT_TX_BIAS_HIGH_WARN   "EVENT=TX_BIAS_HIGH_WARN"
#define QSFP_EVENT_TX_BIAS_LOW_WARN    "EVENT=TX_BIAS_LOW_WARN"

#define QSFP_EVENT_RX_CDR_LOL "EVENT=RX_CDR_LOL"
#define QSFP_EVENT_TX_CDR_LOL "EVENT=TX_CDR_LOL"
#define QSFP_EVENT_TX_ADAPTIVE_EQ_IN_FAIL "EVENT=TX_ADAPTIVE_EQ_IN_FAIL"

#define QSFP_EVENT_ERROR_I2C                "EVENT=ERROR_I2C"
#define QSFP_EVENT_ERROR_HIGH_POWER         "EVENT=ERROR_HIGH_POWER"
#define QSFP_EVENT_I2C_SCL_STUCK            "EVENT=I2C_SCL_STUCK"
#define QSFP_EVENT_I2C_SDA_STUCK            "EVENT=I2C_SDA_STUCK"

#define QSFP_EVENT_TX_ENABLE_FAIL           "EVENT=TX_ENABLE_FAIL"
#define QSFP_EVENT_TX_ENABLE_FAIL_RECOVERY  "EVENT=TX_ENABLE_FAIL_RECOVERY"

#define QSFP_EVENT_BUF_MAX (50)
#define QSFP_VENDOR_PN_STR_LEN (17)

struct dual_tcvr_entry {
    char *name;
    struct list_head list;
};

struct qsfp_eeprom_id {
    union {
        struct sff8636_eeprom_id sff8636;
        struct cmis_eeprom_id cmis;
        struct sfp_eeprom_id sff8472;
    };
};

struct qsfp_diag {
    union {
        struct sff8472_ddm_thresholds sff8472_ddm_limits;
        struct sff8636_ddm_thresholds sff8636_ddm_limits;
        struct cmis_thresholds cmis_ddm_limits;
    };
};

struct qsfp_flags {
    u8 rx_los;
    u8 tx_los;
    u8 tx_fault;

    u8 rx_cdr_lol;
    u8 tx_cdr_lol;
    u8 tx_adap_eq_in_fail;

    union {
        u8 temp:4;
        struct {
            u8 temp_high_alarm:1;
            u8 temp_low_alarm:1;
            u8 temp_high_warn:1;
            u8 temp_low_warn:1;
        };
    };

    union {
        u8 volt:4;
        struct {
            u8 volt_high_alarm:1;
            u8 volt_low_alarm:1;
            u8 volt_high_warn:1;
            u8 volt_low_warn:1;
        };
    };

    union {
        u8 rx_power_high_alarm;
        struct {
            u8 rx1_power_high_alarm:1;
            u8 rx2_power_high_alarm:1;
            u8 rx3_power_high_alarm:1;
            u8 rx4_power_high_alarm:1;
            u8 rx5_power_high_alarm:1;
            u8 rx6_power_high_alarm:1;
            u8 rx7_power_high_alarm:1;
            u8 rx8_power_high_alarm:1;
        };
    };

    union {
        u8 rx_power_low_alarm;
        struct {
            u8 rx1_power_low_alarm:1;
            u8 rx2_power_low_alarm:1;
            u8 rx3_power_low_alarm:1;
            u8 rx4_power_low_alarm:1;
            u8 rx5_power_low_alarm:1;
            u8 rx6_power_low_alarm:1;
            u8 rx7_power_low_alarm:1;
            u8 rx8_power_low_alarm:1;
        };
    };

    union {
        u8 rx_power_high_warn;
        struct {
            u8 rx1_power_high_warn:1;
            u8 rx2_power_high_warn:1;
            u8 rx3_power_high_warn:1;
            u8 rx4_power_high_warn:1;
            u8 rx5_power_high_warn:1;
            u8 rx6_power_high_warn:1;
            u8 rx7_power_high_warn:1;
            u8 rx8_power_high_warn:1;
        };
    };

    union {
        u8 rx_power_low_warn;
        struct {
            u8 rx1_power_low_warn:1;
            u8 rx2_power_low_warn:1;
            u8 rx3_power_low_warn:1;
            u8 rx4_power_low_warn:1;
            u8 rx5_power_low_warn:1;
            u8 rx6_power_low_warn:1;
            u8 rx7_power_low_warn:1;
            u8 rx8_power_low_warn:1;
        };
    };

    union {
        u8 tx_power_high_alarm;
        struct {
            u8 tx1_power_high_alarm:1;
            u8 tx2_power_high_alarm:1;
            u8 tx3_power_high_alarm:1;
            u8 tx4_power_high_alarm:1;
            u8 tx5_power_high_alarm:1;
            u8 tx6_power_high_alarm:1;
            u8 tx7_power_high_alarm:1;
            u8 tx8_power_high_alarm:1;
        };
    };

    union {
        u8 tx_power_low_alarm;
        struct {
            u8 tx1_power_low_alarm:1;
            u8 tx2_power_low_alarm:1;
            u8 tx3_power_low_alarm:1;
            u8 tx4_power_low_alarm:1;
            u8 tx5_power_low_alarm:1;
            u8 tx6_power_low_alarm:1;
            u8 tx7_power_low_alarm:1;
            u8 tx8_power_low_alarm:1;
        };
    };

    union {
        u8 tx_power_high_warn;
        struct {
            u8 tx1_power_high_warn:1;
            u8 tx2_power_high_warn:1;
            u8 tx3_power_high_warn:1;
            u8 tx4_power_high_warn:1;
            u8 tx5_power_high_warn:1;
            u8 tx6_power_high_warn:1;
            u8 tx7_power_high_warn:1;
            u8 tx8_power_high_warn:1;
        };
    };

    union {
        u8 tx_power_low_warn;
        struct {
            u8 tx1_power_low_warn:1;
            u8 tx2_power_low_warn:1;
            u8 tx3_power_low_warn:1;
            u8 tx4_power_low_warn:1;
            u8 tx5_power_low_warn:1;
            u8 tx6_power_low_warn:1;
            u8 tx7_power_low_warn:1;
            u8 tx8_power_low_warn:1;
        };
    };

    union {
        u8 tx_bias_high_alarm;
        struct {
            u8 tx1_bias_high_alarm:1;
            u8 tx2_bias_high_alarm:1;
            u8 tx3_bias_high_alarm:1;
            u8 tx4_bias_high_alarm:1;
            u8 tx5_bias_high_alarm:1;
            u8 tx6_bias_high_alarm:1;
            u8 tx7_bias_high_alarm:1;
            u8 tx8_bias_high_alarm:1;
        };
    };

    union {
        u8 tx_bias_low_alarm;
        struct {
            u8 tx1_bias_low_alarm:1;
            u8 tx2_bias_low_alarm:1;
            u8 tx3_bias_low_alarm:1;
            u8 tx4_bias_low_alarm:1;
            u8 tx5_bias_low_alarm:1;
            u8 tx6_bias_low_alarm:1;
            u8 tx7_bias_low_alarm:1;
            u8 tx8_bias_low_alarm:1;
       };
    };

    union {
        u8 tx_bias_high_warn;
        struct {
            u8 tx1_bias_high_warn:1;
            u8 tx2_bias_high_warn:1;
            u8 tx3_bias_high_warn:1;
            u8 tx4_bias_high_warn:1;
            u8 tx5_bias_high_warn:1;
            u8 tx6_bias_high_warn:1;
            u8 tx7_bias_high_warn:1;
            u8 tx8_bias_high_warn:1;
        };
    };

    union {
        u8 tx_bias_low_warn;
        struct {
            u8 tx1_bias_low_warn:1;
            u8 tx2_bias_low_warn:1;
            u8 tx3_bias_low_warn:1;
            u8 tx4_bias_low_warn:1;
            u8 tx5_bias_low_warn:1;
            u8 tx6_bias_low_warn:1;
            u8 tx7_bias_low_warn:1;
            u8 tx8_bias_low_warn:1;
        };
    };
};

struct qsfp_status {
    u8 present:1;
    u8 rx_los:1;
    u8 tx_fault:1;
    u8 tx_disable:1;
    u8 eth_linkup:1;
};

struct qsfp_support {
    u8 temp_flags:1;
    u8 volt_flags:1;
    u8 rx_los:1;
    u8 rx_cdr_lol:1;
    u8 rx_power_flags:1;
    u8 tx_disable:1;
    u8 tx_los:1;
    u8 tx_cdr_lol:1;
    u8 tx_fault:1;
    u8 tx_adap_eq_in_fail:1;
    u8 tx_power_flags:1;
    u8 tx_bias_flags:1;
    u8 rate_select:1;
};

#if IS_ENABLED(CONFIG_DEBUG_FS)
struct qsfp_simulation {
    u8 remove:1;
    u8 flags:1;
};
#endif

struct qsfp_param_info {
    u8 laser_temp_sup_flag:1;
    u8 laser_temp_thsup_flag:1;
    u8 unused:6;
    u32  ltemp_reg_addr;
    struct sff8636_param_cfg param_cfg;
    struct sff8636_param_thresholds param_th;
}__packed;

struct qsfp {
    struct device *dev;
    struct fpc *fpc;
    struct i2c_adapter *i2c;
    u32 max_power_mW;
    u32 module_power_mW;
    u8 port_num;
    u8 i2c_address_dev0;
    u8 i2c_address_dev1;
    bool module_flat_mem;
    bool need_poll;
    struct delayed_work poll;
    u8 module_power_class;
    u8 module_revision;
    u8 sm_mod_state;
    u8 sm_dev_state;
    u8 sm_link_state;
    u8 sm_mod_tries;
    u8 lane_presence;
    u8 lane_min_speed;
    u8 lane_max_speed;
    u16 fpc_qsfp_i2c_recover_delay;
    size_t i2c_block_size;

    struct delayed_work timeout;
    struct mutex sm_mutex;            /* Protects state machine */

    struct timer_list qsfp_flt_lnkd_timer;
    struct qsfp_eeprom_id id;
    const struct qsfp_spec_ops *spec_ops;

    struct lane* lane[MAX_LANES];
    u8 num_lanes;
    bool is_adapter;

#if IS_ENABLED(CONFIG_DEBUG_FS)
    struct dentry *debugfs_dir;
    struct dentry *module_debugfs_dir;
    struct qsfp_flags sim_flags;
    struct qsfp_simulation sim;
    /* added for debugging purpose will be removed after soaking for sometime */
    u16 reset_counter;
    u16 i2c_stuck_counter;
    u16 read_write_2nd_fail;
#endif
   struct kobject *qsfp_sysfs_dir;
   struct kobject *sensor_sysfs_dir;
   /* Stores presence LOS TX Fault TX Disable status */
   struct qsfp_status status;
   struct qsfp_flags flags;
   /* Features supported/implemented */
   struct qsfp_support support;
   struct qsfp_diag diag;
   struct qsfp_param_info param_info;
};

struct qsfp_spec_ops {
    /* called during module insert to read EEPROM */
    int (*mod_probe)(struct qsfp *qsfp);
    /* Disable uninterested interrupts */
    int (*disable_redundant_irq)(struct qsfp *qsfp);
    int (*disable_enable_lane_irq)(struct qsfp *qsfp, u8 lane, bool enable);
    /* Updates module current flags LOS,TX Fault, alarms, warning etc */
    void (*update_flags)(struct qsfp *qsfp);
    /* Disable TX for whole transceiver module */
    int (*mod_tx_disable)(struct qsfp *qsfp);
    /* Enable TX lanewise */
    int (*lane_tx_enable)(struct lane *lane);
    /* Disable TX lanewise */
    int (*lane_tx_disable)(struct lane *lane);
    /* Check feature like LOS,TX Fault implemented or not and
     * update features field accordingly
     */
    int (*update_features_supported)(struct qsfp *qsfp);
    /* Gets power details like max power and power class */
    int (*module_parse_power)(struct qsfp *qsfp);
    /* called to handle situation of module max power is more
     * than max allowed power
     */
    int (*handle_max_power_exceed)(struct qsfp *qsfp);
    /* configure module for high power */
    int (*mod_high_power)(struct qsfp *qsfp);
    /* configure module for low power */
    int (*mod_low_power)(struct qsfp *qsfp);
    /* Dumps EEPROM data */
    void (*eeprom_print)(struct qsfp *qsfp);
    /* Ethtool callback function to get module info */
    int (*module_info)(struct qsfp *qsfp, struct ethtool_modinfo *modinfo);
    /* Ethtool callback function to get module EEPROM info */
    int (*module_eeprom)(struct qsfp *qsfp, struct ethtool_eeprom *ee, u8 *data);
    /* Gets connector type */
    u8 (*get_connector_type)(struct qsfp *qsfp);
    /* Gets lane speed  mask*/
    int (*get_lane_speed)(struct qsfp *qsfp, trx_speed_mask *speed_mask);
    /* Gets transceive type */
    u8 (*get_transceiver_type)(struct qsfp *qsfp);
    /* Gets link length ramge */
    trx_link_length_range (*get_link_length_range)(struct qsfp *qsfp);
    /* Gets Near-End Implementation */
    int (*get_lanes_presence)(struct qsfp *qsfp, trx_lane_cfg* laneinfo);
    /* Gets Far-End Implementation */
    int (*get_breakout_config)(struct qsfp *qsfp,
                          trx_breakout_cfg* bo_config);
    /* Set Rate select */
    int (*set_rate_select)(struct qsfp *qsfp, bool enable);
    unsigned long (*irq_delay)(struct qsfp *qsfp);
    int (*create_debugfs)(struct qsfp *qsfp);
    int (*spec_sensor_sysfs_init)(struct qsfp *qsfp);
    int (*spec_sensor_sysfs_exit)(struct qsfp *qsfp);
};

enum {
    /* Events */
    QSFP_E_INSERT = 0,
    QSFP_E_REMOVE,
    QSFP_E_DEV_ATTACH,
    QSFP_E_DEV_DETACH,
    QSFP_E_LANE_DOWN,
    QSFP_E_DEV_DOWN,
    QSFP_E_DEV_UP,
    QSFP_E_ETH_UP,
    QSFP_E_ETH_DOWN,
    QSFP_E_TX_FAULT,
    QSFP_E_TX_FAULT_RECOVERY,
    QSFP_E_RX_LOS,
    QSFP_E_RX_LOS_RECOVERY,
    QSFP_E_REVISIT,

    /* Module states */
    QSFP_MOD_EMPTY = 0,
    QSFP_MOD_ERROR_I2C,
    QSFP_MOD_ERROR_HPOWER,
    QSFP_MOD_ERROR_TX_ENABLE_FAIL,
    QSFP_MOD_REJECT_SPEC,
    QSFP_MOD_REJECT_PWR,
    QSFP_MOD_PROBE,
    QSFP_MOD_WAITHPOWER,
    QSFP_MOD_WAITDEV,
    QSFP_MOD_PRESENT,

    /* Upstream Device states */
    QSFP_DEV_DETACHED = 0,
    QSFP_DEV_DOWN,
    QSFP_DEV_UP,

    /* Link states */
    QSFP_S_DOWN = 0,
    QSFP_S_RX_LOS,
    QSFP_S_TX_FAULT,
    QSFP_S_LINK_UP,
};

enum {
    E_UNSUPPORTED_SPEC = 1000,
    E_MAX_POWER_EXCEED,
};

#define PROBE_RETRY             20
#define PROBE_RETRY_TIME_GAP    msecs_to_jiffies(500)
#define MOD_READY_TIME          msecs_to_jiffies(300)

#define QSFP_FAULT_STR_MAX (80)
#define QSFP_FEATURE_STR_MAX (150)
#define QSFP_I2C_FAIL_RETRY (10)
#define FPC_QSFP_I2C_RECOVER_TIME_STEP (200)
#define FPC_QSFP_I2C_RECOVER_TIME_MAX (1000)

enum {
    SFF8024_CONNECTOR_FC1_COPPER = 0x02,
    SFF8024_CONNECTOR_FC2_COPPER = 0x03,
    SFF8024_CONNECTOR_BNC_TNC    = 0x04,
    SFF8024_CONNECTOR_FC_COAX    = 0x05,
    SFF8024_CONNECTOR_CS_OPTICAL = 0x25,
    SFF8024_CONNECTOR_SN_OPTICAL = 0x26,
    SFF8024_CONNECTOR_MPO_2X12   = 0x27,
    SFF8024_CONNECTOR_MPO_1X16   = 0x28,
};

enum {
    SFF8024_ID_GBIC              = 0x01,
    SFF8024_ID_300_XBI           = 0x04,
    SFF8024_ID_XENPAK            = 0x05,
    SFF8024_ID_XFP               = 0x06,
    SFF8024_ID_XFF               = 0x07,
    SFF8024_ID_XFP_E             = 0x08,
    SFF8024_ID_XPAK              = 0x09,
    SFF8024_ID_X2                = 0x0A,
    SFF8024_ID_CXP               = 0x0E,
    SFF8024_ID_SH_ML_HD_4X       = 0x0F,
    SFF8024_ID_H_ML_HD_8X        = 0x10,
    SFF8024_ID_CXP2              = 0x12,
    SFF8024_ID_CPFP_S1_S2        = 0x13,
    SFF8024_ID_SH_ML_HD_4X_FO    = 0x14,
    SFF8024_ID_SH_ML_HD_8X_FO    = 0x15,
    SFF8024_ID_CPFP_S3           = 0x16,
    SFF8024_ID_MICRO_QSFP        = 0x17,
    SFF8024_ID_QSFPDD_CMIS       = 0x18,
    SFF8024_ID_QSFP_8X           = 0x19,
    SFF8024_ID_SFP_DD_2X         = 0x1A,
    SFF8024_ID_DSFP              = 0x1B,
    SFF8024_ID_MLNK_X4           = 0x1C,
    SFF8024_ID_MLNK_X8           = 0x1D,
    SFF8024_ID_QSFP_P_CMIS       = 0x1E,
};

extern const struct of_device_id fpc_qsfp_of_match[];
extern const struct qsfp_spec_ops sff8636_spec_ops;
extern const struct qsfp_spec_ops cmis_spec_ops;
extern const struct qsfp_spec_ops sff8472_spec_ops;
extern const struct sfp_socket_ops lane_ops;

extern int fpc_is_module_present(const struct qsfp *qsfp);
extern int fpc_enable_qsfp_interrupt(const struct qsfp *qsfp);

extern const char *mod_identifier_to_str(u8 spec_id);
extern const char *mod_link_codes_to_str(unsigned short mod_link_codes);

extern int qsfp_read(struct qsfp *qsfp, u32 addr, void *buf, size_t len);
extern int qsfp_write(struct qsfp *qsfp, u32 addr, void *buf, size_t len);
extern u8 qsfp_check(void *buf, size_t len);
extern int qsfp_get_link_type(struct qsfp *qsfp, u8* link_info);
extern void qsfp_sm_mod_next(struct qsfp *qsfp, u8 state, u32 timeout);
extern void qsfp_check_state(struct qsfp *qsfp);

extern int sff8636_create_debugfs_files (struct qsfp *qsfp);
extern int cmis_create_debugfs_files(struct qsfp *qsfp);
extern int sff8472_create_debugfs_files(struct qsfp *qsfp);

extern void lane_sm_event(struct lane *lane, u32 event);
extern void lane_sm_mod_error(struct lane *lane, u8 state);

extern trx_link_length_range qsfp_link_code_to_link_length_range(u8 link_code);
extern trx_link_length_range qsfp_mmf_code_to_link_length_range(u8 mmf_code);
extern trx_link_length_range qsfp_smf_code_to_link_length_range(u8 smf_code);
extern int sff8472_create_sysfs_files(struct qsfp *qsfp);
extern int sff8636_create_sysfs_files(struct qsfp *qsfp);
extern int cmis_create_sysfs_files(struct qsfp *qsfp);
extern int sff8472_remove_sysfs_files(struct qsfp *qsfp);
extern int sff8636_remove_sysfs_files(struct qsfp *qsfp);
extern int cmis_remove_sysfs_files(struct qsfp *qsfp);
extern int qsfp_module_parse_ddm_thresholds(struct qsfp* qsfp);
int qsfp_get_module_eeprom(struct qsfp *qsfp,
                        struct ethtool_eeprom *ee, u8 *data);
int parse_sff8636_page20_21(struct qsfp *qsfp);
int qsfp_module_parse_laser_temp(struct qsfp *qsfp);
extern long trx_calibrate_temp(__be16 tmp_val);
extern void update_runtime_dual_cfg(struct qsfp *qsfp);

extern const char * const link_length_range_to_str[];
extern  const char * const link_type_to_str[];
extern  const char * const reasoncode_to_str[];
extern  const char * const trxspeed_to_str[];
extern  const char * const trxtype_to_str[];

#define LINK_LENGTH_RANGE_MAX_INDEX (11)
#define TRX_TYPE_MAX_INDEX (5)
#define REASON_CODE_MAX_INDEX (4)
#define LINK_TYPE_MAX_INDEX (256)


#define TRX_EEPROM_PAGE_LENGTH    (256)
#define TRX_EEPROM_UP_PAGE_LENGTH    (128)
#define TRX_MAX_DIAG_LOG_MSG_SIZE 512

/* This is used to access the optional upper pages (1 - 17) in the QSFP
 * memory map. Page 1 is available on offset 256 through 383, page 2 -
 * on offset 384 through 511, page 3 - on offset 512 through 639, page
 * 17 - on  2304 through 2431.
 */
#define TRX_PAGE_GET(off) (((off) - \
                TRX_EEPROM_PAGE_LENGTH) / \
                TRX_EEPROM_UP_PAGE_LENGTH + 1)

extern void *trx_ipc_log_buf;
extern struct device *uevent_tcvr_device;
/* Flag to transmit kobject uevents for logging to userspace */
extern u8 is_qxdm_log_en;

#define TRX_IPC_LOG_PAGES 100

#define TRX_IPC_Log(buf, fmt, args...) \
do {\
    ipc_log_string((buf), fmt, ## args); \
} while (0)

#define TRX_LOG_INFO(p, fmt, args...) \
do {\
    char buf_udev[TRX_MAX_DIAG_LOG_MSG_SIZE]; \
    char *msg[] = {buf_udev, NULL}; \
    dev_notice(p->dev, " %s: " fmt, __func__, ## args);\
    if (trx_ipc_log_buf) { \
        TRX_IPC_Log(trx_ipc_log_buf , " %s:%s: " fmt, dev_name(p->dev), __func__\
                                  , ## args); \
    } \
    if (is_qxdm_log_en) { \
        snprintf(buf_udev, TRX_MAX_DIAG_LOG_MSG_SIZE, "TRX_IPC_LOG_MSG_INFO=%s:%s: "\
                     fmt, dev_name(p->dev), __func__, ## args); \
        kobject_uevent_env(&p->dev->kobj, KOBJ_CHANGE, msg); \
    } \
} while (0)

#define TRX_LOG_WARN(p, fmt, args...) \
do {\
    char buf_udev[TRX_MAX_DIAG_LOG_MSG_SIZE]; \
    char *msg[] = {buf_udev, NULL}; \
    dev_warn(p->dev, " %s: " fmt, __func__, ## args);\
    if (trx_ipc_log_buf) { \
        TRX_IPC_Log(trx_ipc_log_buf , " %s:%s: " fmt, dev_name(p->dev), __func__\
                                  , ## args); \
    } \
    if (is_qxdm_log_en) { \
        snprintf(buf_udev, TRX_MAX_DIAG_LOG_MSG_SIZE, "TRX_IPC_LOG_MSG_WARNING=%s:%s: "\
                     fmt, dev_name(p->dev), __func__, ## args); \
        kobject_uevent_env(&p->dev->kobj, KOBJ_CHANGE, msg); \
    } \
} while (0)

#define TRX_LOG_ERR(p, fmt, args...) \
do {\
    char buf_udev[TRX_MAX_DIAG_LOG_MSG_SIZE]; \
    char *msg[] = {buf_udev, NULL}; \
    dev_err(p->dev, " %s: " fmt, __func__, ## args);\
    if (trx_ipc_log_buf) { \
        TRX_IPC_Log(trx_ipc_log_buf , " ERR:%s:%s: " fmt, dev_name(p->dev), __func__\
                                  , ## args); \
    } \
    if (is_qxdm_log_en) { \
        snprintf(buf_udev, TRX_MAX_DIAG_LOG_MSG_SIZE, "TRX_IPC_LOG_MSG_ERROR=%s:%s: "\
                            fmt,dev_name(p->dev), __func__, ## args); \
        kobject_uevent_env(&p->dev->kobj, KOBJ_CHANGE, msg); \
    } \
} while (0)

#define TRX_LOG_PDEV_ERR(p, fmt, args...) \
do {\
    char buf_udev[TRX_MAX_DIAG_LOG_MSG_SIZE]; \
    char *msg[] = {buf_udev, NULL}; \
    dev_err(p->dev, " %s: " fmt, __func__, ## args);\
    if (trx_ipc_log_buf) { \
        TRX_IPC_Log(trx_ipc_log_buf , " ERR:%s:%s: " fmt, dev_name(p->dev), __func__\
                                  , ## args); \
    } \
    if (is_qxdm_log_en) { \
        snprintf(buf_udev, TRX_MAX_DIAG_LOG_MSG_SIZE, "TRX_IPC_LOG_MSG_ERROR=%s:%s: "\
                            fmt,dev_name(p->dev), __func__, ## args); \
        kobject_uevent_env(p->dev.kobj, KOBJ_CHANGE, msg); \
    } \
} while (0)

#define TRX_LOG_INFO_NODEV(fmt, args...) \
do {\
    char buf_udev[TRX_MAX_DIAG_LOG_MSG_SIZE]; \
    char *msg[] = {buf_udev, NULL}; \
    pr_notice("fpc-qsfp %s: " fmt, __func__, ## args);\
    if (trx_ipc_log_buf) { \
        TRX_IPC_Log(trx_ipc_log_buf , " %s: " fmt, __func__, ## args); \
    } \
    if(is_qxdm_log_en && uevent_tcvr_device) {\
        snprintf(buf_udev, TRX_MAX_DIAG_LOG_MSG_SIZE, "TRX_IPC_LOG_MSG_INFO=%s: "\
                            fmt, __func__, ## args); \
        kobject_uevent_env(&uevent_tcvr_device->kobj, KOBJ_CHANGE, msg); \
    } \
} while (0)

#define TRX_LOG_ERR_NODEV(fmt, args...) \
do {\
    char buf_udev[TRX_MAX_DIAG_LOG_MSG_SIZE]; \
    char *msg[] = {buf_udev, NULL}; \
    pr_err("fpc-qsfp %s: " fmt, __func__, ## args);\
    if (trx_ipc_log_buf) { \
        TRX_IPC_Log(trx_ipc_log_buf , " ERR:%s: " fmt, __func__, ## args); \
    } \
    if(is_qxdm_log_en && uevent_tcvr_device) {\
        snprintf(buf_udev, TRX_MAX_DIAG_LOG_MSG_SIZE, "TRX_IPC_LOG_MSG_ERROR=%s: "\
                            fmt, __func__, ## args); \
        kobject_uevent_env(&uevent_tcvr_device->kobj, KOBJ_CHANGE, msg); \
    } \
} while (0)

#define TRX_QXDM_LOG_INFO(p, fmt, args...) \
do {\
  char buf[TRX_MAX_DIAG_LOG_MSG_SIZE]; \
  char *msg[] = {buf, NULL}; \
  snprintf(buf, TRX_MAX_DIAG_LOG_MSG_SIZE, "TRX_LOG_MSG_INFO="fmt, ## args); \
  kobject_uevent_env(&p->dev->kobj, KOBJ_CHANGE, msg); \
} while (0)

#define TRX_QXDM_LOG_ERROR(p, fmt, args...) \
do {\
  char buf[TRX_MAX_DIAG_LOG_MSG_SIZE]; \
  char *msg[] = {buf, NULL}; \
  snprintf(buf, TRX_MAX_DIAG_LOG_MSG_SIZE, "TRX_LOG_MSG_ERROR="fmt, ## args); \
  kobject_uevent_env(&p->dev->kobj, KOBJ_CHANGE, msg); \
} while (0)

#define TRX_QXDM_LOG_DEBUG(p, fmt, args...) \
do {\
  char buf[TRX_MAX_DIAG_LOG_MSG_SIZE]; \
  char *msg[] = {buf, NULL}; \
  snprintf(buf, TRX_MAX_DIAG_LOG_MSG_SIZE, "TRX_LOG_MSG_DEBUG="fmt, ## args); \
  kobject_uevent_env(&p->dev->kobj, KOBJ_CHANGE, msg); \
} while (0)

#endif
