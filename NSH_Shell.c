/***
 * Author: Faustina Nyaung 
 * Date: 1/25/2021 11:52 AM
 **/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>

/*
shell programs
built-in commands :
    (5) jobs, bg, fg, kill, and quit
    executed directly by the shell process
general comands :
    executed in a child process -> by using fork
    make sure to reap all terminated child processs.
- MaxLine: 80, MaxArgc: 80, MaxJob: 5
- Use both execvp() and execv() to allow either case.
    execvp() : Linux commands {ls, cat, sort, ./hellp, ./slp}.
    execv() : Linux commands {/bin/ls, /bin/cat, /bin/sort, hello, slp}.
*/

struct jobs
{
    int jid;
    int pid;
    char status[80]; // Running, Foreground, Stopped
    char command_line[80];
};

const int MAXLINE = 80;
const int MAXARGC = 80;
const int MAXJOB = 5;
int jid_count = 0;
struct jobs jobs_arr[6];
bool isStopped = false;
char inputFile[80];       // contains string name of input.txt
char outputFile[80];      // contains string name of output.txt
bool inputExist = false;  // determines if input is requested
bool outputExist = false; // determines if output is requested

void eval(char *cmdline);
int parseline(char *cmdline, char *argv[MAXARGC]);
bool builtin_command(char *argv[MAXARGC]);
int addJob(int jid, pid_t pid, char status[80], char command_line[80]);
bool check_fg_exist();
void initialize_jobs();
void print_jobs();
int is_jid(char *id_proces);
void decrement_jid(char *id_process);
void fg_command(char *id_process);
void bg_command(char *id_process);
void kill_command(char *id_process);
void deleteJob(int pid);
void sigint_handler(int sig);
void sigtstp_handler(int sig);
void swapContent(struct jobs *dest, struct jobs *source);
void sigchld_handler(int sig);

int main()
{
    char cmdline[MAXLINE];
    initialize_jobs();

    // signal handelrs
    signal(SIGINT, sigint_handler);   // ctrl + c
    signal(SIGTSTP, sigtstp_handler); // ctrl + z
    signal(SIGCHLD, sigchld_handler); // triggers when child is stopped or terminated

    while (1)
    {
        /* read */
        printf("prompt> ");
        fgets(cmdline, MAXLINE, stdin);
        if (feof(stdin)) /*ctrl + D triggers EOF*/
            exit(0);

        /*evaluate*/
        eval(cmdline);
    }
}

/**
 * Evaluate command line
 **/
void eval(char *cmdline)
{
    char *argv[MAXARGC]; /*argv for execve()*/
    int bg;              /*should the job run in bg or fb?*/
    pid_t pid;           /*process id*/
    char cpy_cmdline[80];
    strcpy(cpy_cmdline, cmdline);                // duplicate user cmdline
    cpy_cmdline[strlen(cpy_cmdline) - 1] = '\0'; // removes new line

    outputExist = false; // to restart for the next command
    inputExist = false;
    bg = parseline(cmdline, argv); // tokenize command line

    /**
     * General commands should be executed in a child process,
     * which is spawned by the shell process using a fork command.
     * - Note: reap all terminated child process
     **/
    if (!builtin_command(argv))
    { // if we have a general command

        if (jid_count == MAXJOB)
        {
            printf("Can no longer execute any additional jobs! Maximum of 5 jobs only! Please delete one!\n");
            return;
        }

        pid = fork();

        if (pid == 0)
        { /* child runs user job */
            setpgid(0, 0);

            if (inputExist)
            {
                // if the user requested input.txt
                mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO;
                int inFileID = open(inputFile, O_RDONLY, mode);
                dup2(inFileID, STDIN_FILENO);
            }
            if (outputExist)
            {
                // if the user requested output.txt
                // printf("Output does exist!: OutputFile = %s\n", outputFile);
                mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO;
                int outFileID = open(outputFile, O_CREAT | O_WRONLY | O_TRUNC, mode);
                dup2(outFileID, STDOUT_FILENO);
            }

            if (execv(argv[0], argv) < 0 && execvp(argv[0], argv) < 0)
            {
                printf("%s: Command not found. \n", argv[0]);
                exit(0);
            }
        }

        if (!bg)
        { /* parents waits for fg job to terminate */
            // if we successfully added the job, we increment job counter
            if ((addJob(jid_count, pid, "Foreground", cpy_cmdline) == 1))
            {
                jid_count++;
            }

            int status;
            int child_status;
            waitpid(pid, &child_status, WUNTRACED);

            if (WIFEXITED(child_status))
            {
                // printf("child %d normally exited\n", pid);
                deleteJob(pid);
            }
            else if (WIFSTOPPED(child_status))
            {
                // printf("child %d status changed to stopped\n", pid);
            }
        }
        else
        { /* otherwise, don't wait for bg job */
            int child_status;
            if (addJob(jid_count, pid, "Running", cpy_cmdline))
            {
                jid_count++;
            }
        }
    }
    else
    {

        if (strcmp(argv[0], "jobs") == 0)
        {
            print_jobs();
        }
        else if (strcmp(argv[0], "fg") == 0)
        {
            decrement_jid(argv[1]);
            fg_command(argv[1]);
        }
        else if (strcmp(argv[0], "bg") == 0)
        {
            decrement_jid(argv[1]);
            bg_command(argv[1]);
        }
        else if (strcmp(argv[0], "kill") == 0)
        {
            decrement_jid(argv[1]);
            kill_command(argv[1]);
        }
        else if (strcmp(argv[0], "quit") == 0)
        {
            exit(0);
        }
        else
        {
            printf("Invalid command!\n");
        }
    }
}

