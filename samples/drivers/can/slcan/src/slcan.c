#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/console/console.h>
#include <zephyr/sys/reboot.h>

/* controller area network (CAN) kernel definitions */

/* valid bits in CAN ID for frame formats */
#define CAN_SFF_MASK 0x000007FFU /* standard frame format (SFF) */
#define CAN_EFF_MASK 0x1FFFFFFFU /* extended frame format (EFF) */
#define CAN_ERR_MASK 0x1FFFFFFFU /* omit EFF, RTR, ERR flags */
#define CANXL_PRIO_MASK CAN_SFF_MASK /* 11 bit priority mask */

/*
 * Controller Area Network Identifier structure
 *
 * bit 0-28	: CAN identifier (11/29 bit)
 * bit 29	: error message frame flag (0 = data frame, 1 = error message)
 * bit 30	: remote transmission request flag (1 = rtr frame)
 * bit 31	: frame format flag (0 = standard 11 bit, 1 = extended 29 bit)
 */
typedef uint32_t canid_t;

#define CAN_SFF_ID_BITS		11
#define CAN_EFF_ID_BITS		29
#define CANXL_PRIO_BITS		CAN_SFF_ID_BITS

/* CAN payload length and DLC definitions according to ISO 11898-1 */
#define CAN_MAX_RAW_DLC 15

/*
 * CAN XL payload length and DLC definitions according to ISO 11898-1
 * CAN XL DLC ranges from 0 .. 2047 => data length from 1 .. 2048 byte
 */
#define CANXL_MIN_DLC 0
#define CANXL_MAX_DLC 2047
#define CANXL_MAX_DLC_MASK 0x07FF
#define CANXL_MIN_DLEN 1
#define CANXL_MAX_DLEN 2048

/* maximum rx buffer len: extended CAN frame with timestamp */
#define SLCAN_MTU (sizeof("T1111222281122334455667788EA5F\r") + 1)

#define SLCAN_CMD_LEN 1
#define SLCAN_SFF_ID_LEN 3
#define SLCAN_EFF_ID_LEN 8
#define SLCAN_STATE_LEN 1
#define SLCAN_STATE_BE_RXCNT_LEN 3
#define SLCAN_STATE_BE_TXCNT_LEN 3
#define SLCAN_STATE_FRAME_LEN       (1 + SLCAN_CMD_LEN + \
				     SLCAN_STATE_BE_RXCNT_LEN + \
				     SLCAN_STATE_BE_TXCNT_LEN)

struct can_filter_log {
	uint32_t filter_id;
	int filter_num;
};

const char hex_asc_upper[] = "0123456789ABCDEF";

struct k_work_poll can_frame_rx_work;
CAN_MSGQ_DEFINE(can_frame_rx_msgq, 2);
struct k_poll_event can_frame_rx_events[1] = {
	K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
					K_POLL_MODE_NOTIFY_ONLY,
					&can_frame_rx_msgq, 0)
};

static const uint32_t slcan_bitrate_const[] = {
	10000, 20000, 50000, 100000, 125000,
	250000, 500000, 800000, 1000000
};

// static struct can_filter_log filter_log[(CONFIG_CAN_MAX_EXT_ID_FILTER + CONFIG_CAN_MAX_STD_ID_FILTER * 2)];

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

/**
 * hex_to_bin - convert a hex digit to its real value
 * @ch: ascii character represents hex digit
 *
 * hex_to_bin() converts one hex digit to its actual value or -1 in case of bad
 * input.
 *
 * This function is used to load cryptographic keys, so it is coded in such a
 * way that there are no conditions or memory accesses that depend on data.
 *
 * Explanation of the logic:
 * (ch - '9' - 1) is negative if ch <= '9'
 * ('0' - 1 - ch) is negative if ch >= '0'
 * we "and" these two values, so the result is negative if ch is in the range
 *	'0' ... '9'
 * we are only interested in the sign, so we do a shift ">> 8"; note that right
 *	shift of a negative value is implementation-defined, so we cast the
 *	value to (unsigned) before the shift --- we have 0xffffff if ch is in
 *	the range '0' ... '9', 0 otherwise
 * we "and" this value with (ch - '0' + 1) --- we have a value 1 ... 10 if ch is
 *	in the range '0' ... '9', 0 otherwise
 * we add this value to -1 --- we have a value 0 ... 9 if ch is in the range '0'
 *	... '9', -1 otherwise
 * the next line is similar to the previous one, but we need to decode both
 *	uppercase and lowercase letters, so we use (ch & 0xdf), which converts
 *	lowercase to uppercase
 */
