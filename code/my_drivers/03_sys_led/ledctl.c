// SPDX-License-Identifier: GPL-2.0
/*
 * sys_led 用户态测试程序
 *   用法: ./ledctl <period_ms>    0 = 停止闪烁
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define SYS_LED_MAGIC       'L'
#define SYS_LED_SET_PERIOD  _IOW(SYS_LED_MAGIC, 1, int)

int main(int argc, char **argv)
{
	int fd, ms;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <period_ms>  (0 = stop)\n", argv[0]);
		return 1;
	}
	ms = atoi(argv[1]);

	fd = open("/dev/sys_led", O_RDWR);
	if (fd < 0) {
		perror("open /dev/sys_led");
		return 1;
	}

	if (ioctl(fd, SYS_LED_SET_PERIOD, &ms) < 0) {
		perror("ioctl");
		close(fd);
		return 1;
	}

	printf("period set to %d ms\n", ms);
	close(fd);
	return 0;
}
