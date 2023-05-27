#! /bin/sh

case "$1" in
	start)
		echo "Starting AESD Server"
		start-stop-daemon aesdsocket -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
		;;
	stop)
		echo "Stopping AESD Server"
		start-stop-daemon aesdsocket -K -n aesdsocket
		;;
	*)
		echo "USAGE:: $0 {start|stop}"
	exit 1
esac

exit 0
