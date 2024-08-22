#!/usr/bin/env bash
#
# Test default configuration on examples only, on linux64, with compiler gcc-11.2

CWD=$(cd $(dirname ${BASH_SOURCE[0]}) ; pwd)
source $CWD/common.bash

# Use CHPL_LLVM=none to avoid using a system LLVM potentially linked
# with a different and incompatible version of GCC
export CHPL_LLVM=none
export CHPL_LLVM_SUPPORT=bundled
unset CHPL_LLVM_CONFIG

source /data/cf/chapel/setup_gcc112.bash     # host-specific setup for target compiler

# Set environment variables to nudge cmake towards GCC 11.2
export CHPL_CMAKE_USE_CC_CXX=1
export CC=$(which gcc)
export CXX=$(which g++)

gcc_version=$(gcc -dumpversion)
if [ "$gcc_version" != "11.2.0" ]; then
  echo "Wrong gcc version"
  echo "Expected Version: 11.2.0 Actual Version: $gcc_version"
  exit 2
fi

export CHPL_NIGHTLY_TEST_CONFIG_NAME="linux64-gcc112"

$CWD/nightly -cron -examples ${nightly_args}
