/*
 * exec.h
 *
 *  Created on: Aug 14, 2008
 *      Author: Stefan Bucur <stefanb@zytor.com>
 */

#ifndef EXEC_H_
#define EXEC_H_

#include <sys/module.h>

#define EXEC_ROOT_NAME			"_root_.dyn"

#define EXEC_DIRECTORY		"/dyn/"

/**
 * exec_init - Initialize the dynamic execution environment.
 */
extern int exec_init();

/**
 * load_library - Loads a dynamic library into the environment.
 */
extern int load_library(const char *name);

/**
 * unload_library - unloads a library from the environment.
 */
extern int unload_library(const char *name);

/**
 * spawnv - Executes a program in the current environment.
 */
extern int spawnv(const char *name, const char **argv);

/**
 * spawnl - Executes a program in the current environment.
 */
extern int spawnl(const char *name, const char *arg, ...);

/**
 * exec_term - Releases the resources of the execution environment.
 */
extern void exec_term();


#endif /* EXEC_H_ */
