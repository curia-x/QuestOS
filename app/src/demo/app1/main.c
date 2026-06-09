// SPDX-License-Identifier: GPL-2.0
#include <nolibc.h>

char **environ;

int main(int argc, char **argv, char **envp)
{
	printf("============= app 1 ============\r\n");

	while (1)
		printf("app 1 running ...\r\n");

	return 0;
}
