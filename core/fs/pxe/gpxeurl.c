#include "pxe.h"
#if GPXE

void gpxe_open(struct inode *inode, const char *url)
{
    (void)inode;
    (void)url;
}

#endif /* GPXE */
