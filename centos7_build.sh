#!/bin/bash -e
cd $HOME 
yum -y groupinstall 'Development Tools' 
yum -y install centos-release-scl 
yum -y install devtoolset-7-gcc\* 
yum -y install libtool wget alsa-lib alsa-lib-devel zlib-devel maven which
yum -t install java-11-openjdk-devel
curl -o Miniconda3.7.3.sh https://repo.anaconda.com/miniconda/Miniconda3-4.6.14-Linux-x86_64.sh &&bash Miniconda3.7.3.sh -b -p /conda
export PATH=/conda/bin:$PATH
conda install -y numpy
export JAVA_HOME=/usr/lib/jvm/java-11-openjdk
export PATH=$JAVA_HOME/bin:$PATH
export LD_LIBRARY_PATH=$JAVA_HOME/lib/server/:$LD_LIBRARY_PATH
source /opt/rh/devtoolset-7/enable
curl -L -o boost_1_75_0.tar.gz https://boostorg.jfrog.io/artifactory/main/release/1.75.0/source/boost_1_75_0.tar.gz
tar xf boost_1_75_0.tar.gz -C "$HOME"
cd boost_1_75_0
./bootstrap.sh
./b2 clean
./b2 -j8 -d0 cxxstd=17 cxxflags=-fPIC cflags=-fPIC --without-python link=static threading=multi threadapi=pthread define=BOOST_MATH_DISABLE_FLOAT128
export BOOST_ROOT=$HOME/boost_1_75_0
wget -q http://gitlab.com/libeigen/eigen/-/archive/3.3.7/eigen-3.3.7.tar.gz
tar xf eigen-3.3.7.tar.gz -C $HOME
export EIGEN_ROOT=$HOME/eigen-3.3.7 
export PYTHON_ROOT=/conda
export PYTHON_HOME=/conda
export CMAKE_ARGS="-DBOOST_INCLUDEDIR=$BOOST_ROOT -DBOOST_LIBRARYDIR=$BOOST_ROOT/stage/lib/ -DEigen3_INCLUDE_DIRS=$EIGEN_ROOT -DPYTHON_LIBRARY=$PYTHON_ROOT/lib/libpython3.7m.so -DPYTHON_INCLUDE_DIR=$PYTHON_ROOT/include/python3.7m -DGODEC_ADDITIONAL_INCLUDE_DIRS=$PYTHON_ROOT/lib/python3.7/site-packages/numpy/core/include/ "
cd $TRAVIS_BUILD_DIR && rm -rf cmake-build && mkdir cmake-build && cd cmake-build 
"$MY_CMAKE" -DPLATFORM=$PLATFORM -DVERSION_STRING="$TRAVIS_TAG" $CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=../install ../ 
eval "$MY_MAKE_COMMAND" 
cd ../install 
echo Creating $HOME/godec.$OS_STRING.tgz 
tar cvzf $HOME/godec.$OS_STRING.tgz *
