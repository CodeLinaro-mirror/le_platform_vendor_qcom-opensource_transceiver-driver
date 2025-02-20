// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Code is derived from http://git.armlinux.org.uk/cgit/linux-arm.git/
 * tree/drivers/net/phy/qsfp.c?h=cex7 &
 * http://git.armlinux.org.uk/cgit/linux-arm.git/tree/drivers/
 * net/phy/sfp.c?h=cex7
 *
 * Code is derived from http://git.armlinux.org.uk/cgit/linux-arm.git/tree/
 * drivers/net/phy/sfp-bus.c
 *
 */
#include "fpc.h"
#include "lane.h"
#include "qsfp.h"
#include "transceiver_debugfs.h"
#include "fpc_led.h"
#include "trx_sysfs.h"

/* QSFP Device tree example
 *
 * qsfp_0: qsfp_0 {
 *    compatible = "sff,qsfp";
 *    fpc = <&fpc402_0>;
 *    port-num = <0>;
 *    i2c-address-device0 = <0x20>;
 *    i2c-address-device1 = <0x22>;
 *    maximum-power-milliwatt = <3500>;
 * };
 *
 */

static void qsfp_sm_event(struct qsfp *qsfp, u8 event);
extern struct list_head dual_tcvr_list;

static void* phandle_to_drvdata(u32 phandle)
{
    struct device_node *node;
    struct platform_device *pdev;

    node = of_find_node_by_phandle(phandle);
    if (!node) {
        TRX_LOG_ERR_NODEV("Unable to find node");
        return NULL;
    }

    pdev = of_find_device_by_node(node);
    if (!pdev) {
        TRX_LOG_ERR_NODEV("Unable to get pdev");
        return NULL;
    }

    return platform_get_drvdata(pdev);
}

/*
 * Function to get the qsfp structure from the lane phandle
 */
static struct qsfp* get_qsfp(u32 lane_phandle)
{
    struct lane *lane;

    lane = phandle_to_drvdata(lane_phandle);
    if (!lane) {
        return NULL;
    }

    /* Give qsfp refernce only if lane is present */
    if (lane->status.present) {
        return lane->qsfp;
    } else {
        return NULL;
    }
}

int qsfp_get_link_type(struct qsfp *qsfp, u8* link_info)
{
    u8 connector;
    u8 trx_type;

    if (!qsfp) {
        /* There is chance that Lane/QSFP/FPC probe not yet
         * successfully completed
         */
        TRX_LOG_ERR_NODEV("Unable to get QSFP handler");
        return -EAGAIN;
    }

    if (qsfp->spec_ops) {
        u8 *spec_id = (u8*)&qsfp->id;
        if ((*spec_id == SFF8024_ID_SFP) ||
            (*spec_id == SFF8024_ID_SFF_8472)) {
            if (qsfp->id.sff8472.base.sfp_ct_passive) {
                *link_info = PORT_OTHER;
            } else {
                *link_info = PORT_FIBRE;
            }

            return 0;

        } else {
            connector = qsfp->spec_ops->get_connector_type(qsfp);
            if ((*spec_id == SFF8024_ID_QSFP28_8636) ||
                (*spec_id == SFF8024_ID_QSFP_8436_8636))
            {
                if(connector == SFF8024_CONNECTOR_NOSEPARATE)
                {
                    if(qsfp->id.sff8636.base.ecom_extended == 0x1)
                    {
                        trx_type = qsfp->id.sff8636.ext.link_codes;
                        /* Check for AOC cable types */
                        if((trx_type == 0x01) ||
                           (trx_type == 0x18) ||
                           (trx_type == 0x31) ||
                           (trx_type == 0x33)) {
                               *link_info = PORT_FIBRE;
                               return 0;
                        }
                    }
                }
            }
        }
    } else {
        TRX_LOG_WARN(qsfp, "Spec ops not yet initialised");
        return -EINVAL;
    }

    switch (connector) {
    case SFF8024_CONNECTOR_SC:
    case SFF8024_CONNECTOR_FIBERJACK:
    case SFF8024_CONNECTOR_LC:
    case SFF8024_CONNECTOR_MT_RJ:
    case SFF8024_CONNECTOR_MU:
    case SFF8024_CONNECTOR_OPTICAL_PIGTAIL:
    case SFF8024_CONNECTOR_MPO_1X12:
    case SFF8024_CONNECTOR_MPO_2X16:
    case SFF8024_CONNECTOR_MPO_2X12:
    case SFF8024_CONNECTOR_MPO_1X16:
    case SFF8024_CONNECTOR_CS_OPTICAL:
    case SFF8024_CONNECTOR_SN_OPTICAL:
        *link_info = PORT_FIBRE;
        break;
    case SFF8024_CONNECTOR_RJ45:
        *link_info = PORT_TP;
        break;
    case SFF8024_CONNECTOR_COPPER_PIGTAIL:
        *link_info = PORT_DA;
        break;
    case SFF8024_CONNECTOR_UNSPEC:
    case SFF8024_CONNECTOR_SG:
    case SFF8024_CONNECTOR_HSSDC_II:
    case SFF8024_CONNECTOR_NOSEPARATE:
    case SFF8024_CONNECTOR_MXC_2X16:
    case SFF8024_CONNECTOR_FC1_COPPER:
    case SFF8024_CONNECTOR_FC2_COPPER:
        *link_info = PORT_OTHER;
        break;
    case SFF8024_CONNECTOR_BNC_TNC:
        *link_info = PORT_BNC;
        break;
    default:
        TRX_LOG_WARN(qsfp, "Unknown connector id 0x%X", connector);
        *link_info = PORT_OTHER;
        break;
    }

    /* Assign link type as FIBER for active adapter */
    if(qsfp->is_adapter == 1)
    {
        *link_info = PORT_FIBRE;
    }

    return 0;
}

int qsfp_trx_get_lane_down_reason_code(u32 lane_phandle,
                                       trx_lane_down_reason_code_type *reason)
{
    struct lane *lane;

    if(reason == NULL)
    {
        TRX_LOG_ERR_NODEV("Invalid input argument");
        return -EINVAL;
    }

    lane = phandle_to_drvdata(lane_phandle);
    if (!lane) {
        /* There is chance that Lane/QSFP/FPC probe not yet
         * successfully completed
         */
        TRX_LOG_ERR_NODEV("Unable to get Lane handler");
        return -EAGAIN;
    }

    if (lane->sm_mod_state == QSFP_MOD_EMPTY) {
        *reason = TRX_LOCAL_PLUGOUT;
    } else if ((lane->sm_mod_state == QSFP_MOD_ERROR_I2C) ||
               (lane->sm_mod_state == QSFP_MOD_ERROR_HPOWER) ||
               (lane->sm_mod_state == QSFP_MOD_ERROR_TX_ENABLE_FAIL)) {
        *reason = TRX_ERROR;
    } else if (lane->status.tx_fault) {
        *reason = TRX_TX_FAULT;
    } else if (lane->status.rx_los) {
        *reason = TRX_RX_LOS;
    } else {
        TRX_LOG_ERR(lane, "Not able find reason for link down. "
                           "Module state %u", lane->sm_mod_state);
        return -EINVAL;
    }

    TRX_LOG_INFO(lane, "reason 0x%X", *reason);

    return 0;
}
EXPORT_SYMBOL_GPL(qsfp_trx_get_lane_down_reason_code);

/*
 * API to determine whether link type is optics or copper
 */
int qsfp_eth_get_link_type(u32 lane_phandle, u8* link_info)
{
    struct qsfp *qsfp;

    if(link_info == NULL)
    {
        TRX_LOG_ERR_NODEV("Invalid input argument");
        return -EINVAL;
    }

    qsfp = get_qsfp(lane_phandle);

    return qsfp_get_link_type(qsfp, link_info);
}
EXPORT_SYMBOL_GPL(qsfp_eth_get_link_type);

/*
 * Wrapper API function to call qsfp_eth_get_link_type
 */
int qsfp_trx_get_lane_type(u32 qsfp_phandle, u8* lane_info)
{
    return qsfp_eth_get_link_type(qsfp_phandle, lane_info);
}
EXPORT_SYMBOL_GPL(qsfp_trx_get_lane_type);

/*
 * API to determine lane supported speed
 */
int qsfp_trx_get_lane_speed(u32 lane_phandle, trx_speed_mask *speed_mask)
{
    struct qsfp *qsfp;
    int ret = -EINVAL;

    if(speed_mask == NULL)
    {
        TRX_LOG_ERR_NODEV("Invalid input argument");
        return -EINVAL;
    }

    qsfp = get_qsfp(lane_phandle);
    if (!qsfp) {
        /* There is chance that Lane/QSFP/FPC probe not yet
         * successfully completed
         */
        TRX_LOG_ERR_NODEV("Unable to get QSFP handler");
        return -EAGAIN;
    }

    if (qsfp->spec_ops) {
        ret = qsfp->spec_ops->get_lane_speed(qsfp, speed_mask);
    } else {
        TRX_LOG_WARN(qsfp, "Spec ops not yet initialised");
    }

    return ret;
}
EXPORT_SYMBOL_GPL(qsfp_trx_get_lane_speed);

/*
 * API to determine transceiver type
 */
int qsfp_trx_get_type(u32 lane_phandle, trx_type* type)
{
    struct qsfp *qsfp;
    u8 tansceivertype;

    if(type == NULL)
    {
        TRX_LOG_ERR_NODEV("Invalid input argument");
        return -EINVAL;
    }

    qsfp = get_qsfp(lane_phandle);
    if (!qsfp) {
        /* There is chance that Lane/QSFP/FPC probe not yet
         * successfully completed
         */
        TRX_LOG_ERR_NODEV("Unable to get QSFP handler");
        return -EAGAIN;
    }

    if (qsfp->spec_ops) {
        tansceivertype = qsfp->spec_ops->get_transceiver_type(qsfp);
    } else {
        TRX_LOG_WARN(qsfp, "Spec ops not yet initialised");
        return -EINVAL;
    }

    switch (tansceivertype) {
    case SFF8024_ID_UNK:
        *type = TRX_UNKNOWN;
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        *type = TRX_QSFP_PLS_QSFP28_QSFP56;
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        *type = TRX_QSFPDD;
        break;
    case SFF8024_ID_SFP:
        *type = TRX_SFP;
        break;
    default:
        *type = TRX_UNSUPPORTED;
        break;
    }

    return 0;
}
EXPORT_SYMBOL_GPL(qsfp_trx_get_type);

void qsfp_start_poll(struct qsfp *qsfp, unsigned long delay)
{
    qsfp->need_poll = true;
    mod_delayed_work(system_wq, &qsfp->poll, msecs_to_jiffies(delay));
    TRX_LOG_INFO(qsfp, "Polling started with delay: %lu", delay);
}

static const char * const eth_event_to_str[] = {
    [TRX_IFCONFIG_DOWN] = "IFCONFIG_DOWN",
    [TRX_IFCONFIG_UP] = "IFCONFIG_UP",
    [TRX_ETH_LINK_DOWN] = "ETH_LINK_DOWN",
    [TRX_ETH_LINK_UP] = "ETH_LINK_UP",
};

static int qsfp_configure_speed(struct qsfp* qsfp, trx_speed_mask lane_cfg_speed)
{
    int ret;
    u8 retry = QSFP_I2C_FAIL_RETRY;

    if(!qsfp)
    {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    if (!qsfp->spec_ops) {
        TRX_LOG_ERR(qsfp, "spec_ops is NULL.");
        return 0;
    }

    if (!qsfp->support.rate_select)
    {
        TRX_LOG_INFO(qsfp, "Rate select was not supported ");
        return 0;
    }

    if(lane_cfg_speed == TRX_LANE_SPEED_UNKNOWN)
    {
        TRX_LOG_ERR(qsfp, "Invalid Lane config speed mask: %02X", lane_cfg_speed);
        return -EINVAL;
    }

    if(lane_cfg_speed == qsfp->lane_max_speed)
    {
        while (retry--) {
            ret = qsfp->spec_ops->set_rate_select(qsfp, true);
            if (ret == 0) {
                break;
            }
        }
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Max Rate select failed, ret %d", ret);
            return ret;
        }
    }
    else if(lane_cfg_speed == qsfp->lane_min_speed)
    {
        while (retry--) {
            ret = qsfp->spec_ops->set_rate_select(qsfp, false);
            if (ret == 0) {
                break;
            }
        }
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Min Rate select failed, ret %d", ret);
            return ret;
        }
    }
    else
    {
        TRX_LOG_ERR(qsfp, "Invalid Lane config speed mask: %02X max supported"
                          " speed: %02X Min supported speed: %02x",
                          lane_cfg_speed, qsfp->lane_max_speed, qsfp->lane_min_speed);
        return -EINVAL;
    }
    return 0;
}

