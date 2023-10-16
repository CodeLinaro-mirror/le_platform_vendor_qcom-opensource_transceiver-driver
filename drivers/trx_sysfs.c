/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "trx_sysfs.h"
#include "fpc_led.h"
#include "lane.h"

#define MAX_SYSFS_TRX_FILE_LENGTH (1024)

/* Function to export device state information like
 * port number, link status, and module presence.
 */
static ssize_t trx_state_info_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp = dev_get_drvdata(dev);
    struct lane *lanei;
    ssize_t ret;
    u8 i;

    /* Locking necessary to make sure all states fetched once */
    mutex_lock(&qsfp->sm_mutex);

    ret = scnprintf(buf, PAGE_SIZE, "State description: [Module state , "
    "Upstream Device state  , Link state]\nTransceiver port-%u State: [%s  %s"
    "  %s]\n", qsfp->port_num,  mod_state_to_str(qsfp->sm_mod_state),
    dev_state_to_str(qsfp->sm_dev_state),
    link_state_to_str(qsfp->sm_link_state));

    if (qsfp->status.present) {
        char feature_str[QSFP_FEATURE_STR_MAX] = {0};
        qsfp_fill_features_str(qsfp, feature_str, sizeof(feature_str));
        ret += scnprintf(buf + ret, PAGE_SIZE - ret, "Module present: Yes\n"
               "Module probe attempts: %d\nRX LOS: %d\nTX Fault: %d\nPoll status:"
               " %s\nFeatures: %s\n", PROBE_RETRY - qsfp->sm_mod_tries,
               qsfp->status.rx_los,
               qsfp->status.tx_fault,
               qsfp->need_poll ? "Yes" : "No",
               feature_str);

    } else {
        ret += scnprintf(buf + ret, PAGE_SIZE - ret, "Module present: No\n");
    }

    for (i = 0 ; i < qsfp->num_lanes; i++) {
        lanei = qsfp->lane[i];

        ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\nLane%u State: [%s  %s"
               "  %s]\n", i, mod_state_to_str(lanei->sm_mod_state),
               dev_state_to_str(lanei->sm_dev_state),
               link_state_to_str(lanei->sm_link_state));

        if (lanei->status.present) {
            ret += scnprintf(buf + ret, PAGE_SIZE - ret, "Lane present: Yes\n"
                   "RX LOS: %d\nTX Fault: %d\nTX Disable: %d\n",
                   lanei->status.rx_los,
                   lanei->status.tx_fault,
                   lanei->status.tx_disable);

        } else {
            ret += scnprintf(buf + ret, PAGE_SIZE - ret, "Lane present: No\n");
        }
    }

#if IS_ENABLED(CONFIG_DEBUG_FS)
    if (qsfp->sim.remove) {
        ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\nSimulation Remove: "
                                                     "Yes\n");
    }

    if (qsfp->sim.flags) {
        ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\nSimulation Flags: "
                                                     "Yes\n");
    }
#endif

    mutex_unlock(&qsfp->sm_mutex);

    return ret;
}

/* Function to show LED ON/OFF status from the FPC402 mode select register
 * through SysFS.
 */
static ssize_t trx_led_on_off_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    unsigned int led1_state = 0;
    unsigned int led2_state = 0;
    u8 lbuff = 0;
    int ret = 0;
    u8 modesel = 0;

    ret = fpc_read(qsfp->fpc,
                   FPC_PORT_REG[FPC_LED_MODE_SELECT][qsfp->port_num],
                   &lbuff, sizeof(lbuff));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Fail to read LED mode set register\n");
        return -EINVAL;
    }

    modesel = lbuff & 0x03;
    /* To check LED1 state */
    switch(modesel) {
    case 0x00:
    default:
        led1_state = LED_MODE_OFF;
        break;
    case 0x01:
    case 0x02:
    case 0x03:
        led1_state = LED_MODE_ON;
        break;
    }

    modesel = (lbuff & 0x0C) >> 2;
    /* To check LED2 state */
    switch(modesel) {
    case 0x00:
    default:
        led2_state = LED_MODE_OFF;
        break;
    case 0x01:
    case 0x02:
    case 0x03:
        led2_state = LED_MODE_ON;
        break;
    }

    return scnprintf(buf, MAX_SYSFS_TRX_FILE_LENGTH,"#\n# Specifies port leds on,"
        "off status\n# Led number : LED1:1 LED2:2 both LED1&LED2:3 Any other"
        " value is invalid\n# LED state  : off:0 on:1 Default:0 Any other value"
        " is invalid\n#\nLED1 state: %d\nLED2 state: %d\n",
        led1_state , led2_state);
}

/* Wrapper function to validate the data size from the SysFS file.
 */
int trx_sysfs_validate_and_copy_buf(char *dest_buf,
                      size_t dest_buf_size, char const *source_buf,
                      size_t source_buf_size)
{
    if (source_buf_size > (dest_buf_size - 1)) {
        return -EINVAL;
    }

    strlcpy(dest_buf, source_buf, dest_buf_size);
    if (dest_buf[source_buf_size - 1] == '\n')
        dest_buf[source_buf_size - 1] = '\0';
    return 0;
}

/* Function to store the LED number and state information through
 * SysFS for LED ON/OFF.
 */
static ssize_t trx_led_on_off_store(struct device *dev,
                                   struct device_attribute *attr,
                                   char const *buf, size_t count)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    char buf_local[MAX_SYSFS_TRX_FILE_LENGTH] = {0};
    char led_num_buff[10] = {0};
    char led_state_buff[10] = {0};
    int led_num = 0;
    int led_state = 0;
    int ret = 0;
    ret = trx_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
                     buf, count);
    if (ret != 0) {
        TRX_LOG_ERR(qsfp, "SysFS file length is larger than %zu bytes\n",
                                                    sizeof(buf_local));
        return ret;
    }

    sscanf(buf_local,"%s %d %s %d", led_num_buff, &led_num,
                               led_state_buff, &led_state);

    if ((strncmp(led_num_buff,"led_num:",8) != 0) ||
         (strncmp(led_state_buff,"led_state:",10) != 0))
    {
        TRX_LOG_ERR(qsfp, "Invalid data read from file\n");
        return -EINVAL;
    }

    /* Validate input values from the user. */
    if(((led_num < 1) || (led_num > 3)) ||
        ((led_state < LED_MODE_OFF) ||
        (led_state > LED_MODE_ON)))
    {
        TRX_LOG_ERR(qsfp, "Invalid input data led_num: %d,"
                    "led_state: %d\n", led_num, led_state);
        return -EINVAL;
    }

    if(led_state == LED_MODE_ON)
        ret = transceiver_led_on(qsfp, (u8)led_num);
    else
        ret  = transceiver_led_off(qsfp, (u8)led_num);

    if(ret != 0)
        return -EINVAL;
    else
        return count;
}

/* Function to show LED brightness value from the FPC402 register
 * through SysFS.
 */
