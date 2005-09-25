#ifndef LIBUTIL_MOVEBITS_H
#define LIBUTIL_MOVEBITS_H

#include <inttypes.h>
#include <setjmp.h>

struct movelist {
  uintptr_t dst;
  uintptr_t src;
  uintptr_t len;
  struct movelist *next;
};

struct move_descriptor {
  uint32_t dst;
  uint32_t src;
  uint32_t len;
};

/*
 * Creates a movelist consisting of only one element, and
 * if parent == NULL insert into the movelist chain immediately after
 * the parent element.
 */
struct movelist *
make_movelist(struct movelist *parent, uintptr_t dst,
	      uintptr_t src, uintptr_t len);

/*
 * Convert a movelist into a linear array of struct move_descriptors,
 * returning the number of descriptors and freeing the movelist.
 *
 * Returns (size_t)-1 on failure; if so the movelist is still valid.
 */
size_t
linearize_movelist(struct move_descriptor **d, struct movelist *m);

/*
 * moves is computed from "frags" and "freemem".  "space" lists
 * free memory areas at our disposal, and is (src, cnt) only.
 */

int
compute_movelist(struct movelist **moves, struct movelist *frags,
		 struct movelist *space);

#endif /* LIBUTIL_MOVEBITS_H */
