// SPDX-License-Identifier: GPL-2.0
#include <nolibc.h>

char **environ;

int main(int argc, char **argv, char **envp)
{
	printf("============= app 5 ============\r\n");

	while(1)
		printf("app 5 running ...\r\n");

	return 0;
}
