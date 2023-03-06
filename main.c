// main.c: Driver program of mumsh, Mum's shell
// Created by Mack on Sept. 17, 2022

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include "parse.h"
#include "execute.h"
#include "jobs.h"

// error code
// we have: duplicate redir (d), no program (m), and grammar error (designated) for this
// errors on bad fds are reported by parse(), not by us
// cnf and no such file or directory is reported by execute()
// we only report what we think that "we cannot continue to execute()" here
volatile int error_parsing = 0;

// not quite into global variables especially global pointers, but have to
Job *current_job = NULL;
Job *global_jobs_ptr = NULL;

// signal handler. deals: SIGINT and SIGCHLD
void sig_handler(int signo, siginfo_t *siginfo, void *context);

int main()
{
	// disable stdout buffering
	setbuf(stdout, NULL);

	// set pgroup
	setpgid(getpid(), getpid());
	tcsetpgrp(STDIN_FILENO, getpgrp());

	// signal handling
	struct sigaction sigint_act;
	sigint_act.sa_sigaction = sig_handler;
	sigint_act.sa_flags = SA_SIGINFO | SA_RESTART; // AFAIC all POSIX systems are safe to use SA_RESTART
	sigaction(SIGINT, &sigint_act, NULL);
	sigaction(SIGCHLD, &sigint_act, NULL);
	signal(SIGTTOU, SIG_IGN);

	int return_code = 0;
	// issue a job sequence and initilize it
	Job *jobs = malloc(sizeof(Job));
	init_job(jobs); // this job has jobid 0, meaning it won't be executed
	global_jobs_ptr = jobs;

	// instruction and storage for incremental parsing
	int incremental_parse = 0;
	char *cmdline_save = calloc(sizeof(char), 1026);

	// Exciting! Main RPEL!
	do
	{
		char *cmdline = calloc(sizeof(char), 1026);

		// Print prompt
		if (incremental_parse)
			printf("> ");
		else
			printf("mumsh $ ");

		// Read command line
		fgets(cmdline, 1025, stdin);

		// handle Ctrl-D
		if (feof(stdin))
		{
			printf("exit\n");
			free(cmdline);
			break;
		}
		// handle no input
		if (strlen(cmdline) == 1 && cmdline[0] == '\n')
		{
			// "wait" for finished jobs
			clean_jobs(jobs, 0);
			free(cmdline);
			continue;
		}
		
		// do strcat
		if (incremental_parse)
		{
			char *temp = malloc(1026 * sizeof(char));
			strcpy(temp, cmdline_save);
			strcat(temp, cmdline);
			memset(cmdline, 0, 1026);
			strcpy(cmdline, temp);
			free(temp);
		}
		// for the sake of parse()
		int str_len = (int)strlen(cmdline); // prevent heap buffer overflow on empty input
		cmdline[str_len - 1] = ' ';
		cmdline[str_len] = -1;

		// no error, reset incremental_parse
		incremental_parse = 0;

		// Parse Input
		Job *new_job = malloc(sizeof(Job));
		init_job(new_job);
		return_code = parse(cmdline, new_job);
		// restore cmdline state after parse() succeeds
		cmdline[str_len - 1] = 0;
		cmdline[str_len] = 0;
		// issue corresponding error message to stderr
		if (error_parsing)
		{
			switch (error_parsing)
			{
			case '<':
				printf("syntax error near unexpected token `<'\n");
				clean_all_jobs(new_job);
				free(new_job);
				free(cmdline);
				break;
			case '>':
				printf("syntax error near unexpected token `>'\n");
				clean_all_jobs(new_job);
				free(new_job);
				free(cmdline);
				break;
			case '|':
				printf("syntax error near unexpected token `|'\n");
				clean_all_jobs(new_job);
				free(new_job);
				free(cmdline);
				break;
			case 'i':
				printf("error: duplicated input redirection\n");
				clean_all_jobs(new_job);
				free(new_job);
				free(cmdline);
				break;
			case 'o':
				printf("error: duplicated output redirection\n");
				clean_all_jobs(new_job);
				free(new_job);
				free(cmdline);
				break;
			case 'm':
				printf("error: missing program\n");
				clean_all_jobs(new_job);
				free(new_job);
				free(cmdline);
				break;
			default:
				break;
			}
			error_parsing = 0;
			continue;
		}
		if (return_code != 0)
		{
			// encountered incomplete input
			clean_all_jobs(new_job);
			free(new_job);
			// save command line
			incremental_parse = 1;
			if (return_code <= 3)
				// in quote, retain "original" cmdline
				cmdline[str_len - 1] = '\n';
			strcpy(cmdline_save, cmdline);
			free(cmdline);
			error_parsing = 0;
			continue;
		}

		// add new job to job list
		add_job(new_job, jobs);

		// All Safe, Execute Command
		error_parsing = 0;
		current_job = new_job;
		return_code = execute(new_job, jobs);
		current_job = NULL;

		clean_jobs(jobs, 0);

		// cleanup
		free(cmdline);
		if (return_code == 114514)
			// exit
			break;
	} while (1);

	// cleanup
	clean_all_jobs(jobs);
	free(jobs);
	free(cmdline_save);
	if (return_code == 114514)
		return_code = 0;
	return return_code;
}

void sig_handler(int signo, siginfo_t *siginfo, void *context)
{
	// suppress compiler warning
	while (context)
		break;
	// handle Ctrl-C
	if (signo == SIGINT)
	{
		if (current_job)
		{
			// kill all processes in current job
			killpg(current_job->pgid, SIGINT);
			// set job status to stopped
			current_job->status = 0;
			// set errno to 0 (prevent another exit)
			errno = 0;
		}
		else
		{
			// print new prompt
			// this is default behavior of Bash
			printf("\n");
			// "wait" for finished jobs
			clean_jobs(global_jobs_ptr, 0);
			printf("mumsh $ ");
		}
	}
	// handle SIGCHLD when there's some background job
	else if (signo == SIGCHLD && current_job == NULL)
	{
		// only return if the last process in the job has exited
		// so we find the last process in every jobs
		pid_t target_pid = siginfo->si_pid;

		// !! THIS IS UGLY DO NOT TRY TO COPY ME !!
		// I JUST DON'T WANNA MAINTAIN YET ANOTHER GLOBAL ARRAY
		Job *tmp = global_jobs_ptr;
		global_jobs_ptr = global_jobs_ptr->next;
		if (global_jobs_ptr == NULL)
		{
			global_jobs_ptr = tmp;
			return;
		}
		do
		{
			int brk = 0;
			Task *temp = global_jobs_ptr->tasks;
			while (global_jobs_ptr->tasks != NULL)
			{
				if (global_jobs_ptr->tasks->pid == target_pid)
				{
					global_jobs_ptr->chldcnt--;
					if (global_jobs_ptr->chldcnt == 0)
					{
						brk = 0;
						// set job to be done
						global_jobs_ptr->status = 0;
						// restore current task ptr
						global_jobs_ptr->tasks = temp;
						break;
					}
				}
				global_jobs_ptr->tasks = global_jobs_ptr->tasks->next;
			}
			global_jobs_ptr->tasks = temp;
			global_jobs_ptr = global_jobs_ptr->next;
			if (brk == 0)
				break;
		} while (global_jobs_ptr->next != NULL);
		// restore global_jobs_ptr
		global_jobs_ptr = tmp;

		// !! END VERY UGLY PART !!
	}
	return;
}