void decrement_jid(char *id_process)
{
    char jid_str[25];
    int j_id;
    char temp[25];
    if (id_process[0] == '%')
    {
        char temp[80];
        memcpy(temp, &id_process[1], strlen(id_process));
        j_id = atoi(temp) - 1; // decrement user's j_id
        sprintf(temp, "%%%d", j_id);
        strcpy(id_process, temp);
    }
}

/**
 * print the jobs
 * return void
 **/
void print_jobs()
{
    int i;
    for (i = 0; i < MAXJOB; i++)
    {
        if (jobs_arr[i].jid != -1)
        {
            printf("[%d] (%d) %s %s \n", jobs_arr[i].jid + 1, jobs_arr[i].pid, jobs_arr[i].status, jobs_arr[i].command_line);
        }
    }
}

/**
 * checks if a foreground exist
 * return 1 (true) if ; 0 (false) otherwise
 **/
bool check_fg_exist()
{
    int i;
    bool fg_exist = false;
    for (i = 0; i < MAXJOB; i++)
    {
        if (strcmp(jobs_arr[i].status, "Foreground") == 0)
        {
            fg_exist = true;
            break;
        }
    }
    return fg_exist;
}

/**
 * initizlize the array, int : -1, str: Nan
 * return nothing
 **/
void initialize_jobs()
{
    int i;
    for (i = 0; i < MAXJOB + 1; i++)
    {
        jobs_arr[i].jid = -1;
        jobs_arr[i].pid = -1;
        strcpy(jobs_arr[i].status, "N");
        strcpy(jobs_arr[i].command_line, "N");
    }
}

/**
 * returns 1 if successfully added, 0 otherwise.
 * */
int addJob(int jid, pid_t pid, char *status, char *command_line)
{
    if (pid < 1)
        return 0;

    // if we have a foreground satus, we must check if another foreground exist.
    // we can only have one foreground at a time
    int i;
    if ((strcmp(status, "Foreground") == 0))
    {
        // we have a foreground status
        if (!check_fg_exist())
        {
            for (i = 0; i < MAXJOB; i++)
            {
                // check if the job status is N - "None"
                if (strcmp(jobs_arr[i].status, "N") == 0)
                {
                    jobs_arr[i].jid = jid;
                    jobs_arr[i].pid = pid;
                    strcpy(jobs_arr[i].status, status);
                    strcpy(jobs_arr[i].command_line, command_line);
                    return 1;
                }
            }
        }
    }
    else
    {
        // we have a background status
        for (i = 0; i < MAXJOB; i++)
        {
            // check if the job status is N - "None"
            if (strcmp(jobs_arr[i].status, "N") == 0)
            {
                jobs_arr[i].jid = jid;
                jobs_arr[i].pid = pid;
                strcpy(jobs_arr[i].status, status);
                strcpy(jobs_arr[i].command_line, command_line);
                return 1;
            }
        }
    }

    // if we have a duplicate foreground process
    return 0;
}

