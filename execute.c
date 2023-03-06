// execute.c: handle actual execution of commands
// Created by Mack Sept. 18 2022

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "jobs.h"
#include "execute.h"
#include "cd.h"
#include "pwd.h"

extern Job *current_job;

/**
 * execute - execute "only one" command line
 * @param some_job A Job structure, which contains all the tasks
 * @param jobs for the sake of do_jobs()
 * @return 0 on success, otherwise error
 */
int execute(Job *some_job, Job *jobs)
{
	// save stdin and stdout
	int saved_stdout = dup(STDOUT_FILENO);
	int saved_stdin = dup(STDIN_FILENO);

	int error_code = 0;

	// do nothing if there is no argv[0] in first task (in following tasks, if there's no argv parse() should report error)
	if (some_job->tasks->argv[0] == 0)
	{
		printf("error: missing program\n");
		some_job->status = 0;
		return 0;
	}

	// if there is one and only one built-in command "cd", don't fork()
	if (some_job->tasks->next == NULL)
	{
		if (strcmp(some_job->tasks->argv[0], "cd") == 0 && !(some_job->background))
		{
			Task *curr = some_job->tasks;
			if (curr->argv[1] == 0)
			{
				// cd to $HOME
				// don't handle errors (it's guaranteed that TAs would use cd correctly)
				do_cd(getenv("HOME"));

				// after each successful cd, update OLDPWD
				setenv("OLDPWD", getenv("HOME"), 1);
			}
			else
			{
				// implementation of "cd -" is in do_cd()
				error_code = do_cd(curr->argv[1]);
				if (error_code < 0)
				{
					printf("%s", curr->argv[1]);
					if (errno == ENOENT)
						printf(": No such file or directory\n");
					else if (errno == EACCES)
						printf(": Permission denied\n");
				}
			}
			some_job->status = 0;
			return error_code;
		}
		else if (strcmp(some_job->tasks->argv[0], "exit") == 0 && !(some_job->background))
		{
			printf("exit\n");
			return 114514; // 良い世、来いよ！
		}
	}

	// we expect only one job
	if (some_job->next != NULL)
		return -1;
	// ...but we expect more than one task!
	Task *curr = some_job->tasks;
	Task *next = curr->next;
	Task *prev = curr->prev;

	// prepare fds
	int task_cnt = 0;
	do
	{
		int pipefd[2];
		// check validity of fds
		if (curr->dstfd < 0) // write end of pipe
		{
			// two possibilities: only task or last task
			if (prev == NULL)
			{
				// only task, just skip this task and return
				some_job->status = 0;
				return 0;
			}
			else
			{
				// last task, delete this one
				free(curr->argv);
				prev->next = NULL;
				break;
			}
		}
		if (curr->srcfd < 0) // read end of pipe
		{
			// this only happens to first task, skip this; or if can't, return
			if (next == NULL)
			{
				some_job->status = 0;
				return 0;
			}
			else
			{
				free(curr->argv);
				curr = next;
				next = curr->next;
				prev = NULL;
				curr->prev = NULL;
				// update tasks entry
				some_job->tasks = curr;
			}
		}
		// create pipe
		if (pipe(pipefd) < 0)
			return -1;
		task_cnt++;
		// update fds
		if (next != NULL)
		{
			curr->dstfd = pipefd[1];
			next->srcfd = pipefd[0];
		}
		else
		{
			close(pipefd[1]); // close unused write end
		}
		// update curr, next, prev
		prev = curr;
		curr = next;
		if (next != NULL)
			next = curr->next;
	} while (next != NULL);

	// fd prepared well and piped
	// but...is there an empty task?
	curr = some_job->tasks;
	if (*((curr->argv)[0]) == 0)
	{
		printf("error: missing program\n");
		some_job->status = 0;
		return 0;
	}
	// now do the job!
	do
	{
		curr->pid = fork();
		// set pgid for struct some_job (pgid should be pid of first job)
		if (curr == some_job->tasks && curr->pid != 0) // parent-only
		{
			some_job->pgid = curr->pid;
			// put child to foreground if it's not background
			if (!(some_job->background))
				tcsetpgrp(STDIN_FILENO, some_job->pgid);
		}
		if (curr->pid == 0) // child
		{
			// the most important thing upon successful fork is to restore SIGINT, SIGCHLD and SIGTTOU
			signal(SIGINT, SIG_DFL);
			signal(SIGTTOU, SIG_DFL);
			signal(SIGCHLD, SIG_DFL);

			// if this is the first child (and it's background), set pgid to pid
			if (some_job->background)
			{
				if (curr == some_job->tasks)
					setpgid(getpid(), getpid());
				else
					// attach to pgroup of 1st child
					setpgid(getpid(), some_job->pgid);
			}
			// first do dup2
			// since invalid tasks are already deleted, we don't need to check
			dup2(curr->dstfd, STDOUT_FILENO);
			dup2(curr->srcfd, STDIN_FILENO);
			// next check for built-in commands
			if (strcmp(curr->argv[0], "cd") == 0)
			{
				if (curr->argv[1] == 0)
				{
					// cd to $HOME
					// if $HOME is not set, cd to /
					if (do_cd(getenv("HOME")) < 0)
						do_cd("/");
				}
				else
					error_code = do_cd(curr->argv[1]);
				if (error_code < 0)
				{
					printf("%s", curr->argv[1]);
					if (errno == ENOENT)
						printf(": No such file or directory\n");
					else if (errno == EACCES)
						printf(": Permission denied\n");
				}
				return 114514;
			}
			else if (strcmp(curr->argv[0], "exit") == 0)
			{
				// exit
				// we are essentially subshell tho
				return 114514;
			}
			else if (strcmp(curr->argv[0], "jobs") == 0)
			{
				// list jobs
				do_jobs(jobs);
				return 114514;
			}
			else if (strcmp(curr->argv[0], "pwd") == 0)
			{
				do_pwd();
				return 114514;
			}
			else
			{
				// finally, time to execvp...
				error_code = execvp((curr->argv)[0], curr->argv);
				if (error_code < 0)
					// check errno
					switch (errno)
					{
					case ENOENT:
						printf("%s", (curr->argv)[0]);
						printf(": command not found\n");
						return 114514;
					case EACCES:
						printf("%s", (curr->argv)[0]);
						printf(": Permission denied\n");
						return 114514;
					default:
						// can't handle more...
						break;
					}
				return 114514;
			}
		}
		else // parent
		{
			some_job->chldcnt++;
			// close fds of this child
			if (curr->srcfd != 0)
				close(curr->srcfd);
			if (curr->dstfd != 1)
				close(curr->dstfd);
		}
		error_code = 0;
		curr = curr->next;
	} while (curr != NULL);

	// wait for all children
	// if it's background, don't wait
	int status;
	if (!(some_job->background))
	{
		for (int i = 0; i <= task_cnt; i++)
			waitpid(0, &status, 0);
		some_job->status = 0;
	}
	else
	{
		// print job info
		printf("[%d] %s\n", some_job->jobid, some_job->cmdline);
	}

	// restore stdin and stdout
	dup2(saved_stdin, STDIN_FILENO);
	dup2(saved_stdout, STDOUT_FILENO);
	tcsetpgrp(STDIN_FILENO, getpgrp());

	return error_code;
}