static ssize_t trx_led_blink_set_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    u8 led1_state = 0;
    u8 led2_state = 0;
    u8 led1_on_time = 0;
    u8 led1_off_time = 0;
    u8 led2_on_time = 0;
    u8 led2_off_time = 0;

    char led1_str[100] = {0};
    char led2_str[100] = {0};

    u8 lbuff = 0;
    int ret = 0;
    u8 modesel = 0;

    ret = fpc_read(qsfp->fpc,
                   FPC_PORT_REG[FPC_LED_MODE_SELECT][qsfp->port_num],
                   &lbuff, sizeof(lbuff));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Fail to read LED mode set register\n");
        return -EINVAL;
    }

    modesel = lbuff & 0x03;
    /* To check LED1 state */
    switch(modesel) {
    case 0x00:
    default:
        led1_state = LED_MODE_OFF;
        scnprintf(led1_str, 100, "OFF");
        break;
    case 0x01:
    case 0x02:
        led1_state = LED_MODE_ON;
        scnprintf(led1_str, 100, "ON with no blink");
        break;
    case 0x03:
        led1_state = LED_MODE_BLINK;
        break;
    }

    if(led1_state == LED_MODE_BLINK) {
        ret = fpc_read(qsfp->fpc,
                 FPC_PORT_LED_REG[FPC_LED1_BLINK_ON_TIME_REG][qsfp->port_num],
                 &led1_on_time, sizeof(led1_on_time));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Fail to read LED1 blink on time reg. ret %d\n",
                              ret);
            return -EINVAL;
        }

        ret = fpc_read(qsfp->fpc,
              FPC_PORT_LED_REG[FPC_LED1_BLINK_OFF_TIME_REG][qsfp->port_num],
              &led1_off_time, sizeof(led1_off_time));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Fail to read LED1 blink off time reg. ret %d\n",
                               ret);
            return -EINVAL;
        }

    scnprintf(led1_str, 100, "BLINK with ON_VAL: %d, OFF_VAL: %d",
                             led1_on_time, led1_off_time);
    }

    modesel = (lbuff & 0x0C) >> 2;
    /* To check LED2 state */
    switch(modesel) {
    case 0x00:
    default:
        led2_state = LED_MODE_OFF;
        scnprintf(led2_str, 100, "OFF");
        break;
    case 0x01:
    case 0x02:
        led2_state = LED_MODE_ON;
        scnprintf(led2_str, 100, "ON with no blink");
        break;
     case 0x03:
         led2_state = LED_MODE_BLINK;
         break;
     }

    if(led2_state == LED_MODE_BLINK) {
        ret = fpc_read(qsfp->fpc,
                 FPC_PORT_LED_REG[FPC_LED2_BLINK_ON_TIME_REG][qsfp->port_num],
                 &led2_on_time, sizeof(led2_on_time));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Fail to read LED2 blink on time reg. ret %d\n",
                              ret);
            return -EINVAL;
        }

        ret = fpc_read(qsfp->fpc,
               FPC_PORT_LED_REG[FPC_LED2_BLINK_OFF_TIME_REG][qsfp->port_num],
               &led2_off_time, sizeof(led2_off_time));
        if (ret < 0) {
           TRX_LOG_ERR(qsfp, "Fail to read LED2 blink off time reg. ret %d\n",
                             ret);
           return -EINVAL;
        }

    scnprintf(led2_str, 100, "BLINK with ON_VAL: %d, OFF_VAL: %d",
                                  led2_on_time, led2_off_time);
    }

    return scnprintf(buf, MAX_SYSFS_TRX_FILE_LENGTH,"#\n# Specifies port leds "
           "Blink mode on and off value information\n# LED blink on value: "
           "Minimum:1 Maximum:255 Any other value is invalid\n# LED blink "
           "off value: Minimum:1 Maximum:255 Any other value is "
           "invalid\n#\nLED1 %s\nLED2 %s\n", led1_str , led2_str);
 }

/* Function to store the LED number and on, off, and brightness information
 * through SysFS to set the LED to blink mode.
 */
static ssize_t trx_led_blink_set_store(struct device *dev,
                                   struct device_attribute *attr,
                                   char const *buf, size_t count)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    char buf_local[MAX_SYSFS_TRX_FILE_LENGTH] = {0};

    int led_num = 0;
    char led_num_buff[10] = {0};
    int led_on_val = 0;
    char led_on_val_buff[10] = {0};

    int led_off_val = 0;
    char led_off_val_buff[10] = {0};

    int led_pwm_val = 0;
    char led_pwm_val_buff[10] = {0};

    int ret = 0;
    ret = trx_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
                     buf, count);
    if (ret != 0) {
        TRX_LOG_ERR(qsfp, "SysFS file length is larger than %zu bytes\n",
                                                    sizeof(buf_local));
        return ret;
    }

    sscanf(buf_local,"%s %d %s %d %s %d %s %d", led_num_buff, &led_num,
                        led_on_val_buff, &led_on_val, led_off_val_buff,
                         &led_off_val, led_pwm_val_buff, &led_pwm_val);

    if ((strncmp(led_num_buff,"led_num:",8) != 0) ||
         (strncmp(led_on_val_buff,"led_on_val:",11) != 0) ||
         (strncmp(led_off_val_buff,"led_off_val:",12) != 0) ||
         (strncmp(led_pwm_val_buff,"led_pwm_val:",12) != 0))
    {
        TRX_LOG_ERR(qsfp, "Invalid data read from file\n");
        return -EINVAL;
    }

    /* Validate input values from the user. */
    if(((led_num < 1) || (led_num > 3)) ||
        ((led_pwm_val < 0) || (led_pwm_val > 255)) ||
        ((led_off_val < 1) || (led_off_val > 255)) ||
        ((led_on_val < 1) || (led_on_val > 255)))
    {
        TRX_LOG_ERR(qsfp, "Invalid input data led_num: %d,"
                    "led_pwm_val: %d, led_off_val: %d, led_on_val: %d\n",
                    led_num, led_pwm_val, led_off_val, led_on_val);
        return -EINVAL;
    }

    ret = transceiver_led_blink_set(qsfp, (u8)led_num, (u8)led_on_val,
                                    (u8)led_off_val, (u8)led_pwm_val);
    if(ret != 0)
        return -EINVAL;
    else
        return count;
}

/* Function to show LED blink on and blink off value from the FPC402 register
 * through SysFS.
 */