int qsfp_trx_eth_event_notifier(struct trx_eth_event_t* eth_notifier)
{
    struct lane *lane = NULL;
    struct qsfp *qsfp;
    u8 lanes = 0;
    u8 i;
    int ret;
    bool lane_init_err_flag = 0;

    if(eth_notifier == NULL)
    {
        TRX_LOG_ERR_NODEV("eth notifier is NULL");
        return -EINVAL;
    }

    if (!((eth_notifier->event >= TRX_IFCONFIG_DOWN) && (eth_notifier->event <= TRX_ETH_LINK_UP))) {
        TRX_LOG_ERR_NODEV("Invalid Event %d", eth_notifier->event);
        return -EINVAL;
    }
    if (!eth_notifier->num_lanes) {
        TRX_LOG_INFO_NODEV("Number of lanes zero for Event %s",
                           eth_event_to_str[eth_notifier->event]);
        return -EINVAL;
    }

    if (eth_notifier->lane_phandle == NULL) {
        TRX_LOG_ERR_NODEV("lane phandle is NULL. Event %s lanes %u",
                          eth_event_to_str[eth_notifier->event], eth_notifier->num_lanes);
        return -EINVAL;
    }

    lane = phandle_to_drvdata(eth_notifier->lane_phandle[0]);
    if (!lane) {
        TRX_LOG_ERR_NODEV("Failed to get lane pointer for first lane_phandle "
        "0x%x. Event %s lanes %u", eth_notifier->lane_phandle[0], eth_event_to_str[eth_notifier->event],
        eth_notifier->num_lanes);
        return -EINVAL;
    }

    qsfp = lane->qsfp;
    if (!qsfp) {
        TRX_LOG_ERR(lane, "qsfp is NULL. Event %s lanes %u",
                          eth_event_to_str[eth_notifier->event], eth_notifier->num_lanes);
        return -EINVAL;
    }

    mutex_lock(&qsfp->sm_mutex);

    TRX_LOG_INFO(qsfp, "Event %s lanes %u speed: %02x", eth_event_to_str[eth_notifier->event],
                                         eth_notifier->num_lanes,eth_notifier->eth_cfg_speed);

    if(eth_notifier->event == TRX_IFCONFIG_UP)
    {
        ret = qsfp_configure_speed(qsfp, eth_notifier->eth_cfg_speed);
        if(ret < 0)
        {
            mutex_unlock(&qsfp->sm_mutex);
            TRX_LOG_ERR(qsfp, "Configure speed fail, Event %s lanes %u speed %02x",
                          eth_event_to_str[eth_notifier->event], eth_notifier->num_lanes,
                          eth_notifier->eth_cfg_speed);
            return -EINVAL;
        }
    }

    for (i = 0 ; i < eth_notifier->num_lanes ; i++) {

        lane = phandle_to_drvdata(eth_notifier->lane_phandle[i]);
        if (!lane) {
            TRX_LOG_ERR(qsfp, "Failed to get lane pointer for "
                              "lane_phandle[%u] %u", i, eth_notifier->lane_phandle[i]);
            continue;
        }

        switch (eth_notifier->event) {
        case TRX_IFCONFIG_DOWN:
            if (lane->sm_dev_state == QSFP_DEV_UP) {
                lanes |= (1 << lane->lane_num);
                lane_stop(lane);
                TRX_QXDM_LOG_DEBUG(qsfp, "Port-%u: Lane-%u: Ifconfig Down",
                qsfp->port_num, lane->lane_num);
            } else {
                TRX_LOG_INFO(lane, "Ignoring repeated ifconfig down event");
            }
            break;

        case TRX_IFCONFIG_UP:
            if (lane->sm_dev_state == QSFP_DEV_UP) {
                TRX_LOG_INFO(lane, "Ignoring repeated ifconfig up event");
                TRX_QXDM_LOG_DEBUG(qsfp, "Port-%u: Lane-%u: Ifconfig Up",
                qsfp->port_num, lane->lane_num);
            } else {
                lanes |= (1 << lane->lane_num);
                lane_start(lane);
            }
            break;

        case TRX_ETH_LINK_DOWN:
            if (lane->status.eth_linkup) {
                lanes |= (1 << lane->lane_num);
                lane->status.eth_linkup = 0;
                lane_sm_event(lane, QSFP_E_ETH_DOWN);
                TRX_QXDM_LOG_DEBUG(qsfp, "Port-%u: Lane-%u: Link Down",
                qsfp->port_num, lane->lane_num);
            } else {
                TRX_LOG_INFO(lane, "Ignoring repeated eth down event");
            }
            break;

        case TRX_ETH_LINK_UP:
            if (lane->status.eth_linkup) {
                TRX_LOG_INFO(lane, "Ignoring repeated eth up event");
            } else {
                lanes |= (1 << lane->lane_num);
                /* Receiving the Linkup event from MTIP before sending the
                  sfp_module_insert event to MTIP. Avoid updating the
                  link state to 'link up' until the module initialization
                  is complete.*/
                if(lane->sm_mod_state > QSFP_MOD_ERROR_I2C) {
                    lane->status.eth_linkup = 1;
                    lane_sm_event(lane, QSFP_E_ETH_UP);
                    TRX_QXDM_LOG_DEBUG(qsfp, "Port-%u: Lane-%u: Link Up",
                    qsfp->port_num, lane->lane_num);
                } else {
                    lane_init_err_flag = 1;
                    TRX_LOG_INFO(lane, "Ignoring eth up event before "
                                              "lane initialization.");
                }
            }
            break;
        }
    }

    if (lanes == 0) {
        mutex_unlock(&qsfp->sm_mutex);
        TRX_LOG_INFO(qsfp, "Ignoring repeated %s event", eth_event_to_str[eth_notifier->event]);
        return 0;
    }

    if (!qsfp->spec_ops) {
        mutex_unlock(&qsfp->sm_mutex);
        TRX_LOG_ERR(qsfp, "spec_ops is NULL. Event %s lanes %u",
            eth_event_to_str[eth_notifier->event], eth_notifier->num_lanes);
        return 0;
    }

    if (eth_notifier->event == TRX_IFCONFIG_UP) {
    /* Disable irq as eth link up started */
        if (qsfp->spec_ops->disable_enable_lane_irq &&
            qsfp_atleast_one_flag_supported(qsfp)) {
            qsfp->spec_ops->disable_enable_lane_irq(qsfp, lanes, false);
            /* Polling needed because of irq disabled for few lanes */
            qsfp_start_poll(qsfp, 0);
        }

    } else if (eth_notifier->event == TRX_ETH_LINK_DOWN) {
        bool qsfp_eth_linkup = false;

        if(qsfp_atleast_one_flag_supported(qsfp)) {
            /* Delete the timer if it is still active */
            del_timer(&qsfp->qsfp_flt_lnkd_timer);

            /* Configure the timer to expire after 5 seconds (5 * HZ jiffies)
             * only for optical modules*/
             mod_timer(&qsfp->qsfp_flt_lnkd_timer, jiffies + 5 * HZ);
        }

        /* Disable irq as eth link up in progress */
        if (qsfp->spec_ops->disable_enable_lane_irq &&
            qsfp_atleast_one_flag_supported(qsfp)) {
            qsfp->spec_ops->disable_enable_lane_irq(qsfp, lanes, false);
            /* Polling needed because of irq disabled for few lanes */
            qsfp_start_poll(qsfp, 0);
        }

        for (i = 0 ; i < qsfp->num_lanes ; i++) {
            lane = qsfp->lane[i];
            if (lane && lane->status.eth_linkup &&
               (lane->sm_dev_state == QSFP_DEV_UP)) {
                qsfp_eth_linkup = true;
                break;
            }
        }

        if (qsfp_eth_linkup) {
            qsfp->status.eth_linkup = 1;
        } else {
            qsfp->status.eth_linkup = 0;
            qsfp_sm_event(qsfp, QSFP_E_ETH_DOWN);
        }

    } else if ((eth_notifier->event == TRX_ETH_LINK_UP) &&
    (lane_init_err_flag == 0)) {
    /* Enable irq as eth link up successful */
        if (qsfp_atleast_one_flag_supported(qsfp)) {
            clear_qsfp_reported_faults(qsfp);
            /* Poll to update flags immediately from HW */
            qsfp_start_poll(qsfp, 0);

            if (qsfp->spec_ops->disable_enable_lane_irq) {
                qsfp->spec_ops->disable_enable_lane_irq(qsfp, lanes, true);
            }
        }

        if (!qsfp->status.eth_linkup) {
            qsfp->status.eth_linkup = 1;
            qsfp_sm_event(qsfp, QSFP_E_ETH_UP);
        }
    }

    mutex_unlock(&qsfp->sm_mutex);

    return 0;
}
EXPORT_SYMBOL_GPL(qsfp_trx_eth_event_notifier);

/*
 * API to determine transceiver link length range
 */
int qsfp_trx_get_link_length_range(u32 lane_phandle,
                                   trx_link_length_range* link_length_range)
{
    struct qsfp *qsfp;

    if(link_length_range == NULL)
    {
        TRX_LOG_ERR_NODEV("Invalid input argument");
        return -EINVAL;
    }

    qsfp = get_qsfp(lane_phandle);
    if (!qsfp) {
        /* There is chance that Lane/QSFP/FPC probe not yet
         * successfully completed
         */
        TRX_LOG_ERR_NODEV("Unable to get QSFP handler");
        return -EAGAIN;
    }

    if (qsfp->spec_ops) {
        *link_length_range = qsfp->spec_ops->get_link_length_range(qsfp);
        return 0;
    } else {
        TRX_LOG_WARN(qsfp, "Spec ops not yet initialised");
        return -EINVAL;
    }
}
EXPORT_SYMBOL_GPL(qsfp_trx_get_link_length_range);

/* Link code parsed depending on SFF-8024 table */
trx_link_length_range qsfp_link_code_to_link_length_range(u8 link_code)
{
    switch (link_code) {
    case 0x02:
    case 0x05:
    case 0x11:
    case 0x28:
    case 0x29:
    case 0x41:
        return TRX_SR;

    case 0x03:
    case 0x27:
    case 0x2B:
    case 0x2C:
    case 0x2F:
    case 0x45:
    case 0x46:
    case 0x49:
    case 0x4B:
        return TRX_LR;

    case 0x04:
    case 0x10:
    case 0x2D:
    case 0x2E:
    case 0x34:
    case 0x35:
    case 0x4A:
        return TRX_ER;

    case 0x4C:
        return TRX_ZR;

    case 0x0B:
    case 0x0C:
    case 0x0D:
    case 0x3F:
    case 0x40:
        return TRX_CR;

    case 0x17:
        return TRX_CLR;

    case 0x25:
    case 0x47:
        return TRX_DR;

    case 0x37:
    case 0x38:
    case 0x39:
        return TRX_BR;

    case 0x26:
    case 0x2A:
    case 0x43:
    case 0x48:
        return TRX_FR;

    case 0x3A:
    case 0x36:
        return TRX_VR;

    default:
        return TRX_LINK_UNKNOWN;
    }
}

/* MMF code parsed depending on SFF-8024 table */
trx_link_length_range qsfp_mmf_code_to_link_length_range(u8 mmf_code)
{
    switch (mmf_code) {
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x07:
    case 0x08:
    case 0x09:
    case 0x0C:
    case 0x0D:
    case 0x0E:
    case 0x1B:
    case 0x0F:
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x1A:
        return TRX_SR;

    case 0x1D:
    case 0x1E:
    case 0x1F:
        return TRX_VR;

    default:
        return TRX_LINK_UNKNOWN;
    }
}

/* SMF code parsed depending on SFF-8024 table */
trx_link_length_range qsfp_smf_code_to_link_length_range(u8 smf_code)
{
    switch (smf_code) {
    case 0x38:
    case 0x3A:
        return TRX_SR;

    case 0x04:
    case 0x07:
    case 0x09:
    case 0x0C:
    case 0x0D:
    case 0x16:
    case 0x4A:
    case 0x19:
    case 0x1B:
    case 0x43:
    case 0x1E:
    case 0x39:
    case 0x3B:
    case 0x3C:
    case 0x3D:
        return TRX_LR;

    case 0x05:
    case 0x08:
    case 0x40:
    case 0x0E:
    case 0x4B:
    case 0x4C:
    case 0x41:
    case 0x42:
        return TRX_ER;

    case 0x06:
    case 0x44:
    case 0x4D:
    case 0x3E:
    case 0x3F:
        return TRX_ZR;

    case 0x14:
    case 0x17:
    case 0x1C:
        return TRX_DR;

    case 0x4E:
    case 0x4F:
    case 0x50:
        return TRX_BR;

    case 0x0A:
    case 0x0B:
    case 0x15:
    case 0x18:
    case 0x1A:
    case 0x1D:
        return TRX_FR;

    default:
        return TRX_LINK_UNKNOWN;
    }
}

/*
 * API to determine transceiver near end properties
 */
int qsfp_trx_get_laneconfig(u32 lane_phandle, trx_lane_cfg* laneinfo)
{
    struct qsfp *qsfp;

    if(laneinfo == NULL)
    {
        TRX_LOG_ERR_NODEV("Invalid input argument");
        return -EINVAL;
    }

    qsfp = get_qsfp(lane_phandle);
    if (!qsfp) {
        /* There is chance that Lane/QSFP/FPC probe not yet
         * successfully completed
         */
        TRX_LOG_ERR_NODEV("Unable to get QSFP handler");
        return -EAGAIN;
    }

    if (qsfp->sm_mod_state >= QSFP_MOD_WAITHPOWER) {
        *laneinfo = qsfp->lane_presence;
        return 0;
    } else {
        TRX_LOG_WARN(qsfp, "Lane presence not yet initialised");
        return -EINVAL;
    }
}
EXPORT_SYMBOL_GPL(qsfp_trx_get_laneconfig);

/*
 * API to determine transceiver far end properties
 */
int qsfp_trx_get_breakoutconfig(u32 lane_phandle,
                                trx_breakout_cfg* bout_config)
{
    struct qsfp *qsfp;
    int ret = -EINVAL;

    if(bout_config == NULL)
    {
        TRX_LOG_ERR_NODEV("Invalid input argument");
        return -EINVAL;
    }

    qsfp = get_qsfp(lane_phandle);
    if (!qsfp) {
        /* There is chance that Lane/QSFP/FPC probe not yet
         * successfully completed
         */
        TRX_LOG_ERR_NODEV("Unable to get QSFP handler");
        return -EAGAIN;
    }

    if (qsfp->spec_ops) {
        ret = qsfp->spec_ops->get_breakout_config(qsfp, bout_config);
    } else {
        TRX_LOG_WARN(qsfp, "Spec ops not yet initialised");
    }

    return ret;
}
EXPORT_SYMBOL_GPL(qsfp_trx_get_breakoutconfig);

/*
 * API to determine transceiver information (lane speed, type, and
 * lane configuration, breakout configuration )
 */
int qsfp_trx_get_info(u32 lane_phandle, struct qsfp_info* trx_info)
{
    int ret = -EINVAL;

    /* Local variables to get data */
    trx_speed_mask speed_mask_t = 0;
    trx_type trx_type_t = 0;
    trx_lane_cfg trx_laneinfo_t = 0;
    trx_breakout_cfg trx_bout_config_t = 0;
    trx_link_length_range trx_link_length_range_t = 0;

    ret = qsfp_trx_get_lane_speed(lane_phandle, &speed_mask_t);
    if (ret == 0) {
        trx_info->speed_mask = speed_mask_t;
    } else {
        return ret;
    }

    ret = qsfp_trx_get_type(lane_phandle, &trx_type_t);
    if (ret == 0) {
        trx_info->trx_module_type = trx_type_t;
    } else {
        return ret;
    }

    ret = qsfp_trx_get_link_length_range(lane_phandle, &trx_link_length_range_t);
    if (ret == 0) {
        trx_info->trx_link_length_range = trx_link_length_range_t;
    } else {
        return ret;
    }

    ret = qsfp_trx_get_laneconfig(lane_phandle, &trx_laneinfo_t);
    if (ret == 0) {
        trx_info->trx_laneinfo = trx_laneinfo_t;
    } else {
        return ret;
    }

    ret = qsfp_trx_get_breakoutconfig(lane_phandle, &trx_bout_config_t);
    if (ret == 0) {
        trx_info->trx_bout_cfg = trx_bout_config_t;
    }

    return ret;
}
EXPORT_SYMBOL_GPL(qsfp_trx_get_info);

const char * const link_length_range_to_str[] = {
    [TRX_SR] = "SR",
    [TRX_LR] = "LR",
    [TRX_ER] = "ER",
    [TRX_ZR] = "ZR",
    [TRX_CR] = "CR",
    [TRX_CLR] = "CLR",
    [TRX_DR] = "DR",
    [TRX_BR] = "BR",
    [TRX_FR] = "FR",
    [TRX_VR] = "VR",
};

const char * const link_type_to_str[] = {
    [PORT_OTHER] = "DAC",
    [PORT_DA] = "DAC",
    [PORT_FIBRE] = "FIBER",
    [PORT_TP] = "TP",
    [PORT_BNC] = "BNC",
};

const char * const reasoncode_to_str[] = {
    [TRX_LOCAL_PLUGOUT] = "LOCAL_PLUGOUT",
    [TRX_TX_FAULT] = "TX_FAULT",
    [TRX_RX_LOS] = "RX_LOS",
    [TRX_ERROR] = "ERROR",
};

const char * const trxtype_to_str[] = {
    [TRX_UNKNOWN] = "UNKNOWN",
    [TRX_UNSUPPORTED] = "UNSUPPORTED",
    [TRX_SFP] = "SFP",
    [TRX_QSFP_PLS_QSFP28_QSFP56] = "QSFP_PLS_QSFP28_QSFP56",
    [TRX_QSFPDD] = "QSFPDD",
};

static const char  * const mod_state_strings[] = {
    [QSFP_MOD_EMPTY] = "Empty",
    [QSFP_MOD_ERROR_I2C] = "I2C_ERROR",
    [QSFP_MOD_ERROR_HPOWER] = "High_Power_ERROR",
    [QSFP_MOD_ERROR_TX_ENABLE_FAIL] = "TX_Enable_ERROR",
    [QSFP_MOD_REJECT_SPEC] = "Reject_Spec",
    [QSFP_MOD_REJECT_PWR] = "Reject_Power",
    [QSFP_MOD_PROBE] = "Probe",
    [QSFP_MOD_WAITHPOWER] = "WaitHighPower",
    [QSFP_MOD_WAITDEV] = "WaitDev",
    [QSFP_MOD_PRESENT] = "Present",
};

const char *mod_state_to_str(u8 mod_state)
{
    if (mod_state >= ARRAY_SIZE(mod_state_strings)) {
        return "Unknown module state";
    }

    return mod_state_strings[mod_state];
}

static const char * const dev_state_strings[] = {
    [QSFP_DEV_DETACHED] = "Detached",
    [QSFP_DEV_DOWN] = "Down",
    [QSFP_DEV_UP] = "Up",
};

const char *dev_state_to_str(u8 dev_state)
{
    if (dev_state >= ARRAY_SIZE(dev_state_strings)) {
        return "Unknown device state";
    }

    return dev_state_strings[dev_state];
}

static const char * const event_strings[] = {
    [QSFP_E_INSERT] = "Insert",
    [QSFP_E_REMOVE] = "Remove",
    [QSFP_E_DEV_ATTACH] = "Dev_attach",
    [QSFP_E_DEV_DETACH] = "Dev_detach",
    [QSFP_E_LANE_DOWN] = "Lane_Down",
    [QSFP_E_DEV_DOWN] = "Dev_down",
    [QSFP_E_DEV_UP] = "Dev_up",
    [QSFP_E_ETH_UP] = "Eth_up",
    [QSFP_E_ETH_DOWN] = "Eth_down",
    [QSFP_E_TX_FAULT] = "TX_Fault",
    [QSFP_E_TX_FAULT_RECOVERY] = "TX_Fault_Recovery",
    [QSFP_E_RX_LOS] = "RX_LOS",
    [QSFP_E_RX_LOS_RECOVERY] = "RX_LOS_Recovery",
    [QSFP_E_REVISIT] = "Revisit",
};

const char *event_to_str(u8 event)
{
    if (event >= ARRAY_SIZE(event_strings)) {
        return "Unknown event";
    }

    return event_strings[event];
}

static const char * const sm_state_strings[] = {
    [QSFP_S_DOWN] = "Down",
    [QSFP_S_RX_LOS] = "RX_LOS",
    [QSFP_S_TX_FAULT] = "TX_Fault",
    [QSFP_S_LINK_UP] = "Link_Up",
};

const char *link_state_to_str(u8 sm_link_state)
{
    if (sm_link_state >= ARRAY_SIZE(sm_state_strings)) {
        return "Unknown state";
    }

    return sm_state_strings[sm_link_state];
}

