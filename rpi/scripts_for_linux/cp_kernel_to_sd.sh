set -e
cd ${KERNEL_SRC}


set +e
rmdir ./mnt/fat32
rmdir ./mnt/ext4
rmdir ./mnt

set -e
mkdir ./mnt

mkdir mnt/fat32
mkdir mnt/ext4
sudo mount /dev/sdb1 mnt/fat32
sudo mount /dev/sdb2 mnt/ext4

echo "BOOT - -"
ls mnt/fat32

echo "ROOTFS - -"
ls mnt/ext4


sudo make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- INSTALL_MOD_PATH=mnt/ext4 modules_install


echo "new kernel at mnt/fat32/${KERNEL}-new.img"

sudo cp arch/arm/boot/zImage mnt/fat32/${KERNEL}-new.img
sudo cp arch/arm/boot/dts/*.dtb mnt/fat32/
sudo cp arch/arm/boot/dts/overlays/*.dtb* mnt/fat32/overlays/
sudo cp arch/arm/boot/dts/overlays/README mnt/fat32/overlays/

echo "Copied - will umount"
sudo umount mnt/fat32
sudo umount mnt/ext4