/**
 * tokenize cmdline and inputs result into argv
 * return bg = 1 (true), 0 (false) otherwise
 **/
int parseline(char *cmdline, char *argv[MAXARGC])
{
    char *token;
    int bg_exist = 0;
    int index = 0;

    token = strtok(cmdline, " \n\t");
    while (token != NULL)
    {
        if (strcmp(token, "&") == 0)
        {
            bg_exist = 1;
            break; // we will not test I/O redirection on a background task.
        }

        if (strcmp(token, ">") == 0 || strcmp(token, "<") == 0 || outputExist || inputExist)
        {
            if (outputExist)
            {
                strcpy(outputFile, token);
            }
            else if (strcmp(token, ">") == 0)
            {
                outputExist = true;
            }
            else if (inputExist)
            {
                strcpy(inputFile, token);
            }
            else if (strcmp(token, "<") == 0)
            {
                inputExist = true;
            }
        }
        else
        {
            argv[index] = token;
            index++;
        }
        token = strtok(NULL, " \n\t");
    }

    argv[index] = NULL;

    // int i;
    // for(i=0; i<=index; i++){
    //     printf("[%d] = >>>%s<<<\n", i, argv[i]);
    // }

    return bg_exist;
}

/**
 * determines if argv contains a built in command
 * returns bool Ture if builtin command exist False otherwise
 **/
bool builtin_command(char *argv[MAXARGC])
{
    char *builtin_commands[] = {"jobs", "fg", "bg", "kill", "quit"};
    bool builtin_exist = false;

    int i;
    for (i = 0; i < MAXJOB; i++)
    {
        if (strcmp(argv[0], builtin_commands[i]) == 0)
        {
            builtin_exist = true;
            break;
        }
    }
    return builtin_exist;
}

/*
Have to create 3 signal handler
sigtstp / sigint / sigchild
*/

/**
 * SIGINT handler
 * no return
 **/
void sigint_handler(int sig)
{
    pid_t pid;
    // when we send the SIGINT, we have to send the entire group of the process
    // we need to stop the foreground -> child of the shell process
    // send to the entire group of the foreground
    int i;
    for (i = 0; i < MAXJOB; i++)
    {
        // check if the job status is Foreground
        if ((strcmp(jobs_arr[i].status, "Foreground") == 0))
        {
            pid = jobs_arr[i].pid;
            // check if it's a child
            // reset to the intializer
            jobs_arr[i].jid = -1;
            jobs_arr[i].pid = -1;
            strcpy(jobs_arr[i].status, "N");
            strcpy(jobs_arr[i].command_line, "N");

            jid_count--;

            kill(pid, SIGINT);
        }
    }

    return;
}

/**
 *  SIGCHLD handler
 *  no return
 **/
void sigchld_handler(int sig)
{
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
    }
    return;
}

/**
 * TSTP handler
 * no return
 **/
void sigtstp_handler(int sig)
{
    pid_t pid;
    int status;
    // when we send the SIGINT, we have to send the entire group of the process
    // we need to stop the foreground -> child of the shell process
    // send to the entire group of the foreground
    int i;
    for (i = 0; i < MAXJOB; i++)
    {
        // check if the job status is Foreground
        if ((strcmp(jobs_arr[i].status, "Foreground") == 0))
        {
            pid = jobs_arr[i].pid;
            // reset to the intializer
            strcpy(jobs_arr[i].status, "Stopped"); // change the status
            kill(pid, SIGSTOP);                    // make it stop
        }
    }
    return;
}

