/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */

#ifndef TRX_API_H
#define TRX_API_H

#include <linux/ethtool.h>

/* Macro to identify the breakout configuration is not
   supported case for separable transceiver of type
   QSFP-28, QSFP-56, or QSFP+. */
#define TRX_FAR_END_NOT_MANAGED (0xFF)

/* Enum to identify the PHY event */
typedef enum {
    /* ifconfig down */
    TRX_IFCONFIG_DOWN,
    /* ifconfig up */
    TRX_IFCONFIG_UP,
    /* Ethernet link down */
    TRX_ETH_LINK_DOWN,
    /* Ethernet link up */
    TRX_ETH_LINK_UP,
} trx_phy_event;

/* Enum to identify the lane speed */
typedef enum {
    TRX_LANE_SPEED_UNKNOWN = 0,
    /* Lane speed of 2.5GBPS */
    TRX_LANE_SPEED_2_5G = BIT(0),
    /* Lane speed of 10GBPS */
    TRX_LANE_SPEED_10G = BIT(1),
    /* Lane speed of 25GBPS */
    TRX_LANE_SPEED_25G = BIT(2),
    /* Lane speed of 50GBPS */
    TRX_LANE_SPEED_50G = BIT(3),
    /* Lane speed of 100GBPS */
    TRX_LANE_SPEED_100G = BIT(4),
}trx_lane_speed;

/* Enum to identify the TRX type */
typedef enum {
    TRX_UNKNOWN = 0,
    /* QSFP type not supported */
    TRX_UNSUPPORTED,
    /* SFP transceiver */
    TRX_SFP,
    /* QSFP+/QSFP28/QSFP56 */
    TRX_QSFP_PLS_QSFP28_QSFP56,
    /* QSFP-DD */
    TRX_QSFPDD,
}trx_type;

/* Enum to identify the TRX link length range */
typedef enum {
    TRX_LINK_UNKNOWN = 0,
    /* Short range */
    TRX_SR,
    /* Long range */
    TRX_LR,
    /* Extended range */
    TRX_ER,
    /* Ze best range */
    TRX_ZR,
    /* Copper cable DAC */
    TRX_CR,
    TRX_CLR,
    TRX_DR,
    TRX_BR,
    TRX_FR,
    TRX_VR,
} trx_link_length_range;

typedef enum {
    TRX_LOCAL_PLUGOUT = 0,
    TRX_TX_FAULT,
    TRX_RX_LOS,
    TRX_ERROR,
} trx_lane_down_reason_code_type;

/* The lane configuration */
typedef u8 trx_lane_cfg;

/* The breakout configuration */
typedef u8 trx_breakout_cfg;

/* Transceiver speed mask */
typedef u8 trx_speed_mask;

struct qsfp_info {
    /* TRX type(SFP/QSFP/QSFP-DD)*/
    trx_type trx_module_type;
    /* Lane supported speed mask */
    trx_speed_mask speed_mask;
    /* Near-end configuration */
    trx_lane_cfg trx_laneinfo;
    /* Far-end configuration */
    trx_breakout_cfg trx_bout_cfg;
    /* Link length range */
    trx_link_length_range trx_link_length_range;
};

struct trx_eth_event_t {
    trx_phy_event event;
    u32 *lane_phandle;
    u8 num_lanes;
    trx_lane_speed eth_cfg_speed;
};

/***************************************************************
*
* Function:       qsfp_trx_get_lane_down_reason_code
* Description:    API for determining the lane down reason.
* Inputs:         lane_phandle [in] : Lane phandle to get the Lane structure.
*                 reason [out]   : Lane down reason as per macros defined.
* Return value:   0      : Success
*                -EINVAL : Error
*
****************************************************************/
int qsfp_trx_get_lane_down_reason_code(u32 lane_phandle,
                                       trx_lane_down_reason_code_type* reason);

/***************************************************************
*
* Function:       qsfp_trx_get_lane_type
* Description:    API for determining the type of QSFP TRX lane (DAC, OPTIC,
*                 OR OTHER).
* Inputs:         lane_phandle [in] : Lane phandle to get the QSFP structure.
*                 link_info [out]   : TRX lane type as per macros defined in
*                                     ethtool.h.
* Return value:   0      : Success
*                -EINVAL : Error
*
****************************************************************/
int qsfp_trx_get_lane_type(u32 lane_phandle, u8* link_info);

/***************************************************************
*
* Function:       qsfp_trx_get_lane_speed
* Description:    API to get QSFP TRX lane supported speed.
* Inputs:         lane_phandle [in] : Lane phandle to get the QSFP structure.
*                 trx_speed_mask [out] : Supported TRX lane speeds of type
*                                        trx_speed_mask*.
* Return value:   0      : Success
*                -EINVAL : Error
*
****************************************************************/
int qsfp_trx_get_lane_speed(u32 lane_phandle, trx_speed_mask *speed_mask);

