// parse.c: Syntax parser
// Created by Mack Sept. 18 2022

#include <stdio.h>
#include <stdint.h>
#include "parse.h"
#include "execute.h"
#include "jobs.h"

extern int error_parsing;

/* !! UGLY HELPER FUNCTION !! */
int single_quote_handler(int *in_single_quote, int *in_double_quote, char **current, char **curr_token_begin,
			 int *in_token, int *dest_mem_filled, char ***current_argv, char **dest_ptr)
{
	if (*in_single_quote)
	{
		*in_single_quote = 0;
		**current = '\0';
		// if nothing is quoted do nothing
		if (*current == *curr_token_begin)
			*curr_token_begin = *current + 1;
		else
		{
			// save this token to task, don't move argv
			// arg len: 256 chars on MS Windows is kinda "grace"
			if (*in_token)
				*dest_ptr = **current_argv;
			if (*dest_mem_filled)
				strcat(*dest_ptr, *curr_token_begin);
			else
			{
				if (*in_token)
				{
					**current_argv = calloc(sizeof(char), 256);
					*dest_ptr = **current_argv;
				}
				strcpy(*dest_ptr, *curr_token_begin);
				*dest_mem_filled = 1;
			}
			**current = 39;
			*curr_token_begin = *current + 1;
		}
	}
	else if (!(*in_double_quote))
	{
		// begin of single quote
		**current = 0;
		*in_single_quote = 1;
		// do we have dangling token?
		if (*current - *curr_token_begin)
		{
			// for strcpy needs to know the length
			if (*in_token)
				*dest_ptr = **current_argv;
			if (*dest_mem_filled)
				strcat(*dest_ptr, *curr_token_begin);
			else
			{
				if (*in_token)
				{
					**current_argv = calloc(sizeof(char), 256);
					*dest_ptr = **current_argv;
				}
				strcpy(*dest_ptr, *curr_token_begin);
				*dest_mem_filled = 1;
			}
		}
		*curr_token_begin = *current + 1;
	}
	return 0;
}

/* !! UGLY HELPER FUNCTION !! */
int double_quote_handler(int *in_single_quote, int *in_double_quote, char **current, char **curr_token_begin,
			 int *in_token, int *dest_mem_filled, char ***current_argv, char **dest_ptr)
{
	if (*in_double_quote)
	{
		*in_double_quote = 0;
		**current = '\0';
		// if nothing is quoted do nothing
		if (*current == *curr_token_begin)
			*curr_token_begin = *current + 1;
		else
		{
			// save this token to task, don't move argv
			// arg len: 256 chars on MS Windows is kinda "grace"
			if (*in_token)
				*dest_ptr = **current_argv;
			if (*dest_mem_filled)
				strcat(*dest_ptr, *curr_token_begin);
			else
			{
				if (*in_token)
				{
					**current_argv = calloc(sizeof(char), 256);
					*dest_ptr = **current_argv;
				}
				strcpy(*dest_ptr, *curr_token_begin);
				*dest_mem_filled = 1;
			}
			**current = '"';
			*curr_token_begin = *current + 1;
		}
	}
	else if (!(*in_single_quote))
	{
		// begin of double quote
		**current = 0;
		*in_double_quote = 1;
		// do we have dangling token?
		if (*current - *curr_token_begin)
		{
			// for strcpy needs to know the length
			if (*in_token)
				*dest_ptr = **current_argv;
			if (*dest_mem_filled)
				strcat(*dest_ptr, *curr_token_begin);
			else
			{
				if (*in_token)
				{
					**current_argv = calloc(sizeof(char), 256);
					*dest_ptr = **current_argv;
				}
				strcpy(*dest_ptr, *curr_token_begin);
				*dest_mem_filled = 1;
			}
		}
		*curr_token_begin = *current + 1;
	}
	return 0;
}

/**
 * @brief Helper function to prepare file descriptors.
 * @param redir The file to be opened
 * @param fd Desired file descriptor
 * @param mode 1 for input, 2 for output, 3 for append
 * @return always 0
 */