/*
 * Reads from QSFP memory map using i2c transaction by taking into account
 * page number as well.
 */
static int qsfp_i2c_read(struct qsfp *qsfp, u8 device, u8 page,
                         u8 dev_addr, void *buf, size_t len)
{
    struct i2c_msg msgs[2];
    size_t block_size = qsfp->i2c_block_size;
    size_t this_len;
    u8 i2c_addr;
    int ret;

    i2c_addr = device ? qsfp->i2c_address_dev1 : qsfp->i2c_address_dev0;

    if ((dev_addr + len - 1) > QSFP_PAGE_OFFSET && !qsfp->module_flat_mem) {
        u8 page_buf[2];

        page_buf[0] = QSFP_PAGE_OFFSET;
        page_buf[1] = page;

        msgs[0].addr = i2c_addr;
        msgs[0].flags = 0;   // write
        msgs[0].len = sizeof(page_buf);
        msgs[0].buf = page_buf;

        ret = __i2c_transfer(qsfp->i2c, msgs, 1);
        if (ret < 0) {
            return ret;
        }
    }

    msgs[0].addr = i2c_addr;
    msgs[0].flags = 0;     // write
    msgs[0].len = 1;
    msgs[0].buf = &dev_addr;

    msgs[1].addr = i2c_addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = len;
    msgs[1].buf = buf;

    while (len) {
        this_len = len;
        if (this_len > block_size) {
            this_len = block_size;
        }
        msgs[1].len = this_len;

        ret = __i2c_transfer(qsfp->i2c, msgs, ARRAY_SIZE(msgs));
        if (ret < 0) {
            return ret;
        }

        if (ret != ARRAY_SIZE(msgs)) {
            return -EIO;
        }

        msgs[1].buf += this_len;
        dev_addr += this_len;
        len -= this_len;
    }

    return 0;
}

/*
 * Writes into QSFP memory map using i2c transaction by taking into account
 * page number as well.
 */
static int qsfp_i2c_write(struct qsfp *qsfp, u8 device, u8 page,
                          u8 dev_addr, void *buf, size_t len)
{
    struct i2c_msg msgs[1];
    u8 i2c_addr;
    int ret;

    i2c_addr = device ? qsfp->i2c_address_dev1 : qsfp->i2c_address_dev0;

    if ((dev_addr + len - 1) > QSFP_PAGE_OFFSET && !qsfp->module_flat_mem) {
        u8 page_buf[2];

        page_buf[0] = QSFP_PAGE_OFFSET;
        page_buf[1] = page;

        msgs[0].addr = i2c_addr;
        msgs[0].flags = 0;   // write
        msgs[0].len = sizeof(page_buf);
        msgs[0].buf = page_buf;

        ret = __i2c_transfer(qsfp->i2c, msgs, 1);
        if (ret < 0) {
            return ret;
        }
    }

    msgs[0].addr = i2c_addr;
    msgs[0].flags = 0;
    msgs[0].len = 1 + len;
    msgs[0].buf = kmalloc(1 + len, GFP_KERNEL);
    if (!msgs[0].buf) {
        return -ENOMEM;
    }

    msgs[0].buf[0] = dev_addr;
    memcpy(&msgs[0].buf[1], buf, len);

    ret = __i2c_transfer(qsfp->i2c, msgs, ARRAY_SIZE(msgs));

    kfree(msgs[0].buf);

    if (ret < 0) {
        return ret;
    }

    return ret == ARRAY_SIZE(msgs) ? 0 : -EIO;
}

static int qsfp_i2c_configure(struct qsfp *qsfp)
{
    struct i2c_adapter *i2c = qsfp->fpc->i2c;

    if (!i2c) {
        TRX_LOG_ERR(qsfp, "FPC I2C adapter missing");
        return -ENODEV;
    }

    if (!i2c_check_functionality(i2c, I2C_FUNC_I2C)) {
        TRX_LOG_ERR(qsfp, "I2C expected functionality not supported");
        return -EINVAL;
    }

    qsfp->i2c = i2c;

    return 0;
}

int qsfp_read(struct qsfp *qsfp, u32 addr, void *buf, size_t len)
{
    int ret;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    i2c_lock_bus(qsfp->i2c, I2C_LOCK_SEGMENT);

    ret = qsfp_i2c_read(qsfp, addr >> 16, addr >> 8, addr & 0xFF, buf, len);

    /* On failure try reset of FPC402 for this particular port */
    if (ret) {
        TRX_LOG_ERR(qsfp, "Failed for address 0x%X. ret %d", addr, ret);
        ret = fpc_qsfp_i2c_recover(qsfp);
        if (ret == 0) {
            /* After reset immediately try an attempt to read */
            ret = qsfp_i2c_read(qsfp, addr >> 16, addr >> 8, addr & 0xFF, buf, len);
            if (ret < 0) {
                TRX_LOG_ERR(qsfp, "Failed in second attempt for address 0x%X. ret %d. Delay %u",
                            addr, ret, qsfp->fpc_qsfp_i2c_recover_delay);
                qsfp->read_write_2nd_fail++;
                /* On failure increase fpc_qsfp_i2c_recover_delay */
                if (qsfp->fpc_qsfp_i2c_recover_delay < FPC_QSFP_I2C_RECOVER_TIME_MAX) {
                    qsfp->fpc_qsfp_i2c_recover_delay += FPC_QSFP_I2C_RECOVER_TIME_STEP;
                }
            }
        }
    }

    /* On successful i2c read make fpc_qsfp_i2c_recover_delay zero */
    if (ret == 0) {
        qsfp->fpc_qsfp_i2c_recover_delay = 0;
    }

    i2c_unlock_bus(qsfp->i2c, I2C_LOCK_SEGMENT);

    return ret;
}

int qsfp_write(struct qsfp *qsfp, u32 addr, void *buf, size_t len)
{
    int ret;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    i2c_lock_bus(qsfp->i2c, I2C_LOCK_SEGMENT);

    ret = qsfp_i2c_write(qsfp, addr >> 16, addr >> 8, addr & 0xFF, buf, len);

    /* On failure try reset of FPC402 for this particular port */
    if (ret) {
        TRX_LOG_ERR(qsfp, "Failed for address 0x%X. ret %d", addr, ret);
        ret = fpc_qsfp_i2c_recover(qsfp);
        if (ret == 0) {
            /* After reset immediately try an attempt to write */
            ret = qsfp_i2c_write(qsfp, addr >> 16, addr >> 8, addr & 0xFF, buf, len);
            if (ret < 0) {
                TRX_LOG_ERR(qsfp, "Failed in second attempt for address 0x%X. ret %d. Delay %u",
                            addr, ret, qsfp->fpc_qsfp_i2c_recover_delay);
                qsfp->read_write_2nd_fail++;
                /* On failure increase fpc_qsfp_i2c_recover_delay */
                if (qsfp->fpc_qsfp_i2c_recover_delay < FPC_QSFP_I2C_RECOVER_TIME_MAX) {
                    qsfp->fpc_qsfp_i2c_recover_delay += FPC_QSFP_I2C_RECOVER_TIME_STEP;
                }
            }
        }
    }

    /* On successful i2c write make fpc_qsfp_i2c_recover_delay zero */
    if (ret == 0) {
        qsfp->fpc_qsfp_i2c_recover_delay = 0;
    }

    i2c_unlock_bus(qsfp->i2c, I2C_LOCK_SEGMENT);

    return ret;
}

static int qsfp_set_spec_ops(struct qsfp *qsfp)
{
    int ret;
    u8 *spec_id;

    if (qsfp->spec_ops) {
        return 0;
    }

    ret = qsfp_read(qsfp, 0, &qsfp->id, 1);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to read spec id. ret %d", ret);
        return -EAGAIN;
    }

    spec_id = (u8*)&qsfp->id;

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        qsfp->spec_ops = &sff8636_spec_ops;
        TRX_LOG_INFO(qsfp, "SFF8636 spec id 0x%02X", *spec_id);
        break;

    case SFF8024_ID_SFP:
        qsfp->spec_ops = &sff8472_spec_ops;
        TRX_LOG_INFO(qsfp, "SFP spec id 0x%02X", *spec_id);
        break;

    case SFF8024_ID_QSFPDD_CMIS:
        qsfp->spec_ops = &cmis_spec_ops;
        TRX_LOG_INFO(qsfp, "QSFP-DD CMIS spec id 0x%02X", *spec_id);
        break;

    default:
        TRX_LOG_WARN(qsfp, "Unsupported spec id 0x%02X", *spec_id);
        TRX_QXDM_LOG_ERROR(qsfp, "Port-%u: Unsupported specification id 0x%X",
                                 qsfp->port_num, *spec_id);
        return -E_UNSUPPORTED_SPEC;
    }

    return 0;
}

static void qsfp_update_flags(struct qsfp *qsfp)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
    if (qsfp->sim.flags) {
        /* Update current state from simulated flags */
        qsfp->flags = qsfp->sim_flags;
    } else {
        /* Update current state from HW */
        qsfp->spec_ops->update_flags(qsfp);
    }
#else
    /* Update current state from HW */
    qsfp->spec_ops->update_flags(qsfp);
#endif
}

u8 qsfp_check(void *buf, size_t len)
{
    u8 *p, check;

    for (p = buf, check = 0; len; p++, len--)
        check += *p;

    return check;
}

/* QSFP state machine */
static void qsfp_sm_set_timer(struct qsfp *qsfp, u32 timeout)
{
    if (timeout) {
        mod_delayed_work(system_power_efficient_wq, &qsfp->timeout,
                 timeout);
    } else {
        cancel_delayed_work(&qsfp->timeout);
    }
}

static void qsfp_sm_link_next(struct qsfp *qsfp, u8 state)
{
    qsfp->sm_link_state = state;
}

void qsfp_sm_mod_next(struct qsfp *qsfp, u8 state,
                             u32 timeout)
{
    qsfp->sm_mod_state = state;
    qsfp_sm_set_timer(qsfp, timeout);
}

static void qsfp_sm_link_linkup(struct qsfp *qsfp)
{
    qsfp_sm_link_next(qsfp, QSFP_S_LINK_UP);
    set_linkup_leds(qsfp);
}

static void qsfp_sm_link_check_rx_los(struct qsfp *qsfp)
{
    if (qsfp->status.rx_los) {
        qsfp_sm_link_next(qsfp, QSFP_S_RX_LOS);
    } else if (qsfp->status.eth_linkup) {
        qsfp_sm_link_linkup(qsfp);
    } else {
        qsfp_sm_link_next(qsfp, QSFP_S_DOWN);
    }
}

/*
 * If 'enable' is true push the QSFP to its high power class
 * otherwise push the QSFP to low power class 1
 */
static int qsfp_sm_mod_hpower(struct qsfp *qsfp, bool enable)
{
    int ret;

    /* Module high power is greater than allowed so do not allow
     * switching high power class
     */
    if (enable && qsfp->module_power_mW > qsfp->max_power_mW) {
        ret = qsfp->spec_ops->handle_max_power_exceed(qsfp);
    } else if (enable) {
        ret = qsfp->spec_ops->mod_high_power(qsfp);
    } else {
        ret = qsfp->spec_ops->mod_low_power(qsfp);
    }

    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to set Power class: enable %u "
                        "ret %d", enable, ret);
        return ret;
    }

    if (enable && qsfp->module_power_mW > qsfp->max_power_mW) {
        TRX_LOG_INFO(qsfp, "Module high power is greater than max "
        "allowed so handled by moving to allowed lower power class");
    } else if (enable) {
        TRX_LOG_INFO(qsfp, "Module switched to High Power class");
    } else {
        TRX_LOG_INFO(qsfp, "Module switched to Low Power class");
    }

    return 0;
}

/*
 * Reads power details from EEPROM and assign power fields in qsfp instance
 */
static int qsfp_module_parse_power(struct qsfp *qsfp)
{
    int ret;

    ret = qsfp->spec_ops->module_parse_power(qsfp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "spec parse power failed");
        return ret;
    }

    TRX_LOG_INFO(qsfp, "Module Power class %u Power %u.%uW",
    qsfp->module_power_class, qsfp->module_power_mW / 1000,
    (qsfp->module_power_mW / 100) % 10);

    return 0;
}

static int qsfp_module_tx_disable(struct qsfp *qsfp)
{
    u8 i;
    int ret;
    struct lane *lanei;
    u8 retry = QSFP_I2C_FAIL_RETRY;

    while (retry--) {
        ret = qsfp->spec_ops->mod_tx_disable(qsfp);
        if (ret == 0) {
            break;
        }
    }

    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "TX disable failed. ret %d", ret);
        return ret;
    }

    for (i = 0 ; i < qsfp->num_lanes ; i++) {
        lanei = qsfp->lane[i];
        if (!lanei) {
            continue;
        }
        TRX_LOG_INFO(lanei, "%s -> Disable",
        lanei->status.tx_disable ? "Disabled" : "Enabled");
        lanei->status.tx_disable = 1;
    }

    return 0;
}

static int qsfp_module_parse_speed(struct qsfp* qsfp)
{
    int bit_pos;
    trx_speed_mask speed_mask = TRX_LANE_SPEED_UNKNOWN;
    qsfp->lane_min_speed = TRX_LANE_SPEED_UNKNOWN;
    qsfp->lane_max_speed  = TRX_LANE_SPEED_UNKNOWN;

    if (qsfp->spec_ops) {
        qsfp->spec_ops->get_lane_speed(qsfp, &speed_mask);
    }
    else
    {
        TRX_LOG_WARN(qsfp, "Spec ops not yet initialised");
    }

    if(speed_mask == TRX_LANE_SPEED_UNKNOWN)
    {
      TRX_LOG_WARN(qsfp, "Unable to find Transceiver supported speeds");
      return -EINVAL;
    }

    /* Calculate minimum transceiver supported speed */
    for(bit_pos = 0; bit_pos < 7; bit_pos++ )
    {
        if(speed_mask & (1 << bit_pos))
        {
            qsfp->lane_min_speed |= BIT(bit_pos);
            break;
        }
    }

    /* Calculate maximum transceiver supported speed */
    for(bit_pos= 7; bit_pos >= 0; bit_pos--)
    {
        if(speed_mask & (1 << bit_pos))
        {
            qsfp->lane_max_speed |= BIT(bit_pos);
            break;
        }
    }

    if(qsfp->lane_min_speed == qsfp->lane_max_speed)
    {
        qsfp->lane_min_speed = TRX_LANE_SPEED_UNKNOWN;
    }

    return 0;
}

static int qsfp_sm_mod_probe(struct qsfp *qsfp)
{
    int ret;
    trx_lane_cfg lane_presence;

    ret = qsfp_set_spec_ops(qsfp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Unable to set spec ops. ret %d", ret);
        return ret;
    }

    ret = qsfp->spec_ops->mod_probe(qsfp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "spec mod probe failed. ret %d", ret);
        return ret;
    }

    ret = qsfp->spec_ops->get_lanes_presence(qsfp, &lane_presence);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to get number of lanes. ret %d", ret);
        return ret;
    } else {
        qsfp->lane_presence = lane_presence;
    }

    ret = qsfp->spec_ops->update_features_supported(qsfp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to read features supported by "
                          "transceiver. ret %d", ret);
        return ret;
    }

    ret = qsfp->spec_ops->disable_redundant_irq(qsfp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Disable interrupts failed. ret %d", ret);
    }

    /* TX disable when module inserted */
    if (qsfp->support.tx_disable) {
        ret = qsfp_module_tx_disable(qsfp);
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Module TX Disable failed. ret %d", ret);
            return ret;
        }
    }

    ret = qsfp_module_parse_speed(qsfp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "spec module parse speed failed. ret %d", ret);
        return ret;
    }

    if (qsfp->support.rate_select) {
        ret = qsfp->spec_ops->set_rate_select(qsfp, true);
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Rate select failed. ret %d", ret);
            return ret;
        }
    }

    /* Parse the module power requirement */
    ret = qsfp_module_parse_power(qsfp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "spec module parse power failed. ret %d", ret);
        return ret;
    }

    if (qsfp->module_power_mW > qsfp->max_power_mW) {
        TRX_LOG_INFO(qsfp, "Module max power %u more than max allowed %u",
                           qsfp->module_power_mW, qsfp->max_power_mW);
        ret = qsfp->spec_ops->handle_max_power_exceed(qsfp);
        if (ret < 0) {
           TRX_LOG_ERR(qsfp, "handle max power exceed failed. ret %d", ret);
           return ret;
        }
    }

#if IS_ENABLED(CONFIG_DEBUG_FS)
    /* call spec specific create debugfs function to create files
       specific to transceiver */
    qsfp->spec_ops->create_debugfs(qsfp);