/***************************************************************
*
* Function:       qsfp_trx_get_type
* Description:    API to get QSFP TRX type (SFP/QSFP28/QSFP56/QSFP-DD).
* Inputs:         lane_phandle [in] : Lane phandle to get the QSFP structure.
*                 trx_type_info [out] : trx_type enum pointer to get TRX type.
*
* Return value:   0      : Success
*                -EINVAL : Error
*
****************************************************************/
int qsfp_trx_get_type(u32 lane_phandle, trx_type* trx_type_info);

/***************************************************************
*
* Function:       qsfp_trx_get_laneconfig
* Description:    API to get QSFP TRX hardware supported lane configuration.
* Inputs:         lane_phandle [in] : Lane phandle to get the QSFP structure.
*                 laneinfo [out] : Lane information is encoded in an 8-bit
*                                  value by setting the bit position value
*                                  to 1.
*
* Return value:   0      : Success
*                -EINVAL : Error
*
****************************************************************/
int qsfp_trx_get_laneconfig(u32 lane_phandle, trx_lane_cfg* laneinfo);

/***************************************************************
*
* Function:       qsfp_trx_get_breakoutconfig
* Description:    API to get QSFP TRX far-end breakout configuration.
* Inputs:         lane_phandle [in] : Lane phandle to get the QSFP structure.
*                 bout_config [out] : Breakout configuration of type
*                                     trx_breakout_cfg, value depends on
*                                     trx_type_info.
*
* Return value:   0     : Success
*                -EINVAL : Error
*
****************************************************************/
int qsfp_trx_get_breakoutconfig(u32 lane_phandle,
                                trx_breakout_cfg* bout_config);

/***************************************************************
*
* Function:       qsfp_trx_get_info
* Description:    API to get QSFP TRX properties (lane speed,trx type,
*                 lane configuration, and breakout configuration).
* Inputs:         lane_phandle [in] : Lane phandle to get the QSFP structure.
*                 trx_info [out] : Pointer of type struct qsfp_info.
*
* Return value:   0      : Success
*                -EINVAL : Error
*
****************************************************************/
int qsfp_trx_get_info(u32 lane_phandle, struct qsfp_info* trx_info);

/***************************************************************
*
* Function:       qsfp_trx_eth_event_notifier
* Description:    API to get notification for eth interface events.
* Inputs:         trx_eth_event_t structure pointer which has the input fields
*                 event [in]   : Type of eth interface event.
*                                TRX_IFCONFIG_DOWN : ifconfig Down
*                                TRX_IFCONFIG_UP   : ifconfig Up
*                                TRX_ETH_LINK_DOWN : Ethernet link down
*                                TRX_ETH_LINK_UP   : Ethernet link up
*                 lane_phandle [in] : Array of Lane phandles of size num_lanes
*                 num_lanes [in] : Number of lanes having the event or
*                                  size of lane_phandle  
*                 eth_cfg_speed [in] : ETH interface configured speed of type
*                                      trx_lane_speed.
*
* Return value:   0      : Success
*                -EINVAL : Error
*
****************************************************************/
int qsfp_trx_eth_event_notifier(struct trx_eth_event_t* eth_notifier);

/***************************************************************
*
* Function:       qsfp_trx_get_link_length_range
* Description:    API to get QSFP TRX link length range information.
* Inputs:         lane_phandle [in] : Lane phandle to get the QSFP structure.
*                 link_length_range [out] : Supported transceiver link length
*                                           range information.
*
* Return value:   0                : Success
*                -EINVAL | -EAGAIN : Error
*
****************************************************************/
int qsfp_trx_get_link_length_range(u32 lane_phandle,
                                   trx_link_length_range* link_length_range);

/***************************************************************
*
* Function:       qsfp_trx_get_module_info
* Description:    API to get QSFP TRX module EEPROM type and length
*                 information.
* Inputs:         lane_phandle [in] : Lane phandle to get the QSFP structure.
*                 modinfo [out] : Structure to get module EEPROM type and
*                                 length information.
*
* Return value:   0                : Success
*                -EINVAL | -ENODEV : Error
*
****************************************************************/
int qsfp_trx_get_module_info(u32 lane_phandle,
                                    struct ethtool_modinfo *modinfo );

/***************************************************************
*
* Function:       qsfp_trx_get_module_eeprom
* Description:    API to get TRX EEPROM data.
* Inputs:         lane_phandle [in] : Lane phandle to get the QSFP structure.
*                 ee [in] : Structure to pass offset information to TRX driver.
*                 data [out] : Buffer to get EEPROM data from TRX driver.
*
* Return value:   0                : Success
*                -EINVAL | -ENODEV : Error
*
****************************************************************/
int qsfp_trx_get_module_eeprom(u32 lane_phandle,struct ethtool_eeprom *ee,
                                      u8 *data);

#endif
