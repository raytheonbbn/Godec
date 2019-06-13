#!/bin/bash

gitdir=`git rev-parse --git-dir`/hooks
echo $gitdir
if [ ! -L $gitdir/pre-commit ]
then
  ln -s $PWD/misc/pre-commit $gitdir
fi