#endif

    /* Parse the module ddm thresholds */
    ret = qsfp_module_parse_ddm_thresholds(qsfp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Module parse ddm thresholds failed, ret %d", ret);
        return ret;
    }

    ret  = qsfp_module_parse_laser_temp(qsfp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Module parse laser temperature failed, ret %d", ret);
        return ret;
    }

    module_sysfs_init(qsfp);
    qsfp_sensor_sysfs_init(qsfp);

    qsfp->spec_ops->eeprom_print(qsfp);

    TRX_LOG_INFO(qsfp, "Module probe success on try %u",
                 PROBE_RETRY - qsfp->sm_mod_tries + 1);

    return 0;
}

static void qsfp_sm_mod_remove(struct qsfp *qsfp)
{
    u8 i;
    struct lane *lanei;

    transceiver_led_off(qsfp, QSFP_LED1 | QSFP_LED2);

    for (i = 0 ; i < qsfp->num_lanes ; i++) {
        lanei = qsfp->lane[i];
        if (lanei && (lanei->status.present)) {
            lanei->status.present = 0;
            lane_sm_event(lanei, QSFP_E_REMOVE);
        }
    }

#if IS_ENABLED(CONFIG_DEBUG_FS)
    module_debugfs_exit(qsfp);
    /* Do not clear simulation remove */
    qsfp->sim.flags = 0;
#endif

    module_sysfs_exit(qsfp);
    qsfp_sensor_sysfs_exit(qsfp);
    clear_qsfp_reported_faults(qsfp);

    memset(&qsfp->id, 0, sizeof(qsfp->id));
    memset(&qsfp->support, 0, sizeof(qsfp->support));
    memset(&qsfp->diag, 0, sizeof(qsfp->diag));
    memset(&qsfp->status, 0, sizeof(qsfp->status));
    memset(&qsfp->param_info, 0, sizeof(qsfp->param_info));
    qsfp->module_revision = 0;
    qsfp->module_power_mW = 0;
    qsfp->module_power_class = 0;
    qsfp->spec_ops = NULL;
    qsfp->need_poll = false;
    qsfp->module_flat_mem = 0;
    qsfp->lane_presence = 0;
    qsfp->lane_min_speed = 0;
    qsfp->lane_max_speed = 0;
    qsfp->is_adapter = 0;

    TRX_LOG_INFO(qsfp, "Module removed");
}

/* This state machine tracks the upstream's state */
static void qsfp_sm_device(struct qsfp *qsfp, u32 event)
{
    switch (qsfp->sm_dev_state) {
    default:
        if (event == QSFP_E_DEV_ATTACH) {
            qsfp->sm_dev_state = QSFP_DEV_DOWN;
        }

        break;

    case QSFP_DEV_DOWN:
        if (event == QSFP_E_DEV_DETACH) {
            qsfp->sm_dev_state = QSFP_DEV_DETACHED;
        } else if (event == QSFP_E_DEV_UP) {
            qsfp->sm_dev_state = QSFP_DEV_UP;
        }

        break;

    case QSFP_DEV_UP:
        if (event == QSFP_E_DEV_DETACH) {
            qsfp->sm_dev_state = QSFP_DEV_DETACHED;
        } else if (event == QSFP_E_DEV_DOWN) {
            qsfp->sm_dev_state = QSFP_DEV_DOWN;
        }

        break;
    }
}

static void qsfp_sm_link_check_linkup(struct qsfp *qsfp)
{
    if (qsfp->status.tx_fault) {
        qsfp_sm_link_next(qsfp, QSFP_S_TX_FAULT);
    } else {
        qsfp_sm_link_check_rx_los(qsfp);
    }
}

static void qsfp_sm_link_check_txf_los(struct qsfp *qsfp)
{
    if (qsfp->status.tx_fault) {
        qsfp_sm_link_next(qsfp, QSFP_S_TX_FAULT);
        set_linkdown_leds(qsfp);
    } else if (qsfp->status.rx_los) {
        qsfp_sm_link_next(qsfp, QSFP_S_RX_LOS);
        set_linkdown_leds(qsfp);
    }
}

static void qsfp_sm_link_update_txf_rxlos_status(struct qsfp *qsfp)
{
    u8 i;
    bool rx_los = false;
    bool tx_fault = false;
    struct lane *lanei;

    for (i = 0 ; i < qsfp->num_lanes ; i++) {
        lanei = qsfp->lane[i];
        if (!lanei) {
            continue;
        }
        /* Consider only lanes which are ifconfig up */
        if (lanei->sm_dev_state != QSFP_DEV_UP) {
            continue;
        }

        if (lanei->sm_link_state == QSFP_S_LINK_UP) {
        /* Even if single lane is up, remove LOS and TX Fault */
            qsfp->status.rx_los = 0;
            qsfp->status.tx_fault = 0;
            return;
        }

        if (lanei->status.rx_los) {
            rx_los = true;
        }

        if (lanei->status.tx_fault) {
            tx_fault = true;
        }
    }

    if (rx_los) {
        qsfp->status.rx_los = 1;
    } else {
        qsfp->status.rx_los = 0;
    }

    if (tx_fault) {
        qsfp->status.tx_fault = 1;
    } else {
        qsfp->status.tx_fault = 0;
    }
}

static void qsfp_sm_link(struct qsfp *qsfp, u32 event)
{
    /* The main state machine */
    switch (qsfp->sm_link_state) {
    case QSFP_S_DOWN:
        if (event == QSFP_E_INSERT) {
            set_linkdown_leds(qsfp);

        } else if (event == QSFP_E_REVISIT) {
            qsfp_sm_link_update_txf_rxlos_status(qsfp);
            /* if device is ifconfig up then only try to make it up */
            if ((qsfp->sm_dev_state == QSFP_DEV_UP) &&
                (qsfp->sm_mod_state == QSFP_MOD_PRESENT)) {
                qsfp_sm_link_check_linkup(qsfp);
            }

        } else if ((event == QSFP_E_DEV_UP) || (event == QSFP_E_ETH_UP)) {
            qsfp_sm_link_update_txf_rxlos_status(qsfp);
            /* if module present then only try to make it up */
            if (qsfp->sm_mod_state == QSFP_MOD_PRESENT) {
                qsfp_sm_link_check_linkup(qsfp);
            }

        } else if (event == QSFP_E_TX_FAULT) {
            qsfp_sm_link_next(qsfp, QSFP_S_TX_FAULT);

        } else if (event == QSFP_E_RX_LOS) {
            qsfp_sm_link_next(qsfp, QSFP_S_RX_LOS);
        }

        break;

    case QSFP_S_RX_LOS:
        if (event == QSFP_E_TX_FAULT) {
            qsfp_sm_link_next(qsfp, QSFP_S_TX_FAULT);

        } else if (event == QSFP_E_RX_LOS_RECOVERY) {
            qsfp_sm_link_check_rx_los(qsfp);

        } else if ((event == QSFP_E_DEV_UP) || (event == QSFP_E_ETH_UP)) {
            qsfp_sm_link_update_txf_rxlos_status(qsfp);
            /* DevUp event comes for each of its lane going ifconfig up */
            qsfp_sm_link_check_linkup(qsfp);

        } else if (event == QSFP_E_REMOVE) {
            qsfp_sm_link_next(qsfp, QSFP_S_DOWN);

        } else if ((event == QSFP_E_DEV_DOWN) || (event == QSFP_E_DEV_DETACH)) {
            /* DevDown event comes if all of its lane going ifconfig down */
            qsfp_sm_link_update_txf_rxlos_status(qsfp);
            qsfp_sm_link_next(qsfp, QSFP_S_DOWN);
        }
        /* No need to handle LaneDown because if in RX LOS then there is no TX
         * Fault. Single lane going ifconfig down cant be in linkup as qsfp
         * state is RX LOS so there atleast one of the remaining lane in RX LOS
         * so state wont change
         */
        break;

    case QSFP_S_TX_FAULT:
        if (event == QSFP_E_TX_FAULT_RECOVERY) {
            qsfp_sm_link_check_rx_los(qsfp);

        } else if ((event == QSFP_E_DEV_UP) || (event == QSFP_E_LANE_DOWN) ||
                   (event == QSFP_E_ETH_UP)) {
            qsfp_sm_link_update_txf_rxlos_status(qsfp);
            qsfp_sm_link_check_linkup(qsfp);

        } else if (event == QSFP_E_REMOVE) {
            qsfp_sm_link_next(qsfp, QSFP_S_DOWN);

        } else if ((event == QSFP_E_DEV_DOWN) || (event == QSFP_E_DEV_DETACH)) {
            qsfp_sm_link_update_txf_rxlos_status(qsfp);
            qsfp_sm_link_next(qsfp, QSFP_S_DOWN);
        }

        break;

    case QSFP_S_LINK_UP:
        if (event == QSFP_E_LANE_DOWN) {
            qsfp_sm_link_update_txf_rxlos_status(qsfp);
            qsfp_sm_link_check_txf_los(qsfp);

        } else if (event == QSFP_E_TX_FAULT) {
            qsfp_sm_link_next(qsfp, QSFP_S_TX_FAULT);
            set_linkdown_leds(qsfp);

        } else if (event == QSFP_E_RX_LOS) {
            qsfp_sm_link_next(qsfp, QSFP_S_RX_LOS);
            set_linkdown_leds(qsfp);

        } else if (event == QSFP_E_REMOVE) {
            qsfp_sm_link_next(qsfp, QSFP_S_DOWN);

        } else if (event == QSFP_E_ETH_DOWN) {
            qsfp_sm_link_next(qsfp, QSFP_S_DOWN);
            set_linkdown_leds(qsfp);

        } else if ((event == QSFP_E_DEV_DOWN) || (event == QSFP_E_DEV_DETACH)) {
            qsfp_sm_link_update_txf_rxlos_status(qsfp);
            qsfp_sm_link_next(qsfp, QSFP_S_DOWN);
        }
        /* No need to handle DevUp event as it wont change current state */

        break;

    }
}

static void qsfp_sm_mod_error(struct qsfp *qsfp, u8 err)
{
    struct lane *lanei;
    u8 i;

    if (qsfp->sm_mod_tries) {
        qsfp->sm_mod_tries--;
        qsfp_sm_set_timer(qsfp, PROBE_RETRY_TIME_GAP);
        TRX_LOG_INFO(qsfp, "please wait, module slow to respond");
        return;
    }

    TRX_LOG_ERR(qsfp, "Module state set to %s", mod_state_to_str(err));
    qsfp_sm_mod_next(qsfp, err, 0);

    if (err == QSFP_MOD_ERROR_I2C) {
        char *msg_i2c[] = {QSFP_EVENT_ERROR_I2C, NULL};
        kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_i2c);
        TRX_LOG_INFO(qsfp, "Fault Report: %s", msg_i2c[0]);
        TRX_QXDM_LOG_ERROR(qsfp, "Port-%u: Transceiver init failed due to "
                                 "I2C error", qsfp->port_num);
    } else if (err == QSFP_MOD_ERROR_HPOWER) {
        char *msg_hpow[] = {QSFP_EVENT_ERROR_HIGH_POWER, NULL};
        kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_hpow);
        TRX_LOG_INFO(qsfp, "Fault Report: %s", msg_hpow[0]);
        TRX_QXDM_LOG_ERROR(qsfp, "Port-%u: Transceiver init failed due to "
        "error in switching to high power", qsfp->port_num);
    }

    for (i = 0 ; i < qsfp->num_lanes ; i++) {
        lanei = qsfp->lane[i];
        if (lanei && (lanei->status.present)) {
            lane_sm_mod_error(lanei, err);
        }
    }
}

static void qsfp_remove_update_devstate(struct qsfp *qsfp)
{
    u8 i;
    u8 new_dev_state;
    struct lane *lanei;

    /* Initialise it as detached and update as per lanes state */
    new_dev_state = QSFP_DEV_DETACHED;

    for (i = 0 ; i < qsfp->num_lanes ; i++) {
        lanei = qsfp->lane[i];
        if (!lanei) {
            continue;
        }
        if (lanei->sm_dev_state == QSFP_DEV_UP) {
            /* Atleast one lane is up mark QSFP device state as up */
            new_dev_state = QSFP_DEV_UP;
            break;
        } else if (lanei->sm_dev_state == QSFP_DEV_DOWN) {
        /* Atleast one lane is attached mark QSFP device state
         * as attached
         */
            new_dev_state = QSFP_DEV_DOWN;
        }
    }

    if (new_dev_state != qsfp->sm_dev_state) {
        qsfp->sm_dev_state = new_dev_state;
    }
}

static void qsfp_insert_update_devstate(struct qsfp *qsfp)
{
    u8 i;
    u8 new_dev_state;
    struct lane *lanei;

    /* Initialise it as detached and update as per lanes state */
    new_dev_state = QSFP_DEV_DETACHED;

    for (i = 0 ; i < qsfp->num_lanes ; i++) {
        lanei = qsfp->lane[i];
        if (lanei && lanei->status.present) {
            if (lanei->sm_dev_state == QSFP_DEV_UP) {
            /* Atleast one present lane is up mark QSFP device state as up */
                new_dev_state = QSFP_DEV_UP;
                break;
            } else if (lanei->sm_dev_state == QSFP_DEV_DOWN) {
            /* Atleast one present lane is attached mark QSFP device state
             * as attached
             */
                new_dev_state = QSFP_DEV_DOWN;
            }
        }
    }

    if (new_dev_state != qsfp->sm_dev_state) {
        qsfp->sm_dev_state = new_dev_state;
        if (new_dev_state == QSFP_DEV_DETACHED) {
            TRX_LOG_INFO(qsfp, "Device state changed to Detach");
            qsfp_sm_link(qsfp, QSFP_E_DEV_DETACH);
        } else if (new_dev_state == QSFP_DEV_DOWN) {
            TRX_LOG_INFO(qsfp, "Device state changed to Down");
            qsfp_sm_link(qsfp, QSFP_E_DEV_DOWN);
        }
    }
}

void qsfp_fill_features_str(const struct qsfp *qsfp, char *buf, int len)
{
    const struct qsfp_support *sup = &qsfp->support;

    scnprintf(buf, len, "%s%s%s%s%s%s%s%s%s%s%s%s%s",
    sup->temp_flags ? "Temperature ":"", sup->volt_flags ? "Voltage ":"",
    sup->rx_los ? "RX_LOS ":"", sup->rx_cdr_lol ? "RX_CDR_LOL ":"",
    sup->rx_power_flags ? "RX_Power ":"", sup->tx_los ? "TX_LOS ":"",
    sup->tx_cdr_lol ? "TX_CDR_LOL ":"", sup->tx_fault ? "TX_Fault ":"",
    sup->tx_adap_eq_in_fail ? "TX_Adaptive_EQ_Input_Fail ":"",
    sup->tx_power_flags ? "TX_Power ":"", sup->tx_bias_flags ? "TX_Bias ":"",
    sup->tx_disable ? "TX_Disable ":"", sup->rate_select ? "Rate_select ":"");
}

static void qsfp_print_features_supported(const struct qsfp *qsfp)
{
    char feature_str[QSFP_FEATURE_STR_MAX] = {0};

    qsfp_fill_features_str(qsfp, feature_str, sizeof(feature_str));

    TRX_LOG_INFO(qsfp, "Features: %s", feature_str);
    TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Features: %s", qsfp->port_num, feature_str);
}

bool qsfp_atleast_one_flag_supported(const struct qsfp *qsfp)
{
    const struct qsfp_support *sup = &qsfp->support;

    /* tx disable and rate select not considered they are not flags */
    if (sup->temp_flags || sup->volt_flags || sup->rx_los || sup->rx_cdr_lol ||
        sup->rx_power_flags || sup->tx_los || sup->tx_cdr_lol || sup->tx_fault ||
        sup->tx_adap_eq_in_fail || sup->tx_power_flags || sup->tx_bias_flags) {
        return true;
    } else {
        return false;
    }
}

static void qsfp_sm_mod_present(struct qsfp *qsfp)
{
    u8 i;
    int ret;
    struct lane *lanei;
    trx_lane_cfg lane_presence = qsfp->lane_presence;

    ret = qsfp_sm_mod_hpower(qsfp, true);
    if (ret < 0) {
        qsfp_sm_mod_error(qsfp, QSFP_MOD_ERROR_HPOWER);
        return;
    }

    qsfp_sm_mod_next(qsfp, QSFP_MOD_PRESENT, 0);

    for (i = 0 ; i < qsfp->num_lanes ; i++) {
        lanei = qsfp->lane[i];
        if (lanei && (lane_presence & 1)) {
            lanei->status.present = 1;
            lane_sm_event(lanei, QSFP_E_INSERT);
            TRX_QXDM_LOG_DEBUG(qsfp, "Port-%u: Lane-%u: Insert event",
                              qsfp->port_num, lanei->lane_num);
        }
        lane_presence >>= 1;
    }

    if (qsfp_atleast_one_flag_supported(qsfp)) {
    /* check state if there is no initial interrupt (ex: sff8472) and
     * Needed in case interrupt called before EEPROM read and
     * in case transceiver module reset fails
     * 300ms delay added as device takes time to reach high power state
     */
        qsfp_start_poll(qsfp, 300);
    }

    /* Update device state as per lanes lane_presence */
    qsfp_insert_update_devstate(qsfp);

    TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Transceiver init successful", qsfp->port_num);
}

