The jog-shuttle controller can be used to control the player. Events from the jog-shuttle controller are read through an event device file /dev/input/eventxxx; the code that reads from this device file is in ingex/player/ingex_player/shuttle_input.c and the mapping of jog-shuttle events to player control is in ingex/player/ingex_player/shuttle_input_connect.c.

The player needs to have read permission for the event device and by default a non-root user does not have read permission.

Edit the udev configuration file, /etc/udev/rules.d/50-udev-default.rules, as root user and change the mode in the line
 KERNEL=="mouse*|mice|event*", NAME="input/%k", MODE="0640" 
to 0644 
 KERNEL=="mouse*|mice|event*", NAME="input/%k", MODE="0644" 

Unplug the jog-shuttle controller and restart the udev system 
 > sudo /sbin/service boot.udev restart 
Plug the controller back in and check that there is at least one device (which is probably the jog-shuttle) can be read by non-root users 
 > ls -l /dev/input/event* 


The problem with the generic Linux driver for the jog-shuttle controller is that it treats a 0 jog position (the position ranges from 0 to 255) and neutral shuttle position as non-events and therefore these positions can be missed. The patch file addresses this problem with a simple hack that is similar to hacks used for other devices. This hack only addresses the 0 jog position problem and the player code has a workaround to detect the neutral shuttle position.

This is not a major problem in the applications we have and therefore I haven't bothered with creating a patch for every kernel version that comes along. I haven't created a patch for the baseline openSUSE 10.3 kernel, for instance.

The modifications contained in the patch file are relatively minor.

1) Check patch file and modified sources match the kernel sources

2) copy modified sources to kernel sources

3) compile the kernel usbhid module
> cd /usr/src/linux
> make mrproper
> make cloneconfig
> make scripts
> make prepare

> cd /usr/src/linux/drivers/usb/input
> make -C /usr/src/linux M=`pwd`

4) make module directory
> mkdir /lib/modules/`uname -r`/contour

5) copy usbhid.ko
> cp /usr/src/linux/drivers/usb/input/usbhid.ko /lib/modules/`uname -r`/contour

6) copy setup scipt
> cp qc_player_setup /etc/init.d/

7) install setup script
> /sbin/chkconfig --add qc_player_setup
> /sbin/chkconfig qc_player_setup 35
> /sbin/chkconfig --terse qc_player_setup

8) check
> /etc/init.d/qc_player_setup start

Philip