static ssize_t trx_led_brightness_set_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    unsigned int led1_state = 0;
    u8 led1_pwm_val = 0;
    unsigned int led2_state = 0;
    u8 led2_pwm_val = 0;
    char led1_str[50] = {0};
    char led2_str[50] = {0};

    u8 lbuff = 0;
    int ret = 0;
    u8 modesel = 0;

    ret = fpc_read(qsfp->fpc,
                   FPC_PORT_REG[FPC_LED_MODE_SELECT][qsfp->port_num],
                   &lbuff, sizeof(lbuff));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Fail to read LED mode set register\n");
        return -EINVAL;
    }

    modesel = lbuff & 0x03;
    /* To check LED1 state */
    switch(modesel) {
    case 0x00:
    default:
        led1_state = LED_MODE_OFF;
        scnprintf(led1_str, 50, "OFF");
        break;
    case 0x01:
        led1_state = LED_MODE_ON;
        scnprintf(led1_str, 50, "ON with constant, no PWM");
        break;
    case 0x02:
        led1_state = LED_MODE_PWM;
        break;
    case 0x03:
        led1_state = LED_MODE_BLINK;
        break;
    }

    if((led1_state == LED_MODE_PWM) ||
        (led1_state == LED_MODE_BLINK)) {
        ret = fpc_read(qsfp->fpc,
                FPC_PORT_LED_REG[FPC_LED1_PWM_CTRL_REG][qsfp->port_num],
                &led1_pwm_val, sizeof(led1_pwm_val));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Fail to read LED1 pwm ctrl reg. ret %d\n", ret);
            return -EINVAL;
        }

        if(led1_state == LED_MODE_PWM)
            scnprintf(led1_str, 50, "ON with PWM: %d", led1_pwm_val);
        else
            scnprintf(led1_str, 50, "BLINK with PWM: %d", led1_pwm_val);
    }

    modesel = (lbuff & 0x0C) >> 2;
    /* To check LED2 state */
    switch(modesel) {
    case 0x00:
    default:
        led2_state = LED_MODE_OFF;
        scnprintf(led2_str, 50, "OFF");
        break;
    case 0x01:
        led2_state = LED_MODE_ON;
        scnprintf(led2_str, 50, "ON with constant, no PWM");
        break;
    case 0x02:
        led2_state = LED_MODE_PWM;
        break;
    case 0x03:
        led2_state = LED_MODE_BLINK;
        break;
    }

    if( (led2_state == LED_MODE_PWM) ||
        (led2_state == LED_MODE_BLINK) ) {
        ret = fpc_read(qsfp->fpc,
                FPC_PORT_LED_REG[FPC_LED2_PWM_CTRL_REG][qsfp->port_num],
                &led2_pwm_val, sizeof(led2_pwm_val));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Fail to read LED2 pwm ctrl reg. ret %d\n", ret);
            return -EINVAL;
        }

        if(led2_state == LED_MODE_PWM)
            scnprintf(led2_str, 50, "ON with PWM: %d", led2_pwm_val);
        else
            scnprintf(led2_str, 50, "BLINK with PWM: %d", led2_pwm_val);
    }

    return scnprintf(buf, MAX_SYSFS_TRX_FILE_LENGTH,"#\n# Specifies port leds"
           " PWM and Blink mode brightness value information\n# PWM mode "
           "brightness value: Minimum:0 Maximum:254 Any other value "
           "is invalid\n# Blink mode brightness value: Minimum:0 Maximum:255"
           " Any other value is invalid\n#   If brightness value = 0, the "
           "transceiver driver will internally calculate brightness\n#   "
           "value based on the trx maximum supported speed\n#\nLED1 %s\n"
           "LED2 %s\n", led1_str , led2_str);
}

/* Function to store the LED number and brightness information
 * through SysFS to set the LED to PWM mode.
 */
static ssize_t trx_led_brightness_set_store(struct device *dev,
                                   struct device_attribute *attr,
                                   char const *buf, size_t count)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    char buf_local[MAX_SYSFS_TRX_FILE_LENGTH] = {0};
    int led_num = 0;
    char led_num_buff[10] = {0};

    int led_pwm_val = 0;
    char led_pwm_val_buff[10] = {0};

    int ret = 0;
    ret = trx_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
                     buf, count);
    if (ret != 0) {
        TRX_LOG_ERR(qsfp, "SysFS file length is larger than %zu bytes\n",
                                                    sizeof(buf_local));
        return ret;
    }

    sscanf(buf_local,"%s %d %s %d", led_num_buff, &led_num,
                           led_pwm_val_buff, &led_pwm_val);

    if ((strncmp(led_num_buff,"led_num:",8) != 0) ||
         (strncmp(led_pwm_val_buff,"led_pwm_val:",12) != 0))
    {
        TRX_LOG_ERR(qsfp, "Invalid data read from file\n");
        return -EINVAL;
    }

    /* Validate input values from the user. */
    if(((led_num < 1) || (led_num > 3)) ||
       ((led_pwm_val < 0)|| (led_pwm_val > 254)))
    {
        TRX_LOG_ERR(qsfp, "Invalid input data led_num: %d,"
                    "led_pwm_val: %d\n", led_num, led_pwm_val);
        return -EINVAL;
    }

    ret = transceiver_led_brightness_set(qsfp, (u8)led_num, (u8)led_pwm_val);
    if(ret != 0)
        return -EINVAL;
    else
        return count;
}

/* Wraper function to write specification ID information to the SysFS buffer.
 */
inline ssize_t sysfs_spec_info_print(char *buf, u8 spec_id)
{
    if (spec_id == 0x00) {
        return scnprintf(buf, PAGE_SIZE, "Specification Identifier {0x%02X}\n"
                      "Unknown module\n",spec_id);
    }
    else {
        return scnprintf(buf, PAGE_SIZE, "Specification Identifier {0x%02X}\n"
                      "%s Transceiver module is not supported\n",
                      spec_id,mod_identifier_to_str(spec_id));
    }
}

/* Function to export transceiver device temperature information to Sysfs.
 */
static ssize_t trx_temperature_show(struct device *dev,
                       struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id = (u8*)&qsfp->id;
    struct sfp_eeprom_id *id;
    char temperature_data[75] = {0};
    int16_t tempc = 0;
    int ret = 0;

    /* Ensure that the transceiver is inserted before processing. */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return scnprintf(buf, PAGE_SIZE,"QSFP transceiver not inserted\n");
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        id = &qsfp->id.sff8472;
        /* Check for DDM support, Address A0h, Byte 92 Bit 6 */
        if(id->ext.diagmon & SFF8472_DIAGMON_DDM)
        {
            /* Address A2h, Bytes 96-97 */
            ret = qsfp_read(qsfp, SFF8472_TEMP, &tempc,
                                        sizeof(tempc));
            if (ret < 0) {
                return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
            }

            /* Check for Internal calibration for DDM supported SFP,
             * Address A0h, Byte 92 Bit 5.
             */
            if(id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
            {
                return scnprintf(buf, PAGE_SIZE, "%s\n",
                          calc_common_temperature(tempc,
                          temperature_data));
            }
            else /* SFP supported External calibration */
            {
                return scnprintf(buf, PAGE_SIZE, "%s\n",
                       calc_external_calib_temperature(qsfp, tempc,
                       temperature_data));
            }
        }
        else
        {
            return scnprintf(buf, PAGE_SIZE, "TRX temperature measurement not "
                                "supported on non-DDM transceiver devices.\n");
        }
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h Bytes 22-23 */
        ret = qsfp_read(qsfp, SFF8636_TEMPERATURE, &tempc,
                                           sizeof(tempc));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE,"QSFP read error: %d\n", ret);
        }

        return scnprintf(buf, PAGE_SIZE, "%s\n", calc_common_temperature(tempc,
                                         temperature_data));
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            return scnprintf(buf, PAGE_SIZE,"TRX temperature measurement"
                                            " not supported\n");
        }

        /*Support advertised in page 01h:159.0 */
        if (!cmis_id->ext.temp_mon_sup) {
            return scnprintf(buf, PAGE_SIZE, "TRX temperature measurement"
                                             " not supported\n");
        }

        /* Page 00h Bytes 14-15 */
        ret = qsfp_read(qsfp, CMIS_MOD_TEMPMON, &tempc,
                               sizeof(tempc));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
        }

        return scnprintf(buf, PAGE_SIZE, "%s\n", calc_common_temperature(tempc,
                                         temperature_data));
    default:
        return sysfs_spec_info_print(buf,*spec_id);
    }
}

