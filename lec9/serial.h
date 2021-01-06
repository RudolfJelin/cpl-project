
#ifndef __SERIAL_H__
#define __SERIAL_H__

int serial_init(char * path);
int serial_close(int hSerial);
int serial_write(int hSerial, char * chBuffOut, int iBuffOutSize);
int serial_read(int hSerial, char * chBuffIn, int iBuffInSize);

#endif
