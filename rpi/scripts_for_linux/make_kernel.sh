# ON an just-installed Ubuntu box (recommend quad-core)
# this script can build the Linux kernel to the point
# where you can build drivers with it.

set -e

echo "will cd to folder"
cd ~/rpi

echo "will load env vars"
source ./rpienv.sh

echo "will check compiler presence"
${CCPREFIX}gcc --version

echo "will cd to kernel"
cd ${KERNEL_SRC}


echo "will make mrproper"
make mrproper


# since raspberry was nice enough to can the config, shouldn't need this anymore (hoping)
#echo "will expand config"
#zcat ~/rpi/${CONFIG_GZ_FILE} > .config 

#NOTE: While attempting to match my build of the kernel to a canned image,
#      I tried to extract the config from the Pi and apply it to the kernel build.
#      Since then, I  opted to just build the entire kernel and flash it into
#      The image myself - so the need to apply the config went away 
#      - hence, the lines below are commented out
#echo "will apply configs"
#KERNEL=kernel
#make ARCH=arm CROSS_COMPILE=${CCPREFIX} olddefconfig

echo "will run the very long build..."
make -j 6 ARCH=arm CROSS_COMPILE=${CCPREFIX} bcmrpi_defconfig

echo "will run dtbs"
make ARCH=arm CROSS_COMPILE=${CCPREFIX} zImage modules dtbs
