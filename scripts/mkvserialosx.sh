#@/bin/sh
#
# uses socat (1.7.3+) to create a virtual serial port pair
# useful for testing etc.
MASTER=master
SLAVE=slave
USER=$(id -un)
GROUP=$(id -gn)
sudo socat -dddd -lf /tmp/socat pty,link=/dev/$MASTER,raw,echo=0,user=$USER,group=GROUP pty,link=/dev/slave,raw,echo=0,user=$USER,group=$GROUP
