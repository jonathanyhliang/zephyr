/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public API for Serial-Line CAN
 *
 * Serial-Line CAN is a transport protocol for CAN (Controller Area Network)
 */

#ifndef ZEPHYR_INCLUDE_SLCAN_H_
#define ZEPHYR_INCLUDE_SLCAN_H_

#include <zephyr/types.h>
#include <zephyr/drivers/can.h>

/**
 * @brief CAN SLCAN Interface
 * @defgroup can_slcan CAN SLCAN Interface
 * @ingroup CAN
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encapsulate underlying CAN driver frames in Serial-Line CAN messages 
 *
 * This function encapsulates the underlying CAN driver frames in Serial-Line CAN
 * messages and stuffs the serial message into tty buffer
 *
 * @param s         Pointer to tty buffer
 * @param frame     Pointer to incoming CAN frame
 *
 */
void slcan_encaps(char *s, struct can_frame *frame);

/**
 * @brief Decapsulate Serial-Line CAN messages in underlying CAN driver frames
 *
 * This function decapsulates the Serial-Line CAN messages in underlying CAN driver
 * frames
 *
 * @param s         Pointer to tty buffer
 * @param frame     Pointer to incoming CAN frame
 *
 */
int slcan_decaps(char *s);

char *slcan_getline(char c);

int slcan_init(void);


/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_SLCAN_H_ */
