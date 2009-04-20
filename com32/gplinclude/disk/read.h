#ifndef _READ_H_
#define _READ_H_
void *read_mbr(int drive);
void *dev_read(int drive, unsigned int lba, int sectors);
void *read_sectors(struct driveinfo* drive_info, const unsigned int lba,
		   const int sectors);
#endif /* _READ_H */
