/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef FPC_LED_H
#define FPC_LED_H

#include "fpc.h"

#define FPC_PORT_LED1_SEL_BLINK (BIT(1)|BIT(0))
#define FPC_PORT_LED2_SEL_BLINK (BIT(3)|BIT(2))

/****** ERROR Conditions ***************/
#define SUCCESS                             0
#define ERROR_FPC_PROBE                    -1
#define ERROR_FPC_I2C_READ                 -2
#define ERROR_FPC_I2C_WRITE                -3
#define ERROR_QSFP_MODULE_PRESENCE         -4
#define ERROR_INVALID_PWM_REG_INPUT        -5
#define ERROR_INVALID_LED_NUM_INPUT        -6
#define ERROR_INVALID_FPC_INSTANCE_INPUT   -7
#define ERROR_INVALID_QSFP_PORT_NUM_INPUT  -8
#define ERROR_INVALID_BLINK_INPUT          -9

static const u8 FPC_PORT_LED_REG[][FPC_MAX_PORTS] = {
    /* FPC_LED1_PWM_CTRL_REG */
    {0x14 , 0x34 , 0x54 , 0x74},
    /* FPC_LED2_PWM_CTRL_REG */
    {0x15 , 0x35 , 0x55 , 0x75},
    /* FPC_LED1_BLINK_ON_TIME_REG */
    {0x16 , 0x36 , 0x56 , 0x76},
    /* FPC_LED1_BLINK_OFF_TIME_REG */
    {0x17 , 0x37 , 0x57 , 0x77},
    /* FPC_LED2_BLINK_ON_TIME_REG */
    {0x18 , 0x38 , 0x58 , 0x78},
    /* FPC_LED2_BLINK_OFF_TIME_REG */
    {0x19 , 0x39 , 0x59 , 0x79},
};

enum {
    FPC_LED1_PWM_CTRL_REG,
    FPC_LED2_PWM_CTRL_REG,
    FPC_LED1_BLINK_ON_TIME_REG,
    FPC_LED1_BLINK_OFF_TIME_REG,
    FPC_LED2_BLINK_ON_TIME_REG,
    FPC_LED2_BLINK_OFF_TIME_REG,
};

enum {
    LED_MODE_OFF = 0,
    LED_MODE_ON,
    LED_MODE_PWM,
    LED_MODE_BLINK,
};

/***************************************************************
*
* Function:       transceiver_led_on
* Description:    Function to set fpc port LED ON.
* Inputs:         const struct qsfp *: QSFP structure pointer.
*                 led_num: selects between LED1 and LED2, where QSFP_LED1
*                          is OUT_C (LED1) and QSFP_LED2 is OUT_D (LED2).
*
****************************************************************/
int transceiver_led_on (const struct qsfp *qsfp, u8 led_num);

/***************************************************************
*
* Function:       transceiver_led_off
* Description:    Function to set fpc port LED OFF.
* Inputs:         const struct qsfp *: QSFP structure pointer.
*                 led_num: selects between LED1 and LED2, where QSFP_LED1
*                          is OUT_C (LED1) and QSFP_LED2 is OUT_D (LED2).
*
****************************************************************/
int transceiver_led_off(const struct qsfp *qsfp, u8 led_num);

/***************************************************************
*
* Function:       transceiver_led_brightness_set
* Description:    Function to set fpc port LED brightness based on user input.
* Inputs:         struct qsfp *: QSFP structure pointer.
*                 led_num: selects between LED1 and LED2, where QSFP_LED1
*                          is OUT_C (LED1) and QSFP_LED2 is OUT_D (LED2).
*                 pwm_val: value of brightness where 254 is brightest.
*
****************************************************************/
int transceiver_led_brightness_set(struct qsfp *qsfp,
                                   u8 led_num, u8 pwm_val);

/***************************************************************
*
* Function:       transceiver_led_blink_set
* Description:    Function to set fpc port LED Blink on/off/on brightness based
*                 on user input.
* Inputs:         struct qsfp *: QSFP structure pointer.
*                 led_num: selects between LED1 and LED2, where QSFP_LED1
*                          is OUT_C (LED1) and QSFP_LED2 is OUT_D (LED2).
*                 on_val: LED on time, range from 1 to 255 where as 1 is 2.5 ms
*                 off_val: LED off time, range from 1 to 255 where as 1
*                          is 2.5 ms
*                 pwm_val: value of brightness where 255 is the brightest.
*
****************************************************************/
int transceiver_led_blink_set(struct qsfp *qsfp,
                              u8 led_num, u8 on_val,
                              u8 off_val, u8 pwm_val);

int led_calculate_pwm_value(struct qsfp *qsfp, u8* pwm_reg, bool pwm_mode);
int mod_link_codes_to_speed(unsigned short mod_link_codes,
                            u16* max_data_speed);
extern int fpc_read(const struct fpc *, u8 , void *, size_t );
extern int fpc_write(const struct fpc *, u8, void *, size_t);

extern const u8 FPC_PORT_REG[][FPC_MAX_PORTS];

#endif
