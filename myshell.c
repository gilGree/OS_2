#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

/*to prevent implicit declaration of function*/
int check_if_pipe_included(int count, char **arglist);
int executing_commands(char **arglist);
int executing_commands_in_the_background(int count, char **arglist);
int piping(int index, char **arglist);
int input_redirecting(int count, char **arglist);
int output_redirecting_appending(int count, char **arglist);

int prepare(void) {
    /*After prepare() finishes the process should not terminate upon SIGINT.*/
    if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
        perror("Error - failed to change signal SIGINT handling");
        return -1;
    }
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) { /*dealing with zombies*/
        perror("Error - failed to change signal SIGCHLD handling");
        return -1;
    }
    return 0;
}

int process_arglist(int count, char **arglist) {
    /*Each if condition causes the execution of a function thats responsible for a shell functionality*/
    int return_value = 0;
    int index;
    if (*arglist[count - 1] == '&') {
        return_value = executing_commands_in_the_background(count, arglist);
    } else if (count > 1 && *arglist[count - 2] == '<') {
        return_value = input_redirecting(count, arglist);
    } else if (count > 1 && strcmp(arglist[count - 2],">>")==0) {
        return_value = output_redirecting_appending(count, arglist);
    } else if ((index = check_if_pipe_included(count, arglist)) != -1) {
        return_value = piping(index, arglist);
    } else {
        return_value = executing_commands(arglist);
    }
    return return_value;
}

int finalize(void) {
    return 0;
}

int check_if_pipe_included(int count, char **arglist) {
    for (int i = 0; i < count; i++) {
        if (*arglist[i] == '|') {
            return i;
        }
    }
    return -1;
}

int executing_commands(char **arglist) {
    pid_t pid = fork();
    if (pid == -1) { /*fork failed*/
        perror("Error - failed forking");
        return 0; /*error in the original process*/
    } else if (pid == 0) { /*Child process*/
        if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
            /*Foreground child processes should terminate upon SIGINT*/
            perror("Error - failed to change signal SIGINT handling");
            exit(1);
        }
        if (signal(SIGCHLD, SIG_DFL) ==
            SIG_ERR) { /*restore to default SIGCHLD handling in case that execvp don't change signals*/
            perror("Error - failed to change signal SIGCHLD handling");
            exit(1);
        }
        if (execvp(arglist[0], arglist) == -1) { /*executing command failed*/
            perror("Error - failed executing the command");
            exit(1);
        }
    }
    /*Parent process*/
    if (waitpid(pid, NULL, 0) == -1 && errno != ECHILD && errno != EINTR) {/*ECHILD and EINTR are not considered as errors*/
        perror("Error - waitpid failed");
        return 0; /*error in the original process*/
    }
    return 1; /*no error occurs*/
}

int executing_commands_in_the_background(int count, char **arglist) {
    pid_t pid = fork();
    arglist[count - 1] = NULL;
    if (pid == -1) { /*fork failed*/
        perror("Error - failed forking");
        return 0; /*error in the original process*/
    } else if (pid == 0) { /*Child process*/
        if (signal(SIGCHLD, SIG_DFL) ==
            SIG_ERR) { /*restore to default SIGCHLD handling in case that execvp don't change signals*/
            perror("Error - failed to change signal SIGCHLD handling");
            exit(1);
        }
        if (execvp(arglist[0], arglist) == -1) { /*executing command failed*/
            perror("Error - failed executing the command");
            exit(1);
        }
    }
    /*Parent process*/
    return 1; /*no error occurs*/
}

