#!/bin/sh
#
#  Copyright (c) 2017, The OpenThread Authors.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the name of the copyright holder nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#

set -e
set -x

TOOLS_HOME=$HOME/.cache/tools
[ -d $TOOLS_HOME ] || mkdir -p $TOOLS_HOME

die() {
	echo " *** ERROR: " $*
	exit 1
}

case $TRAVIS_OS_NAME in
linux)
    # Uncrustify
    [ $BUILD_TARGET != pretty-check ] || [ "$($TOOLS_HOME/usr/bin/uncrustify --version)" = 'Uncrustify-0.65_f' ] || (cd /tmp &&
        wget https://github.com/uncrustify/uncrustify/archive/uncrustify-0.65.tar.gz &&
        tar xzf uncrustify-0.65.tar.gz &&
        cd uncrustify-uncrustify-0.65 &&
        mkdir build &&
        cd build &&
        cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr .. &&
        make && make install DESTDIR=$TOOLS_HOME) || die

    # Unittest
    [ $BUILD_TARGET != posix-check ] || [ -f $TOOLS_HOME/usr/lib/libCppUTest.a ] || (cd /tmp &&
        wget https://github.com/cpputest/cpputest/archive/v3.8.tar.gz &&
        tar xzf v3.8.tar.gz &&
        cd cpputest-3.8 &&
        ./autogen.sh &&
        ./configure --prefix=/usr &&
        make && make install DESTDIR=$TOOLS_HOME) || die

    # Enable IPv6
    [ $BUILD_TARGET != posix-check ] || (echo 0 | sudo tee /proc/sys/net/ipv6/conf/all/disable_ipv6) || die

    # Prepare Raspbian image
    [ $BUILD_TARGET != raspbian-gcc ] || {
        sudo apt-get install --allow-unauthenticated -y qemu qemu-user-static binfmt-support parted

        git clone --depth 1 https://github.com/ryankurte/docker-rpi-emu.git

        IMAGE_FILE=2017-04-10-raspbian-jessie-lite.img
        [ -f $TOOLS_HOME/images/$IMAGE_FILE ] || {
            # unit MB
            EXPAND_SIZE=1024

            [ -d $TOOLS_HOME/images ] || mkdir -p $TOOLS_HOME/images

            (cd /tmp &&
                curl -LO http://director.downloads.raspberrypi.org/raspbian_lite/images/raspbian_lite-2017-04-10/2017-04-10-raspbian-jessie-lite.zip &&
                unzip 2017-04-10-raspbian-jessie-lite.zip &&
                dd if=/dev/zero bs=1048576 count=$EXPAND_SIZE >> $IMAGE_FILE &&
                mv $IMAGE_FILE $TOOLS_HOME/images/$IMAGE_FILE)

            (cd docker-rpi-emu/scripts &&
                sudo ./expand.sh $TOOLS_HOME/images/$IMAGE_FILE $EXPAND_SIZE)
        }
    }
    ;;

*)
    die
    ;;
esac
