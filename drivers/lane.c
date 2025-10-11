// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * http://git.armlinux.org.uk/cgit/linux-arm.git/tree/drivers/
 * net/phy/sfp.c?h=cex7
 *
 */

#include "lane.h"
#include "qsfp.h"

static void lane_sm_link_next(struct lane *lane, u8 state)
{
    lane->sm_link_state = state;
}

static void lane_sm_mod_next(struct lane *lane, u8 state)
{
    lane->sm_mod_state = state;
}

static void lane_sm_link_upstream_linkdown(const struct lane *lane)
{
    rtnl_lock();
    sfp_link_down(lane->sfp_bus);
    rtnl_unlock();
    TRX_LOG_INFO(lane, "sfp_link_down upstream ops called");
}

static int lane_tx_enable(struct lane *lane)
{
    int ret;
    u8 retry = QSFP_I2C_FAIL_RETRY;

    if (!lane->qsfp->support.tx_disable) {
        return 0;
    }

    while (retry--) {
        ret = lane->qsfp->spec_ops->lane_tx_enable(lane);
        if (ret == 0) {
            TRX_LOG_INFO(lane, "%s -> Enable",
                         lane->status.tx_disable ? "Disabled" : "Enabled");
            lane->status.tx_disable = 0;
            break;
        }
    }

    return ret;
}

static int lane_tx_disable(struct lane *lane)
{
    int ret;
    u8 retry = QSFP_I2C_FAIL_RETRY;

    if (!lane->qsfp->support.tx_disable) {
        return 0;
    }

    while (retry--) {
        ret = lane->qsfp->spec_ops->lane_tx_disable(lane);
        if (ret == 0) {
            TRX_LOG_INFO(lane, "%s -> Disable",
                         lane->status.tx_disable ? "Disabled" : "Enabled");
            lane->status.tx_disable = 1;
            break;
        }
    }

    return ret;
}

static void lane_sm_link_linkup(struct lane *lane)
{
    rtnl_lock();
    sfp_link_up(lane->sfp_bus);
    rtnl_unlock();

    TRX_LOG_INFO(lane, "sfp_link_up upstream ops called");

    if (lane->status.eth_linkup) {
        lane_sm_link_next(lane, QSFP_S_LINK_UP);
    } else {
        lane_sm_link_next(lane, QSFP_S_DOWN);
    }
}

static void lane_sm_link_check_rx_los(struct lane *lane)
{
    if (lane->status.rx_los) {
        lane_sm_link_next(lane, QSFP_S_RX_LOS);
    } else {
        lane_sm_link_linkup(lane);
    }
}

/* This state machine tracks the upstream's state */
static void lane_sm_device(struct lane *lane, u32 event)
{
    switch (lane->sm_dev_state) {
    default:
        if (event == QSFP_E_DEV_ATTACH) {
            lane->sm_dev_state = QSFP_DEV_DOWN;
        }

        break;

    case QSFP_DEV_DOWN:
        if (event == QSFP_E_DEV_DETACH) {
            lane->sm_dev_state = QSFP_DEV_DETACHED;
        } else if (event == QSFP_E_DEV_UP) {
            lane->sm_dev_state = QSFP_DEV_UP;
        }

        break;

    case QSFP_DEV_UP:
        if (event == QSFP_E_DEV_DETACH) {
            lane->sm_dev_state = QSFP_DEV_DETACHED;
        } else if (event == QSFP_E_DEV_DOWN) {
            lane->sm_dev_state = QSFP_DEV_DOWN;
        }

        break;
    }
}

