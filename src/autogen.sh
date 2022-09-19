#!/bin/sh

set -x

aclocal

# detect libtoolize on linux or glibtoolize in some systems
if (libtoolize --version) < /dev/null > /dev/null 2>&1; then
  LIBTOOLIZE=libtoolize
elif (glibtoolize --version) < /dev/null > /dev/null 2>&1; then
  LIBTOOLIZE=glibtoolize
else
  echo "libtoolize or glibtoolize was not found! Please install libtool." 1>&2
  exit 1
fi

$LIBTOOLIZE --copy

automake --add-missing --force-missing --copy
autoconf
# Our ylwrap gets clobbered by the above.
git checkout ylwrap
