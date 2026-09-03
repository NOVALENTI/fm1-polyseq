#!/bin/sh
# Run the pi32v2 target build inside the fm1-pi32v2 container.
# If you have the patched toolchain extracted locally, pass its bin dir:
#   ./docker/run-target.sh /path/to/pi32v2-toolchain/bin
# otherwise it runs with the image PATH (works once the toolchain is baked
# into the image under /opt/pi32v2, or fails with pi32v2-gcc not found).
set -eu
TOOLCHAIN_BIN="${1:-}"
EXTRA_MOUNT=""
if [ -n "$TOOLCHAIN_BIN" ]; then
  EXTRA_MOUNT="-v $TOOLCHAIN_BIN:/opt/pi32v2-toolchain:ro"
  export EXTRA_MOUNT
fi
# shellcheck disable=SC2086
docker run --rm -v "$(pwd):/work" -w /work $EXTRA_MOUNT \
  fm1-pi32v2 sh -c 'export PATH="/opt/pi32v2-toolchain:${PATH}:/opt/pi32v2/bin:${PATH}"; make target'