static void lane_sm_mod_remove(struct lane *lane)
{
    u8 temp = lane->sm_mod_state;

    /* module state should be moved to empty first before calling sfp_link_down()
     * as it will trigger call to qsfp_trx_get_lane_down_reason_code() from mtip
     * driver which uses module state to get last link down reason.
     */
    lane_sm_mod_next(lane, QSFP_MOD_EMPTY);

    /* Upstream remove no need to be called in case module state in WaitDev */
    if (temp == QSFP_MOD_PRESENT) {
        /* This upstream linkdown can be removed after implementing custom
         * module remove for Ethernet driver as sfp_module_remove() not
         * reaching ethernet driver due to phylink framework
         */
        lane_sm_link_upstream_linkdown(lane);
        rtnl_lock();
        sfp_module_remove(lane->sfp_bus);
        rtnl_unlock();
        TRX_LOG_INFO(lane, "sfp_module_remove upstream ops called");
    }

    memset(&lane->status, 0, sizeof(lane->status));
}

static void lane_sm_mod_insert(struct lane *lane)
{
    int ret;

    rtnl_lock();
    /* Report the module insertion to the upstream device */
    ret = sfp_module_insert(lane->sfp_bus,
                           (const struct sfp_eeprom_id*)&lane->qsfp->id);
    rtnl_unlock();
    if (ret < 0) {
        TRX_LOG_INFO(lane, "Ignore sfp_module_insert upstream"
                           " ops error. ret %d", ret);
    } else {
        TRX_LOG_INFO(lane, "sfp_module_insert upstream ops successful");
    }

    lane_sm_mod_next(lane, QSFP_MOD_PRESENT);
}

/* This state machine tracks the insert/remove state of the module, probes
 * the on-board EEPROM, and sets up the power level.
 */
static void lane_sm_module(struct lane *lane, u32 event)
{
    /* Handle remove event globally, it resets this state machine */
    if (event == QSFP_E_REMOVE) {
        return lane_sm_mod_remove(lane);
    }

    switch (lane->sm_mod_state) {
    case QSFP_MOD_EMPTY:
        if (event == QSFP_E_INSERT) {
            /* Ensure that the device state not detached before proceeding */
            if (lane->sm_dev_state < QSFP_DEV_DOWN) {
                lane_sm_mod_next(lane, QSFP_MOD_WAITDEV);
            } else {
                lane_sm_mod_insert(lane);
            }
        }
        break;

    case QSFP_MOD_WAITDEV:
        if (event == QSFP_E_DEV_ATTACH) {
            lane_sm_mod_insert(lane);
        }

        break;

    case QSFP_MOD_PRESENT:
        if (event == QSFP_E_DEV_DETACH) {
            lane_sm_mod_next(lane, QSFP_MOD_WAITDEV);
        }

        break;

    case QSFP_MOD_ERROR_I2C:
    case QSFP_MOD_ERROR_HPOWER:
    case QSFP_MOD_ERROR_TX_ENABLE_FAIL:
    case QSFP_FPC_I2C_HUNG:
        break;
    }
}

static void lane_sm_link_linkdown(struct lane *lane)
{
    int ret;

    ret = lane_tx_disable(lane);
    if (ret < 0) {
        /* TX disable failure not considered fatal */
        TRX_LOG_ERR(lane, "TX Disable failed. ret %d", ret);
    }

    lane_sm_link_next(lane, QSFP_S_DOWN);
}

void lane_sm_mod_error(struct lane *lane, u8 err)
{
    lane_sm_mod_next(lane, err);
    lane_sm_link_next(lane, QSFP_S_DOWN);

    if (lane->sm_link_state == QSFP_S_LINK_UP) {
        lane_sm_link_upstream_linkdown(lane);
    }

    /* Only TX enable fail error handled at lane level */
    if (lane->sm_mod_state == QSFP_MOD_ERROR_TX_ENABLE_FAIL) {
        char *msg_tx_enable_fail[] = {QSFP_EVENT_TX_ENABLE_FAIL, NULL};
        kobject_uevent_env(&lane->dev->kobj, KOBJ_CHANGE, msg_tx_enable_fail);
        TRX_LOG_INFO(lane, "Fault Report: %s", msg_tx_enable_fail[0]);
    }
}

