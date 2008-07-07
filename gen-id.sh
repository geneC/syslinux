#!/bin/sh
#
# Create a 10-character ID for this build.  If we're using a git tree,
# generate an ID of the form g[-*]XXXXXXXX (* = modified); otherwise use
# the passed-in timestamp.
#

if test -n "$GIT_DIR" -o -d ../.git -o -f ../.git; then
    ver="$(git rev-parse HEAD | cut -c1-8)"
    if test -n "$ver"; then
	if test -n "$(git diff-index --name-only HEAD)"; then
	    ver='g*'"$ver"
	else
	    ver='g-'"$ver"
	fi
    fi
fi
if test -z "$ver"; then
  echo "$1"
else
  echo "$ver"
fi