/* Function to export transceiver device supply voltage information to Sysfs.
 */
static ssize_t trx_supply_voltage_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    struct cmis_eeprom_id *cmis_id;
    struct sfp_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;
    char voltage_data[75] = {0};
    u16 supply_voltage_t = 0;
    int ret = 0;

    /* Ensure that the transceiver is inserted before processing. */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return scnprintf(buf, PAGE_SIZE, "QSFP transceiver not inserted\n");
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        id = &qsfp->id.sff8472;
        /* Check for DDM support, Address A0h, Byte 92 Bit 6 */
        if(id->ext.diagmon & SFF8472_DIAGMON_DDM)
        {
            /* Address A2h, Bytes 98-99 */
            ret = qsfp_read(qsfp, SFF8472_VCC, &supply_voltage_t,
                                       sizeof(supply_voltage_t));
            if (ret < 0) {
                return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
            }

            /* Check for Internal calibration for DDM supported SFP,
             * Address A0h, Byte 92 Bit 5.
             */
            if(id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
            {
                return scnprintf(buf, PAGE_SIZE, "%s\n",
                       calc_common_svoltage(supply_voltage_t,
                       voltage_data));
            }
            else /* SFP supported External calibration */
            {
                return scnprintf(buf, PAGE_SIZE, "%s\n",
                                 calc_external_calib_svoltage(qsfp,
                                 supply_voltage_t,
                                 voltage_data));
            }
        }
        else
        {
            return scnprintf(buf, PAGE_SIZE,"TRX supply voltage measurement "
                          "not supported on non-DDM transceiver devices.\n");
        }
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h Bytes 26-27 */
        ret = qsfp_read(qsfp, SFF8636_SUPPLY_VOLTAGE, &supply_voltage_t,
                              sizeof(supply_voltage_t));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
        }

        return scnprintf(buf, PAGE_SIZE, "%s\n",
                              calc_common_svoltage(supply_voltage_t,
                              voltage_data));
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            return scnprintf(buf, PAGE_SIZE,"TRX supply voltage measurement"
                                            " not supported\n");
        }

        /*Support advertised in page 01h:159.1 */
        if (!cmis_id->ext.volt_mon_sup) {
            return scnprintf(buf, PAGE_SIZE,"TRX supply voltage measurement"
                                            " not supported\n");
        }

        /* Page 00h Bytes 16-17 */
        ret = qsfp_read(qsfp, CMIS_MOD_VCCMON, &supply_voltage_t,
                              sizeof(supply_voltage_t));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
        }

        return scnprintf(buf, PAGE_SIZE,"%s\n",
                              calc_common_svoltage(supply_voltage_t,
                              voltage_data));
    default:
        return sysfs_spec_info_print(buf,*spec_id);
    }
}

/* Function to export transceiver device Channel Monitor value of Rx
 * Power information to Sysfs.
 */
static ssize_t trx_rx_power_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    struct cmis_eeprom_id *cmis_id;
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id = (u8*)&qsfp->id;
    char rx_power_data[500] ={0};
    u16 rx_power_t[4] = {0};
    u8  rx_power[8] = {0};
    u8  sfp_rx_power[2] = {0};
    u16 cmis_rx_power_t[8] = {0};
    u8  cmis_rx_power[16] = {0};
    int ret = 0;

    /* Ensure that the transceiver is inserted before processing. */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return scnprintf(buf, PAGE_SIZE, "QSFP transceiver not inserted\n");
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;
        /* Check for DDM support, Address A0h, Byte 92 Bit 6 */
        if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_DDM)
        {
            /* Address A2h, Bytes 104-105 */
            ret = qsfp_read(qsfp, SFF8472_RX_POWER, sfp_rx_power,
                                           sizeof(sfp_rx_power));
            if (ret < 0) {
                return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
            }

            /* Check for Internal calibration for DDM supported SFP,
             * Address A0h, Byte 92 Bit 5.
             */
            if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
            {
                rx_power_t[0] = (( sfp_rx_power[0] << 8) | sfp_rx_power[1]);
                /* rx power in milliWatts (rx_power_t * 0.1 μW/ 1000) */
                scnprintf(rx_power_data,500,"Rx Power Lane1: %d.%03d mW",
                                rx_power_t[0]/10000,rx_power_t[0]%10000);
                return scnprintf(buf, PAGE_SIZE, "%s\n", rx_power_data);
            }
            else /* SFP supported External calibration */
            {
                /* The procedure to calculate Rx power in the case of
                 * external calibration was not clear, and what Rx_PWR_ADe4-1
                 * signifies was not clear. We need to revisit this later.
                 */
                return scnprintf(buf, PAGE_SIZE, "TRX optical rx power"
                       " measurement not supported for external calibration"
                       " type.\n");
            }
        }
        else
        {
            return scnprintf(buf, PAGE_SIZE, "TRX optical rx power measurement"
                           " not supported on non-DDM transceiver devices.\n");
        }
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h Bytes 34-41 */
        ret = qsfp_read(qsfp, SFF8636_RX_POWER, rx_power,
                              sizeof(rx_power));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE,"QSFP read error: %d\n", ret);
        }

        rx_power_t[0] = (( rx_power[0] << 8) | rx_power[1]);
        rx_power_t[1] = (( rx_power[2] << 8) | rx_power[3]);
        rx_power_t[2] = (( rx_power[4] << 8) | rx_power[5]);
        rx_power_t[3] = (( rx_power[6] << 8) | rx_power[7]);

        /* rx power in milliwatts (rx_power_t * 0.1 μW/ 1000) */
        scnprintf(rx_power_data,500,"Rx Power Lane1: %d.%03d mW\n"
                                    "Rx Power Lane2: %d.%03d mW\n"
                                    "Rx Power Lane3: %d.%03d mW\n"
                                    "Rx Power Lane4: %d.%03d mW",
                          rx_power_t[0]/10000,rx_power_t[0]%10000,
                          rx_power_t[1]/10000,rx_power_t[1]%10000,
                          rx_power_t[2]/10000,rx_power_t[2]%10000,
                          rx_power_t[3]/10000,rx_power_t[3]%10000);
        return scnprintf(buf, PAGE_SIZE, "%s\n", rx_power_data);
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            return scnprintf(buf, PAGE_SIZE,"TRX optical rx power "
                                     "measurement not supported\n");
        }

        /*Support advertised in page 01h:160.2 */
        if(!cmis_id->ext.rx_optical_pow_mon_sup ) {
            return scnprintf(buf, PAGE_SIZE, "TRX optical rx power "
                                     "measurement not supported\n");
        }

        /* Page 11h Bytes 186-201 */
        ret = qsfp_read(qsfp, CMIS_RX_POWER, cmis_rx_power,
                              sizeof(cmis_rx_power));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
        }

        cmis_rx_power_t[0] = (( cmis_rx_power[0] << 8) | cmis_rx_power[1]);
        cmis_rx_power_t[1] = (( cmis_rx_power[2] << 8) | cmis_rx_power[3]);
        cmis_rx_power_t[2] = (( cmis_rx_power[4] << 8) | cmis_rx_power[5]);
        cmis_rx_power_t[3] = (( cmis_rx_power[6] << 8) | cmis_rx_power[7]);
        cmis_rx_power_t[4] = (( cmis_rx_power[8] << 8) | cmis_rx_power[9]);
        cmis_rx_power_t[5] = (( cmis_rx_power[10] << 8) | cmis_rx_power[11]);
        cmis_rx_power_t[6] = (( cmis_rx_power[12] << 8) | cmis_rx_power[13]);
        cmis_rx_power_t[7] = (( cmis_rx_power[14] << 8) | cmis_rx_power[15]);

        /* rx power in milliwatts (rx_power_t * 0.1 μW/ 1000) */
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
        return scnprintf(buf, PAGE_SIZE, "%s\n", rx_power_data);
    default:
        return sysfs_spec_info_print(buf,*spec_id);
    }
}

