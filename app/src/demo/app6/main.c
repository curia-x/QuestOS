// SPDX-License-Identifier: GPL-2.0
#include <nolibc.h>

char **environ;

int main(int argc, char **argv, char **envp)
{
	printf("============= app 6 ============\r\n");

	while(1)
		printf("app 6 running ...\r\n");

	return 0;
}
