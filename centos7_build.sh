#!/bin/bash -e
cd $HOME 
yum -y groupinstall 'Development Tools' 
yum -y install centos-release-scl 
yum -y install devtoolset-7-gcc\* 
yum -y install libtool wget alsa-lib alsa-lib-devel rh-python36-python rh-python36-python-devel rh-python36-numpy java-1.8.0-openjdk java-1.8.0-openjdk-devel zlib-devel maven which 
export JAVA_HOME=$(realpath $(dirname $(which java))/..) 
source /opt/rh/devtoolset-7/enable 
wget -q https://dl.bintray.com/boostorg/release/1.68.0/source/boost_1_68_0.tar.gz 
tar xf boost_1_68_0.tar.gz -C "$HOME" 
cd $HOME/boost_1_68_0 
./bootstrap.sh 
./b2 clean 
./b2 -j8 -d0 cxxstd=17 cxxflags=-fPIC cflags=-fPIC --without-python link=static threading=multi threadapi=pthread define=BOOST_MATH_DISABLE_FLOAT128 
export BOOST_ROOT=$HOME/boost_1_68_0 
wget -q http://bitbucket.org/eigen/eigen/get/3.3.7.tar.bz2 
tar jxf 3.3.7.tar.bz2 -C $HOME 
export EIGEN_ROOT=$HOME/eigen-eigen-323c052e1731 
export PYTHON_ROOT=/opt/rh/rh-python36/root/ 
export PYTHON_HOME=/opt/rh/rh-python36/root/usr/ 
export CMAKE_ARGS="-DBOOST_INCLUDEDIR=$BOOST_ROOT -DBOOST_LIBRARYDIR=$BOOST_ROOT/stage/lib/ -DEigen3_INCLUDE_DIRS=$EIGEN_ROOT -DPYTHON_LIBRARY=$PYTHON_ROOT/usr/lib64/libpython3.6m.so -DPYTHON_INCLUDE_DIR=$PYTHON_ROOT/usr/include/python3.6m -DGODEC_ADDITIONAL_INCLUDE_DIRS=$PYTHON_ROOT/usr/lib64/python3.6/site-packages/numpy/core/include/ " 
cd $TRAVIS_BUILD_DIR && rm -rf cmake-build && mkdir cmake-build && cd cmake-build 
"$MY_CMAKE" -DPLATFORM=$PLATFORM -DVERSION_STRING="$TRAVIS_TAG" $CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=../install ../ 
eval "$MY_MAKE_COMMAND" 
cd ../install 
tar cvzf $HOME/godec.$OS_STRING.tgz *
