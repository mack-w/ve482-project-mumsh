// jobs.h: Symbol definitions for struct Job and ways to manipulate them
// Created by Mack on Oct.1 2022

#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

// Task structure definition. Tasks start from taskid 0.
typedef struct _task
{
	int taskid;
	pid_t pid;
	int srcfd;
	int dstfd;
	char **argv;
	struct _task *prev;
	struct _task *next;
} Task;

// Job structure definition. Jobs start from jobid 1;
// Jobid 0 is reserved for the job sequence.
typedef struct _job
{
	int jobid;
	int chldcnt;
	int background;
	char *cmdline;
	pid_t pgid;
	Task *tasks;
	int status;
	struct _job *prev;
	struct _job *next;
} Job;

void init_job(Job *job);
int add_job(Job *new_job, Job *jobs);
Task *add_task(Job *job);
int clean_jobs(Job *jobs, int verbose);
int clean_all_jobs(Job *jobs);
int do_jobs(Job *jobs);

#endif
