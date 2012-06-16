#ifndef _ERRNO_H
#define _ERRNO_H

extern int errno;

#define	ENOENT		 2	/* No such file or directory */
#define	EINTR		 4	/* Interrupted system call */
#define	EIO		 5	/* I/O error */
#define	EBADF		 9	/* Bad file number */
#define	EAGAIN		11	/* Try again */
#define	ENOMEM		12	/* Out of memory */
#define	EACCES		13	/* Permission denied */
#define	EFAULT		14	/* Bad address */
#define	ENOTDIR		20	/* Not a directory */
#define	EISDIR		21	/* Is a directory */
#define	EINVAL		22	/* Invalid argument */
#define	EMFILE		24	/* Too many open files */
#define	ENOTTY		25	/* Not a typewriter */
#define	ENOSPC		28	/* No space left on device */
#define	ERANGE		34	/* Math result not representable */
#define	ENOSYS		38	/* Function not implemented */

#endif /* _ERRNO_H */
