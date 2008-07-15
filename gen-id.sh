#!/bin/sh
#
# Create a 10-character ID for this build.  If we're using a git tree,
# generate an ID of the form g[-*]XXXXXXXX (* = modified); otherwise use
# the passed-in timestamp.
#

if test -n "$GIT_DIR" -o -d ../.git -o -f ../.git; then
    ver="$(git describe | cut -d- -f3-)"
    if test -n "$ver"; then
	if test -n "$(git diff-index --name-only HEAD)"; then
	    ver="${ver}"\*
	fi
    fi
fi
if test -z "$ver"; then
  echo "$1"
else
  echo "$ver"
fi