int piping(int index, char **arglist) {
    /*execute the commands that seperated by piping*/
    int pipefd[2];
    arglist[index] = NULL;
    if (pipe(pipefd) == -1) {
        perror("Error - pipe failed");
        return 0;
    }
    pid_t pid_first = fork(); /*Creating the first child*/
    if (pid_first == -1) { /*fork failed*/
        perror("Error - failed forking");
        return 0; /*error in the original process*/
    } else if (pid_first == 0) { /*First child process*/
        if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
            /*Foreground child processes should terminate upon SIGINT*/
            perror("Error - failed to change signal SIGINT handling");
            exit(1);
        }
        if (signal(SIGCHLD, SIG_DFL) ==
            SIG_ERR) { /*restore to default SIGCHLD handling in case that execvp don't change signals*/
            perror("Error - failed to change signal SIGCHLD handling");
            exit(1);
        }
        close(pipefd[0]);/*This child don't need to read the pipe*/
        if (dup2(pipefd[1], 1) == -1) {
            perror("Error - failed to refer the stdout of the first child to the pipe");
            exit(1);
        }
        close(pipefd[1]);
        if (execvp(arglist[0], arglist) == -1) { /*executing command failed*/
            perror("Error - failed executing the command");
            exit(1);
        }
    }
    /*parent process*/
    pid_t pid_second = fork(); /*Creating the second child*/
    if (pid_second == -1) { /*fork failed*/
        perror("Error - failed forking");
        return 0; /*error in the original process*/
    } else if (pid_second == 0) { /*Second child process*/
        if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
            /*Foreground child processes should terminate upon SIGINT*/
            perror("Error - failed to change signal SIGINT handling");
            exit(1);
        }
        if (signal(SIGCHLD, SIG_DFL) ==
            SIG_ERR) { /*restore to default SIGCHLD handling in case that execvp don't change signals*/
            perror("Error - failed to change signal SIGCHLD handling");
            exit(1);
        }
        close(pipefd[1]);/*This child don't need to write the pipe*/
        if (dup2(pipefd[0], 0) == -1) {
            perror("Error - failed to refer the stdin of the second child from the pipe");
            exit(1);
        }
        close(pipefd[0]);
        if (execvp(arglist[index + 1], arglist + index + 1) == -1) { /*executing command failed*/
            perror("Error - failed executing the command");
            exit(1);
        }
    }
    /*the parent process*/
    /*closing two ends of the pipe*/
    close(pipefd[0]);
    close(pipefd[1]);
    /*waiting for the first child*/
    if (waitpid(pid_first, NULL, 0) == -1 && errno != ECHILD && errno != EINTR) {/*ECHILD and EINTR are not considered as errors*/
        perror("Error - waitpid failed");
        return 0; /*error in the original process*/
    }
    /*waiting for the second child*/
    if (waitpid(pid_second, NULL, 0) == -1 && errno != ECHILD && errno != EINTR) {/*ECHILD and EINTR are not considered as errors*/
        perror("Error - waitpid failed");
        return 0; /*error in the original process*/
    }
    return 1; /*no error occurs*/
}

int input_redirecting(int count, char **arglist) {
    int fd;
    arglist[count-2]=NULL;
    pid_t pid = fork(); /*Creating the child*/
    if (pid == -1) { /*fork failed*/
        perror("Error - failed forking");
        return 0; /*error in the original process*/
    } else if (pid == 0) { /*child process*/
        if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
            /*Foreground child processes should terminate upon SIGINT*/
            perror("Error - failed to change signal SIGINT handling");
            exit(1);
        }
        if (signal(SIGCHLD, SIG_DFL) ==
            SIG_ERR) { /*restore to default SIGCHLD handling in case that execvp don't change signals*/
            perror("Error - failed to change signal SIGCHLD handling");
            exit(1);
        }
        fd = open(arglist[count-1],O_RDONLY);
        if (fd == -1) {
            perror("Error - Failed opening the file");
            exit(1);
        }
        if (dup2(fd, 0) == -1) {
            perror("Error - failed to refer the stdin to the file");
            exit(1);
        }
        if (execvp(arglist[0], arglist) == -1) { /*executing command failed*/
            perror("Error - failed executing the command");
            exit(1);
        }
        close(fd);
    }
    /*Parent process*/
    if (waitpid(pid, NULL, 0) == -1 && errno != ECHILD && errno != EINTR) {/*ECHILD and EINTR are not considered as errors*/
        perror("Error - waitpid failed");
        return 0; /*error in the original process*/
    }
    return 1; /*no error occurs*/
}

int output_redirecting_appending(int count, char **arglist) {
    /*execute the command so that the standard output is redirected to the output file*/
    arglist[count - 2] = NULL;
    pid_t pid = fork();
    if (pid == -1) { /*fork failed*/
        perror("Error - failed forking");
        return 0; /*error in the original process*/
    } else if (pid == 0) { /*Child process*/
        if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
            /*Foreground child processes should terminate upon SIGINT*/
            perror("Error - failed to change signal SIGINT handling");
            exit(1);
        }
        if (signal(SIGCHLD, SIG_DFL) ==
            SIG_ERR) { /*restore to default SIGCHLD handling in case that execvp don't change signals*/
            perror("Error - failed to change signal SIGCHLD handling");
            exit(1);
        }
        int fd = open(arglist[count - 1], O_WRONLY | O_CREAT | O_APPEND,
                      0777); /*create or overwrite a file for redirecting the output of the command and set the permissions in creating*/
        if (fd == -1) {
            perror("Error - Failed opening the file");
            exit(1);
        }
        if (dup2(fd, 1) == -1) {
            perror("Error - failed to refer the stdout to the file");
            exit(1);
        }
        close(fd);
        if (execvp(arglist[0], arglist) == -1) { /*executing command failed*/
            perror("Error - failed executing the command");
            exit(1);
        }
    }
    /*Parent process*/
    if (waitpid(pid, NULL, 0) == -1 && errno != ECHILD && errno != EINTR) {/*ECHILD and EINTR are not considered as errors*/
        perror("Error - waitpid failed");
        return 0; /*error in the original process*/
    }
    return 1; /*no error occurs*/
}
