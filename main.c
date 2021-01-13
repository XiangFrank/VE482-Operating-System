//
// Created by frank on 2019/9/18.
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

int pid_set[20];
int In = 0;
int Out = 1;
int D_quote = 0;
int S_quote = 0;
int L_arrow = 0;
int R_arrow = 0;
int PIPE = 0;
char*** commands_set;
int size[20];
int num = 0;
int n = 0;
int m = 0;
int background_or_not = 0;
int total_jobs = 0;
char command[1025];
int command_len = 0;

struct background_info{
    //int job_ID;
    //int status[20][1];
    int len_command;
    int process_num;
    int process_ID[20];
    char command[1025];
};

struct background_info list[20];

void pwd(){
    char* buffer = malloc(50 * sizeof(char));
    char* cwd = getcwd(buffer, 50);
    if(NULL == cwd){
        perror("Get current working directory fail.\n");
        exit(-1);
    }
    else{
        printf("%s\n", cwd);
    }
    free(buffer);
}

void jobs(){
    for(int i = 0; i < total_jobs; i ++){
        int status = 0;
        printf("[%d] ", i + 1);
        for(int j = 0; j < list[i].process_num; j++){
            if(waitpid(list[i].process_ID[j], 0, WNOHANG) == 0) status = 1;
        }
        if(status == 1) printf("running ");
        else printf("done ");
        for(int j = 0; j < list[i].len_command; j ++){
            printf("%c", list[i].command[j]);
        }
        printf("\n");
    }
}

void execute_command(char** command, int i, int pid){
    if(pid == 0) {
        char* commands[2*i];
        int count = 0;
        for (int k = 0; k < i; k++) {
            if(strcmp(command[k], "\0") == 0) {
                continue;
            }
            else if(strcmp(command[k], "<\0") == 0){
                while(k < i-1 && strcmp(command[k+1], "\0") == 0){
                    k++;
                }
                int fd = open(command[k+1], O_RDONLY, 0777);
                if(fd == -1) {
                    printf("%s: No such file or directory\n", command[k + 1]);
                    exit(1);
                    //Out = 2;
                }
                if(In != 0) {
                    printf("error: duplicated input redirection\n");
                    close(In);
                    exit(1);
                    //Out = 2;
                }
                In = fd;
                k ++;
            }
            else if(strcmp(command[k], ">\0") == 0){
                while(k < i-1 && strcmp(command[k+1], "\0") == 0){
                    k++;
                }
                int fd = open(command[k+1],O_CREAT|O_WRONLY|O_TRUNC, 0777);
                if(fd == -1) {
                    printf("%s: Permission denied\n", command[k + 1]);
                    exit(1);
                }
                if(Out != 1) {
                    printf("error: duplicated output redirection\n");
                    close(Out);
                    exit(1);
                    //Out = 2;
                }
                Out = fd;
                k ++;
            }
            else if(strcmp(command[k], ">>\0") == 0){
                while(k < i-1 && strcmp(command[k+1], "\0") == 0){
                    k++;
                }
                int fd = open(command[k+1],O_WRONLY|O_CREAT|O_APPEND, 0777);
                if(fd == -1) {
                    //perror(command[k + 1]);
                    printf("%s: Permission denied\n", command[k + 1]);
                    exit(1);
                }
                if(Out != 1) {
                    printf("error: duplicated output redirection\n");
                    close(Out);
                    exit(1);
                    //Out = 2;
                }
                Out = fd;
                k ++;
            }
            else{
                commands[count] = command[k];
                count++;
            }
        }
        commands[count] = NULL;
        dup2(In, 0);
        dup2(Out, 1);
        if(Out != 1) close(Out);
        if(In != 0) close(In);
        if(commands[0] != NULL) {
            if (strcmp(commands[0], "pwd\0") == 0) pwd();
            else if (strcmp(commands[0], "cd\0") == 0);
            else if (strcmp(commands[0], "jobs\0") == 0);
            else {
                int ret = 0;
                ret = execvp(commands[0], commands);
                if(ret == -1) {
                    printf("%s: command not found\n", commands[0]);
                    exit(1);
                }
            }
        }
        exit(1);
    }
    else{
        char* commands = NULL;
        int cd_or_not = 0;
        for (int k = 0; k < i; k++) {
            if(strcmp(command[k], "\0") == 0) {
                continue;
            }
            else if(strcmp(command[k], "cd\0") == 0){
                cd_or_not = 1;
            }
            else if(strcmp(command[k], "jobs\0") == 0) jobs();
            else {
                if(cd_or_not == 0) return;
                commands = command[k];
                break;
            }
        }
        if(commands != NULL) {
            int id = chdir(commands);
            if(id < 0) perror(commands);
        }
    }
}