/* This state machine tracks the insert/remove state of the module, probes
 * the on-board EEPROM, and sets up the power level.
 */
static void qsfp_sm_module(struct qsfp *qsfp, u32 event)
{
    int ret;

    /* Handle remove event globally, it resets this state machine */
    if (event == QSFP_E_REMOVE) {
        qsfp_sm_mod_remove(qsfp);
        qsfp_sm_mod_next(qsfp, QSFP_MOD_EMPTY, 0);
        /* Update device state after removal */
        qsfp_remove_update_devstate(qsfp);
        return;
    }

    switch (qsfp->sm_mod_state) {
    case QSFP_MOD_EMPTY:
        if (event == QSFP_E_INSERT) {
            /* Try to read EEPROM after MOD_READY_TIME (300ms) */
            qsfp_sm_mod_next(qsfp, QSFP_MOD_PROBE, MOD_READY_TIME);
            qsfp->sm_mod_tries = PROBE_RETRY;
        }
        break;

    case QSFP_MOD_PROBE:
        if (event != QSFP_E_REVISIT) {
            break;
        }

        ret = qsfp_sm_mod_probe(qsfp);

        if (ret == -E_UNSUPPORTED_SPEC) {
            qsfp_sm_mod_next(qsfp, QSFP_MOD_REJECT_SPEC, 0);
            break;
        } else if (ret == -E_MAX_POWER_EXCEED) {
            qsfp_sm_mod_next(qsfp, QSFP_MOD_REJECT_PWR, 0);
            TRX_QXDM_LOG_ERROR(qsfp, "Port-%u: Max power exceeded", qsfp->port_num);
            break;
        } else if (ret < 0) {
            qsfp_sm_mod_error(qsfp, QSFP_MOD_ERROR_I2C);
            break;
        }

        qsfp_print_features_supported(qsfp);

        if (qsfp->sm_dev_state < QSFP_DEV_DOWN) {
            qsfp_sm_mod_next(qsfp, QSFP_MOD_WAITDEV, 0);
        } else {
            qsfp_sm_mod_next(qsfp, QSFP_MOD_WAITHPOWER, 0);
            qsfp_sm_mod_present(qsfp);
        }

        break;

    case QSFP_MOD_WAITHPOWER:
        if (event == QSFP_E_DEV_DETACH) {
            qsfp_sm_mod_next(qsfp, QSFP_MOD_WAITDEV, 0);
        } else if (event == QSFP_E_REVISIT) {
            qsfp_sm_mod_present(qsfp);
        }

        break;

    case QSFP_MOD_WAITDEV:
        if (event == QSFP_E_DEV_ATTACH) {
            qsfp_sm_mod_next(qsfp, QSFP_MOD_WAITHPOWER, 0);
            qsfp_sm_mod_present(qsfp);
        }

        break;

    case QSFP_MOD_PRESENT:
        if (event == QSFP_E_DEV_DETACH) {
            qsfp_sm_mod_hpower(qsfp, false);
            qsfp_sm_mod_next(qsfp, QSFP_MOD_WAITDEV, 0);
        }

        break;

    case QSFP_MOD_REJECT_SPEC:
    case QSFP_MOD_REJECT_PWR:
    case QSFP_MOD_ERROR_I2C:
    case QSFP_MOD_ERROR_HPOWER:
    case QSFP_MOD_ERROR_TX_ENABLE_FAIL:
         break;
    }
}

static void qsfp_sm_event(struct qsfp *qsfp, u8 event)
{
    TRX_LOG_INFO(qsfp, "Enter [%s:%s:%s]   Event: %s",
                       mod_state_to_str(qsfp->sm_mod_state),
                       dev_state_to_str(qsfp->sm_dev_state),
                       link_state_to_str(qsfp->sm_link_state),
                       event_to_str(event));

    qsfp_sm_device(qsfp, event);
    qsfp_sm_module(qsfp, event);
    qsfp_sm_link(qsfp, event);

    TRX_LOG_INFO(qsfp, "Exit  [%s:%s:%s]\n",
                       mod_state_to_str(qsfp->sm_mod_state),
                       dev_state_to_str(qsfp->sm_dev_state),
                       link_state_to_str(qsfp->sm_link_state));

}

void qsfp_attach(const struct lane *lane)
{
    struct qsfp *qsfp = lane->qsfp;

    /* Already attached */
    if (qsfp->sm_dev_state > QSFP_DEV_DETACHED) {
        TRX_LOG_INFO(qsfp, "QSFP already in attached state. Ignoring"
                           " attach event");
        return;
    }

    if (qsfp->sm_mod_state == QSFP_MOD_PRESENT) {
        /* when module inserted check whether lane is active */
        if (lane->status.present) {
            qsfp_sm_event(qsfp, QSFP_E_DEV_ATTACH);
        } else {
            TRX_LOG_INFO(qsfp, "Lane not present, its attach event ignored");
        }
    } else {
        qsfp_sm_event(qsfp, QSFP_E_DEV_ATTACH);
    }
}

void qsfp_detach(const struct lane *lane)
{
    u8 i;
    bool detach = true;
    struct qsfp *qsfp = lane->qsfp;
    struct lane *lanei;

    if (qsfp->sm_mod_state == QSFP_MOD_PRESENT) {
        if (!(lane->status.present)) {
            TRX_LOG_INFO(qsfp, "Lane not present, its detach event ignored");
            return;
        }

        for (i = 0 ; i < qsfp->num_lanes ; i++) {
            lanei = qsfp->lane[i];
            if (!lanei) {
                continue;
            }
            /* Detach only when all present lanes in detached state */
            if ((lanei->status.present) &&
                (lanei->sm_dev_state != QSFP_DEV_DETACHED)) {
                detach = false;
                break;
            }
        }
    } else {
        for (i = 0 ; i < qsfp->num_lanes ; i++) {
            lanei = qsfp->lane[i];
            /* Detach only when all lanes in detached state */
            if (lanei && (lanei->sm_dev_state != QSFP_DEV_DETACHED)) {
                detach = false;
                break;
            }
        }
    }

    if (detach) {
        qsfp_sm_event(qsfp, QSFP_E_DEV_DETACH);
    }

}

void qsfp_start(const struct lane *lane)
{
    struct qsfp *qsfp = lane->qsfp;

    /* DevUp event comes for each of its present lane going ifconfig up */
    if (qsfp->sm_mod_state == QSFP_MOD_PRESENT) {
        if (lane->status.present) {
            qsfp_sm_event(qsfp, QSFP_E_DEV_UP);
        } else {
            TRX_LOG_INFO(qsfp, "Lane not present, its up event ignored");
        }
    } else {
        qsfp_sm_event(qsfp, QSFP_E_DEV_UP);
    }
}

void qsfp_stop(const struct lane *lane)
{
    u8 i;
    bool down = true;
    struct qsfp *qsfp = lane->qsfp;
    struct lane *lanei;

    if (qsfp->sm_mod_state == QSFP_MOD_PRESENT) {
        if (!(lane->status.present)) {
            TRX_LOG_INFO(qsfp, "Lane not present, its down event ignored");
            return;
        }

        for (i = 0 ; i < qsfp->num_lanes ; i++) {
            lanei = qsfp->lane[i];
            if (!lanei) {
                continue;
            }
            if ((lanei->status.present) &&
                (lanei->sm_dev_state == QSFP_DEV_UP)) {
                down = false;
                break;
            }
        }
    } else {
        for (i = 0 ; i < qsfp->num_lanes ; i++) {
            lanei = qsfp->lane[i];
            if (lanei && (lanei->sm_dev_state == QSFP_DEV_UP)) {
                TRX_LOG_INFO(qsfp, "Atleast one lane of qsfp is up, "
                                   "its lane down event ignored");
                return;
            }
        }
    }

    if (down) {
    /* Last present lane going ifconfig down */
        qsfp_sm_event(qsfp, QSFP_E_DEV_DOWN);
    } else {
    /* Single present lane going ifconfig down but not the last one
     * to go down. For the last present lane going ifconfig down
     * we get DevDown event instead of LaneDown
     */
        qsfp_sm_event(qsfp, QSFP_E_LANE_DOWN);
    }
}

int qsfp_module_info(struct sfp *sfp, struct ethtool_modinfo *modinfo)
{
    struct lane *lane = (struct lane*)sfp;
    struct qsfp *qsfp = lane->qsfp;

    TRX_LOG_INFO(qsfp, "initiated from lane %s", dev_name(lane->dev));

    if (!qsfp->spec_ops) {
        TRX_LOG_INFO(qsfp, "Module not present or not supported");
        return -ENODEV;
    }

    return qsfp->spec_ops->module_info(lane->qsfp, modinfo);
}

int qsfp_get_module_eeprom(struct qsfp *qsfp,
                        struct ethtool_eeprom *ee, u8 *data)
{
    int ret = 0;
    u32 offset = 0, length = 0, addr = 0;
    u8  page = 0;

    if (ee->len == 0)
    {
        TRX_LOG_ERR(qsfp, "Invalid EEPROM length\n");
        return -EINVAL;
    }

    offset = ee->offset;
    length = ee->len;

    if(offset >= TRX_EEPROM_PAGE_LENGTH)
    {
        page = TRX_PAGE_GET(offset);
        offset -= TRX_EEPROM_UP_PAGE_LENGTH * page;
    }

    addr = QSFP_ADDR(0, page, offset);

    TRX_LOG_INFO(qsfp, "page: %u offset: %u addr: 0x%02X"
                " length %u\n", page, offset, addr, length);

    ret = qsfp_read(qsfp, addr, data, length);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Fail to read EEPROM from offset %u "
                 "length %u. ret %d", ee->offset, ee->len, ret);
    }

    return ret;
}

int qsfp_module_eeprom(struct sfp *sfp, struct ethtool_eeprom *ee,
                 u8 *data)
{
    struct lane *lane = (struct lane*)sfp;
    struct qsfp *qsfp = lane->qsfp;

    TRX_LOG_INFO(qsfp, "offset %u length %u", ee->offset, ee->len);


    if (!qsfp->spec_ops) {
        TRX_LOG_INFO(qsfp, "Module not present or not supported");
        return -ENODEV;
    }

    return qsfp->spec_ops->module_eeprom(lane->qsfp, ee, data);
}

int qsfp_module_eeprom_by_page(struct sfp *sfp,
                     const struct ethtool_module_eeprom *page,
                     struct netlink_ext_ack *extack)
{
    int ret;
    struct lane *lane = (struct lane*)sfp;
    struct qsfp *qsfp = lane->qsfp;

    TRX_LOG_INFO(qsfp, "bank %u page %u offset %u length %u",
                 page->bank, page->page, page->offset, page->length);

    if (page->bank) {
        TRX_LOG_ERR(qsfp, "Banks not supported");
        NL_SET_ERR_MSG(extack, "Banks not supported");
        return -EOPNOTSUPP;
    }

    if (page->i2c_address != 0x50) {
        TRX_LOG_ERR(qsfp, "I2C address 0x%X not supported Only address 0x50"
                          " supported", page->i2c_address);
        NL_SET_ERR_MSG(extack, "Only address 0x50 supported");
        return -EOPNOTSUPP;
    }

    ret = qsfp_read(qsfp, (page->page << 8) | page->offset,
            page->data, page->length);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Fail to read EEPROM from page %u offset %u "
                    "length %u. ret %d", page->page, page->offset,
                    page->length, ret);
    }

    return ret;
}

int qsfp_trx_get_module_info(u32 lane_phandle, struct ethtool_modinfo *modinfo)
{
    struct lane *lane;

    if(modinfo == NULL)
        return -EINVAL;

    lane = phandle_to_drvdata(lane_phandle);
    if (!lane)
        return -EINVAL;

    if (!lane->status.present)
        return -EINVAL;

    return qsfp_module_info((struct sfp *)lane,modinfo);
}
EXPORT_SYMBOL_GPL(qsfp_trx_get_module_info);

int qsfp_trx_get_module_eeprom(u32 lane_phandle, struct ethtool_eeprom *ee, u8 *data)
{
    struct lane *lane;

    if((ee == NULL) || (data == NULL))
        return -EINVAL;

    lane = phandle_to_drvdata(lane_phandle);
    if (!lane)
        return -EINVAL;

    if (!lane->status.present)
        return -EINVAL;

    return qsfp_module_eeprom((struct sfp *)lane, ee, data);
}
EXPORT_SYMBOL_GPL(qsfp_trx_get_module_eeprom);

static void qsfp_timeout(struct work_struct *work)
{
    struct qsfp *qsfp = container_of(work, struct qsfp, timeout.work);

    mutex_lock(&qsfp->sm_mutex);

    qsfp_sm_event(qsfp, QSFP_E_REVISIT);

    mutex_unlock(&qsfp->sm_mutex);
}

static void qsfp_lane_fault_report(struct qsfp *qsfp, u8 chgd,
                                   u8 new_flag, char *str, u8* fault_app_flag)
{
    u8 i;
    char rstr[QSFP_FAULT_STR_MAX];
    struct lane *lanei;
    char *msg[] = {str, NULL};
    char *msg_recovery[] = {rstr, NULL};
    bool rx_los = false;
    bool tx_fault = false;

    scnprintf(rstr, QSFP_FAULT_STR_MAX, "%s_RECOVERY", str);

    if (!strncmp(str, QSFP_EVENT_RX_LOS, sizeof(QSFP_EVENT_RX_LOS))) {
        rx_los = true;
    } else if (!strncmp(str, QSFP_EVENT_TX_FAULT, sizeof(QSFP_EVENT_TX_FAULT))) {
        tx_fault = true;
    }

    for (i = 0 ; i < qsfp->num_lanes ; i++, chgd >>= 1, new_flag >>= 1) {
        lanei = qsfp->lane[i];
        if (!lanei) {
            continue;
        }
        if (chgd & 1) {
            /* Send lane faults only when the link is up or
             * the timer is active. */
            if(!lanei->status.eth_linkup) {
                if(! timer_pending(&qsfp->qsfp_flt_lnkd_timer)) {
                    continue;
                }
            }

            if (new_flag & 1) {
                kobject_uevent_env(&lanei->dev->kobj, KOBJ_CHANGE, msg);
                TRX_LOG_INFO(lanei, "Fault Report: %s", str);
                if (rx_los) {
                    TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Lane-%u: RX LOS detected",
                                      qsfp->port_num, lanei->lane_num);
                } else if (tx_fault) {
                    TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Lane-%u: TX Fault detected",
                                      qsfp->port_num, lanei->lane_num);
                }
                *fault_app_flag |= (1 << i);
            } else {
                kobject_uevent_env(&lanei->dev->kobj, KOBJ_CHANGE, msg_recovery);
                TRX_LOG_INFO(lanei, "Fault Report Recovery: %s", rstr);
                if (rx_los) {
                    TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Lane-%u: RX LOS recovered",
                                      qsfp->port_num, lanei->lane_num);
                } else if (tx_fault) {
                    TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Lane-%u: TX Fault recovered",
                                      qsfp->port_num, lanei->lane_num);
                }
                *fault_app_flag &= ~(1 << i);
            }
        }
    }
}

