/*
 * A dir test module
 */
#include <stdio.h>
#include <console.h>
#include <string.h>
#include <com32.h>
#include <dirent.h>

int main(int argc, char *argv[])
{
	DIR *dir;
	struct dirent *de;
	
	openconsole(&dev_null_r, &dev_stdcon_w);

	if (argc != 2) {
		printf("Usage: dir direcotry\n");
		return 0;
	}
	
	dir = opendir(argv[1]);
	printf("back from in main ...? \n");
	if (dir == NULL) {
		printf("Unable to read dir: %s\n", argv[1]);
		return 0;
	}
		
	while ((de = readdir(dir)) != NULL)
		printf("%s\n", de->d_name);
	
	closedir(dir);
	
	return 0;
}
  
