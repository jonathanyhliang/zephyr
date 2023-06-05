#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/console/console.h>
#include <zephyr/canbus/slcan.h>

void main(void)
{
    int ret;
    char *s;

    ret = slcan_init();
    if (ret < 0) {
        return;
    }

    ret = console_init();
	if (ret < 0) {
		return;
	}

    while (1) {
        s = slcan_getline((char)console_getchar());
        if (!s) {
            (void)slcan_decaps(s);
        }
    }
}