static void qsfp_temp_fault_report(struct qsfp *qsfp, u8 chgd)
{
    u8 new_flag;

    new_flag = qsfp->flags.temp;

    if (chgd & QSFP_TEMP_HIGH_ALARM) {
        if (new_flag & QSFP_TEMP_HIGH_ALARM) {
            char *msg_high_alarm[] = {QSFP_EVENT_TEMP_HIGH_ALARM, NULL};
            kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_high_alarm);
            qsfp->flags_reported_faults.temp_high_alarm = 1;
            TRX_LOG_INFO(qsfp, "Fault Report: %s", msg_high_alarm[0]);
            TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Temperature high alarm", qsfp->port_num);
        } else {
            char *msg_high_alarm_recovery[] = {QSFP_EVENT_TEMP_HIGH_ALARM_RECOVERY, NULL};
            kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_high_alarm_recovery);
            qsfp->flags_reported_faults.temp_high_alarm = 0;
            TRX_LOG_INFO(qsfp, "Fault Report Recovery: %s", msg_high_alarm_recovery[0]);
            TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Temperature high alarm recovery", qsfp->port_num);
        }
    }

    if (chgd & QSFP_TEMP_LOW_ALARM) {
        if (new_flag & QSFP_TEMP_LOW_ALARM) {
            char *msg_low_alarm[] = {QSFP_EVENT_TEMP_LOW_ALARM, NULL};
            kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_low_alarm);
            qsfp->flags_reported_faults.temp_low_alarm = 1;
            TRX_LOG_INFO(qsfp, "Fault Report: %s", msg_low_alarm[0]);
            TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Temperature low alarm", qsfp->port_num);
        } else {
            char *msg_low_alarm_recovery[] = {QSFP_EVENT_TEMP_LOW_ALARM_RECOVERY, NULL};
            kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_low_alarm_recovery);
            qsfp->flags_reported_faults.temp_low_alarm = 0;
            TRX_LOG_INFO(qsfp, "Fault Report Recovery: %s", msg_low_alarm_recovery[0]);
            TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Temperature low alarm recovery", qsfp->port_num);
        }
    }

    if (chgd & QSFP_TEMP_HIGH_WARN) {
        if (new_flag & QSFP_TEMP_HIGH_WARN) {
            char *msg_high_warn[] = {QSFP_EVENT_TEMP_HIGH_WARN, NULL};
            kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_high_warn);
            qsfp->flags_reported_faults.temp_high_warn = 1;
            TRX_LOG_INFO(qsfp, "Fault Report: %s", msg_high_warn[0]);
            TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Temperature high warning", qsfp->port_num);
        } else {
            char *msg_high_warn_recovery[] = {QSFP_EVENT_TEMP_HIGH_WARN_RECOVERY, NULL};
            kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_high_warn_recovery);
            qsfp->flags_reported_faults.temp_high_warn = 0;
            TRX_LOG_INFO(qsfp, "Fault Report Recovery: %s", msg_high_warn_recovery[0]);
            TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Temperature high warning recovery", qsfp->port_num);
        }
    }

    if (chgd & QSFP_TEMP_LOW_WARN) {
        if (new_flag & QSFP_TEMP_LOW_WARN) {
            char *msg_low_warn[] = {QSFP_EVENT_TEMP_LOW_WARN, NULL};
            kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_low_warn);
            qsfp->flags_reported_faults.temp_low_warn = 1;
            TRX_LOG_INFO(qsfp, "Fault Report: %s", msg_low_warn[0]);
            TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Temperature low warning", qsfp->port_num);
        } else {
            char *msg_low_warn_recovery[] = {QSFP_EVENT_TEMP_LOW_WARN_RECOVERY, NULL};
            kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_low_warn_recovery);
            qsfp->flags_reported_faults.temp_low_warn = 0;
            TRX_LOG_INFO(qsfp, "Fault Report Recovery: %s", msg_low_warn_recovery[0]);
            TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Temperature low warning recovery", qsfp->port_num);
        }
    }
}

static void qsfp_volt_fault_report(struct qsfp *qsfp, u8 chgd)
{
    u8 new_flag;

    new_flag = qsfp->flags.volt;

    if (chgd & QSFP_VOLT_HIGH_ALARM) {
        if (new_flag & QSFP_VOLT_HIGH_ALARM) {
            char *msg_high_alarm[] = {QSFP_EVENT_VOLT_HIGH_ALARM, NULL};
            kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_high_alarm);
            qsfp->flags_reported_faults.volt_high_alarm = 1;
            TRX_LOG_INFO(qsfp, "Fault Report: %s", msg_high_alarm[0]);
            TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Voltage high alarm", qsfp->port_num);
        } else {
            char *msg_high_alarm_recovery[] = {QSFP_EVENT_VOLT_HIGH_ALARM_RECOVERY, NULL};
            kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_high_alarm_recovery);
            qsfp->flags_reported_faults.volt_high_alarm = 0;
            TRX_LOG_INFO(qsfp, "Fault Report Recovery: %s", msg_high_alarm_recovery[0]);
            TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Voltage high alarm recovery", qsfp->port_num);
        }
    }

    if (chgd & QSFP_VOLT_LOW_ALARM) {
        if (new_flag & QSFP_VOLT_LOW_ALARM) {
            char *msg_low_alarm[] = {QSFP_EVENT_VOLT_LOW_ALARM, NULL};
            kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_low_alarm);
            qsfp->flags_reported_faults.volt_low_alarm = 1;
            TRX_LOG_INFO(qsfp, "Fault Report: %s", msg_low_alarm[0]);
            TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Voltage low alarm", qsfp->port_num);
        } else {
            char *msg_low_alarm_recovery[] = {QSFP_EVENT_VOLT_LOW_ALARM_RECOVERY, NULL};
            kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_low_alarm_recovery);
            qsfp->flags_reported_faults.volt_low_alarm = 0;
            TRX_LOG_INFO(qsfp, "Fault Report Recovery: %s", msg_low_alarm_recovery[0]);
            TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Voltage low alarm recovery", qsfp->port_num);
        }
    }

    if (chgd & QSFP_VOLT_HIGH_WARN) {
        if (new_flag & QSFP_VOLT_HIGH_WARN) {
            char *msg_high_warn[] = {QSFP_EVENT_VOLT_HIGH_WARN, NULL};
            kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_high_warn);
            qsfp->flags_reported_faults.volt_high_warn = 1;
            TRX_LOG_INFO(qsfp, "Fault Report: %s", msg_high_warn[0]);
            TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Voltage high warning", qsfp->port_num);
        } else {
            char *msg_high_warn_recovery[] = {QSFP_EVENT_VOLT_HIGH_WARN_RECOVERY, NULL};
            kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_high_warn_recovery);
            qsfp->flags_reported_faults.volt_high_warn = 0;
            TRX_LOG_INFO(qsfp, "Fault Report Recovery: %s", msg_high_warn_recovery[0]);
            TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Voltage high warning recovery", qsfp->port_num);
        }
    }

    if (chgd & QSFP_VOLT_LOW_WARN) {
        if (new_flag & QSFP_VOLT_LOW_WARN) {
            char *msg_low_warn[] = {QSFP_EVENT_VOLT_LOW_WARN, NULL};
            kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_low_warn);
            qsfp->flags_reported_faults.volt_low_warn = 1;
            TRX_LOG_INFO(qsfp, "Fault Report: %s", msg_low_warn[0]);
            TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Voltage low warning", qsfp->port_num);
        } else {
            char *msg_low_warn_recovery[] = {QSFP_EVENT_VOLT_LOW_WARN_RECOVERY, NULL};
            kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_low_warn_recovery);
            qsfp->flags_reported_faults.volt_low_warn = 0;
            TRX_LOG_INFO(qsfp, "Fault Report Recovery: %s", msg_low_warn_recovery[0]);
            TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Voltage low warning recovery", qsfp->port_num);
        }
    }
}

static void qsfp_lane_rxlos_txf(struct qsfp *qsfp, u8 chgd_rxlos, u8 chgd_txf)
{
    u8 i;
    struct lane *lanei;
    u8 rx_los;
    u8 tx_fault;

    rx_los = qsfp->flags.rx_los;
    tx_fault = qsfp->flags.tx_fault;

    for (i = 0 ; i < qsfp->num_lanes ; i++, rx_los >>= 1, tx_fault >>= 1,
         chgd_rxlos >>= 1, chgd_txf >>= 1) {

        lanei = qsfp->lane[i];
        if (!lanei) {
            continue;
        }

        if (chgd_rxlos & 1) {
            if (rx_los & 1) {
                lanei->status.rx_los = 1;
                lane_sm_event(lanei, QSFP_E_RX_LOS);
            } else {
                lanei->status.rx_los = 0;
                lane_sm_event(lanei, QSFP_E_RX_LOS_RECOVERY);
            }
        }

        if (chgd_txf & 1) {
            if (tx_fault & 1) {
                lanei->status.tx_fault = 1;
                lane_sm_event(lanei, QSFP_E_TX_FAULT);
            } else {
                lanei->status.tx_fault = 0;
                lane_sm_event(lanei, QSFP_E_TX_FAULT_RECOVERY);
            }
        }
    }

    /* when qsfp state is down, all its lanes were ifconfig down
     * we will consider events only from lanes which are ifconfig up
     * Do not update qsfp state
     */
    if (qsfp->sm_dev_state == QSFP_DEV_UP) {
        struct qsfp_status old_status = qsfp->status;

        qsfp_sm_link_update_txf_rxlos_status(qsfp);

        if (old_status.rx_los) {
            if (!qsfp->status.rx_los) {
                qsfp->status.rx_los = 0;
                qsfp_sm_event(qsfp, QSFP_E_RX_LOS_RECOVERY);
            }
        } else {
            if (qsfp->status.rx_los) {
                qsfp->status.rx_los = 1;
                qsfp_sm_event(qsfp, QSFP_E_RX_LOS);
            }
        }

        if (old_status.tx_fault) {
            if (!qsfp->status.tx_fault) {
                qsfp->status.tx_fault = 0;;
                qsfp_sm_event(qsfp, QSFP_E_TX_FAULT_RECOVERY);
            }
        } else {
            if (qsfp->status.tx_fault) {
                qsfp->status.tx_fault = 1;
                qsfp_sm_event(qsfp, QSFP_E_TX_FAULT);
            }
        }

    } else {
        TRX_LOG_INFO(qsfp, "Not updating qsfp state as all lanes were"
                           " ifconfig down");
    }
}

static void qsfp_lane_fault_clear(const struct qsfp *qsfp, u8 chgd,
                                   u8 new_flag, char *str)
{
    u8 i;
    char rstr[QSFP_FAULT_STR_MAX];
    struct lane *lanei;
    char *msg_recovery[] = {rstr, NULL};
    bool rx_los = false;
    bool tx_fault = false;

    scnprintf(rstr, QSFP_FAULT_STR_MAX, "%s_RECOVERY", str);

    if (!strncmp(str, QSFP_EVENT_RX_LOS, sizeof(QSFP_EVENT_RX_LOS))) {
        rx_los = true;
    } else if (!strncmp(str, QSFP_EVENT_TX_FAULT, sizeof(QSFP_EVENT_TX_FAULT))) {
        tx_fault = true;
    }

    for (i = 0 ; i < qsfp->num_lanes ; i++, chgd >>= 1, new_flag >>= 1) {
        lanei = qsfp->lane[i];
        if (!lanei) {
            continue;
        }
        if (chgd & 1) {
            if (!(new_flag & 1)) {
                kobject_uevent_env(&lanei->dev->kobj, KOBJ_CHANGE, msg_recovery);
                TRX_LOG_INFO(lanei, "Fault Report Recovery: %s", rstr);
                if (rx_los) {
                    TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Lane-%u: RX LOS recovered",
                                      qsfp->port_num, lanei->lane_num);
                } else if (tx_fault) {
                    TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Lane-%u: TX Fault recovered",
                                      qsfp->port_num, lanei->lane_num);
                }
            }
        }
    }
}

void clear_qsfp_reported_faults(struct qsfp *qsfp)
{
    struct qsfp_flags fault_reported_flags;
    struct qsfp_flags reset_flags;
    struct qsfp_flags chgd;
    fault_reported_flags = qsfp->flags_reported_faults;

    memset(&qsfp->flags, 0, sizeof(qsfp->flags));
    reset_flags = qsfp->flags;

    chgd.rx_los = fault_reported_flags.rx_los ^ reset_flags.rx_los;
    chgd.tx_los = fault_reported_flags.tx_los ^ reset_flags.tx_los;
    chgd.tx_fault = fault_reported_flags.tx_fault ^ reset_flags.tx_fault;
    chgd.rx_cdr_lol = fault_reported_flags.rx_cdr_lol ^ reset_flags.rx_cdr_lol;
    chgd.tx_cdr_lol = fault_reported_flags.tx_cdr_lol ^ reset_flags.tx_cdr_lol;
    chgd.tx_adap_eq_in_fail = fault_reported_flags.tx_adap_eq_in_fail ^
                                         reset_flags.tx_adap_eq_in_fail;

    chgd.temp = fault_reported_flags.temp ^ reset_flags.temp;
    chgd.volt = fault_reported_flags.volt ^ reset_flags.volt;

    chgd.rx_power_high_alarm = fault_reported_flags.rx_power_high_alarm ^
                                         reset_flags.rx_power_high_alarm;
    chgd.rx_power_low_alarm = fault_reported_flags.rx_power_low_alarm ^
                                          reset_flags.rx_power_low_alarm;
    chgd.rx_power_high_warn = fault_reported_flags.rx_power_high_warn ^
                                          reset_flags.rx_power_high_warn;
    chgd.rx_power_low_warn = fault_reported_flags.rx_power_low_warn ^
                                           reset_flags.rx_power_low_warn;

    chgd.tx_power_high_alarm = fault_reported_flags.tx_power_high_alarm ^
                                         reset_flags.tx_power_high_alarm;
    chgd.tx_power_low_alarm = fault_reported_flags.tx_power_low_alarm ^
                                          reset_flags.tx_power_low_alarm;
    chgd.tx_power_high_warn = fault_reported_flags.tx_power_high_warn ^
                                          reset_flags.tx_power_high_warn;
    chgd.tx_power_low_warn = fault_reported_flags.tx_power_low_warn ^
                                           reset_flags.tx_power_low_warn;

    chgd.tx_bias_high_alarm = fault_reported_flags.tx_bias_high_alarm ^
                                        reset_flags.tx_bias_high_alarm;
    chgd.tx_bias_low_alarm = fault_reported_flags.tx_bias_low_alarm ^
                                         reset_flags.tx_bias_low_alarm;
    chgd.tx_bias_high_warn = fault_reported_flags.tx_bias_high_warn ^
                                         reset_flags.tx_bias_high_warn;
    chgd.tx_bias_low_warn = fault_reported_flags.tx_bias_low_warn ^
                                          reset_flags.tx_bias_low_warn;

    if(chgd.rx_los) {
        qsfp_lane_fault_clear(qsfp, chgd.rx_los, reset_flags.rx_los,
                                                 QSFP_EVENT_RX_LOS);
    }

    if(chgd.tx_los) {
        qsfp_lane_fault_clear(qsfp, chgd.tx_los, reset_flags.tx_los,
                                                 QSFP_EVENT_TX_LOS);
    }

    if (chgd.temp) {
        qsfp_temp_fault_report(qsfp, chgd.temp);
    }

    if (chgd.volt) {
        qsfp_volt_fault_report(qsfp, chgd.volt);
    }

    if (chgd.rx_power_high_alarm) {
        qsfp_lane_fault_clear(qsfp, chgd.rx_power_high_alarm,
                               reset_flags.rx_power_high_alarm,
                               QSFP_EVENT_RX_POWER_HIGH_ALARM);
    }

    if (chgd.rx_power_low_alarm) {
        qsfp_lane_fault_clear(qsfp, chgd.rx_power_low_alarm,
                               reset_flags.rx_power_low_alarm,
                               QSFP_EVENT_RX_POWER_LOW_ALARM);
    }

    if (chgd.rx_power_high_warn) {
        qsfp_lane_fault_clear(qsfp, chgd.rx_power_high_warn,
                               reset_flags.rx_power_high_warn,
                               QSFP_EVENT_RX_POWER_HIGH_WARN);
    }

    if (chgd.rx_power_low_warn) {
        qsfp_lane_fault_clear(qsfp, chgd.rx_power_low_warn,
                               reset_flags.rx_power_low_warn,
                               QSFP_EVENT_RX_POWER_LOW_WARN);
    }

    if (chgd.tx_power_high_alarm) {
        qsfp_lane_fault_clear(qsfp, chgd.tx_power_high_alarm,
                               reset_flags.tx_power_high_alarm,
                               QSFP_EVENT_TX_POWER_HIGH_ALARM);
    }

    if (chgd.tx_power_low_alarm) {
        qsfp_lane_fault_clear(qsfp, chgd.tx_power_low_alarm,
                               reset_flags.tx_power_low_alarm,
                               QSFP_EVENT_TX_POWER_LOW_ALARM);
    }

    if (chgd.tx_power_high_warn) {
        qsfp_lane_fault_clear(qsfp, chgd.tx_power_high_warn,
                               reset_flags.tx_power_high_warn,
                               QSFP_EVENT_TX_POWER_HIGH_WARN);
    }

    if (chgd.tx_power_low_warn) {
        qsfp_lane_fault_clear(qsfp, chgd.tx_power_low_warn,
                               reset_flags.tx_power_low_warn,
                               QSFP_EVENT_TX_POWER_LOW_WARN);
    }

    if (chgd.tx_bias_high_alarm) {
        qsfp_lane_fault_clear(qsfp, chgd.tx_bias_high_alarm,
                               reset_flags.tx_bias_high_alarm,
                               QSFP_EVENT_TX_BIAS_HIGH_ALARM);
    }

    if (chgd.tx_bias_low_alarm) {
        qsfp_lane_fault_clear(qsfp, chgd.tx_bias_low_alarm,
                               reset_flags.tx_bias_low_alarm,
                               QSFP_EVENT_TX_BIAS_LOW_ALARM);
    }

    if (chgd.tx_bias_high_warn) {
        qsfp_lane_fault_clear(qsfp, chgd.tx_bias_high_warn,
                               reset_flags.tx_bias_high_warn,
                               QSFP_EVENT_TX_BIAS_HIGH_WARN);
    }

    if (chgd.tx_bias_low_warn) {
        qsfp_lane_fault_clear(qsfp, chgd.tx_bias_low_warn,
                               reset_flags.tx_bias_low_warn,
                               QSFP_EVENT_TX_BIAS_LOW_WARN);
    }

    if (chgd.rx_cdr_lol) {
        qsfp_lane_fault_clear(qsfp, chgd.rx_cdr_lol, reset_flags.rx_cdr_lol,
                                                     QSFP_EVENT_RX_CDR_LOL);
    }

    if (chgd.tx_cdr_lol) {
        qsfp_lane_fault_clear(qsfp, chgd.tx_cdr_lol, reset_flags.tx_cdr_lol,
                                                     QSFP_EVENT_TX_CDR_LOL);
    }

    if (chgd.tx_adap_eq_in_fail) {
        qsfp_lane_fault_clear(qsfp, chgd.tx_adap_eq_in_fail,
                               reset_flags.tx_adap_eq_in_fail,
                               QSFP_EVENT_TX_ADAPTIVE_EQ_IN_FAIL);
    }

    memset(&qsfp->flags_reported_faults, 0, sizeof(qsfp->flags_reported_faults));
    return;
}

