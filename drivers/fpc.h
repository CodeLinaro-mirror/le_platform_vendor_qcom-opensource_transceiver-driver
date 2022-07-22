/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef LINUX_FPC_H
#define LINUX_FPC_H

#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif

/* FPC Device tree example
 *
 * fpc402_0: fpc402_0 {
 *    compatible = "sff,fpc402";
 *    instance-num = <0>;
 *    i2c-bus = <&qupv3_se9_i2c>;
 *    i2c-address = <0x04>;
 *    interrupt-gpio = <&tlmm 86 0>;
 * };
 *
 */

#define FPC_COMPATIBLE "sff,fpc402"

#define FPC_MAX_PORTS (4)

#define FPC_MAX_INSTANCES (2)

#define FPC_DEFAULT_I2C_ADDRESS (0x1E)

#define FPC_RESET_REGISTER (0x0)
#define FPC_I2C_DEVICE_ID_REGISTER  (0x1)
#define FPC_INTERRUPT_STATUS_REGISTER (0x6)
#define FPC_IN_B_STATUS_REGISTER (0x7)
#define FPC_I2C_SCL_STUCK_INTERRUPT_REGISTER (0x9B)
#define FPC_I2C_SDA_STUCK_INTERRUPT_REGISTER (0x9C)

#define FPC_RESET_SEQUENCE (BIT(7)|BIT(3)|BIT(2)|BIT(1)|BIT(0))
#define FPC_ENABLE_I2C_STUCK_INTERRUPT (0xF)
#define FPC_I2C_STUCK_STATUS_MASK (0xF0)

/* Enable interrupt for QSFP input lines
 * input A (interrupt) on both rising and falling edge
 * input B (module present) on both rising and falling edge
 * input C not used, 110011
 */
#define FPC_ENABLE_INPUT_A_B_INTERRUPT (BIT(5)|BIT(4)|BIT(1)|BIT(0))
#define FPC_IN_A_INT_RISING_EDGE_MASK (BIT(0))
#define FPC_IN_A_INT_FALLING_EDGE_MASK (BIT(1))
#define FPC_IN_B_MOD_PRESENT_RISING_EDGE_MASK (BIT(4))
#define FPC_IN_B_MOD_PRESENT_FALLING_EDGE_MASK (BIT(5))

#define FPC_LED1_ON (BIT(0))
#define FPC_LED2_ON (BIT(2))
#define FPC_QSFP_LED1_ON  (BIT(0))
#define FPC_QSFP_LED1_OFF (~FPC_QSFP_LED1_ON)
#define FPC_QSFP_LED2_ON  (BIT(2))
#define FPC_QSFP_LED2_OFF (~FPC_QSFP_LED2_ON)

#define FPC_OUT_A_B_ENABLE_REGISTER (0x08)
#define FPC_OUT_A_B_VALUE (0x0A)
#define FPC_QSFP_RESET_SEQUENCE (0x00)
#define FPC_OUT_A_DISABLE (0x00)
#define FPC_OUT_A_ENABLE (0x0F)

#define FPC_PREFETCH_GATE (0x0B)
#define FPC_PREFETCH_START (BIT(1))
#define FPC_PREFETCH_STOP (BIT(2))
#define FPC_PREFETCH_PERIOID_STEP (5)

enum {
    FPC_LED_MODE_SELECT,
    FPC_INPUT_PIN_INTERRUPT_ENABLE,
    FPC_INPUT_PIN_INTERRUPT_STATUS,
    FPC_PREFETCH_CONTROL,
    FPC_PREFETCH_OFFSET,
    FPC_PREFETCH_PERIOD,
};

enum {
   QSFP_PRESENT,
   QSFP_NOT_PRESENT,
};

struct fpc {
    struct qsfp *qsfp[FPC_MAX_PORTS];
    struct i2c_adapter *i2c;
    struct gpio_desc *interrupt_gpio;
    struct device *dev;
    int gpio_irq;
    u8 i2c_address;
    u8 instance_num;

#if IS_ENABLED(CONFIG_DEBUG_FS)
    struct dentry *debugfs_dir;
#endif
};

extern int qsfp_probe(struct platform_device *pdev);
extern int qsfp_remove(struct platform_device *pdev);
extern void qsfp_shutdown(struct platform_device *pdev);
extern void qsfp_check_state(struct qsfp *qsfp);
extern void qsfp_module_insert_irq(struct qsfp *qsfp);
extern void qsfp_module_remove_irq(struct qsfp *qsfp);
extern void qsfp_falling_edge_irq(struct qsfp *qsfp);
extern void qsfp_rising_edge_irq(struct qsfp *qsfp);

#endif
