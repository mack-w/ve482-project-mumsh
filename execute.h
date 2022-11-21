// execute.h: definition for the runtine of execution
// Created by Mack Sept. 18 2022

#ifndef EXECUTE_H
#define EXECUTE_H

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include "jobs.h"

int execute(Job *some_job, Job *jobs);

#endif
