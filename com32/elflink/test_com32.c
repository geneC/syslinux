#include <stdio.h>
#include <stdlib.h>
#include <console.h>
#include <string.h>

#include <sys/module.h>

#include "exec.h"

#define INFO_PRINT(fmt, args...)	printf("[COM32] " fmt, ##args)

#define MAX_COMMAND_SIZE 	80
#define COMMAND_DELIM		" \t\n"
#define MAX_COMMAND_ARGS	(MAX_COMMAND_SIZE/2)



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

void process_spawn() {
	// Compose the command line
	char **cmd_line = malloc((MAX_COMMAND_ARGS+1)*sizeof(char*));
	int argc = 0, result;
	char *crt_arg;

	do {
		crt_arg = strtok(NULL, COMMAND_DELIM);
		if (crt_arg != NULL && strlen(crt_arg) > 0) {
			cmd_line[argc] = crt_arg;
			argc++;
		} else {
			break;
		}
	} while (argc < MAX_COMMAND_ARGS);

	cmd_line[argc] = NULL;

	if (cmd_line[0] == NULL) {
		printf("You must specify an executable module.\n");
	} else {
		result = spawnv(cmd_line[0], cmd_line);

		printf("Spawn returned %d\n", result);
	}

	free(cmd_line);
}

void process_library(int load) {
	char *crt_lib;
	int result;

	while ((crt_lib = strtok(NULL, COMMAND_DELIM)) != NULL) {
		if (strlen(crt_lib) > 0) {
			if (load)
				result = load_library(crt_lib);
			else
				result = unload_library(crt_lib);

			if (result == 0) {
				printf("Library '%s' %sloaded successfully.\n", crt_lib,
						load ? "" : "un");
			} else {
				printf("Could not %sload library '%s': error %d\n",
						load ? "" : "un", crt_lib, result);
			}
		}
	}
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
		process_library(1);
	} else if (strcmp(cmd_name, "spawn") == 0) {
		process_spawn();
	} else if (strcmp(cmd_name, "unload") == 0) {
		process_library(0);
	} else if (strcmp(cmd_name, "list") == 0) {

	} else {
		printf("Unknown command. Type 'help' for a list of valid commands.\n");
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
