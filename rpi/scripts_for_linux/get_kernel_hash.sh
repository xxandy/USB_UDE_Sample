# the Raspberry Pi Firmware hash and Kernel hash go hand in hand, and 
# must be matched!
# From a booted Raspberry Pi, you get the firmware hash with the 
# get_f_hash.sh script (see ../scripts_for_pi).
# then you use the firmware hash and replace the setting below (FIRMWARE_HASH_FROM_RPI),
# and run this script - this will get you a value you can use in for KERNEL_HAS in rpienv.sh.
#
# finally, you put both hashes in the rpienv.sh, before you get the
# Linux source and try to build.

FIRMWARE_HASH_FROM_RPI=3347884c7df574bbabeff6dca63caf686e629699

wget https://raw.github.com/raspberrypi/firmware/$FIRMWARE_HASH_FROM_RPI/extra/git_hash -O -


