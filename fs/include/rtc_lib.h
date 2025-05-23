#ifndef RTC_LIB_H_
#define RTC_LIB_H_

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>

static const struct device *rtc;

static int rtc_init(void);
int rtc_write(const struct device *rtc, const struct rtc_time* current);
int rtc_read(const struct device *rtc, struct rtc_time* current);

static int rtc_init(void) {
    rtc = DEVICE_DT_GET(DT_NODELABEL(rtc));

    if (!device_is_ready(rtc)) {
        printk("RTC device not ready!\n");
        return -ENODEV;
    }

    printk("🕒 RTC initialized\n");
    return 0;
}

int rtc_write(const struct device *rtc, const struct rtc_time* current) {
	
	return rtc_set_time(rtc, current);
}

int rtc_read(const struct device *rtc, struct rtc_time* current) {

	return rtc_get_time(rtc, current);
}

#endif