/* Function to export transceiver device Channel Monitor value of Tx
 * Bias Current information to Sysfs.
 */
static ssize_t trx_tx_bias_current_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    struct cmis_eeprom_id *cmis_id;
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id = (u8*)&qsfp->id;
    char tx_bias_current_data[500] = {0};
    u32 tx_bias_current_t[4] = {0};
    u16 tx_bias_current = 0;
    u8 tx_bias[8] = {0};
    u8 cmis_tx_bias[16] = {0};
    u8 sfp_tx_bias[2] = {0};
    u32 cmis_tx_bias_current_t[8] = {0};
    u8 cmis_tx_bias_multiplier = 1;
    int ret = 0;

    /* Ensure that the transceiver is inserted before processing. */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return scnprintf(buf, PAGE_SIZE, "QSFP transceiver not inserted\n");
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;
        /* Check for DDM support, Address A0h, Byte 92 Bit 6 */
        if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_DDM)
        {
            /* Address A2h, Bytes 100-101 */
            ret = qsfp_read(qsfp, SFF8472_TX_BIAS, sfp_tx_bias,
                                          sizeof(sfp_tx_bias));
            if (ret < 0) {
                return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
            }

            tx_bias_current = (( sfp_tx_bias[0] << 8) | sfp_tx_bias[1]);

            /* Check for Internal calibration for DDM supported SFP,
             * Address A0h, Byte 92 Bit 5.
             */
            if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
            {
                /* tx_bias_current in  Micro Amp */
                tx_bias_current_t[0] = tx_bias_current * 2;
                scnprintf(tx_bias_current_data,500,"Tx Bias Current "
                          "Lane1: %d.%03d mA",tx_bias_current_t[0]/1000,
                          tx_bias_current_t[0]%1000);
                return scnprintf(buf, PAGE_SIZE, "%s\n", tx_bias_current_data);
            }
            else /* SFP supported External calibration */
            {
                return scnprintf(buf, PAGE_SIZE, "%s\n",
                                 calc_external_calib_txi(qsfp, tx_bias_current,
                                 tx_bias_current_data));
            }
        }
        else
        {
            return scnprintf(buf, PAGE_SIZE,"TRX tx bias current measurement"
                         " not supported on non-DDM transceiver devices.\n");
        }
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h Bytes 42-49 */
        ret = qsfp_read(qsfp, SFF8636_TX_BIAS, tx_bias,
                              sizeof(tx_bias));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
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
        return scnprintf(buf, PAGE_SIZE, "%s\n", tx_bias_current_data);
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            return scnprintf(buf, PAGE_SIZE, "TRX tx bias current measurement "
                                                            "not supported\n");
        }

        /*Support advertised in page 01h:160.0 */
        if(!cmis_id->ext.tx_bias_mon_sup ) {
            return scnprintf(buf, PAGE_SIZE, "TRX tx bias current measurement "
                                                            "not supported\n");
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
        /* reserved case is not expected from OIF-CMIS-05.2 Rev */
        case 0x03:
            return scnprintf(buf, PAGE_SIZE, "TRX tx bias current multiplier "
                             "was reserved not expected for OIF-CMIS-05.2\n");
        }

        /* Page 11h Bytes 170-185 */
        ret = qsfp_read(qsfp, CMIS_TX_BIAS, cmis_tx_bias,
                              sizeof(cmis_tx_bias));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
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
        return scnprintf(buf, PAGE_SIZE, "%s\n", tx_bias_current_data);
    default:
        return sysfs_spec_info_print(buf,*spec_id);
    }
}

/* Function to export transceiver device Channel Monitor value of Tx
 * Power information to Sysfs.
 */
