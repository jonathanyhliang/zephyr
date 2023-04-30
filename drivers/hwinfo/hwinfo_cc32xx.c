/*
 * Copyright (c) 2023 Jonathan Liang
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ti/drivers/net/wifi/device.h>
#include <zephyr/drivers/hwinfo.h>
#include <string.h>
#include <zephyr/sys/byteorder.h>


#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hwinfo, CONFIG_HAWKBIT_LOG_LEVEL);

ssize_t z_impl_hwinfo_get_device_id(uint8_t *buffer, size_t length)
{
    uint8_t udid[16] = {0};
    uint16_t configSize = sizeof(udid);
    uint8_t configOpt = SL_DEVICE_IOT_UDID;

    int ret;

    ret = sl_DeviceGet(SL_DEVICE_IOT, &configOpt, &configSize,(uint8_t*)(&udid));
    if (ret < 0) {
        LOG_ERR("device id read %d", ret);
        return -EINVAL;
    }

	if (length > sizeof(udid)) {
		length = sizeof(udid);
	}

	memcpy(buffer, udid, length);

	return length;
}
