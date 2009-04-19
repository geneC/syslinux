#ifndef _WRITE_H_
#define _WRITE_H_
int write_sectors(const struct driveinfo* drive_info, const unsigned int lba,
		  const void *data, const int size);
int write_verify_sector(struct driveinfo* drive_info,
			const unsigned int lba,
			const void *data);
int write_verify_sectors(struct driveinfo* drive_info,
			 const unsigned int lba,
			 const void *data, const int size);
#endif