int prepare_fd(char *redir, int *fd, int mode)
{
	switch (mode)
	{
	case 1: // input redir
		*fd = open(redir, O_RDONLY);
		if (*fd == -1)
			// we want to report error here
			switch (errno)
			{
			case 1: // EPERM
				printf("%s", redir);
				printf(": Permission denied\n");
				break;
			case 2: // ENOENT
				printf("%s", redir);
				printf(": No such file or directory\n");
			}
		// we don't do the actual job of dup2() or pipe() here, dispatch it to execute()
		break;
	case 2: // output redir
		*fd = open(redir, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (*fd == -1)
		{
			// the only reason why we can't write to a file is Permission denied
			printf("%s", redir);
			printf(": Permission denied\n");
		}
		break;
	case 3: // append redir
		*fd = open(redir, O_WRONLY | O_CREAT | O_APPEND, 0644);
		if (*fd == -1)
		{
			printf("%s", redir);
			printf(": Permission denied\n");
		}
		break;
	default: // error
		return EBADF;
	}
	return 0;
}

/**
 * @brief Command line parser.
 * Handles quotes, redirections and pipes well.
 * @param cmdline Command line
 * @param new_job Pointer to pre malloc'd job struct
 * @return 0 on success; 1 if waiting for single quote; 2 double; 4 pipe;
 * 8 input redir; 16 output; 32 append; value to be OR'd
 */
int parse(char *cmdline, Job *new_job)
{
	// okay, whatever we do first prepare a backup of cmdline
	char *cmdline_save = calloc(sizeof(char), 1026);
	strcpy(cmdline_save, cmdline);

	// indicators
	int in_token = 0;
	if (*cmdline == '<' || *cmdline == '>' || *cmdline == '|')
		in_token = 0;
	int in_single_quote = 0;
	int in_double_quote = 0;
	int wait_for_pipe = 0;
	int in_inredir = 0;
	int in_outredir = 0;
	int in_appredir = 0;
	int in_redirected = 0;
	int out_redirected = 0;

	int dest_mem_filled = 0;
	char *dest_ptr = NULL;

	// pointers
	char *curr_token_begin = cmdline;
	char *prev_token_end = cmdline;
	char *current = cmdline;
	char *current_next = NULL;

	// we want to maintain a pointer to current task
	Task *current_task = new_job->tasks;
	// more specifically a pointer to current operating argv is desired
	char **current_argv = new_job->tasks->argv;
	dest_mem_filled = 0;

	// pre-allocate memory for redirections
	char *inredir = calloc(sizeof(char), 256);
	char *outredir = calloc(sizeof(char), 256);
	char *appredir = calloc(sizeof(char), 256);

	// set job internals ready
	memcpy(new_job->cmdline, cmdline, strlen(cmdline));
	new_job->cmdline[strlen(cmdline) - 1] = 0;
	new_job->cmdline[strlen(cmdline)] = 0;

	// begin parsing
	// !! WARNING: SHIT PILE BELOW !!
	while (*current != -1)
	{
		switch (*current)
		{
		case 39: // single quote
			// UGLY: FOR CPPLINT CHECK
			single_quote_handler(&in_single_quote, &in_double_quote, &current, &curr_token_begin,
					     &in_token, &dest_mem_filled, &current_argv, &dest_ptr);
			break;

		case 34: // double quote
			// UGLY: FOR CPPLINT CHECK
			double_quote_handler(&in_single_quote, &in_double_quote, &current, &curr_token_begin,
					     &in_token, &dest_mem_filled, &current_argv, &dest_ptr);
			break;

		case 32: // space
			// only do something if we are not in quote
			if (!in_double_quote && !in_single_quote)
			{
				// end of token
				*current = 0;
				// check in redirection...
				if (in_appredir)
				{
					in_appredir = 0;
					if (!out_redirected)
					{
						if (dest_mem_filled)
							strcat(appredir,
							       curr_token_begin);
						else
						{
							strcpy(appredir,
							       curr_token_begin);
							dest_mem_filled = 1;
						}
						out_redirected = 1;
						prev_token_end = current - 1;
					}
					else
					{
						error_parsing =
						    'o'; // duplicate output redirection
						goto accidental_end;
					}
				}
				else if (in_inredir)
				{
					in_inredir = 0;
					if (!in_redirected)
					{
						if (dest_mem_filled)
							strcat(inredir,
							       curr_token_begin);
						else
						{
							strcpy(inredir,
							       curr_token_begin);
							dest_mem_filled = 1;
						}
						in_redirected = 1;
						prev_token_end = current - 1;
					}
					else
					{
						error_parsing = 'i'; // duplicate input redirectino
						goto accidental_end;
					}
				}
				else if (in_outredir)
				{
					in_outredir = 0;
					if (!out_redirected)
					{
						if (dest_mem_filled)
							strcat(outredir,
							       curr_token_begin);
						else
						{
							strcpy(outredir,
							       curr_token_begin);
							dest_mem_filled = 1;
						}
						out_redirected = 1;
						prev_token_end = current - 1;
					}
					else
					{
						error_parsing = 'o';
						goto accidental_end;
					}
				}
				// also check for pipe
				else if (wait_for_pipe)
				{
					current_next = current + 1;
					if (*current_next != 32)
						wait_for_pipe = 0;
					curr_token_begin = current + 1;
				}
				// finally, save to token
				// note: only need to do so if we are in token; skip if not
				else if (in_token)
				{
					if (dest_mem_filled)
						strcat(*current_argv,
						       curr_token_begin);
					else
					{
						*current_argv = calloc(sizeof(char), 256);
						strcpy(*current_argv,
						       curr_token_begin);
						dest_mem_filled = 1;
					}
					// now is time to move argv pointer
					current_argv++;
					// set previous token end position
					prev_token_end = current - 1;
				}
				// disable flags (if needed; if no token encountered, shouldn't disable)
				in_token = 0;
				dest_mem_filled = 0;
				if (current_argv > current_task->argv) // token should have been saved above
					wait_for_pipe = 0;
				// don't touch redir carelessly
				if (*inredir)
					in_inredir = 0;
				if (*outredir)
					in_outredir = 0;
				if (*appredir)
					in_appredir = 0;
				curr_token_begin = current + 1;
			}
			// if in quote do nothing
			break;

		case 124: // pipe
			// if in quote, do nothing
			if (in_double_quote || in_single_quote)
			{
				if (!in_appredir && !in_outredir && !in_inredir)
					in_token = 1;
				break;
			}
			// okay, we are not in quote
			// check for grammar error
			if (current == cmdline)
			{
				// pipe is the first token
				error_parsing = '|';
				goto accidental_end;
			}
			// and more grammar checks...
			switch (*prev_token_end)
			{
			case 60: // input redirection
				error_parsing = '|';
				goto accidental_end;
			case 62: // output redirection
				error_parsing = '|';
				goto accidental_end;
			case 124: // pipe
				error_parsing =
				    'm'; // m stands for missing program
				goto accidental_end;
			}
			// now we are safe for grammar errors
			// check if in token. if it is copy it to argv
			if (in_token && !(in_double_quote || in_single_quote))
			{
				*current = '\0';
				if (dest_mem_filled)
					strcat(*current_argv, curr_token_begin);
				else
				{
					*current_argv = calloc(sizeof(char), 256);
					strcpy(*current_argv, curr_token_begin);
					dest_mem_filled = 1;
				}
				current_argv++;
				curr_token_begin = current + 1;
				// restore pipe
				*current = '|';
			}
			// check if input redirection (only for first task)
			else if (in_inredir && current_task->taskid == 0)
			{
				if (!in_redirected)
				{
					if (dest_mem_filled)
						strcat(*current_argv,
						       curr_token_begin);
					else
					{
						*current_argv = calloc(sizeof(char), 256);
						strcpy(*current_argv,
						       curr_token_begin);
						dest_mem_filled = 1;
					}
				}
				else
				{
					error_parsing = 'i';
					goto accidental_end;
				}
				curr_token_begin = current + 1;
			}
			// throw an error if in output redirection or duplicate input
			else if (out_redirected || (in_inredir && current_task->taskid > 0))
			{
				if (in_inredir || in_redirected)
					error_parsing = 'i';
				else
					error_parsing = 'o';
				goto accidental_end;
			}
			// now we are safe for everything
			// fds for this task is not set, set them
			if (*inredir)
				// we care only about assigning fd, errors are handled by execute();
				// if invalid fd, no execvp for this task and pipes go through
				prepare_fd(inredir, &(current_task->srcfd), 1);
			if (*outredir)
				prepare_fd(outredir, &(current_task->dstfd), 2);
			if (*appredir)
				prepare_fd(appredir, &(current_task->dstfd), 3);
			// terminate argv[] by NULL (reqired by execvp)
			current_argv = NULL;
			// request a new task and acquire argv pointer
			current_task = add_task(new_job);
			current_argv = current_task->argv;
			// clear file names
			memset(inredir, 0, 256);
			memset(outredir, 0, 256);
			memset(appredir, 0, 256);
			// do the rountine
			in_token = 0;
			dest_mem_filled = 0;
			in_redirected = 1; // we pipe output of "this" task to "next" task
			in_inredir = 0;
			in_outredir = 0;
			in_appredir = 0;
			wait_for_pipe = 1;
			prev_token_end = current;
			break;

		case 60: // input redirection
			// if in quote, do nothing
			if (in_single_quote || in_double_quote)
			{
				if (!in_appredir && !in_outredir && !in_inredir)
					in_token = 1;
				break;
			}
			// okay, we are not in quote

			// check if already redirected
			if (in_redirected)
			{
				error_parsing = 'i';
				goto accidental_end;
			}
			// check for grammar error
			else if ((*(prev_token_end) == '<' || *prev_token_end == '|' ||
				  *prev_token_end == '>') &&
				 prev_token_end != cmdline)
			{
				error_parsing = '<';
				goto accidental_end;
			}
			// check whether there is token
			if (in_token || in_appredir || in_outredir)
			{
				*current = '\0';
				if (in_token)
					dest_ptr = *current_argv;
				else if (in_appredir)
				{
					dest_ptr = appredir;
					in_appredir = 0;
					out_redirected = 1;
				}
				else
				{
					dest_ptr = outredir;
					in_outredir = 0;
					out_redirected = 1;
				}
				if (dest_mem_filled)
					strcat(dest_ptr, curr_token_begin);
				else
				{
					if (in_token)
					{
						*current_argv = calloc(sizeof(char), 256);
						dest_ptr = *current_argv;
					}
					strcpy(dest_ptr, curr_token_begin);
					dest_mem_filled = 1;
				}
				if (in_token)
					current_argv++;
				curr_token_begin = current + 1;
				// restore current
				*current = '<';
			}
			// okay, we are safe for a new redirection
			// set flags and pointers
			in_inredir = 1;
			// note: we don't want to set "redirected" here
			// since we have not been "redirected" yet.
			// set "redirected" after a new token is encountered.
			in_token = 0;
			prev_token_end = current;
			// directly go to next char except space (makes lives easier for current++)
			do
			{
				current++;
			} while (*current == ' ');
			current--;
			curr_token_begin = current + 1;
			dest_ptr = inredir;
			dest_mem_filled = 0;
			break;

		case 62: // output redirection
			if (in_single_quote || in_double_quote)
			{
				if (!in_appredir && !in_outredir && !in_inredir)
					in_token = 1;
				break;
			}

			if (out_redirected)
			{
				error_parsing = 'o';
				goto accidental_end;
			}

			else if ((*prev_token_end == '<' || *prev_token_end == '|' ||
				  *prev_token_end == '>') &&
				 prev_token_end != cmdline)
			{
				error_parsing = '>';
				goto accidental_end;
			}

			// check token
			if (in_token || in_inredir)
			{
				*current = '\0';
				if (in_token)
					dest_ptr = *current_argv;
				else
				{
					dest_ptr = inredir;
					in_inredir = 0;
					in_redirected = 1;
				}
				if (dest_mem_filled)
					strcat(dest_ptr, curr_token_begin);
				else
				{
					if (in_token)
					{
						*current_argv = calloc(sizeof(char), 256);
						dest_ptr = *current_argv;
					}
					strcpy(dest_ptr, curr_token_begin);
					dest_mem_filled = 1;
				}
				if (in_token)
					current_argv++;
				curr_token_begin = current + 1;
				*current = '>';
			}
			// okay, we are safe
			// things got a bit tricky here: we need to know which mode
			// are we opening TRUNC? or are we opening APPEND?
			current_next = current + 1;
			if (*current_next == 62)
			{
				// APPEND
				in_appredir = 1;
				current = current_next;
				prev_token_end = current;
				do
				{
					current++;
				} while (*current == ' ');
				current--;
				dest_ptr = appredir;
				dest_mem_filled = 0;
				curr_token_begin = current + 1;
			}
			else if (*current_next != 0)
			{
				in_outredir = 1;
				prev_token_end = current;
				do
				{
					current++;
				} while (*current == ' ');
				current--;
				dest_ptr = outredir;
				dest_mem_filled = 0;
				curr_token_begin = current + 1;
			}
			in_token = 0;
			break;

		default:
			// we don't handle '&' here, leave it alone
			if (*current == '&')
				break;
			if (!(in_inredir || in_outredir || in_appredir))
				in_token = 1;
			if (wait_for_pipe && *current != ' ')
			{
				wait_for_pipe = 0;
				curr_token_begin = current;
			}
			break;
		}

		// whatever we do, move to next character
		current++;
	}
	// !! END SHIT PILE !!

	// at end of command line... lingering issues...
	if (!(in_single_quote || in_double_quote || in_appredir || in_inredir || in_outredir || wait_for_pipe))
	{
		// now we are at the end of command line, do we have files to open...
		// caution, we need to handle this only if we're not in quote, pipe, redir etc.
		if (*inredir)
		{
			prepare_fd(inredir, &current_task->srcfd, 1);
			// since we are the last task
			// and we have valid input redir
			// we need to (re)set the flag
			in_inredir = 0;
		}
		// bind stdout to end of task
		if (*outredir)
			prepare_fd(outredir, &current_task->dstfd, 2);
		else if (*appredir)
			prepare_fd(appredir, &current_task->dstfd, 3);
		else
			current_task->dstfd = 1;
		// terminate argv[] by NULL (reqired by execvp)
		current_argv = NULL;
		// last thing, are we background?
		while (*current == '\0' || *current == -1)
		{
			if (current == cmdline)
				break;
			current--;
		}
		if (*current == '&')
			new_job->background = 1;
		else if (*current == '|' && (!in_single_quote && !in_double_quote))
			wait_for_pipe = 1;
	}

	// cleanup
	goto end;

end:
	// restore original cmdline
	memset(cmdline, 0, 1026);
	strcpy(cmdline, cmdline_save);
	free(cmdline_save);
	free(inredir);
	free(outredir);
	free(appredir);
	return (in_appredir << 5) | (in_double_quote << 1) | in_single_quote | (in_inredir << 3) | (in_outredir << 4) |
	       (wait_for_pipe << 2);
accidental_end:
	memset(cmdline, 0, 1026);
	strcpy(cmdline, cmdline_save);
	free(cmdline_save);
	free(inredir);
	free(outredir);
	free(appredir);
	new_job->status = 0;
	new_job->background = 0;
	return (in_appredir << 5) | (in_double_quote << 1) | in_single_quote | (in_inredir << 3) | (in_outredir << 4) |
	       (wait_for_pipe << 2);
}
