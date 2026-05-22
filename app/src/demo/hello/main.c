// SPDX-License-Identifier: GPL-2.0
#include <nolibc.h>

char **environ;

int main(int argc, char **argv, char **envp)
{
	printf("============= app 2 ============\n");

	while (1)
		printf("app 2 running ...\n");

	return 0;
}