void signal_handler(){
    int i = 0;
    while(pid_set[i] != -1){
        kill(pid_set[i], 9);
        i ++;
    }
    background_or_not = 0;
    In = 0;
    Out = 1;
    D_quote = 0;
    S_quote = 0;
    PIPE = 0;
    num = 0;
    n = 0;
    m = 0;
    if(i == 0){
        printf("\nmumsh $ ");
        fflush(stdout);
    }
}

int get_commands(){
    int j = 0;
    char c;
    c = getchar();
    if(c == EOF){
        printf("exit\n");
        fflush(stdout);
        for(int k = 0; k < 20; k++){
            for(int l = 0; l < 128; l++){
                free(commands_set[k][l]);
            }
            free(commands_set[k]);
        }
        free(commands_set);
        return -1;
    }
    while (c != '\n') {
        if(c == '&') background_or_not = 1;
        else if(c != ' ') background_or_not = 0;
        command[j] = c;
        j++;
        c = getchar();
    }
    j++;
    command[j-1] = ' ';
    command_len = j;
    for(int k = 0; k < j; k++){
        if(D_quote == 1 || S_quote == 1){
            if(command[k] == '"' && D_quote == 1){
                D_quote = 0;
                //commands_set[num][m][n] = '\0';
                //n = 0;
                //m ++;
            }
            else if(command[k] == '\'' && S_quote == 1){
                S_quote = 0;
                //commands_set[num][m][n] = '\0';
                //n = 0;
                //m ++;
            }
            else {
                commands_set[num][m][n] = command[k];
                n++;
            }
        }
        else if(command[k] == ' '){
            commands_set[num][m][n] = '\0';
            n = 0;
            m++;
        }
        else if(command[k] == '&' && background_or_not == 1){
            continue;
        }
        else if(command[k] == '>'){
            commands_set[num][m][n] = '\0';
            if(n != 0) {
                n = 0;
                m++;
            }
            if(command[k+1] == '>'){
                if(L_arrow == 1 || R_arrow == 1){
                    printf("syntax error near unexpected token `>>'\n");
                    background_or_not = 0;
                    In = 0;
                    Out = 1;
                    D_quote = 0;
                    S_quote = 0;
                    PIPE = 0;
                    num = 0;
                    n = 0;
                    m = 0;
                    return -2;
                }
                k++;
                commands_set[num][m][0] = '>';
                commands_set[num][m][1] = '>';
                commands_set[num][m][2] = '\0';
                m++;
            }
            else{
                if(L_arrow == 1 || R_arrow == 1){
                    printf("syntax error near unexpected token `>'\n");
                    background_or_not = 0;
                    In = 0;
                    Out = 1;
                    D_quote = 0;
                    S_quote = 0;
                    PIPE = 0;
                    num = 0;
                    n = 0;
                    m = 0;
                    return -2;
                }
                commands_set[num][m][0] = '>';
                commands_set[num][m][1] = '\0';
                m++;
            }
            R_arrow = 1;
        }
        else if(command[k] == '<'){
            if(num > 0){
                printf("error: duplicated input redirection\n");
                background_or_not = 0;
                In = 0;
                Out = 1;
                D_quote = 0;
                S_quote = 0;
                PIPE = 0;
                num = 0;
                n = 0;
                m = 0;
                return -2;
            }
            if(L_arrow == 1 || R_arrow == 1){
                printf("syntax error near unexpected token `<'\n");
                background_or_not = 0;
                In = 0;
                Out = 1;
                D_quote = 0;
                S_quote = 0;
                PIPE = 0;
                num = 0;
                n = 0;
                m = 0;
                return -2;
            }
            L_arrow = 1;
            commands_set[num][m][n] = '\0';
            if(n != 0) {
                n = 0;
                m++;
            }
            commands_set[num][m][0] = '<';
            commands_set[num][m][1] = '\0';
            m++;
        }
        else if(command[k] == '|'){
            if(R_arrow == 1 || L_arrow == 1){
                printf("syntax error near unexpected token `|'\n");
                background_or_not = 0;
                In = 0;
                Out = 1;
                D_quote = 0;
                S_quote = 0;
                PIPE = 0;
                num = 0;
                n = 0;
                m = 0;
                return -2;
            }
            PIPE = 1;
            if(n != 0) commands_set[num][m][n] = '\0';
            if(n !=0 ) size[num] = m + 1;
            else size[num] = m;
            num++;
            m = 0;
            n = 0;
        }
        else if(command[k] == '"' || command[k] == '\''){
            PIPE = 0; L_arrow = 0; R_arrow = 0;
            if(command[k] == '"') D_quote = 1;
            else if(command[k] == '\'') S_quote = 1;
            /*if(n != 0) {
                n = 0;
                m++;
            }*/
        }
        else{
            PIPE = 0; L_arrow = 0; R_arrow = 0;
            commands_set[num][m][n] = command[k];
            n++;
        }
    }
    size[num] = m;
    return 0;
}