static ssize_t trx_tx_power_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    struct sff8636_eeprom_id *id;
    struct cmis_eeprom_id *cmis_id;
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id = (u8*)&qsfp->id;
    char tx_power_data[500] = {0};
    u16 tx_power_t[4] = {0};
    u8  tx_power[8] = {0};
    u8  sfp_tx_power[2] = {0};
    u8  diagmon;
    u8  cmis_tx_power[16] = {0};
    u16 cmis_tx_power_t[8] = {0};
    int ret = 0;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return scnprintf(buf, PAGE_SIZE,"QSFP transceiver not inserted\n");
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;
        /* Check for DDM support, Address A0h, Byte 92 Bit 6 */
        if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_DDM)
        {
            /* Address A2h, Bytes 102-103 */
            ret = qsfp_read(qsfp, SFF8472_TX_POWER, sfp_tx_power,
                                           sizeof(sfp_tx_power));
            if (ret < 0) {
                return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
            }

            tx_power_t[0] = (( sfp_tx_power[0] << 8) | sfp_tx_power[1]);

            /* Check for Internal calibration for DDM supported SFP,
             * Address A0h, Byte 92 Bit 5.
             */
            if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
            {
                /* tx power in milliWatts (tx_power_t * 0.1 μW/ 1000) */
                scnprintf(tx_power_data,500,"Tx Power Lane1: %d.%03d mW",
                                tx_power_t[0]/10000,tx_power_t[0]%10000);
                return scnprintf(buf, PAGE_SIZE, "%s\n", tx_power_data);
            }
            else /* SFP supported External calibration */
            {
                return scnprintf(buf, PAGE_SIZE, "%s\n",
                                 calc_external_calib_txpwr(qsfp, tx_power_t[0],
                                 tx_power_data));
            }
        }
        else
        {
            return scnprintf(buf, PAGE_SIZE, "Transmitter power measurement"
                        " not supported on non-DDM transceiver devices.\n");
        }
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        diagmon = id->ext.diagmon;
        if(!(diagmon & BIT(2))) {
            return scnprintf(buf, PAGE_SIZE, "Transmitter power measurement"
                                             " not supported\n");
        }
        /* Page 00h Bytes 50-57 */
        ret = qsfp_read(qsfp, SFF8636_TX_POWER, tx_power,
                              sizeof(tx_power));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
        }

        tx_power_t[0] = (( tx_power[0] << 8) | tx_power[1]);
        tx_power_t[1] = (( tx_power[2] << 8) | tx_power[3]);
        tx_power_t[2] = (( tx_power[4] << 8) | tx_power[5]);
        tx_power_t[3] = (( tx_power[6] << 8) | tx_power[7]);

        /* tx power in milliwatts (tx_power_t * 0.1 μW/ 1000) */
        scnprintf(tx_power_data,500,"Tx Power Lane1: %d.%03d mW\n"
                                    "Tx Power Lane2: %d.%03d mW\n"
                                    "Tx Power Lane3: %d.%03d mW\n"
                                    "Tx Power Lane4: %d.%03d mW",
                          tx_power_t[0]/10000,tx_power_t[0]%10000,
                          tx_power_t[1]/10000,tx_power_t[1]%10000,
                          tx_power_t[2]/10000,tx_power_t[2]%10000,
                          tx_power_t[3]/10000,tx_power_t[3]%10000);
        return scnprintf(buf, PAGE_SIZE, "%s\n", tx_power_data);
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            return scnprintf(buf, PAGE_SIZE, "Transmitter power measurement"
                                             " not supported\n");
        }

        /*Support advertised in page 01h:160.1 */
        if(!cmis_id->ext.tx_optical_pow_mon_sup) {
            return scnprintf(buf, PAGE_SIZE, "Transmitter power measurement "
                                             "not supported\n");
        }

        /* Page 11h Bytes 154-169 */
        ret = qsfp_read(qsfp, CMIS_TX_POWER, cmis_tx_power,
                              sizeof(cmis_tx_power));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
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
        return scnprintf(buf, PAGE_SIZE, "%s\n", tx_power_data);
    default:
        return sysfs_spec_info_print(buf,*spec_id);
    }
}


ssize_t calc_external_calib_ddm_sys(struct qsfp *qsfp, char *buf,
                 struct sff8472_ddm_thresholds* ddm_limits)
{
    struct sff8472_temp_diag temp_ext_cal = {0};
    struct sff8472_vcc_diag vcc_ext_cal = {0};
    struct sff8472_txi_diag txi_ext_cal = {0};
    struct sff8472_txpwr_diag txpwr_ext_cal = {0};
    int ret = 0;

    ret = qsfp_read(qsfp, SFF8472_TEMP_EXT, &temp_ext_cal,
                        sizeof(temp_ext_cal));
    if (ret < 0) {
        return scnprintf(buf, PAGE_SIZE,"QSFP read error for temperature "
                            "external calibration constants: %d\n\n", ret);
    }

    ret = qsfp_read(qsfp, SFF8472_VCC_EXT, &vcc_ext_cal,
                                   sizeof(vcc_ext_cal));
    if (ret < 0) {
        return scnprintf(buf, PAGE_SIZE,"QSFP read error for supply voltage"
                            " external calibration constants: %d\n\n", ret);
    }

    ret = qsfp_read(qsfp, SFF8472_TXPWR_EXT, &txpwr_ext_cal,
                                     sizeof(txpwr_ext_cal));
    if (ret < 0) {
        return scnprintf(buf, PAGE_SIZE,"QSFP read error for tx power external"
                                        " calibration constants: %d\n\n", ret);
    }

    ret = qsfp_read(qsfp, SFF8472_TXI_EXT, &txi_ext_cal,
                                   sizeof(txi_ext_cal));
    if (ret < 0) {
        return scnprintf(buf, PAGE_SIZE,"QSFP read error for tx bias current "
                           "of external calibration constants: %d\n\n", ret);
    }

    return scnprintf(buf, PAGE_SIZE, "************ temperature threshold "
    "limits ************\ntemp_high_alarm: %d °C \ntemp_low_alarm : %d °C "
    "\ntemp_high_warn : %d °C \ntemp_low_warn  : %d °C \n\n"
    "********** supply voltage threshold limits ***********\n"
    "volt_high_alarm: %d mV \nvolt_low_alarm : %d mV \n"
    "volt_high_warn : %d mV \nvolt_low_warn  : %d mV \n\n"
    "************* tx power threshold limits **************\n"
    "txpwr_high_alarm: %d µW \ntxpwr_low_alarm : %d µW \n"
    "txpwr_high_warn : %d µW \ntxpwr_low_warn: %d µW \n\n"
    "************** tx bias threshold limits **************\n"
    "bias_high_alarm: %d mA \nbias_low_alarm : %d mA \n"
    "bias_high_warn : %d mA \nbias_low_warn  : %d mA \n\n",
    trx_ext_temp_ddm(ddm_limits->temp_high_alarm, &temp_ext_cal),
    trx_ext_temp_ddm(ddm_limits->temp_low_alarm, &temp_ext_cal),
    trx_ext_temp_ddm(ddm_limits->temp_high_warn, &temp_ext_cal),
    trx_ext_temp_ddm(ddm_limits->temp_low_warn, &temp_ext_cal),
    trx_ext_vcc_ddm(ddm_limits->volt_high_alarm, &vcc_ext_cal),
    trx_ext_vcc_ddm(ddm_limits->volt_low_alarm, &vcc_ext_cal),
    trx_ext_vcc_ddm(ddm_limits->volt_high_warn, &vcc_ext_cal),
    trx_ext_vcc_ddm(ddm_limits->volt_low_warn, &vcc_ext_cal),
    trx_ext_ddm_power(ddm_limits->txpwr_high_alarm, &txpwr_ext_cal),
    trx_ext_ddm_power(ddm_limits->txpwr_low_alarm, &txpwr_ext_cal),
    trx_ext_ddm_power(ddm_limits->txpwr_high_warn, &txpwr_ext_cal),
    trx_ext_ddm_power(ddm_limits->txpwr_low_warn, &txpwr_ext_cal),
    trx_ext_ddm_txbias(ddm_limits->bias_high_alarm, &txi_ext_cal),
    trx_ext_ddm_txbias(ddm_limits->bias_low_alarm, &txi_ext_cal),
    trx_ext_ddm_txbias(ddm_limits->bias_high_warn, &txi_ext_cal),
    trx_ext_ddm_txbias(ddm_limits->bias_low_warn, &txi_ext_cal));
}

/* Function to export transceiver ddm threshold values information to Sysfs.
 */
