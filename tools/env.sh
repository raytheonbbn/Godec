#!/bin/bash -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

if [ "$(expr substr $(uname -s) 1 9)" == "CYGWIN_NT" ]; then
    export SCRIPT_DIR_WIN=`cygpath -m $SCRIPT_DIR`
fi

if [ -z ${JAVA_HOME+x} ]
then 
    echo "Environment variable JAVA_HOME not set"
    exit -1
fi

export LC_ALL=C

if [ "$(expr substr $(uname -s) 1 9)" == "CYGWIN_NT" ]; then
  if [ -z ${PYTHON_HOME+x} ]
  then 
    echo "Environment variable PYTHON_HOME not set"
    exit -1
  fi
  JAVA_HOME_CYGWIN=$(cygpath -u "$JAVA_HOME")
  PYTHON_HOME_CYGWIN=$(cygpath -u "$PYTHON_HOME")
  JVM_DLL=$(find "$JAVA_HOME_CYGWIN" -name "jvm.dll")
  export JVM_DIR=$(dirname "$JVM_DLL")
  SCRIPT_DIR_WIN=$(cygpath -m $SCRIPT_DIR)
  export JAVA_CLASSPATH="$SCRIPT_DIR_WIN/java/*;$JAVA_CLASSPATH"
  export JAVA_LIBRARY_PATH="$SCRIPT_DIR_WIN/bin;$PYTHON_HOME;$JAVA_LIBRARY_PATH"
  export PATH=$PYTHON_HOME_CYGWIN:$PATH 
else
  JVM_DLL=$(find "$JAVA_HOME" -name "libjvm.so")
  LIBGODEC_RPATH=$(objdump -x $SCRIPT_DIR/bin/libgodec.so |grep RPATH | awk '{print $2;}')
  export JVM_DIR=$(dirname $JVM_DLL)
  export JAVA_CLASSPATH="$SCRIPT_DIR/java/*:$JAVA_CLASSPATH"
  export LD_LIBRARY_PATH=$JVM_DIR:$LIBGODEC_RPATH:$SCRIPT_DIR/bin:$LD_LIBRARY_PATH 
  export JAVA_LIBRARY_PATH="$SCRIPT_DIR/bin:$LIBGODEC_RPATH:$JAVA_LIBRARY_PATH:$PATH"
fi

export PATH=$SCRIPT_DIR/bin:$JVM_DIR:$PATH