int hex_to_bin(unsigned char ch)
{
	unsigned char cu = ch & 0xdf;
	return -1 +
		((ch - '0' +  1) & (unsigned)((ch - '9' - 1) & ('0' - 1 - ch)) >> 8) +
		((cu - 'A' + 11) & (unsigned)((cu - 'F' - 1) & ('A' - 1 - cu)) >> 8);
}

#define hex_asc_upper_lo(x)	hex_asc_upper[((x) & 0x0f)]
#define hex_asc_upper_hi(x)	hex_asc_upper[((x) & 0xf0) >> 4]

static inline char *hex_byte_pack_upper(char *buf, uint8_t byte)
{
	*buf++ = hex_asc_upper_hi(byte);
	*buf++ = hex_asc_upper_lo(byte);
	return buf;
}

static int slcan_bump_frame(char *s, struct can_frame *frame)
{
	int i, tmp;
    char *endptr;
	uint32_t tmpid;
	char *cmd = s;

    frame->flags = 0;

	switch (*cmd) {
	case 'r':
		frame->flags |= CAN_FRAME_RTR;
	case 't':
		/* store dlc ASCII value and terminate SFF CAN ID string */
		frame->dlc = cmd[SLCAN_CMD_LEN + SLCAN_SFF_ID_LEN];
		cmd[SLCAN_CMD_LEN + SLCAN_SFF_ID_LEN] = 0;
		/* point to payload data behind the dlc */
		cmd += SLCAN_CMD_LEN + SLCAN_SFF_ID_LEN + 1;
		break;
	case 'R':
		frame->flags |= CAN_FRAME_RTR;
	case 'T':
        frame->flags |= CAN_FRAME_IDE;
		/* store dlc ASCII value and terminate EFF CAN ID string */
		frame->dlc = s[SLCAN_CMD_LEN + SLCAN_EFF_ID_LEN];
		cmd[SLCAN_CMD_LEN + SLCAN_EFF_ID_LEN] = 0;
		/* point to payload data behind the dlc */
		cmd += SLCAN_CMD_LEN + SLCAN_EFF_ID_LEN + 1;
		break;
	default:
        printf("slcan: failed to parse frame type (%c).\n", *cmd);
		return -EINVAL;
	}

	tmpid = strtoul(s + SLCAN_CMD_LEN, &endptr, 16);
    if (*endptr != '\0') {
        printf("slcan: failed to parse frame id.\n");
        return -EINVAL;
    }

	frame->id = tmpid;

	/* get len from sanitized ASCII value */
	if (frame->dlc >= '0' && frame->dlc < '9') {
		frame->dlc -= '0';
    } else {
        printf("slcan: failed to parse frame dlc.\n");
		return -EINVAL;
    }

	/* RTR frames may have a dlc > 0 but they never have any data bytes */
	if (!(frame->flags & CAN_FRAME_RTR)) {
		for (i = 0; i < frame->dlc; i++) {
			tmp = hex_to_bin(*cmd++);
			if (tmp < 0) {
                printf("slcan: failed to parse frame dlc.\n");
		        return -EINVAL;
            }

			frame->data[i] = (tmp << 4);
			tmp = hex_to_bin(*cmd++);
			if (tmp < 0) {
                printf("slcan: failed to parse frame dlc.\n");
		        return -EINVAL;
            }

			frame->data[i] |= tmp;
		}
	}

	if (*cmd != '\r') {
		printf("slcan: invalid frame terminator.\n");
		return -EINVAL;
	}

    return 0;
}

