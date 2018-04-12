# Run this script on the Raspberry Pi to get firmware hash,
# if you want to build a local kernel that matches a specific
# version of the Pi kernel.
# (It turns out this isn't as important, as I ended up
#  flashing the entire kernel)

echo "will get firmware hash"
FIRMWARE_HASH=$(zgrep "* firmware as of" /usr/share/doc/raspberrypi-bootloader/changelog.Debian.gz | head -1 | awk '{ print $5 }')
echo "hash is [$FIRMWARE_HASH]"

