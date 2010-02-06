#ifndef YMSEND_H
#define YMSEND_H

#include "mystuff.h"
#include "file.h"

void send_ymodem(struct serial_if *, struct file_info *,
		 void (*)(void *, size_t, struct file_info *, size_t));
void end_ymodem(struct serial_if *);

#endif /* YMSEND_H */
