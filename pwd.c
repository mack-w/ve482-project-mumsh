#include <stdio.h>
#include "pwd.h"

void do_pwd(void)
{
	char cwd[1024];
	getcwd(cwd, sizeof(cwd));
	printf("%s\n", cwd);
	return;
}