void shell(){
    commands_set = malloc(20 * sizeof(char**));
    for(int i = 0; i < 20; i++){
        commands_set[i] = malloc(128 * sizeof(char*));
        for(int j = 0; j < 128; j++){
            commands_set[i][j] = malloc(256 * sizeof(char));
        }
    }
    while(1) {
        for(int i = 0; i < 20; i++){
            pid_set[i] = -1;
        }
        background_or_not = 0;
        In = 0;
        Out = 1;
        D_quote = 0;
        S_quote = 0;
        PIPE = 0;
        num = 0;
        n = 0;
        m = 0;
        for(int k = 0; k < 20; k ++){
            size[k] = 0;
        }
        printf("mumsh $ ");
        fflush(stdout);
        signal(SIGINT, signal_handler);
        int check = get_commands();
        if(check == -1) return;
        else if(check == -2) continue;
        //printf("%d, %d\n", num, size[0]);
        while(S_quote == 1 || D_quote == 1 || PIPE == 1 || L_arrow == 1 || R_arrow == 1){
            printf("> ");
            fflush(stdout);
            check = get_commands();
            if(check == -2) continue;
        }
        //printf("%d, %d\n", num, size[0]);
        /*for(int k = 0; k <= num; k++){
            for(int i = 0; i < size[k]; i ++){
                printf("%s\n", commands_set[k][i]);
            }
            printf("This command is end\n");
        }*/
        if (strcmp(commands_set[0][0], "exit\0") == 0) {
            for(int k = 0; k < 20; k++){
                for(int l = 0; l < 128; l++){
                    free(commands_set[k][l]);
                }
                free(commands_set[k]);
            }
            free(commands_set);
            printf("exit\n");
            fflush(stdout);
            return;
        }
        if(num > 0) {
            int empty_or_not = 1;
            for (int k = 0; k <= num; k++) {
                empty_or_not = 1;
                for (int i = 0; i < size[k]; i++) {
                    if (strcmp(commands_set[k][i], "\0") != 0) empty_or_not = 0;
                }
                if (empty_or_not == 1) {
                    printf("error: missing program\n");
                    break;
                }
            }
            if (empty_or_not == 1) {
                continue;
            }
        }
        int** pipe_set = malloc(num * sizeof(int*));
        for(int k = 0; k < num; k++){
            pipe_set[k] = malloc(2 * sizeof(int));
        }
        for(int k = 0; k < num; k++){
            pipe(pipe_set[k]);
        }
        pid_t pid = 1;
        int count = 0;
        for(int k = 0; k <= num; k++) {
            if(pid != 0) {
                count ++;
                pid = fork();
            }
            if(pid > 0) pid_set[k] = pid;
            else if(pid == 0){
                if(num != 0) {
                    if (k == 0) {
                        In = 0;
                        //dup2(pipe_set[k][1], Out);
                        Out = pipe_set[k][1];
                        for(int l = 0; l < num; l++){
                            if(l == k) close(pipe_set[k][0]);
                            else{
                                close(pipe_set[l][0]);
                                close(pipe_set[l][1]);
                            }
                        }
                    }
                    else if(k == num){
                        In = pipe_set[k-1][0];
                        Out = 1;
                        for(int l = 0; l < num; l++){
                            if(l == k - 1) close(pipe_set[k - 1][1]);
                            else{
                                close(pipe_set[l][0]);
                                close(pipe_set[l][1]);
                            }
                        }
                    }
                    else{
                        Out = pipe_set[k][1];
                        //Out = 1;
                        In = pipe_set[k-1][0];
                        for(int l = 0; l < num; l++){
                            if(l == k) close(pipe_set[k][0]);
                            else if (l == k - 1) close(pipe_set[k - 1][1]);
                            else{
                                close(pipe_set[l][0]);
                                close(pipe_set[l][1]);
                            }
                        }
                    }
                }
            }
            execute_command(commands_set[k], size[k], pid);
        }
        if(background_or_not == 1){
            printf("[%d] ", total_jobs + 1);
            for(int k = 0; k <= num; k++){
                list[total_jobs].process_ID[k] = pid_set[k];
                //printf("(%d) ", pid_set[k]);
            }
            //list[total_jobs].job_ID = total_jobs;
            list[total_jobs].process_num = num + 1;
            for(int i = 0; i < command_len; i ++){
                list[total_jobs].command[i] = command[i];
                printf("%c", command[i]);
            }
            printf("\n");
            list[total_jobs].len_command = command_len;
            total_jobs++;
        }
        for(int k = 0; k < num; k++){
            close(pipe_set[k][0]);
            close(pipe_set[k][1]);
            free(pipe_set[k]);
        }
        if(pid > 0 && count == num + 1){
            for(int k = 0; k <= num; k++){
                //printf("%d\n", pid_set[k]);
                if(background_or_not == 1) waitpid(pid_set[k], NULL, WNOHANG);
                else waitpid(pid_set[k], NULL, 0);
                //printf("%d\n", kill(pid_set[k], 0));
            }
        }
        free(pipe_set);
        //free(commands_set);
    }
}

int main(){
    shell();
    return 0;
}
