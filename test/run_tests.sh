#!/bin/bash -v
set -e

windows_skipped_tests="python.test"

if [ "$#" -ne 1 ]; then
    echo "Usage: run_tests.sh <installation directory>"
    exit -1
fi

. $1/env.sh

for reg_test in *.test; do
    if [ "$(expr substr $(uname -s) 1 9)" == "CYGWIN_NT" ]; then
      if [[ $windows_skipped_tests == *"$reg_test"* ]]; then
        continue;
      fi
    fi
    echo "############################ Starting $reg_test ##########################"
    time ./$reg_test
    if [ $? -ne 0 ]
    then
        echo "############################ Failed $reg_test ##########################"
        exit 1
    fi
    echo "############################ Completed $reg_test ##########################"
done
