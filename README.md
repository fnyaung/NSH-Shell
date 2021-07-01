# NSH-Shell
A virtual command-line shell that allows users to run multiple C programs and to enter command lines.  (C)

# Features of NSH Shell:
## Job Control
There are three types 
of job statuses:
- **Foreground**: When you enter a command in a terminal window, the command occupies that 
terminal window until it completes. This is a foreground job.
    ```
    Prompt > hello
    ```
- **Background**: When you enter an ampersand (&) symbol at the end of a command line, the 
command runs without blocking the terminal window. The shell prompt is displayed 
immediately after you press Return. This is an example of a background job.
    ```
    Prompt > hello &
    ```
- **Stopped**: If you press ctrl-Z while a foreground job is executing, the job stops. 

## **Built-In Commands**
- **jobs**: List the running and stopped background jobs. Status can be “Running”, “Foreground”, 
and “Stopped”. Format is following.
    - [<job_id>] (<pid>) <status> <command_line>
       
       e.g.
        ```
        prompt> jobs
        [1] (30522) Running hello &
        [2] (30527) Stopped sleep
        ```

- **fg** <job_id|pid>: Change a stopped or running background job to a running in the foreground. 
There can only be one foreground job at a time, so the previous foreground job should be 
stopped. A user may use either job_id or pid. In case job_id is used, the JID must be preceded 
by the “%” character. 

    e.g. 
    ```
    prompt> fg %1
    prompt> fg 30522
    ```

- **bg** <job_id|pid>: Change a stopped background job to a running background job.
- **kill** <job_id|pid>: Terminate a job by sending it a SIGINT signal. Be sure to reap a terminated 
process.
- **quit**: Ends the shell process. 

## **I/O Redirection**
- To redirect standard output to a file, the ">" character is used 
like this:
    ``` 
    prompt> ls > file_list.tx
    ```
- To redirect standard input from a file instead of the keyboard, the "<" character is used like this:
    ```
    prompt> sort < file_list.txt
    ```

- To redirect standard output 
to another file like this:
    ``` 
    prompt> sort < file_list.txt > sorted_file_list.txt
    ```