#!/bin/bash
readonly TARGET_IP="192.168.7.2"
readonly TARGET_PORT="3000"
readonly PROGRAM="prog"
readonly PROGRAM_PATH="build/prog"
readonly TARGET_DIR="/home/root"
readonly SERVICE="ele4205-serveur"

#Necessary for beginning pattern recognition by VS Code
echo "Deploying on Target"

#Stop the boot service, kill gdbserver on target and delete old binary.
#The service holds /dev/video0 and port 4099, so a debug session started while
#it runs would fail to open the camera or to bind. Stopping it does not disable
#it: the unit still starts on the next boot, which is what the demo needs.
#Errors are swallowed so the script also works before the service is installed.
ssh root@${TARGET_IP} "sh -c '/etc/init.d/${SERVICE} stop 2>/dev/null; /usr/bin/killall -q gdbserver; rm -f ${TARGET_DIR}/${PROGRAM}; exit 0'"

#Send the program to the Target
scp ${PROGRAM_PATH} root@${TARGET_IP}:${TARGET_DIR}/

#Necessary for ending pattern recognition by VS Code
echo "Starting gdbserver on Target"

#Start gdbserver on target
ssh -t root@${TARGET_IP} "sh -c 'cd ${TARGET_DIR}; gdbserver localhost:${TARGET_PORT} ./${PROGRAM}'"