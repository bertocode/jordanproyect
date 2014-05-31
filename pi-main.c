#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>			//Used for UART
#include <fcntl.h>			//Used for UART
#include <termios.h>		//Used for UART

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define Data_record                         0
#define End_Of_File_record                  1
#define Extended_Segment_Address_Record     2
#define Start_Segment_Address_Record        3
#define Extended_Linear_Address_Record      4
#define Start_Linear_Address_Record         5

typedef struct {
	uint8_t numBytes;
	uint16_t offset;
	uint8_t type;
	uint8_t data[0x10];
	uint8_t checksum;
} hex_line;

uint32_t stringToHex(char* s,uint8_t lenght);
uint8_t parse_line(char* line, hex_line* data);

int main()
{
	printf("M2C Raspberry Pi host program\n");
	//-------------------------
	//----- SETUP USART 0 -----
	//-------------------------
	//At bootup, pins 8 and 10 are already set to UART0_TXD, UART0_RXD (ie the alt0 function) respectively
	int uart0_filestream = -1;

	//OPEN THE UART
	//The flags (defined in fcntl.h):
	//	Access modes (use 1 of these):
	//		O_RDONLY - Open for reading only.
	//		O_RDWR - Open for reading and writing.
	//		O_WRONLY - Open for writing only.
	//
	//	O_NDELAY / O_NONBLOCK (same function) - Enables nonblocking mode. When set read requests on the file can return immediately with a failure status
	//											if there is no input immediately available (instead of blocking). Likewise, write requests can also return
	//											immediately with a failure status if the output can't be written immediately.
	//
	//	O_NOCTTY - When set and path identifies a terminal device, open() shall not cause the terminal device to become the controlling terminal for the process.
	uart0_filestream = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	if (uart0_filestream == -1)
	{
		//ERROR - CAN'T OPEN SERIAL PORT
		printf("Error - Unable to open UART.  Ensure it is not in use by another application\n");
	}
	
	//CONFIGURE THE UART
	//The flags (defined in /usr/include/termios.h - see http://pubs.opengroup.org/onlinepubs/007908799/xsh/termios.h.html):
	//	Baud rate:- B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, B230400, B460800, B500000, B576000, B921600, B1000000, B1152000, B1500000, B2000000, B2500000, B3000000, B3500000, B4000000
	//	CSIZE:- CS5, CS6, CS7, CS8
	//	CLOCAL - Ignore modem status lines
	//	CREAD - Enable receiver
	//	IGNPAR = Ignore characters with parity errors
	//	ICRNL - Map CR to NL on input (Use for ASCII comms where you want to auto correct end of line characters - don't use for bianry comms!)
	//	PARENB - Parity enable
	//	PARODD - Odd parity (else even)
	struct termios options;
	tcgetattr(uart0_filestream, &options);
	options.c_cflag = B115200 | CS8 | CLOCAL | CREAD | PARENB | PARODD;		//<Set baud rate
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	tcflush(uart0_filestream, TCIFLUSH);
	tcsetattr(uart0_filestream, TCSANOW, &options);


	printf("WAITING FOR BYTES OR SOMETHING\n");

	while (1)
	{
	//----- CHECK FOR ANY RX BYTES -----
	if (uart0_filestream != -1)
	{
		// Read up to 255 characters from the port if they are there
		unsigned char rx_buffer[256];
		int rx_length = read(uart0_filestream, (void*)rx_buffer, 255);		//Filestream, buffer to store in, number of bytes to read (max)
		if (rx_length > 0)
		{
			//Bytes received
			rx_buffer[rx_length] = '\0';
			printf("%i bytes read : 0x%X\n", rx_length, rx_buffer[0]);

			static const char filename[] = "main.hex";
			FILE *file = fopen ( filename, "r" );
			if ( file != NULL )
			{
				char* line;
				line = (char*) malloc(50 * sizeof(char)); /* or other suitable maximum line size */
				while ( fgets ( line, 50 * sizeof(char), file ) != NULL ) /* read a line */
				{
					//fputs ( line, stdout ); /* write the line */

					hex_line hexLine;
					if(!parse_line(line, &hexLine))
						return 0;

                                        printf(ANSI_COLOR_YELLOW ":" ANSI_COLOR_RESET);
					printf(ANSI_COLOR_GREEN "%02X" ANSI_COLOR_RESET, hexLine.numBytes);
                                        printf(ANSI_COLOR_BLUE "%04X" ANSI_COLOR_RESET, hexLine.offset);
                                        printf(ANSI_COLOR_RED "%02X" ANSI_COLOR_RESET, hexLine.type);
					for (uint8_t i=0; i<0x10; i++)
	                                        printf(ANSI_COLOR_CYAN "%02X" ANSI_COLOR_RESET, hexLine.data[i]);
                                        printf(ANSI_COLOR_MAGENTA "%02X" ANSI_COLOR_RESET "\n", hexLine.checksum);




					if (uart0_filestream != -1)
					{
						int count = write(uart0_filestream, &hexLine, sizeof(hexLine));
						if (count < 0)
							printf("UART TX error\n");
					}

					// Esperamos a un input desde la UART para seguir enviado.
					while(read(uart0_filestream, (void*)rx_buffer, 255) <= 0);

					// Estamos enviando el fw, pero si recibimos una orden que no sea la de
					// continuar, reiniciamos el proceso.
					if (rx_buffer[0] != 0xF0)
					{
						rewind(file);
						printf("Enviando fichero de firmware desde el principio a traves de la UART\n");
					}
				}
				fclose ( file );
			}
			else
				perror ( filename ); /* why didn't the file open? */
		}
	}
	}
	return 0;
}

// Parse the given line and store the content in the structure '*data'
uint8_t parse_line(char* line, hex_line* data) {

	uint8_t i;

	if(*line!=':')
		return 0;
	else
	{
		line++;
		data->numBytes  =   (uint8_t)   stringToHex(line, 2);
		line+=2;
		data->offset    =   (uint16_t)  stringToHex(line, 4);
		line+=4;
		data->type      =   (uint8_t)   stringToHex(line, 2);
		line+=2;

		for (i = 0; i < data->numBytes; i++)
			data->data[i] = (uint8_t) stringToHex(line+2*i, 2);

		data->checksum = (uint8_t) stringToHex(line+2*i, 2);

		return 1;
	}
}



// return x^y
uint32_t power(uint32_t x, uint32_t y) {
	uint32_t i,res=x;
	if(y==0)
		return 1;

	if(y==1)
		return x;

	for(i=0;i<(y-1);i++)
		res *= x;

	return res;
}



// return a number written in hexadecimal of 'lenght' caracters long

uint32_t stringToHex(char* s,uint8_t lenght)
{

	uint32_t num = 0,i;

	for (i=0; i<lenght; i++)
	{
		if(s[i]>='0' && s[i]<='9') {
			s[i] -= '0';
		}

		if(s[i]>='A' && s[i]<='F') {
			s[i] -= 'A';
			s[i] += 10;

		}
		num += s[i]*power(16,lenght-i-1);
	}

	return num;
}