static ssize_t trx_ddm_thresholds_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id = (u8*)&qsfp->id;
    struct sff8472_ddm_thresholds ddm_limits = {0};
    struct sff8636_ddm_thresholds sff8636_ddm_limits = {0};
    struct cmis_thresholds cmis_ddm_limits = {0};
    int ret = 0;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return scnprintf(buf, PAGE_SIZE,"QSFP transceiver not inserted\n");
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;

        if(!(sff8472_id->ext.enhopts & SFP_ENHOPTS_ALARMWARN))
        {
            return scnprintf(buf, PAGE_SIZE, "TRX does not support alarm and "
                                                "warning threshold limits\n");
        }

        /* Address A2h, Bytes 0-39 */
        ret = qsfp_read(qsfp, SFF8472_DDM_TH, &ddm_limits,
                                     sizeof(ddm_limits));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
        }

        if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_EXT_CAL)
        {
           return calc_external_calib_ddm_sys(qsfp, buf, &ddm_limits);
        }
        else /* SFP supported Internal calibration */
        {
            return scnprintf(buf, PAGE_SIZE, "************ temperature "
                    "threshold limits ************\n"
                    "temp_high_alarm: %d °C \ntemp_low_alarm : %d °C "
                    "\ntemp_high_warn : %d °C \ntemp_low_warn  : %d °C \n\n"
                    "********** supply voltage threshold limits ***********\n"
                    "volt_high_alarm: %d mV \nvolt_low_alarm : %d mV \n"
                    "volt_high_warn : %d mV \nvolt_low_warn : %d mV \n\n"
                    "************* tx power threshold limits **************\n"
                    "txpwr_high_alarm: %d µW \ntxpwr_low_alarm : %d µW \n"
                    "txpwr_high_warn : %d µW \ntxpwr_low_warn  : %d µW \n\n"
                    "************* rx power threshold limits **************\n"
                    "rxpwr_high_alarm: %d µW \nrxpwr_low_alarm : %d µW \n"
                    "rxpwr_high_warn : %d µW \nrxpwr_low_warn  : %d µW \n\n"
                    "************** tx bias threshold limits **************\n"
                    "bias_high_alarm: %d mA \nbias_low_alarm : %d mA \n"
                    "bias_high_warn : %d mA \nbias_low_warn : %d mA \n\n",
                    trx_calibrate_temp(ddm_limits.temp_high_alarm),
                    trx_calibrate_temp(ddm_limits.temp_low_alarm),
                    trx_calibrate_temp(ddm_limits.temp_high_warn),
                    trx_calibrate_temp(ddm_limits.temp_low_warn),
                    trx_calibrate_vcc(ddm_limits.volt_high_alarm),
                    trx_calibrate_vcc(ddm_limits.volt_low_alarm),
                    trx_calibrate_vcc(ddm_limits.volt_high_warn),
                    trx_calibrate_vcc(ddm_limits.volt_low_warn),
                    trx_calibrate_power(ddm_limits.txpwr_high_alarm),
                    trx_calibrate_power(ddm_limits.txpwr_low_alarm),
                    trx_calibrate_power(ddm_limits.txpwr_high_warn),
                    trx_calibrate_power(ddm_limits.txpwr_low_warn),
                    trx_calibrate_power(ddm_limits.rxpwr_high_alarm),
                    trx_calibrate_power(ddm_limits.rxpwr_low_alarm),
                    trx_calibrate_power(ddm_limits.rxpwr_high_warn),
                    trx_calibrate_power(ddm_limits.rxpwr_low_warn),
                    trx_calibrate_txbias(ddm_limits.bias_high_alarm),
                    trx_calibrate_txbias(ddm_limits.bias_low_alarm),
                    trx_calibrate_txbias(ddm_limits.bias_high_warn),
                    trx_calibrate_txbias(ddm_limits.bias_low_warn));
        }
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:

        /* Page 00h, Byte-2 Bit-2 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            return scnprintf(buf, PAGE_SIZE, "TRX does not support alarm and "
                                                "warning threshold limits\n");
        }

        /* Page 03h Bytes 128-199 */
        ret = qsfp_read(qsfp, SFF8636_DDM_TH, &sff8636_ddm_limits,
                                     sizeof(sff8636_ddm_limits));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
        }

        return scnprintf(buf, PAGE_SIZE, "************ temperature "
                 "threshold limits ************\n"
                "temp_high_alarm: %d °C \ntemp_low_alarm : %d °C "
                "\ntemp_high_warn : %d °C \ntemp_low_warn  : %d °C \n\n"
                "********** supply voltage threshold limits ***********\n"
                "volt_high_alarm: %d mV \nvolt_low_alarm : %d mV \n"
                "volt_high_warn : %d mV \nvolt_low_warn : %d mV \n\n"
                "************* tx power threshold limits **************\n"
                "txpwr_high_alarm: %d µW \ntxpwr_low_alarm : %d µW \n"
                "txpwr_high_warn : %d µW \ntxpwr_low_warn  : %d µW \n\n"
                "************* rx power threshold limits **************\n"
                "rxpwr_high_alarm: %d µW \nrxpwr_low_alarm : %d µW \n"
                "rxpwr_high_warn : %d µW \nrxpwr_low_warn  : %d µW \n\n"
                "************** tx bias threshold limits **************\n"
                "bias_high_alarm: %d mA \nbias_low_alarm : %d mA \n"
                "bias_high_warn : %d mA \nbias_low_warn : %d mA \n\n",
                trx_calibrate_temp(sff8636_ddm_limits.temp_high_alarm),
                trx_calibrate_temp(sff8636_ddm_limits.temp_low_alarm),
                trx_calibrate_temp(sff8636_ddm_limits.temp_high_warn),
                trx_calibrate_temp(sff8636_ddm_limits.temp_low_warn),
                trx_calibrate_vcc(sff8636_ddm_limits.volt_high_alarm),
                trx_calibrate_vcc(sff8636_ddm_limits.volt_low_alarm),
                trx_calibrate_vcc(sff8636_ddm_limits.volt_high_warn),
                trx_calibrate_vcc(sff8636_ddm_limits.volt_low_warn),
                trx_calibrate_power(sff8636_ddm_limits.txpwr_high_alarm),
                trx_calibrate_power(sff8636_ddm_limits.txpwr_low_alarm),
                trx_calibrate_power(sff8636_ddm_limits.txpwr_high_warn),
                trx_calibrate_power(sff8636_ddm_limits.txpwr_low_warn),
                trx_calibrate_power(sff8636_ddm_limits.rxpwr_high_alarm),
                trx_calibrate_power(sff8636_ddm_limits.rxpwr_low_alarm),
                trx_calibrate_power(sff8636_ddm_limits.rxpwr_high_warn),
                trx_calibrate_power(sff8636_ddm_limits.rxpwr_low_warn),
                trx_calibrate_txbias(sff8636_ddm_limits.bias_high_alarm),
                trx_calibrate_txbias(sff8636_ddm_limits.bias_low_alarm),
                trx_calibrate_txbias(sff8636_ddm_limits.bias_high_warn),
                trx_calibrate_txbias(sff8636_ddm_limits.bias_low_warn));
    case SFF8024_ID_QSFPDD_CMIS:
        /* Page 00h, Byte-2 Bit-7 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            return scnprintf(buf, PAGE_SIZE,"TRX does not support alarm and "
                                                "warning threshold limits\n");
        }

        /* Page 02h Bytes 128-199 */
        ret = qsfp_read(qsfp, CMIS_DDM_TH, &cmis_ddm_limits,
                                   sizeof(cmis_ddm_limits));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
        }

        return scnprintf(buf, PAGE_SIZE, "************ temperature "
                 "threshold limits ************\n"
                "temp_high_alarm: %d °C \ntemp_low_alarm : %d °C "
                "\ntemp_high_warn : %d °C \ntemp_low_warn  : %d °C \n\n"
                "********** supply voltage threshold limits ***********\n"
                "volt_high_alarm: %d mV \nvolt_low_alarm : %d mV \n"
                "volt_high_warn : %d mV \nvolt_low_warn : %d mV \n\n"
                "************* tx power threshold limits **************\n"
                "txpwr_high_alarm: %d µW \ntxpwr_low_alarm : %d µW \n"
                "txpwr_high_warn : %d µW \ntxpwr_low_warn  : %d µW \n\n"
                "************* rx power threshold limits **************\n"
                "rxpwr_high_alarm: %d µW \nrxpwr_low_alarm : %d µW \n"
                "rxpwr_high_warn : %d µW \nrxpwr_low_warn  : %d µW \n\n"
                "************** tx bias threshold limits **************\n"
                "bias_high_alarm: %d mA \nbias_low_alarm : %d mA \n"
                "bias_high_warn : %d mA \nbias_low_warn : %d mA \n\n",
                trx_calibrate_temp(cmis_ddm_limits.temp_high_alarm),
                trx_calibrate_temp(cmis_ddm_limits.temp_low_alarm),
                trx_calibrate_temp(cmis_ddm_limits.temp_high_warn),
                trx_calibrate_temp(cmis_ddm_limits.temp_low_warn),
                trx_calibrate_vcc(cmis_ddm_limits.volt_high_alarm),
                trx_calibrate_vcc(cmis_ddm_limits.volt_low_alarm),
                trx_calibrate_vcc(cmis_ddm_limits.volt_high_warn),
                trx_calibrate_vcc(cmis_ddm_limits.volt_low_warn),
                trx_calibrate_power(cmis_ddm_limits.txpwr_high_alarm),
                trx_calibrate_power(cmis_ddm_limits.txpwr_low_alarm),
                trx_calibrate_power(cmis_ddm_limits.txpwr_high_warn),
                trx_calibrate_power(cmis_ddm_limits.txpwr_low_warn),
                trx_calibrate_power(cmis_ddm_limits.rxpwr_high_alarm),
                trx_calibrate_power(cmis_ddm_limits.rxpwr_low_alarm),
                trx_calibrate_power(cmis_ddm_limits.rxpwr_high_warn),
                trx_calibrate_power(cmis_ddm_limits.rxpwr_low_warn),
                trx_calibrate_txbias(cmis_ddm_limits.bias_high_alarm),
                trx_calibrate_txbias(cmis_ddm_limits.bias_low_alarm),
                trx_calibrate_txbias(cmis_ddm_limits.bias_high_warn),
                trx_calibrate_txbias(cmis_ddm_limits.bias_low_warn));
    default:
        return sysfs_spec_info_print(buf,*spec_id);
    }

}

