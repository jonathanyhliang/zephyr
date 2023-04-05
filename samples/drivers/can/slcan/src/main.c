#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/console/console.h>

extern int slcan_init(void);
extern void slcan_decaps(char *s);

static inline char *serial_getline(void)
{
    static char ln[CONFIG_CONSOLE_INPUT_MAX_LINE_LEN];
    char c;
    int pos = 0;

    while (1) {
        c = console_getchar();
        if (c != '\0') {
            ln[pos] = c;
            pos++;
            if (pos >= CONFIG_CONSOLE_INPUT_MAX_LINE_LEN) {
                pos = 0;
            } else {
                if (ln[pos -1] == '\r') {
                    ln[pos] = '\0';
                    return ln;
                }
            }
        }
    }
}

void main(void)
{
    int ret;
    char *s;

    ret = slcan_init();
    if (ret < 0) {
        return;
    }

    console_init();

    while (1) {
        s = serial_getline();
        slcan_decaps(s);
    }
}