static void lane_sm_link_check_linkup(struct lane *lane)
{
    int ret;

    ret = lane_tx_enable(lane);
    if (ret < 0) {
        TRX_LOG_ERR(lane, "TX Enable failed. ret %d", ret);
        TRX_QXDM_LOG_ERROR(lane, "Port-%u: Lane-%u: TX enable failed",
                                 lane->qsfp->port_num, lane->lane_num);
        lane_sm_mod_error(lane, QSFP_MOD_ERROR_TX_ENABLE_FAIL);
        return;

    } else if (lane->sm_mod_state == QSFP_MOD_ERROR_TX_ENABLE_FAIL) {
        /* Ifconfig up recovers TX enable fail error */
        char *msg_tx_enable_fail_recovery[] = {QSFP_EVENT_TX_ENABLE_FAIL_RECOVERY, NULL};
        lane->sm_mod_state = QSFP_MOD_PRESENT;
        kobject_uevent_env(&lane->dev->kobj, KOBJ_CHANGE, msg_tx_enable_fail_recovery);
        TRX_LOG_INFO(lane, "Fault Report Recovery: %s", msg_tx_enable_fail_recovery[0]);
    }

    if (lane->status.tx_fault) {
        lane_sm_link_next(lane, QSFP_S_TX_FAULT);
    } else {
        lane_sm_link_check_rx_los(lane);
    }
}

static void lane_sm_link(struct lane *lane, u32 event)
{
    /* The main state machine */
    switch (lane->sm_link_state) {
    case QSFP_S_DOWN:
        if (event == QSFP_E_INSERT) {
            /* if device is ifconfig up then only try to make it up */
            if (lane->sm_dev_state == QSFP_DEV_UP) {
                lane_sm_link_check_linkup(lane);
            }
        } else if (event == QSFP_E_DEV_UP) {
            /* if module present then only try to make it up */
            if ((lane->sm_mod_state == QSFP_MOD_PRESENT) ||
                (lane->sm_mod_state == QSFP_MOD_ERROR_TX_ENABLE_FAIL)) {
                lane_sm_link_check_linkup(lane);
            }
        } else if (event == QSFP_E_TX_FAULT) {
            /* if device is ifconfig up then only send
             * link down and change lane link state */
            if (lane->sm_dev_state == QSFP_DEV_UP) {
                lane_sm_link_upstream_linkdown(lane);
                lane_sm_link_next(lane, QSFP_S_TX_FAULT);
           }
        } else if (event == QSFP_E_RX_LOS) {
            /* if device is ifconfig up then only send
             * link down and change lane link state */
            if (lane->sm_dev_state == QSFP_DEV_UP) {
                lane_sm_link_upstream_linkdown(lane);
                lane_sm_link_next(lane, QSFP_S_RX_LOS);
            }
        } else if (event == QSFP_E_ETH_UP) {
            lane_sm_link_next(lane, QSFP_S_LINK_UP);
        } else if ((event == QSFP_E_DEV_DOWN) ||
                   (event == QSFP_E_DEV_DETACH)) {
            /* Handle tx disable in case of link down state */
            lane_sm_link_linkdown(lane);
        }

        break;

    case QSFP_S_RX_LOS:
        if (event == QSFP_E_TX_FAULT) {
            lane_sm_link_next(lane, QSFP_S_TX_FAULT);
        } else if ((event == QSFP_E_RX_LOS_RECOVERY) ||
                   (event == QSFP_E_ETH_UP)) {
            lane_sm_link_linkup(lane);
        } else if (event == QSFP_E_REMOVE) {
            lane_sm_link_next(lane, QSFP_S_DOWN);
        } else if ((event == QSFP_E_DEV_DOWN) ||
                   (event == QSFP_E_DEV_DETACH)) {
            lane_sm_link_linkdown(lane);
        }

        break;

    case QSFP_S_TX_FAULT:
        if ((event == QSFP_E_TX_FAULT_RECOVERY) ||
            (event == QSFP_E_ETH_UP)) {
            lane_sm_link_check_rx_los(lane);
        } else if (event == QSFP_E_REMOVE) {
            lane_sm_link_next(lane, QSFP_S_DOWN);
        } else if ((event == QSFP_E_DEV_DOWN) ||
                   (event == QSFP_E_DEV_DETACH)) {
            lane_sm_link_linkdown(lane);
        }

        break;

    case QSFP_S_LINK_UP:
        if (event == QSFP_E_TX_FAULT) {
            lane_sm_link_upstream_linkdown(lane);
            lane_sm_link_next(lane, QSFP_S_TX_FAULT);
        } else if (event == QSFP_E_RX_LOS) {
            lane_sm_link_upstream_linkdown(lane);
            lane_sm_link_next(lane, QSFP_S_RX_LOS);
        } else if (event == QSFP_E_REMOVE) {
            lane_sm_link_next(lane, QSFP_S_DOWN);
        } else if ((event == QSFP_E_DEV_DOWN) ||
                   (event == QSFP_E_DEV_DETACH)) {
            /* calling lane_sm_link_upstream_linkdown() is not needed here as
             * dev down is internal event
             */
            lane_sm_link_linkdown(lane);
        } else if (event == QSFP_E_ETH_DOWN) {
            lane_sm_link_next(lane, QSFP_S_DOWN);
        }
        break;
    }
}