static DEVICE_ATTR(state_info, S_IRUGO, trx_state_info_show, NULL);
static DEVICE_ATTR(led_on_off, 0644, trx_led_on_off_show,
                                   trx_led_on_off_store);
static DEVICE_ATTR(led_blink_set, 0644, trx_led_blink_set_show,
                                      trx_led_blink_set_store);
static DEVICE_ATTR(led_brightness_set, 0644, trx_led_brightness_set_show,
                                           trx_led_brightness_set_store);

static DEVICE_ATTR(temperature, S_IRUGO, trx_temperature_show, NULL);
static DEVICE_ATTR(supply_voltage, S_IRUGO, trx_supply_voltage_show, NULL);
static DEVICE_ATTR(rx_power, S_IRUGO, trx_rx_power_show, NULL);
static DEVICE_ATTR(tx_bias_current, S_IRUGO, trx_tx_bias_current_show, NULL);
static DEVICE_ATTR(tx_power, S_IRUGO, trx_tx_power_show, NULL);
static DEVICE_ATTR(ddm_thresholds, S_IRUGO, trx_ddm_thresholds_show, NULL);


/* Transceiver port static attributes */
static struct attribute *trx_attrs[] = {
    &dev_attr_state_info.attr,
    &dev_attr_led_on_off.attr,
    &dev_attr_led_blink_set.attr,
    &dev_attr_led_brightness_set.attr,
    NULL
};

/* Transceiver module runtime attributes */
static struct attribute *trx_module_attrs[] = {
    &dev_attr_temperature.attr,
    &dev_attr_supply_voltage.attr,
    &dev_attr_rx_power.attr,
    &dev_attr_tx_bias_current.attr,
    &dev_attr_tx_power.attr,
    &dev_attr_ddm_thresholds.attr,
    NULL
};

static const struct attribute_group trx_attr_group = {
    .attrs = trx_attrs,
};

static const struct attribute_group trx_module_attr_group = {
    .attrs = trx_module_attrs,
};

/* Initialization function for creating SysFS attribute files
 * for transceiver.
 */
int qsfp_sysfs_init(struct qsfp *qsfp)
{
    int ret = 0;

    ret = sysfs_create_group(qsfp->qsfp_sysfs_dir, &trx_attr_group);
    if(ret != 0) {
        TRX_LOG_ERR(qsfp, "SysFS trx attr group create failure\n");
        sysfs_remove_group(qsfp->qsfp_sysfs_dir, &trx_attr_group);
        return ret;
    }
    return 0;
}

/* Deinitilization function to delete SysFS attribute files for
 * transceiver.
 */
int qsfp_sysfs_exit(struct qsfp *qsfp)
{
    sysfs_remove_group(qsfp->qsfp_sysfs_dir, &trx_attr_group);
    return 0;
}

/* Initialization function for creating SysFS runtime attribute files
 * for transceiver.
 */
int module_sysfs_init(struct qsfp *qsfp)
{
    int ret = 0;

    ret = sysfs_create_group(qsfp->qsfp_sysfs_dir, &trx_module_attr_group);
    if(ret != 0) {
        TRX_LOG_ERR(qsfp, "SysFS trx module attr group create failure\n");
        sysfs_remove_group(qsfp->qsfp_sysfs_dir, &trx_module_attr_group);
        return ret;
    }
    return 0;
}

/* Deinitilization function to delete SysFS runtime attribute files for
 * transceiver.
 */
int module_sysfs_exit(struct qsfp *qsfp)
{
    sysfs_remove_group(qsfp->qsfp_sysfs_dir, &trx_module_attr_group);
    return 0;
}