/*
 * Checks QSFP status for TX/RX LOS, TX Fault, Temperature, Voltage, Power etc
 */
void qsfp_check_state(struct qsfp *qsfp)
{
    u8 i;
    struct lane *lanei;
    struct qsfp_flags oldf;
    struct qsfp_flags chgd;
    const struct qsfp_flags *newf;
    struct qsfp_flags *fault_app_flags;

    mutex_lock(&qsfp->sm_mutex);

    if (qsfp_set_spec_ops(qsfp) < 0) {
        TRX_LOG_ERR(qsfp, "Unable to set the spec ops");
        mutex_unlock(&qsfp->sm_mutex);
        return;
    }

    /* Save previous flags */
    oldf = qsfp->flags;

    fault_app_flags = &qsfp->flags_reported_faults;

    memset(&qsfp->flags, 0, sizeof(qsfp->flags));

    qsfp_update_flags(qsfp);

    newf = &qsfp->flags;

    chgd.rx_los = oldf.rx_los ^ newf->rx_los;
    chgd.tx_los = oldf.tx_los ^ newf->tx_los;
    chgd.tx_fault = oldf.tx_fault ^ newf->tx_fault;

    chgd.rx_cdr_lol = oldf.rx_cdr_lol ^ newf->rx_cdr_lol;
    chgd.tx_cdr_lol = oldf.tx_cdr_lol ^ newf->tx_cdr_lol;
    chgd.tx_adap_eq_in_fail = oldf.tx_adap_eq_in_fail ^ newf->tx_adap_eq_in_fail;

    chgd.temp = oldf.temp ^ newf->temp;
    chgd.volt = oldf.volt ^ newf->volt;

    chgd.rx_power_high_alarm = oldf.rx_power_high_alarm ^ newf->rx_power_high_alarm;
    chgd.rx_power_low_alarm = oldf.rx_power_low_alarm ^ newf->rx_power_low_alarm;
    chgd.rx_power_high_warn = oldf.rx_power_high_warn ^ newf->rx_power_high_warn;
    chgd.rx_power_low_warn = oldf.rx_power_low_warn ^ newf->rx_power_low_warn;

    chgd.tx_power_high_alarm = oldf.tx_power_high_alarm ^ newf->tx_power_high_alarm;
    chgd.tx_power_low_alarm = oldf.tx_power_low_alarm ^ newf->tx_power_low_alarm;
    chgd.tx_power_high_warn = oldf.tx_power_high_warn ^ newf->tx_power_high_warn;
    chgd.tx_power_low_warn = oldf.tx_power_low_warn ^ newf->tx_power_low_warn;

    chgd.tx_bias_high_alarm = oldf.tx_bias_high_alarm ^ newf->tx_bias_high_alarm;
    chgd.tx_bias_low_alarm = oldf.tx_bias_low_alarm ^ newf->tx_bias_low_alarm;
    chgd.tx_bias_high_warn = oldf.tx_bias_high_warn ^ newf->tx_bias_high_warn;
    chgd.tx_bias_low_warn = oldf.tx_bias_low_warn ^ newf->tx_bias_low_warn;

    if (chgd.rx_los) {
        TRX_LOG_INFO(qsfp, "RX LOS: Current [0x%02X] Next [0x%02X], Changed "
        "[0x%02X]", oldf.rx_los, newf->rx_los, chgd.rx_los);
        qsfp_lane_fault_report(qsfp, chgd.rx_los, newf->rx_los,
                           QSFP_EVENT_RX_LOS, &fault_app_flags->rx_los);
    }

    if (chgd.tx_fault) {
        TRX_LOG_INFO(qsfp, "TX Fault: Current [0x%02X] Next [0x%02X] Changed "
        "[0x%02X]", oldf.tx_fault, newf->tx_fault, chgd.tx_fault);

        qsfp_lane_fault_report(qsfp, chgd.tx_fault, newf->tx_fault,
                              QSFP_EVENT_TX_FAULT, &fault_app_flags->tx_fault);
    }

    if (chgd.tx_los) {
        TRX_LOG_INFO(qsfp, "TX LOS: Current [0x%02X] Next [0x%02X], Changed "
        "[0x%02X]", oldf.tx_los, newf->tx_los, chgd.tx_los);

        qsfp_lane_fault_report(qsfp, chgd.tx_los, newf->tx_los,
                           QSFP_EVENT_TX_LOS, &fault_app_flags->tx_los);
    }

    if (chgd.temp) {
        TRX_LOG_INFO(qsfp, "Temperature: Current [0x%02X] Next [0x%02X], "
        "Changed [0x%02X]", oldf.temp, newf->temp, chgd.temp);

        qsfp_temp_fault_report(qsfp, chgd.temp);
    }

    if (chgd.volt) {
        TRX_LOG_INFO(qsfp, "Voltage: Current [0x%02X] Next [0x%02X], Changed "
        "[0x%02X]", oldf.volt, newf->volt, chgd.volt);

        qsfp_volt_fault_report(qsfp, chgd.volt);
    }

    if (chgd.rx_power_high_alarm) {
        TRX_LOG_INFO(qsfp, "RX Power High Alarm: Current [0x%02X] Next "
        "[0x%02X], Changed [0x%02X]", oldf.rx_power_high_alarm,
        newf->rx_power_high_alarm, chgd.rx_power_high_alarm);

        qsfp_lane_fault_report(qsfp, chgd.rx_power_high_alarm,
                newf->rx_power_high_alarm,QSFP_EVENT_RX_POWER_HIGH_ALARM,
                &fault_app_flags->rx_power_high_alarm);
    }
    if (chgd.rx_power_low_alarm) {
        TRX_LOG_INFO(qsfp, "RX Power Low Alarm: Current [0x%02X] Next [0x%02X]"
        ", Changed [0x%02X]", oldf.rx_power_low_alarm,
        newf->rx_power_low_alarm, chgd.rx_power_low_alarm);

        qsfp_lane_fault_report(qsfp, chgd.rx_power_low_alarm,
                               newf->rx_power_low_alarm,
                               QSFP_EVENT_RX_POWER_LOW_ALARM,
                               &fault_app_flags->rx_power_low_alarm);
    }
    if (chgd.rx_power_high_warn) {
        TRX_LOG_INFO(qsfp, "RX Power High Warning: Current [0x%02X] Next "
        "[0x%02X], Changed [0x%02X]", oldf.rx_power_high_warn,
        newf->rx_power_high_warn, chgd.rx_power_high_warn);

        qsfp_lane_fault_report(qsfp, chgd.rx_power_high_warn,
               newf->rx_power_high_warn, QSFP_EVENT_RX_POWER_HIGH_WARN,
               &fault_app_flags->rx_power_high_warn);
    }
    if (chgd.rx_power_low_warn) {
        TRX_LOG_INFO(qsfp, "RX Power Low Warning: Current [0x%02X] Next "
        "[0x%02X], Changed [0x%02X]", oldf.rx_power_low_warn,
        newf->rx_power_low_warn, chgd.rx_power_low_warn);

        qsfp_lane_fault_report(qsfp, chgd.rx_power_low_warn,
                               newf->rx_power_low_warn,
                               QSFP_EVENT_RX_POWER_LOW_WARN,
                               &fault_app_flags->rx_power_low_warn);
    }

    if (chgd.tx_power_high_alarm) {
        TRX_LOG_INFO(qsfp, "TX Power High Alarm: Current [0x%02X] Next "
        "[0x%02X], Changed [0x%02X]", oldf.tx_power_high_alarm,
        newf->tx_power_high_alarm, chgd.tx_power_high_alarm);

        qsfp_lane_fault_report(qsfp, chgd.tx_power_high_alarm,
                               newf->tx_power_high_alarm,
                               QSFP_EVENT_TX_POWER_HIGH_ALARM,
                               &fault_app_flags->tx_power_high_alarm);
    }
    if (chgd.tx_power_low_alarm) {
        TRX_LOG_INFO(qsfp, "TX Power Low Alarm: Current [0x%02X] Next "
        "[0x%02X], Changed [0x%02X]", oldf.tx_power_low_alarm,
        newf->tx_power_low_alarm, chgd.tx_power_low_alarm);

        qsfp_lane_fault_report(qsfp, chgd.tx_power_low_alarm,
                               newf->tx_power_low_alarm,
                               QSFP_EVENT_TX_POWER_LOW_ALARM,
                               &fault_app_flags->tx_power_low_alarm);
    }
    if (chgd.tx_power_high_warn) {
        TRX_LOG_INFO(qsfp, "TX Power High Warning: Current [0x%02X] Next "
        "[0x%02X], Changed [0x%02X]", oldf.tx_power_high_warn,
        newf->tx_power_high_warn, chgd.tx_power_high_warn);

        qsfp_lane_fault_report(qsfp, chgd.tx_power_high_warn,
                               newf->tx_power_high_warn,
                               QSFP_EVENT_TX_POWER_HIGH_WARN,
                               &fault_app_flags->tx_power_high_warn);
    }
    if (chgd.tx_power_low_warn) {
        TRX_LOG_INFO(qsfp, "TX Power Low Warning: Current [0x%02X] Next "
        "[0x%02X], Changed [0x%02X]", oldf.tx_power_low_warn,
        newf->tx_power_low_warn, chgd.tx_power_low_warn);

        qsfp_lane_fault_report(qsfp, chgd.tx_power_low_warn,
                               newf->tx_power_low_warn,
                               QSFP_EVENT_TX_POWER_LOW_WARN,
                               &fault_app_flags->tx_power_low_warn);
    }

    if (chgd.tx_bias_high_alarm) {
        TRX_LOG_INFO(qsfp, "TX Bias High Alarm: Current [0x%02X] Next "
        "[0x%02X], Changed [0x%02X]", oldf.tx_bias_high_alarm,
        newf->tx_bias_high_alarm, chgd.tx_bias_high_alarm);

        qsfp_lane_fault_report(qsfp, chgd.tx_bias_high_alarm,
                               newf->tx_bias_high_alarm,
                               QSFP_EVENT_TX_BIAS_HIGH_ALARM,
                               &fault_app_flags->tx_bias_high_alarm);
    }
    if (chgd.tx_bias_low_alarm) {
        TRX_LOG_INFO(qsfp, "TX Bias Low Alarm: Current [0x%02X] Next "
        "[0x%02X], Changed [0x%02X]", oldf.tx_bias_low_alarm,
        newf->tx_bias_low_alarm, chgd.tx_bias_low_alarm);

        qsfp_lane_fault_report(qsfp, chgd.tx_bias_low_alarm,
                               newf->tx_bias_low_alarm,
                               QSFP_EVENT_TX_BIAS_LOW_ALARM,
                               &fault_app_flags->tx_bias_low_alarm);
    }
    if (chgd.tx_bias_high_warn) {
        TRX_LOG_INFO(qsfp, "TX Bias High Warning: Current [0x%02X] Next "
        "[0x%02X], Changed [0x%02X]", oldf.tx_bias_high_warn,
        newf->tx_bias_high_warn, chgd.tx_bias_high_warn);

        qsfp_lane_fault_report(qsfp, chgd.tx_bias_high_warn,
                               newf->tx_bias_high_warn,
                               QSFP_EVENT_TX_BIAS_HIGH_WARN,
                               &fault_app_flags->tx_bias_high_warn);
    }
    if (chgd.tx_bias_low_warn) {
        TRX_LOG_INFO(qsfp, "TX Bias Low Warning: Current [0x%02X] Next "
        "[0x%02X], Changed [0x%02X]", oldf.tx_bias_low_warn,
        newf->tx_bias_low_warn, chgd.tx_bias_low_warn);

        qsfp_lane_fault_report(qsfp, chgd.tx_bias_low_warn,
                               newf->tx_bias_low_warn,
                               QSFP_EVENT_TX_BIAS_LOW_WARN,
                               &fault_app_flags->tx_bias_low_warn);
    }

    if (chgd.rx_cdr_lol) {
        TRX_LOG_INFO(qsfp, "RX CDR LOL: Current [0x%02X] Next [0x%02X], "
        "Changed [0x%02X]", oldf.rx_cdr_lol, newf->rx_cdr_lol,
        chgd.rx_cdr_lol);

        qsfp_lane_fault_report(qsfp, chgd.rx_cdr_lol, newf->rx_cdr_lol,
                          QSFP_EVENT_RX_CDR_LOL, &fault_app_flags->rx_cdr_lol);
    }
    if (chgd.tx_cdr_lol) {
        TRX_LOG_INFO(qsfp, "TX CDR LOL: Current [0x%02X] Next [0x%02X], "
        "Changed [0x%02X]", oldf.tx_cdr_lol, newf->tx_cdr_lol,
        chgd.tx_cdr_lol);

        qsfp_lane_fault_report(qsfp, chgd.tx_cdr_lol, newf->tx_cdr_lol,
                          QSFP_EVENT_TX_CDR_LOL, &fault_app_flags->tx_cdr_lol);
    }
    if (chgd.tx_adap_eq_in_fail) {
        TRX_LOG_INFO(qsfp, "TX Adaptive EQ IN Fail: Current [0x%02X] Next "
        "[0x%02X], Changed [0x%02X]", oldf.tx_adap_eq_in_fail,
        newf->tx_adap_eq_in_fail, chgd.tx_adap_eq_in_fail);

        qsfp_lane_fault_report(qsfp, chgd.tx_adap_eq_in_fail,
                               newf->tx_adap_eq_in_fail,
                               QSFP_EVENT_TX_ADAPTIVE_EQ_IN_FAIL,
                               &fault_app_flags->tx_adap_eq_in_fail);
    }

    oldf.rx_los  = 0;
    oldf.tx_fault = 0;

    for (i = 0 ; i < qsfp->num_lanes ; i++) {
        lanei = qsfp->lane[i];
        if (lanei) {
            if (lanei->status.rx_los) {
                oldf.rx_los |= (1 << i);
            } else {
                oldf.rx_los &= (~(1 << i));
            }
            if (lanei->status.tx_fault) {
                oldf.tx_fault |= (1 << i);
            } else {
                oldf.tx_fault &= (~(1 << i));
            }
        }
    }

    chgd.rx_los = oldf.rx_los ^ newf->rx_los;
    chgd.tx_fault = oldf.tx_fault ^ newf->tx_fault;

    if (chgd.rx_los | chgd.tx_fault) {
        qsfp_lane_rxlos_txf(qsfp, chgd.rx_los, chgd.tx_fault);
    }

    mutex_unlock(&qsfp->sm_mutex);
}

static void qsfp_poll(struct work_struct *work)
{
    struct qsfp *qsfp = container_of(work, struct qsfp, poll.work);
    bool temp = qsfp->need_poll;

    qsfp_check_state(qsfp);

    if (qsfp->need_poll) {
        if (temp == false) {
            TRX_LOG_INFO(qsfp, "Polling started");
        }
        /* Poll once per second */
        mod_delayed_work(system_wq, &qsfp->poll, msecs_to_jiffies(1000));
    } else {
        if (temp == true) {
            TRX_LOG_INFO(qsfp, "Polling stoped");
        }
    }
}

