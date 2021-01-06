#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int serial_init(char * path)
{
	
	// Init
	// hopefully this part is correct
	int hSerial = open( path, O_RDWR | O_NONBLOCK | O_NDELAY); 
	// argv[1] is the path to the serial interface given by the commandline argument
	fcntl(hSerial, F_SETFL, 0);
	
	if (hSerial <= 0){
		//return -1; //TODO
	} // ends init with -1 so main can quit as well
	
	int iRetVal;
	
	struct termios o_tty;
	memset (&o_tty, 0, sizeof(o_tty));
	iRetVal = tcgetattr(hSerial, &o_tty);
	
	cfsetispeed(&o_tty, B9600);
	cfsetispeed(&o_tty, B9600);
	
	o_tty.c_cflag |= (CLOCAL | CREAD);
	o_tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	o_tty.c_oflag &= ~(OPOST);
	o_tty.c_iflag = 0;
	o_tty.c_iflag &= ~(INLCR|ICRNL);
	o_tty.c_cc[VMIN] = 0;
	o_tty.c_cc[VTIME] = 10;
	
	o_tty.c_cflag &= ~PARENB;
	o_tty.c_cflag &= ~CSTOPB;
	o_tty.c_cflag &= ~CSIZE;
	o_tty.c_cflag |= CS8;
	
	tcsetattr(hSerial, TCSANOW, &o_tty);

	return hSerial;
}

int serial_close(int hSerial)
{
	close(hSerial); // frees all mallocated memory
	return 0;
}

int serial_write(int hSerial, char * chBuffOut, int iBuffOutSize)
{
	int n_written = write(hSerial, chBuffOut, iBuffOutSize);
	return n_written;
}

int serial_read(int hSerial, char * chBuffIn, int iBuffInSize)
{
	int n_read = read(hSerial, chBuffIn, iBuffInSize);
	return n_read;
}


