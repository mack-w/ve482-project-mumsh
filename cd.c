#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#include <stdio.h>
#include <stdlib.h>
#include "cd.h"

int do_cd(char *cmdline)
{
	// implement "cd -"
	// don't handle errors (it's guaranteed to be correct)
	if (strcmp(cmdline, "-") == 0)
	{
		// save current working directory
		char *cwd = getcwd(NULL, 0);
		// cd to OLDPWD
		chdir(getenv("OLDPWD"));
		// print OLDPWD
		printf("%s\n", getenv("OLDPWD"));
		// update OLDPWD
		setenv("OLDPWD", cwd, 1);
		// free memory
		free(cwd);
		return 0;
	}
	char *cwd = getcwd(NULL, 0);
	int error_code = chdir(cmdline);
	if (error_code == 0)
		// update OLDPWD
		setenv("OLDPWD", cwd, 1);
	free(cwd);
	return error_code;
}
