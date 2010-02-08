#ifndef SRECSEND_H
#define SRECSEND_H

#include "mystuff.h"
#include "file.h"

void send_srec(struct serial_if *, struct file_info *,
		 void (*)(void *, size_t, struct file_info *, size_t));

#endif /* SRECSEND_H */
