[Back](../README.md)

Building Godec from source   
---

Godec is entirely Cmake-based on all platforms, including Windows (this means the Visual Studio version has to be VS 2017 or later).

The Linux-based cmake version needs to be 3.12 or higher. Annoyingly many Linux distributions come with older versions, so you might have to download it from https://cmake.org/download/ and compile it.

[Linux](#Linux)

[Windows](#Windows)

[Raspberry Pi](#Raspberry-Pi)

[Android](#Android)

---

### Linux
  Install the following packages:

  `sudo apt-get install libboost-dev-all libeigen3-dev libasound2-dev maven icu python3-numpy`

  `mkdir cmake-build`

  `cd cmake-build`

  `cmake -DPLATFORM=Linux -DCMAKE_INSTALL_PREFIX=../install ..`

  `make install`

---

### Windows
  As mentioned, Visual Studio 2017 or higher is required. Also, unlike in the Linux systems, the various packages (Boost, Eigen) need to be manually downloaded and some of them need to be precompiled.

  #### Boost
  Download Boost 1.68 from https://www.boost.org/users/history/version_1_68_0.html

  Open "Visual Studio Developer Command Prompt", cd to the Boost folder

  `.\bootstrap.bat`

  `b2.exe -j4 toolset=msvc-14.1 address-model=64 variant=release threading=multi runtime-link=shared link=static --build-type=complete --layout=versioned --without-python`

  #### Eigen
  Download Eigen 3.3.4: http://eigen.tuxfamily.org/index.php?title=Main_Page

  #### Maven
  Download Maven: https://maven.apache.org/download.cgi

  All in the Visual Studio command window:

  `set PATH=%PATH%;c:\<your_path>\apache-maven-3.6.0\bin`

  `set BOOST_ROOT=C:\<your_path>\boost_1_68_0`

  `set EIGEN_ROOT=C:\<your_path>\eigen-eigen-5a0156e40feb`

  `mkdir cmake-build`

  `cd cmake-build`

  `cmake -DPLATFORM=Windows -DBoost_NO_SYSTEM_PATHS=ON -DBOOST_INCLUDEDIR=%BOOST_ROOT% -DBOOST_LIBRARYDIR=%BOOST_ROOT%\stage\lib -DEigen3_INCLUDE_DIRS=%EIGEN_ROOT% ..`

  `cmake --build . --target install --config Release`


---

### Raspberry-Pi
  Godec for the Raspberry Pi is best cross-compiled on a regular Linux machine. For that it is necessary to fetch some of the libraries from an actual Raspberry Pi installation.

  On the Pi, run:

  `sudo apt-get install libboost1.62-all-dev libasound2-dev openjdk-8-jdk `

  `mkdir -p rpi_ext/lib rpi_ext/include rpi_ext/include/jvm`

  `cp -r /usr/include/boost /usr/include/alsa rpi_ext/include`

  `cp -r /usr/lib/jvm/jdk-8-oracle-arm32-vfp-hflt/include rpi_ext/include/jvm`

  `cp -r /usr/lib/arm-linux-gnueabihf/libboost_* /usr/lib/arm-linux-gnueabihf/libasound* /usr/lib/jvm/jdk-8-oracle-arm32-vfp-hflt/jre/lib/* rpi_ext/lib/`

  tar up this rpi_ext folder and copy it over to the Linux machine. Then, in a command prompt

  `sudo apt-get install gcc-7-arm-linux-gnueabihf g++-7-arm-linux-gnueabihf`

  `export CXX=arm-linux-gnueabihf-g++`

  `export RPI_EXT=<your_path>/rpi_ext`

  `mkdir cmake-build`

  `cd cmake-build`

  `cmake -DPLATFORM=RaspberryPi -DBoost_NO_SYSTEM_PATHS=ON -DBOOST_INCLUDEDIR=$RPI_EXT/include -DBOOST_LIBRARYDIR=$RPI_EXT/lib -DALSA_LINKER_DIR=$RPI_EXT/lib  -DALSA_INCLUDE_DIRS=$RPI_EXT/include/alsa/include -DJNI_INCLUDE_DIRS="$RPI_EXT/include/jvm/include/;$RPI_EXT/include/jvm/include/linux" -DJAVA_JVM_LIBRARY=$RPI_EXT/lib/arm/client/libjvm.so -DCMAKE_INSTALL_PREFIX=../install ..`

  `make install`
  

---

### Android

  The Android build also has to be cross-compiled (for obvious reasons). On a Linux machine:

  `apt-get install libeigen3-dev maven openjdk-8-jdk maven`

  #### NDK

  Download the Android NDK 18b here: https://developer.android.com/ndk/downloads/older_releases.html
  
We need a clang toolchain to compile Godec. In the NDK main folder:

  `./build/tools/make_standalone_toolchain.py --arch arm --api 21 --install-dir <your_path>/android-clang-toolchain`

  #### Boost 

  Clone this repo: https://github.com/moritz-wundke/Boost-for-Android

  `export CXX=<your_path>/android-clang-toolchain/bin/arm-linux-androideabi-clang++`
  
  `./build-android.sh --boost=1.68.0 --arch=armeabi-v7a <NDK_ROOT>`

  `export BOOST_ROOT=<your_path>/Boost-for-Android/build/out/armeabi-v7a/`

  One problem that arises is that Boost's output files have the wrong file names, so we need to create symbolic links to the right names:

  `cd <your_path>/Boost-for-Android/build/out/armeabi-v7a/lib/`

  `for f in libboost*clang*; do b=`basename $f -clang-mt-a32-1_68.a`; ln -s $f $b.a; done`

  In the Godec main folder:

  `mkdir cmake-build`

  `cd cmake-build`

  `cmake -DPLATFORM=Android -DBoost_NO_SYSTEM_PATHS=ON -DBOOST_INCLUDEDIR=$BOOST_ROOT/include/boost-1_68/ -DBOOST_LIBRARYDIR=$BOOST_ROOT/lib/ -DCMAKE_INSTALL_PREFIX=../install ..`

