#include <stdio.h>
#include <stdlib.h>
#include <console.h>
#include <string.h>

#include <sys/module.h>

#include "exec.h"

#define INFO_PRINT(fmt, args...)	printf("[COM32] " fmt, ##args)

#define MAX_COMMAND_SIZE 	80
#define COMMAND_DELIM		" \t\n"



void print_help() {
	printf("List of available commands:\n");
	printf("exit - exits the program\n");
	printf("help - shows this message\n");
	printf("load <library>... - loads the libraries into the environment\n");
	printf("spawn <executable> <args> - launches an executable module\n");
	printf("unload <library>... - unloads the libraries from the environment\n");
	printf("list - prints the currently loaded modules\n");
}

void print_prompt() {
	printf("\nelflink> ");
}

void read_command(char *cmd, int size) {
	char *nl = NULL;
	fgets(cmd, size, stdin);

	// Strip the newline
	nl = strchr(cmd, '\n');

	if (nl != NULL)
		*nl = '\0';
}

int process_command(char *cmd) {
	char *cmd_name;

	cmd_name = strtok(cmd, COMMAND_DELIM);

	if (strcmp(cmd_name, "exit") == 0) {
		printf("Goodbye!\n");
		return 1;
	} else if (strcmp(cmd_name, "help") == 0) {
		print_help();
	} else if (strcmp(cmd_name, "load") == 0) {

	} else if (strcmp(cmd_name, "spawn") == 0) {

	} else if (strcmp(cmd_name, "unload") == 0) {

	} else if (strcmp(cmd_name, "list") == 0) {

	}

	return 0;
}



int main(int argc, char **argv) {
	int done = 0;
	int res;
	char command[MAX_COMMAND_SIZE] = {0};

	// Open a standard r/w console
	openconsole(&dev_stdcon_r, &dev_stdcon_w);

	res = exec_init();
	if (res != 0) {
		printf("Failed to initialize the execution environment.\n");
		return res;
	} else {
		printf("Execution environment initialized successfully.\n");
	}


	printf("\nFor a list of available commands, type 'help'.\n");

	do {
		print_prompt();
		read_command(command, MAX_COMMAND_SIZE);
		done = process_command(command);

	} while (!done);

	exec_term();

	return 0;
}