static int slcan_bump_filter(char *s, struct can_filter *filter)
{
    char *endptr;
	uint32_t tmpid, tmpmsk;

    filter->flags = CAN_FILTER_DATA;

	switch (*s) {
	case 'r':
		filter->flags |= CAN_FILTER_RTR;
	case 't':
		s[SLCAN_CMD_LEN + SLCAN_SFF_ID_LEN] = 0;
		break;
	case 'R':
		filter->flags |= CAN_FILTER_RTR;
	case 'T':
        filter->flags |= CAN_FILTER_IDE;
		s[SLCAN_CMD_LEN + SLCAN_EFF_ID_LEN] = 0;
		break;
	default:
        printf("slcan: failed to parse filter type (%c).\n", *s);
		return -EINVAL;
	}

	tmpid = strtoul(s + SLCAN_CMD_LEN, &endptr, 16);
    if (*endptr != '\0') {
        printf("slcan: failed to parse filter id.\n");
        return -EINVAL;
    }

	filter->id = tmpid;

    if (!(filter->flags & CAN_FILTER_IDE)) {
        tmpmsk = strtoul(s + SLCAN_CMD_LEN + SLCAN_SFF_ID_LEN, &endptr, 16);
    } else {
        tmpmsk = strtoul(s + SLCAN_CMD_LEN + SLCAN_EFF_ID_LEN, &endptr, 16);
    }
    
    if (*endptr != '\0') {
        printf("slcan: failed to parse filter mask.\n");
        return -EINVAL;
    }

	filter->mask = tmpmsk;

    return 0;
}

/* Encapsulate one can_frame and stuff into a TTY queue. */
static void slcan_encaps(char *s, struct can_frame *frame)
{
	int i;
	unsigned char *pos = s;
	unsigned char *endpos;
	uint32_t id = frame->id;

	if (frame->flags & CAN_FRAME_RTR)
		*pos = 'R'; /* becomes 'r' in standard frame format (SFF) */
	else
		*pos = 'T'; /* becomes 't' in standard frame format (SSF) */

	/* determine number of chars for the CAN-identifier */
	if (frame->flags & CAN_FRAME_IDE) {
		id &= CAN_EXT_ID_MASK;
		endpos = pos + SLCAN_EFF_ID_LEN;
	} else {
		*pos |= 0x20; /* convert R/T to lower case for SFF */
		id &= CAN_STD_ID_MASK;
		endpos = pos + SLCAN_SFF_ID_LEN;
	}

	/* build 3 (SFF) or 8 (EFF) digit CAN identifier */
	pos++;
	while (endpos >= pos) {
		*endpos-- = hex_asc_upper[id & 0xf];
		id >>= 4;
	}

	pos += (frame->flags & CAN_FRAME_IDE) ?
		SLCAN_EFF_ID_LEN : SLCAN_SFF_ID_LEN;

	*pos++ = frame->dlc + '0';

	/* RTR frames may have a dlc > 0 but they never have any data bytes */
	if (!(frame->flags & CAN_FRAME_RTR)) {
		for (i = 0; i < frame->dlc; i++) {
			pos = hex_byte_pack_upper(pos, frame->data[i]);
        }
	}

	*pos++ = '\r';
    *pos++ = '\0';
}

static void can_frame_rx_work_handler(struct k_work *work)
{
	struct can_frame frame;
    char line[SLCAN_MTU];
	int ret;

	while (k_msgq_get(&can_frame_rx_msgq, &frame, K_NO_WAIT) == 0) {
        slcan_encaps(line, &frame);
	}

    printf("%s", line);

	ret = k_work_poll_submit(&can_frame_rx_work, can_frame_rx_events,
				 ARRAY_SIZE(can_frame_rx_events), K_FOREVER);
	if (ret != 0) {
		printf("slcan: failed to resubmit msgq polling: %d", ret);
	}
}

