# On a brand new linux box, this gets the
# source code for both the Raspberry Pi tools and 
# for a particular version of Linux so you can build and run
# drivers

set -e

echo "will test for git"
git --version

echo "will make folder"
mkdir ~/rpi

echo "will cd to folder"
cd ~/rpi

echo "will get scripts from host"
cp ~/host/rpi/* .

echo "will load env vars"
source ./rpienv.sh

echo "will clone tools"
git clone https://github.com/raspberrypi/tools

echo "will check compiler presence"
${CCPREFIX}gcc --version

echo "will clone linux"
git clone https://github.com/raspberrypi/linux

echo "will cd to kernel"
cd ${KERNEL_SRC}

echo "will checkout kernel hash"
git checkout ${KERNEL_HASH}


echo "done"