void qsfp_irq(struct qsfp *qsfp)
{
    unsigned long delay;

#if IS_ENABLED(CONFIG_DEBUG_FS)
    /* Added check as interrupt still can come in simulated case
     * it should be ignored
     */
    if (qsfp->sim.remove || qsfp->sim.flags) {
        TRX_LOG_INFO(qsfp, "Transceiver in simulation state. IRQ ignored");
        return;
    }
#endif

    if (qsfp_set_spec_ops(qsfp) < 0) {
        if (qsfp->status.present) {
            /* To make sure event wont get missed */
            TRX_LOG_ERR(qsfp, "Unable to set the spec ops. Process irq "
                              "after 1sec");
            delay = msecs_to_jiffies(1000);
        } else {
            /* During transceiver removal if qsfp interrupt line in falling
             * edge then after removal it will be moved default rising edge.
             * It is expected to come along with removal interrupt but
             * sometimes it comes late so it need to be ignored
             */
            TRX_LOG_ERR(qsfp, "Ignoring interrupt generated after "
                              "transceiver got removed");
            return;
        }
    } else {
        delay = qsfp->spec_ops->irq_delay(qsfp);
    }

    qsfp->need_poll = false;
    mod_delayed_work(system_wq, &qsfp->poll, delay);
}

void qsfp_stop_poll(struct qsfp *qsfp)
{
    if (qsfp->need_poll) {
        qsfp->need_poll = false;
        cancel_delayed_work_sync(&qsfp->poll);
        TRX_LOG_INFO(qsfp, "Polling stoped");
    }
}

void qsfp_module_insert_irq(struct qsfp *qsfp)
{
    char *msg_insert[] = {QSFP_EVENT_INSERT, NULL};
    TRX_LOG_INFO(qsfp, "");

    mutex_lock(&qsfp->sm_mutex);

    qsfp->status.present = 1;
    qsfp_sm_event(qsfp, QSFP_E_INSERT);

    mutex_unlock(&qsfp->sm_mutex);

    kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_insert);
    TRX_LOG_INFO(qsfp, "%s event Reported", msg_insert[0]);

    TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Transceiver inserted", qsfp->port_num);
}

void qsfp_module_remove_irq(struct qsfp *qsfp)
{
    char *msg_remove[] = {QSFP_EVENT_REMOVE, NULL};
    TRX_LOG_INFO(qsfp, "");

    qsfp_stop_poll(qsfp);

    mutex_lock(&qsfp->sm_mutex);

    qsfp->status.present = 0;
    qsfp_sm_event(qsfp, QSFP_E_REMOVE);

    mutex_unlock(&qsfp->sm_mutex);

    kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_remove);
    TRX_LOG_INFO(qsfp, "%s event Reported", msg_remove[0]);

    TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Transceiver removed", qsfp->port_num);
}

void update_runtime_dual_cfg(struct qsfp *qsfp)
{
    struct sff8636_eeprom_id *id = NULL;
    struct cmis_eeprom_id *cmis_id = NULL;
    struct sfp_eeprom_id *sff8472_id = NULL;
    char vendor_pn[QSFP_VENDOR_PN_STR_LEN];
    struct dual_tcvr_entry *entry;
    u8 *spec_id = (u8*)&qsfp->id;

    if(!qsfp)
    {
        TRX_LOG_ERR_NODEV("QSFP is NULL");
        return;
    }

    if((!qsfp->status.present) ||
       (qsfp->sm_mod_state < QSFP_MOD_PROBE))
    {
        TRX_LOG_ERR(qsfp, "module not present or not initialised\n");
        return;
    }

    if(qsfp->sm_link_state == QSFP_S_LINK_UP)
    {
        TRX_LOG_ERR(qsfp, "Link is up\n");
        return;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;
        strlcpy(vendor_pn, sff8472_id->base.vendor_pn, QSFP_VENDOR_PN_STR_LEN-1);
        vendor_pn[QSFP_VENDOR_PN_STR_LEN - 1] = '\0';
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        strlcpy(vendor_pn, id->base.vendor_pn, QSFP_VENDOR_PN_STR_LEN-1);
        vendor_pn[QSFP_VENDOR_PN_STR_LEN - 1] = '\0';
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;
        strlcpy(vendor_pn, cmis_id->base.vendor_pn, QSFP_VENDOR_PN_STR_LEN-1);
        vendor_pn[QSFP_VENDOR_PN_STR_LEN - 1] = '\0';
        break;
    default:
        vendor_pn[0] = '\0';
        return;
    }

    /* Compare vendor PN with the list */
    if (!list_empty(&dual_tcvr_list)) {
        /* check for dual rate tcvr */
        list_for_each_entry(entry, &dual_tcvr_list, list) {
            if ((entry->name != NULL) &&
                (strlen(entry->name) != 0 ) &&
                (strncmp(vendor_pn, entry->name,
                   strlen(entry->name)) == 0)) {
                TRX_LOG_INFO(qsfp, "%s Matches with  %s\n", entry->name,
                                  vendor_pn);
                qsfp->sim.remove = 1;
                qsfp_module_remove_irq(qsfp);
                TRX_LOG_INFO(qsfp, "------ SIMULATED REMOVE due to"
                                          " config change ------");
                qsfp->sim.remove = 0;
                qsfp_module_insert_irq(qsfp);
                TRX_LOG_INFO(qsfp, "------ SIMULATED INSERT due to"
                                          " config change ------");
             }
         }
    } else {
        TRX_LOG_ERR(qsfp, "List is Empty\n");
    }

}

void qsfp_flt_report_timer_callback(struct timer_list *timer) {
    TRX_LOG_INFO_NODEV("qsfp_fault report timer expired\n");
}

/*
 * Allocates QSFP instance
 */
static struct qsfp *qsfp_alloc(struct device *dev)
{
    struct qsfp *qsfp;

    qsfp = kzalloc(sizeof(*qsfp), GFP_KERNEL);
    if (!qsfp) {
        return ERR_PTR(-ENOMEM);
    }

    qsfp->dev = dev;

    mutex_init(&qsfp->sm_mutex);
    INIT_DELAYED_WORK(&qsfp->poll, qsfp_poll);
    INIT_DELAYED_WORK(&qsfp->timeout, qsfp_timeout);

    timer_setup(&qsfp->qsfp_flt_lnkd_timer, qsfp_flt_report_timer_callback, 0);

    /* valid port numbers are 0,1,2,3.
     * FPC_MAX_PORTS signifies invalid port number
     * */
    qsfp->port_num = FPC_MAX_PORTS;

    /* Some SFP modules and also some Linux I2C drivers do not like reads
     * longer than 16 bytes, so read the EEPROM in chunks of 16 bytes at
     * a time.
     */
    qsfp->i2c_block_size = 16;
    qsfp->need_poll = false;
    qsfp->debugfs_dir = NULL;
    qsfp->module_debugfs_dir = NULL;
    qsfp->qsfp_sysfs_dir = NULL;
    qsfp->sensor_sysfs_dir = NULL;
    qsfp->lane_min_speed = 0;
    qsfp->lane_max_speed = 0;
    qsfp->is_adapter = 0;
    memset(&qsfp->param_info, 0, sizeof(qsfp->param_info));
    return qsfp;
}

static void qsfp_cleanup(void *data)
{
    struct qsfp *qsfp = data;

    TRX_LOG_INFO(qsfp, "");

    cancel_delayed_work_sync(&qsfp->poll);
    cancel_delayed_work_sync(&qsfp->timeout);
    del_timer(&qsfp->qsfp_flt_lnkd_timer);

    kfree(qsfp);
}

int qsfp_probe_cleanup(struct qsfp* qsfp)
{
    int i= 0;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    cancel_delayed_work_sync(&qsfp->poll);
    cancel_delayed_work_sync(&qsfp->timeout);

    for (i = 0 ; i < qsfp->num_lanes ; i++) {
        struct lane *lanei = qsfp->lane[i];
        if (lanei->sfp_bus) {
            TRX_LOG_INFO(lanei, "sfp unregister socket");
            sfp_unregister_socket(lanei->sfp_bus);
            lanei->sfp_bus = NULL;
        }
    }

#if IS_ENABLED(CONFIG_DEBUG_FS)
    module_debugfs_exit(qsfp);
    qsfp_debugfs_exit(qsfp);
#endif
    module_sysfs_exit(qsfp);
    qsfp_sysfs_exit(qsfp);
    memset(&qsfp->param_info, 0, sizeof(qsfp->param_info));

    return 0;
}

/*
 * Probe function which processes QSFP device node
 * it reads i2c address of device0 and device1 to be configured
 * handle to parent FPC ,port number and max power
 */
int qsfp_probe(struct platform_device *pdev)
{
    struct device_node *node = pdev->dev.of_node;
    const struct of_device_id *id;
    struct qsfp *qsfp;
    struct lane *lanei;
    int lane_entries;
    u32 phandle = 0;
    u32 temp = 0;
    int ret;
    u8 i;

    qsfp = qsfp_alloc(&pdev->dev);
    if (IS_ERR(qsfp)) {
        TRX_LOG_PDEV_ERR(&pdev, "qsfp_alloc failed");
        return PTR_ERR(qsfp);
    }

    ret = devm_add_action(qsfp->dev, qsfp_cleanup, qsfp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "devm_add_action failed. ret %d", ret);
        qsfp_cleanup(qsfp);
        return ret;
    }

    if (!node) {
        TRX_LOG_ERR(qsfp, "dev node not found");
        return -EINVAL;
    }

    id = of_match_node(fpc_qsfp_of_match, node);
    if (WARN_ON(!id)) {
        TRX_LOG_ERR(qsfp, "Node match id not found");
        return -EINVAL;
    }

    ret = of_property_read_u32(node, "fpc", &phandle);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Unable to read property 'fpc'. ret %d", ret);
        return ret;
    }

    qsfp->fpc = phandle_to_drvdata(phandle);
    if (!qsfp->fpc) {
        TRX_LOG_INFO(qsfp, "Unable to get FPC handler");
        return -EPROBE_DEFER;
    }

    ret = qsfp_i2c_configure(qsfp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "I2C configuration failed. ret %d", ret);
        return ret;
    }

    ret = device_property_read_u32(&pdev->dev, "port-num", &temp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Fail to get 'port-num' property. ret %d", ret);
        return ret;
    }
    qsfp->port_num = temp & 0xFF;

    TRX_LOG_INFO(qsfp, "port_num %u", qsfp->port_num);

    if (qsfp->port_num < 0 || qsfp->port_num >= FPC_MAX_PORTS) {
        TRX_LOG_ERR(qsfp, "port_num %0X is Invalid", qsfp->port_num);
        return -EINVAL;
    }

    ret = device_property_read_u32(&pdev->dev, "maximum-power-milliwatt",
                                   &qsfp->max_power_mW);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Fail to get 'maximum-power-milliwatt' "
                        "property. ret %d", ret);
        return ret;
    }
    TRX_LOG_INFO(qsfp, "Maximum-power-milliwatt %u", qsfp->max_power_mW);

    TRX_LOG_INFO(qsfp, "Host maximum power %u.%uW",
               qsfp->max_power_mW / 1000, (qsfp->max_power_mW / 100) % 10);

    ret = device_property_read_u32(&pdev->dev, "i2c-address-device0",
                                   &temp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Fail to get 'i2c-address-device0' property. "
                        "ret %d", ret);
        return ret;
    }
    qsfp->i2c_address_dev0 = temp & 0xFF;

    TRX_LOG_INFO(qsfp, "I2C-address-device0  0x%02X (0x%02X)",
               qsfp->i2c_address_dev0, qsfp->i2c_address_dev0 >> 1);

    ret = device_property_read_u32(&pdev->dev, "i2c-address-device1",
                                   &temp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Fail to get 'i2c-address-device1' property. "
                        "ret %d", ret);
        return ret;
    }
    qsfp->i2c_address_dev1 = temp & 0xFF;
    TRX_LOG_INFO(qsfp, "I2C-address-device1  0x%02X (0x%02X)",
               qsfp->i2c_address_dev1, qsfp->i2c_address_dev1 >> 1);

    /* Convert 8-bit to 7-bit i2c address
     * In HW spec they mention i2c address in 8-bit form with last bit 0
     * but in actual __i2c_transfer we have to use 7-bit i2c address so
     * it need right shift by one position.
     */
    qsfp->i2c_address_dev0 >>= 1;
    qsfp->i2c_address_dev1 >>= 1;

    qsfp->fpc->qsfp[qsfp->port_num] = qsfp;


    /* Changed position of lane parsing to avoid sfp socket register in case
     * of probe failures during device tree parsing */

    /* get the number of lanes */
    if (!of_get_property(pdev->dev.of_node, "lanes", &lane_entries)) {
        TRX_LOG_ERR(qsfp, "Unable to read lanes");
        return -ENODEV;
    }

    qsfp->num_lanes = lane_entries/(sizeof(u32));

    if (qsfp->num_lanes > MAX_LANES) {
        TRX_LOG_ERR(qsfp, "Number of lanes %u is more than max %u allowed",
                          qsfp->num_lanes, MAX_LANES);
        return -EINVAL;
    }

    TRX_LOG_INFO(qsfp, "Number of Lanes %u", qsfp->num_lanes);

    /* Parse through list of lanes */
    for (i = 0; i < qsfp->num_lanes; i++) {
        ret = of_property_read_u32_index(pdev->dev.of_node, "lanes", i,
                                         &phandle);
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Fail to get phandle for Lane%u", i);
            return -EPROBE_DEFER;
        }

        lanei = phandle_to_drvdata(phandle);
        if (!lanei) {
            TRX_LOG_ERR(qsfp, "Fail to get drvdata from phandle %u for "
                               "Lane%u\n", phandle, i);
            return -EPROBE_DEFER;
        }
        lanei->qsfp = qsfp;
        lanei->lane_num = i;
        lanei->sfp_bus = sfp_register_socket(lanei->dev, (struct sfp*)lanei,
                                        &lane_ops);
        if (!lanei->sfp_bus) {
            TRX_LOG_ERR(qsfp, "Socket register failed");
            return -ENOMEM;
        }
        qsfp->lane[i] = lanei;
        TRX_LOG_INFO(qsfp, "lane: %s index: %u", dev_name(lanei->dev), i);
    }

    qsfp->qsfp_sysfs_dir = &pdev->dev.kobj;

    qsfp_debugfs_init(qsfp);
    qsfp_sysfs_init(qsfp);

    /* During probe if QSFP module already inserted then we are not getting
     * interrupt for same so we are reading Module Present GPIO line to know
     * its presence
     */
    ret = fpc_is_module_present(qsfp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "fpc_is_module_present failed. ret %d", ret);
        ret = -EPROBE_DEFER;
        goto probe_cleanup;
    } else if (ret == QSFP_PRESENT) {
        TRX_LOG_INFO(qsfp, "QSFP present during probe");
        qsfp_module_insert_irq(qsfp);
    } else {
        TRX_LOG_INFO(qsfp, "QSFP Port Empty during bootup");
        TRX_QXDM_LOG_INFO(qsfp, "Port-%u: Empty during bootup", qsfp->port_num);
    }

    ret = fpc_enable_qsfp_interrupt(qsfp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Enable QSFP interrupt failed. ret %d", ret);
        ret =  -EPROBE_DEFER;
        goto probe_cleanup;
    }

    transceiver_led_off(qsfp, QSFP_LED1 | QSFP_LED2);

    /* set driver data once everything is successful */
    platform_set_drvdata(pdev, qsfp);

    TRX_LOG_INFO(qsfp, "Success");

    return 0;
probe_cleanup:
    qsfp_probe_cleanup(qsfp);
    return ret;
}

int qsfp_remove(struct platform_device *pdev)
{
    struct qsfp *qsfp = platform_get_drvdata(pdev);
    struct lane *lanei;
    u8 i;

    TRX_LOG_INFO(qsfp, "");

    transceiver_led_off(qsfp, QSFP_LED1 | QSFP_LED2);

    cancel_delayed_work_sync(&qsfp->poll);
    cancel_delayed_work_sync(&qsfp->timeout);

#if IS_ENABLED(CONFIG_DEBUG_FS)
    qsfp_debugfs_exit(qsfp);
#endif

    /* Manage the cleanup of module sysfs attributes
       During driver remove.
    */
    module_sysfs_exit(qsfp);
    qsfp_sysfs_exit(qsfp);
    memset(&qsfp->param_info, 0, sizeof(qsfp->param_info));

    mutex_lock(&qsfp->sm_mutex);

    for (i = 0 ; i < qsfp->num_lanes ; i++) {
        lanei = qsfp->lane[i];
        if (lanei) {
            lanei->qsfp = NULL;
        }
    }

    if (qsfp->fpc) {
        qsfp->fpc->qsfp[qsfp->port_num] = NULL;
    }

    mutex_unlock(&qsfp->sm_mutex);

    return 0;
}