void lane_sm_event(struct lane *lane, u32 event)
{
    TRX_LOG_INFO(lane, "Enter [%s:%s:%s]   Event: %s",
                       mod_state_to_str(lane->sm_mod_state),
                       dev_state_to_str(lane->sm_dev_state),
                       link_state_to_str(lane->sm_link_state),
                       event_to_str(event));

    lane_sm_device(lane, event);
    lane_sm_module(lane, event);
    lane_sm_link(lane, event);

    TRX_LOG_INFO(lane, "Exit  [%s:%s:%s]",
                       mod_state_to_str(lane->sm_mod_state),
                       dev_state_to_str(lane->sm_dev_state),
                       link_state_to_str(lane->sm_link_state));

}

static void lane_attach(struct sfp *sfp)
{
    struct lane *lane = (struct lane*)sfp;
    struct qsfp *qsfp = lane->qsfp;

    if (!qsfp) {
        TRX_LOG_ERR(lane, "qsfp is NULL");
        return;
    }

    /* rtnl lock already taken by the caller, it is released as transceiver
     * operation may take longer time and order of locking rtnl and sm_mutex
     * should follow the order of 1st sm_mutex 2nd rtnl. Attach event may call
     * upstream ops sfp_module_insert() and rtnl lock is must before calling
     * upstream ops. rtnl released here and taken before sfp_module_insert()
     * and released immediately.
     */
    rtnl_unlock();
    mutex_lock(&qsfp->sm_mutex);

    lane_sm_event(lane, QSFP_E_DEV_ATTACH);

    qsfp_attach(lane);

    mutex_unlock(&qsfp->sm_mutex);
    rtnl_lock();
}

static void lane_detach(struct sfp *sfp)
{
    struct lane *lane = (struct lane*)sfp;
    struct qsfp *qsfp = lane->qsfp;

    if (!qsfp) {
        TRX_LOG_ERR(lane, "qsfp is NULL");
        return;
    }

    /* rtnl lock already taken by the caller, it is released as transceiver
     * operation may take longer time and order of locking rtnl and sm_mutex
     * should follow the order of 1st sm_mutex 2nd rtnl.
     */
    rtnl_unlock();
    mutex_lock(&qsfp->sm_mutex);

    lane_sm_event(lane, QSFP_E_DEV_DETACH);

    qsfp_detach(lane);

    mutex_unlock(&qsfp->sm_mutex);
    rtnl_lock();
}

