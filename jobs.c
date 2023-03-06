// jobs.c:  Manipulate jobs
// Created by Mack Oct.1 2022

#include <stdio.h>
#include <stdlib.h>
#include "jobs.h"

void init_task(Task *task)
{
	task->taskid = 0;
	task->pid = 0;
	task->srcfd = 0; // defaults to stdin
	task->dstfd = 1; // defaults to stdout
	task->argv = calloc(sizeof(char *), 512);
	task->prev = NULL;
	task->next = NULL;
}

void append_task(Task *task, Task *tasks)
{
	if (tasks == NULL)
	{
		tasks = task;
		return;
	}
	Task *tmp = tasks;
	while (tmp->next != NULL)
		tmp = tmp->next;
	tmp->next = task;
	task->taskid = tmp->taskid + 1;
	task->prev = tmp;
	task->srcfd = task->prev->dstfd + 1;
	task->dstfd = task->srcfd + 1;
}

void free_argv(char **argv)
{
	int i = 0;
	while (argv[i] != 0)
	{
		free(argv[i]);
		i++;
	}
}

void clean_tasks(Task *tasks)
{
	if (tasks == NULL)
		return;
	Task *tmp = tasks;
	while (tmp->next != NULL)
		tmp = tmp->next;
	while (tmp->prev != NULL)
	{
		tmp = tmp->prev;
		free_argv(tmp->next->argv);
		free(tmp->next->argv);
		free(tmp->next);
	}
	free_argv(tmp->argv);
	free(tmp->argv);
	free(tmp);
	tasks = NULL;
}

/**
 * The function name is self-explainatory.
 * @param job: the job to be initialized
 */
void init_job(Job *job)
{
	job->jobid = 0;
	job->chldcnt = 0;
	job->cmdline = malloc(1024 * sizeof(char));
	job->pgid = 0;
	job->tasks = malloc(sizeof(Task));
	init_task(job->tasks);
	job->status = 1;
	job->prev = NULL;
	job->next = NULL;
	job->background = 0;
}

/**
 * Add a job to the job list
 * @param new_job the job to be added
 * @param jobs the job list
 */
int add_job(Job *new_job, Job *jobs)
{
	if (jobs == NULL)
	{
		jobs = new_job;
		return 0;
	}
	Job *tmp = jobs;
	while (tmp->next != NULL)
		tmp = tmp->next;
	tmp->next = new_job;
	new_job->prev = tmp;
	new_job->jobid = tmp->jobid + 1;
	return 0;
}

/**
 * This function allocates and returns a pointer to "the new task"
 * @param job the job to which the task is added
 * @return aforementioned pointer
 */
Task *add_task(Job *jobs)
{
	if (jobs == NULL)
		return NULL;
	Job *tmp = jobs;
	while (tmp->next != NULL)
		tmp = tmp->next;
	Task *task = malloc(sizeof(Task));
	init_task(task);
	append_task(task, tmp->tasks);
	return task;
}

/**
 * This function not only "wait" (as-if) for terminated jobs
 * but also offers an option to print out who had terminated
 * @param jobs the job list
 * @param verbose 1 for printing all jobs
 */
int clean_jobs(Job *jobs, int verbose)
{
	// update entry point
	Job *saved_jobs = jobs;
	Job *saved_entry = jobs->next;
	// if there's only one job (job 0) do nothing
	if (jobs->next == NULL)
		return 0;
	jobs = jobs->next;
	jobs->prev = NULL;

	while (jobs != NULL)
	{
		if (jobs->status == 1)
		{
			// print running jobs if verbose (don't print ourselves)
			if (verbose && jobs->next)
			{
				printf("[");
				printf("%d", jobs->jobid);
				printf("] ");
				printf("running ");
				printf("%s", jobs->cmdline);
				printf("\n");
				fflush(stdout);
			}
			jobs = jobs->next;
		}
		// JOJ specifoc workaround
		// Object-oriented programming (x) JOJ-oriented programming (o)
		else if (verbose || (!verbose && !(jobs->background)))
		{
			// done job needs to be cleared
			Job *tmp = jobs;
			if (jobs->prev != NULL)
			{
				jobs->prev->next = jobs->next;
				if (jobs->next != NULL)
					jobs->next->prev = jobs->prev;
			}
			else
			{
				saved_entry = jobs->next;
				if (jobs->next != NULL)
					jobs->next->prev = NULL;
			}
			jobs = jobs->next;
			clean_tasks(tmp->tasks);
			// print finished background jobs
			if (tmp->background && verbose)
			{
				printf("[");
				printf("%d", tmp->jobid);
				printf("] ");
				printf("done ");
				printf("%s", tmp->cmdline);
				printf("\n");
				fflush(stdout);
			}
			free(tmp->cmdline);
			free(tmp);
		}
		else
			// wait for God to clean those jobs
			// I gotta live in Bengbu
			jobs = jobs->next;
	}
	if (saved_jobs->next != NULL)
		saved_jobs->next = saved_entry;
	return 0;
}

/**
 * This function cleans all jobs in the job list.
 * Only call this function when the shell is exiting
 * @param jobs the job list
 */
int clean_all_jobs(Job *jobs)
{
	// update entry point
	clean_tasks(jobs->tasks);
	free(jobs->cmdline);
	// handle ctrl-d
	if (jobs->next)
	{
		jobs = jobs->next;
		jobs->prev = NULL;
	}
	else
		return 0;

	if (jobs == NULL)
		return 0;
	Job *tmp = jobs;
	while (tmp->next != NULL)
		tmp = tmp->next;
	while (tmp->prev != NULL)
	{
		tmp = tmp->prev;
		clean_tasks(tmp->next->tasks);
		free(tmp->next->cmdline);
		free(tmp->next);
	}
	clean_tasks(tmp->tasks);
	free(tmp->cmdline);
	free(tmp);
	jobs = NULL;
	return 0;
}

/**
 * Shell built-in "jobs" command
 * Wrapper of clean_jobs()
 * @param jobs the job list
 */
int do_jobs(Job *jobs)
{
	return clean_jobs(jobs, 1);
}
