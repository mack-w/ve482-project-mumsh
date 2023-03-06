// parse.h: Syntax parser definitions
// created by Mack Sept.18 2022

#ifndef PARSE_H
#define PARSE_H

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "jobs.h"

int parse(char *cmdline, Job *new_job);

#endif