/* Called during ifconfig up */
void lane_start(struct lane *lane)
{
    struct qsfp *qsfp = lane->qsfp;

    if (!qsfp) {
        TRX_LOG_ERR(lane, "qsfp is NULL");
        return;
    }

    lane_sm_event(lane, QSFP_E_DEV_UP);

    qsfp_start(lane);
}

/* Called during ifconfig down */
void lane_stop(struct lane *lane)
{
    struct qsfp *qsfp = lane->qsfp;

    if (!qsfp) {
        TRX_LOG_ERR(lane, "qsfp is NULL");
        return;
    }

    lane_sm_event(lane, QSFP_E_DEV_DOWN);

    qsfp_stop(lane);
}

static void lane_dummy_start(struct sfp *sfp)
{
    /* Called lane_start using qsfp_trx_ifconfig_notifier API */
}

static void lane_dummy_stop(struct sfp *sfp)
{
    /* Called lane_stop using qsfp_trx_ifconfig_notifier API */
}

const struct sfp_socket_ops lane_ops = {
    .attach = lane_attach,
    .detach = lane_detach,
    .start = lane_dummy_start,
    .stop = lane_dummy_stop,
    .module_info = qsfp_module_info,
    .module_eeprom = qsfp_module_eeprom,
    .module_eeprom_by_page = qsfp_module_eeprom_by_page,
};

static struct lane *lane_alloc(struct device *dev)
{
    struct lane *lane;

    lane = kzalloc(sizeof(*lane), GFP_KERNEL);
    if (!lane) {
        return ERR_PTR(-ENOMEM);
    }

    lane->dev = dev;

    return lane;
}

static void lane_cleanup(void *data)
{
    struct lane *lane = data;

    TRX_LOG_INFO(lane, "");

    kfree(lane);
}

int lane_probe(struct platform_device *pdev)
{
    struct device_node *node = pdev->dev.of_node;
    const struct of_device_id *id;
    struct lane *lane;
    int ret;

    lane = lane_alloc(&pdev->dev);
    if (IS_ERR(lane)) {
        TRX_LOG_PDEV_ERR(&pdev, "lane_alloc failed");
        return PTR_ERR(lane);
    }

    ret = devm_add_action(lane->dev, lane_cleanup, lane);
    if (ret < 0) {
        TRX_LOG_ERR(lane, "devm_add_action failed. ret %d", ret);
        lane_cleanup(lane);
        return ret;
    }

    id = of_match_node(fpc_qsfp_of_match, node);
    if (WARN_ON(!id)) {
        TRX_LOG_ERR(lane, "Node match id not found");
        return -EINVAL;
    }

    platform_set_drvdata(pdev, lane);

    TRX_LOG_INFO(lane, "Success");

    return 0;
}

int lane_remove(struct platform_device *pdev)
{
    struct lane *lane = platform_get_drvdata(pdev);
    struct qsfp *qsfp = lane->qsfp;

    TRX_LOG_INFO(lane, "");

    if (!qsfp) {
        TRX_LOG_INFO(lane, "qsfp is NULL");

        rtnl_lock();
        lane->status.present = 0;
        lane_sm_event(lane, QSFP_E_REMOVE);
        rtnl_unlock();

        if (lane->sfp_bus) {
            sfp_unregister_socket(lane->sfp_bus);
        }

        return 0;
    }

    mutex_lock(&qsfp->sm_mutex);
    rtnl_lock();

    lane->status.present = 0;
    lane_sm_event(lane, QSFP_E_REMOVE);
    qsfp->lane[lane->lane_num] = NULL;

    rtnl_unlock();
    mutex_unlock(&qsfp->sm_mutex);

    if (lane->sfp_bus) {
        sfp_unregister_socket(lane->sfp_bus);
    }

    return 0;
}
