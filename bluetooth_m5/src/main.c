/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <zephyr/kernel.h>

extern void init_ble_with_scanning(void);
extern void gatt_write_cmd(const char *string);

int main(void) {
	(void)init_ble_with_scanning();
	(void)gatt_write_cmd("2525");

	return 0;
}
