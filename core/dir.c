#include <stdio.h>
#include <string.h>
#include <dir.h>
#include <fs.h>
#include <core.h>

/* The dir log structure, to log the status of the dir_buf. */
struct dir_log {
    int offset;   /* how far from the dir_buf */
    int index;    /* which dir entry have we go */
};
static struct dir_log log = {0, 0};

/* The dir buffer used by fill_dir to store the newly read dirs*/
#define DB_SIZE 2048
char dir_buf[DB_SIZE];

void opendir(com32sys_t *regs)
{	
    int ds = regs->ds; /* save ds */
	
    regs->ds = regs->es;
    regs->es = ds;
    mangle_name(regs);
    regs->ds = ds;  /* restore ds */
    searchdir(regs);	
}

/*
 * Fill the dir buffer; return 1 for not full, 0 for full
 */
int fill_dir(struct dirent *de)
{
    int de_len = de->d_reclen;
    if (log.offset + de_len <= DB_SIZE) {
	memcpy(dir_buf + log.offset, de, de_len);
	log.offset += de_len;
	log.index ++;
	return 1;
    }

	return 0;
}

/*
 * Read one dirent at one time. 
 *
 * @input: _esi_ register stores the address of DIR structure
 * @output: _eax_ register stores the address of newly read dirent structure
 */
void readdir(com32sys_t *regs)
{
    extern struct fs_info *this_fs;
    DIR *dir = (DIR *)regs->esi.l;	
    struct dirent *de = NULL;
    static int offset;
	
	/* If we haven't fill the dir buffer, fill it */
    if (log.index == 0) {
	this_fs->fs_ops->readdir(this_fs, dir);
	if (log.offset == 0) {
		regs->eax.l = 0;
		return;
	}
	offset = 0; /* reset the _offset_ */
    }
	
    if (offset < log.offset) {
	de = (struct dirent *)(dir_buf + offset);
	offset += de->d_reclen;
    } 
    if (offset >= log.offset) /* reach the end of buffer, reset it */
	memset(&log, 0, sizeof log);
	
    /* Return the newly read de in _eax_ register */
    regs->eax.l = (uint32_t)de;
}

void closedir(com32sys_t *regs)
{
    regs->esi.w[0] = 0;
}


