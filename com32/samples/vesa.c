#include <stdio.h>
#include <inttypes.h>
#include "../lib/sys/vesa/video.h"

int __vesacon_init(void);
void vesacon_write_at(int row, int col, const char *str, uint8_t attr, int rev);

int main(void)
{
  int row, col;
  int attr;
  char attr_buf[16];

  __vesacon_init();
  vesacon_load_background("stacy.png");

  row = col = 0;
  
  for (attr = 0; attr < 256; attr++) {
    snprintf(attr_buf, sizeof attr_buf, " [%02X] ", attr);
    __vesacon_write_at(row, col, attr_buf, attr, attr & 3);
    row++;
    if (row >= 29) {
      row = 0;
      col += 8;
    }
  }  

  while (1)
    asm volatile("hlt");
}
