#!/bin/bash
#
# Run the plugin tests. smartmet-plugin-test compares responses byte by byte,
# but the PNG encoding of an image depends on the libpng and zlib versions.
# A failed test whose result and expected output are both PNG images therefore
# passes if smartimagediff finds the images equal.

set -u -o pipefail

log=$(mktemp)
trap 'rm -f "$log"' EXIT

smartmet-plugin-test "$@" 2>&1 | tee "$log"
status=$?

is_png() { [ "$(head -c 8 "$1" 2>/dev/null | od -An -c | tr -d ' ')" = '211PNG\r\n032\n' ]; }

failed=0
accepted=0
for name in $(grep -a -E '^\S+ \.+ FAIL' "$log" | cut -d' ' -f1); do
  if is_png "output/$name" && is_png "failures/$name" &&
    smartimagediff "failures/$name" "output/$name" >/dev/null 2>&1; then
    echo "$name: images are equal, only the PNG encoding differs"
    rm -f "failures/$name"
    accepted=$((accepted + 1))
  else
    failed=$((failed + 1))
  fi
done

# No failed tests listed: trust the exit status (covers timeouts and crashes)
if [ $((failed + accepted)) -eq 0 ]; then
  exit $status
fi

echo "$accepted failed image comparisons accepted, $failed tests failed"
[ "$failed" -eq 0 ]
