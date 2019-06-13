#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ASTYLERC=$DIR/astylerc
if [ "$(expr substr $(uname -s) 1 9)" == "CYGWIN_NT" ]; then
ASTYLE=$DIR/Astyle.exe
ASTYLERC=`cygpath -w $ASTYLERC | sed 's%\\\\%\/%g'`
else
ASTYLE=$DIR/astyle
fi

$ASTYLE --options=$ASTYLERC $1
