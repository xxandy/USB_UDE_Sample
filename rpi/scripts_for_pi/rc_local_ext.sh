set +e

echo "rc_local_ext.sh starts"

dmesg -c > /dev/null

modprobe dwc2
if [ ! $? -eq 0 ]
then
    echo "error loading dwc2"
else
    echo "dwc2 loaded ok"
fi


modprobe libcomposite
if [ ! $? -eq 0 ]
then
    echo "error installing libcomposite"
    dmesg
else
    echo "libcomposite installed ok"
fi



insmod /boot/simpleufn.ko
if [ ! $? -eq 0 ]
then
    echo "error installing simpleufn"
    dmesg
else
    echo "simpleufn installed ok"
fi


insmod /boot/simplegadget.ko pattern=2
if [ ! $? -eq 0 ]
then
    echo "error installing simplegadget pattern 2"
    dmesg
else
    echo "simplegadget installed ok pattern 2"
    echo "START MODULE LIST"
    lsmod
    echo "END MODULE LIST"
fi





echo "rc_local_ext.sh ends"