void slcan_decaps(char *s)
{
	int ret = -EINVAL;
    char *endptr;
    long bitrate;
    struct can_frame frame;

	switch(*s) {
	/* Open CAN Node */
	case 'O':
		if (s[SLCAN_CMD_LEN] == '\r') {
			ret = can_start(can_dev);
		}
		if (ret != 0) {
			printf("slcan: failed to start CAN controller (err %d)\n", ret);
		}
		break;
	/* Close CAN Node */
	case 'C':
		if (s[SLCAN_CMD_LEN] == '\r') {
			ret = can_stop(can_dev);
		}
		if (ret != 0) {
			printf("slcan: failed to stop CAN controller (err %d)\n", ret);
		}
		break;
	/* Set CAN speed 0..8*/
	case 's':
		if (s[SLCAN_CMD_LEN + 1] == '\r') {
			/* Terminate SLCAN speed string */
			s[SLCAN_CMD_LEN + 1] = 0;
			bitrate = strtol(s + SLCAN_CMD_LEN, &endptr, 10);
			if (*endptr != '\0') {
				printf("slcan: failed to parse bitrate %lx\n", bitrate);
			}
			if (bitrate < 0 || bitrate > 8) {
				printf("slcan: bitrate out of range (0..8)\n");
			}
			ret = can_set_bitrate(can_dev, slcan_bitrate_const[bitrate]);
		}
		if (ret != 0) {
			printf("slcan: failed to set bitrate (err %d)\n", ret);
		}

		break;
	/* Parse CAN frame */
	case 't':
	case 'T':
	case 'r':
	case 'R':
		ret = slcan_bump_frame(s, &frame);
		if (ret != 0) {
			printf("slcan: failed to parse CAN frame (err %d)\n", ret);
		} else {
			ret = can_send(can_dev, &frame, K_NO_WAIT, NULL, NULL);
			if (ret != 0) {
				printf("slcan: failed to enqueue CAN frame (err %d)\n", ret);
			}
		}
		break;
	case 'b':
		if (!strncmp(s, "bbbbbb\r", 6U)) {
			sys_reboot(SYS_REBOOT_COLD);
		}
		break;
	default:
		printf("slcan: failed to parse frame type (%c).", *s);
		break;
    }
}

int slcan_init(void)
{
	int ret;
	struct can_filter filter[4] = {
		{
			.id = CAN_MAX_STD_ID,
			.mask = 0x0,
			.flags = CAN_FILTER_DATA
		},
		{
			.id = CAN_MAX_STD_ID,
			.mask = 0x0,
			.flags = CAN_FILTER_RTR
		},
		{
			.id = CAN_MAX_EXT_ID,
			.mask = 0x0,
			.flags = CAN_FILTER_DATA | CAN_FILTER_IDE
		},
		{
			.id = CAN_MAX_EXT_ID,
			.mask = 0x0,
			.flags = CAN_FILTER_IDE | CAN_FILTER_RTR
		}
	};

	if (!device_is_ready(can_dev)) {
		printf("slcan: device %s not ready.\n", can_dev->name);
		return -ENODEV;
	}

#ifdef CONFIG_LOOPBACK_MODE
	ret = can_set_mode(can_dev, CAN_MODE_LOOPBACK);
	if (ret != 0) {
		printf("slcan: error setting CAN mode [%d]\n", ret);
		return -EPERM;
	}
#endif

	k_work_poll_init(&can_frame_rx_work, can_frame_rx_work_handler);
    ret = k_work_poll_submit(&can_frame_rx_work, can_frame_rx_events,
				 ARRAY_SIZE(can_frame_rx_events), K_FOREVER);
	if (ret != 0) {
		printf("Failed to submit msgq polling: %d", ret);
		return -EPERM;
	}

	for (int i = 0; i < 4; i++) {
		ret = can_add_rx_filter_msgq(can_dev, &can_frame_rx_msgq, &filter[i]);
		if (ret < 0) {
			printf("slcan: failed to config CAN filter (err %d)\n", ret);
			return -EINVAL;
		}
	}

	return 0;
}
