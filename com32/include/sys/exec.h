/*
 * exec.h
 *
 *  Created on: Aug 14, 2008
 *      Author: Stefan Bucur <stefanb@zytor.com>
 */

#ifndef EXEC_H_
#define EXEC_H_

#include <sys/module.h>
#include <stdlib.h>

/**
 * EXEC_ROOT_NAME - The name of the ELF module associated with the COM32 module.
 *
 * This is a shallow ELF module, that contains only the symbol table for
 * the code and data sections of the loaded COM32 root module.
 */
#define EXEC_ROOT_NAME			"_root_.c32"

/**
 * MODULES_DEP - The name of the standard module dependency file
 *
 * This is the file which contains information about the module dependency
 * graph ( what other modules it depends on ). The file format is identical
 * to the standard linux modules.dep file... for more information check out the
 * man page ).
 */
#define MODULES_DEP "modules.dep"

/**
 * spawn_load - Load a library module or executes an executable one
 * @name	the name of the library/executable to use, including the extension
 * 			(e.g. 'sort.c32')
 * @argc:	the number of string arguments in @argv
 * @argv:	a NULL-terminated vector of string arguments, starting with
 * 			the program name.
 *
 * This procedure in essence loads takes the name of a module and checks to see what
 * kind of module it is ( executable or library ), after which is performs the
 * appropriate action, either spawning or simply loading the module into memory.
 */
extern int spawn_load(const char *name, int argc, char **argv);

extern int module_load_dependencies(const char*name,const char*dep_file);

/**
 * exec_init - Initialize the dynamic execution environment.
 *
 * Among others, it initializes the module subsystem and loads the root
 * module into memory. You should note the difference between the module
 * management API, and the execution API:
 *  - the module system is a static one - it only manages the data structures
 *  and their relationship. It does not describe the way modules are executed,
 *  when and how they are loaded/unloaded, etc. It acts as a service layer for
 *  the execution API.
 *  - the execution environment is the dynamic part of the SYSLINUX dynamic
 *  module API - it implements the behavior of the modules: it
 *  triggers the execution of initialization and termination functions for
 *  libraries, executes the modules marked as executable, handles dynamic
 *  memory cleanup, etc. In other words, at this layer the code and data
 *  loaded by the lower module layer gets to be executed by the CPU,
 *  thus becoming part of the SYSLINUX environment.
 */
extern int exec_init(void);


/**
 * load_library - Loads a dynamic library into the environment.
 * @name: 	the name of the library to load, including the extension
 * 			(e.g. 'sort.c32')
 *
 * A dynamic library is an ELF module that may contain initialization and
 * termination routines, but not a main routine. At the same time, any memory
 * allocations using malloc() and its derivatives are made on behalf of the
 * currently executing program or the COM32 root module. If the library
 * is unloaded, no memory cleanup is performed.
 */
extern int load_library(const char *name);

/**
 * unload_library - unloads a library from the environment.
 * @name:	the name of the library to unload, including the extension
 * 			(e.g. 'sort.c32')
 *
 * Note that no memory allocated by the library code is cleaned up, as the
 * allocations belong to the innermost calling program in the call stack.
 */
extern int unload_library(const char *name);

/**
 * spawnv - Executes a program in the current environment.
 * @name:	the name of the program to spawn, including the extension
 * 			(e.g. 'hello.c32')
 * @argv:	a NULL-terminated vector of string arguments, starting with
 * 			the program name.
 *
 * A program is an ELF module that contains a main routine. A program is
 * loaded into memory, executed, then unloaded, thus remaining in memory only
 * while the main() function is executing. A program also defines a
 * memory allocation context, and a simple garbage collection mechanism
 * it thus provided. This is done by internally associating with the program
 * module each pointer returned by malloc(). After the program finishes
 * its execution, all the unallocated memory pertaining to the program
 * is automatically cleaned up.
 *
 * Note that this association takes place both for the allocations happening
 * directly in the program, or indirectly through a library function. Libraries
 * do not create allocation contexts, thus each allocation they made belong
 * to the innermost calling program.
 */
extern int spawnv(const char *name, const char **argv);

/**
 * spawnl - Executes a program in the current environment.
 * @name:	the name of the program to spawn, including the extension
 * 			(e.g. 'hello.c32')
 * @arg:	the first argument (argv[0]) to be passed to the main function
 * 			of the program
 * @...:	optional subsequent arguments that are passed o the main function
 * 			of the program
 *
 * This is another version of the spawn routine. Please see 'spawnv' for
 * a full presentation.
 */
extern int spawnl(const char *name, const char *arg, ...);

/**
 * exec_term - Releases the resources of the execution environment.
 */
extern void exec_term(void);


#endif /* EXEC_H_ */