void fg_command(char *id_process)
{
    int j_id;
    pid_t pid;
    int i;
    // if we have a jid, extract the number of jid
    if ((j_id = is_jid(id_process)) >= 0)
    {
        for (i = 0; i < MAXJOB; i++)
        {
            // if we have a matching jid
            if (jobs_arr[i].jid == j_id)
            {
                strcpy(jobs_arr[i].status, "Foreground");
                pid = jobs_arr[i].pid;
                kill(pid, SIGCONT);
                int status;
                int child_status;
                waitpid(pid, &child_status, WUNTRACED);
            }
        }
    }
    else
    {
        // we have a direct pid
        pid = atoi(id_process);
        for (i = 0; i < MAXJOB; i++)
        {
            if (jobs_arr[i].pid == pid)
            {
                strcpy(jobs_arr[i].status, "Foreground");
                kill(pid, SIGCONT);
                int status;
                int child_status;
                waitpid(pid, &child_status, WUNTRACED);
            }
        }
    }
}

void kill_command(char *id_process)
{
    int j_id;
    pid_t pid;
    int i;
    // if we have a jid, extract the number of jid
    if ((j_id = is_jid(id_process)) >= 0)
    {
        for (i = 0; i < MAXJOB; i++)
        {
            // if we have a matching jid
            if (jobs_arr[i].jid == j_id)
            {
                pid = jobs_arr[i].pid;
                deleteJob(pid);
                kill(pid, SIGCONT);
                kill(pid, SIGINT);
            }
        }
    }
    else
    {
        // we have a direct pid
        pid = atoi(id_process);
        for (i = 0; i < MAXJOB; i++)
        {
            if (jobs_arr[i].pid == pid)
            {
                deleteJob(pid);
                kill(pid, SIGCONT);
                kill(pid, SIGINT);
            }
        }
    }
}

/**
 * handle the fg_command's id
 * return the only numbers
**/
int is_jid(char *id_proces)
{
    // int new_id;

    if (id_proces[0] == '%')
    {
        char temp[80];
        // get the string,
        memcpy(temp, &id_proces[1], strlen(id_proces));
        // change it to int
        return atoi(temp);
    }
    return -1; // no found
}

void bg_command(char *id_process)
{
    int j_id;
    pid_t pid;
    int i;

    if ((j_id = is_jid(id_process)) >= 0)
    {
        for (i = 0; i < MAXJOB; i++)
        {
            // if we have a matching jid
            if (jobs_arr[i].jid == j_id)
            {
                if (strcmp(jobs_arr[i].status, "Stopped") == 0)
                {
                    pid = jobs_arr[i].pid;
                    // reset to the intializer
                    strcpy(jobs_arr[i].status, "Running"); // change the status
                    kill(pid, SIGCONT);
                }
            }
        }
    }
    else
    {
        // we have a direct pid
        pid = atoi(id_process);
        for (i = 0; i < MAXJOB; i++)
        {
            if (jobs_arr[i].pid == pid)
            {
                if (strcmp(jobs_arr[i].status, "Stopped") == 0)
                {
                    // reset to the intializer
                    strcpy(jobs_arr[i].status, "Running");
                    kill(pid, SIGCONT);
                }
            }
        }
    }
    return;
}

void swapContent(struct jobs *dest, struct jobs *source)
{
    struct jobs temp = *dest;
    *dest = *source;
    *source = temp;
}

/**
 * Evaluate command line
 **/
void deleteJob(int pid)
{
    int i;
    for (i = 0; i < MAXJOB; i++)
    {
        // check if the job status is Foreground
        if ((jobs_arr[i].pid == pid))
        {
            // reset to the intializer
            jobs_arr[i].jid = -1;
            jobs_arr[i].pid = -1;
            strcpy(jobs_arr[i].status, "N");
            strcpy(jobs_arr[i].command_line, "N");
            jid_count--;
        }
    }
    // sort job_count array
    if (jid_count >= 1)
    {
        for (i = 0; i <= jid_count; i++)
        {
            if ((jobs_arr[i].jid == -1) && (jobs_arr[i + 1].jid != -1))
            {
                jobs_arr[i + 1].jid--;
                swapContent(&jobs_arr[i], &jobs_arr[i + 1]);
            }
        }
    }

    // printf("===== Printing ALL jobs =====\n");
    // for(i=0; i<MAXJOB; i++){
    //     printf("[%d] (%d) %s %s \n", jobs_arr[i].jid+1, jobs_arr[i].pid, jobs_arr[i].status, jobs_arr[i].command_line);
    // }
}