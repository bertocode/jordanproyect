#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>		//Used for UART
#include <fcntl.h>			//Used for UART
#include <termios.h>		//Used for UART
#include <wiringPi.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define GPIO_RESET_PIN 1

typedef struct {
	uint8_t numBytes;
	uint16_t offset;
	uint8_t type;
	uint8_t data[0x10];
	uint8_t checksum;
} hex_line;

uint32_t stringToHex(char* s,uint8_t lenght);
uint8_t parse_line(char* line, hex_line* data);
void print_hex_line(hex_line l);
void print_error(void);
void print_ok(void);

int main()
{
	printf("M2C Raspberry Pi host program\n");

	int uart0_filestream = -1;

	 // Indice de la última pagina que enviamos. Se usa para evitar hacer rewinds cada vez que nos piden una.
	uint32_t last_line_index = 0;

	// Buffer de lectura de archivo
	char* line = (char*) malloc(50 * sizeof(char));
	// Struct para almacenar las lineas ihex parseadas
	hex_line hexLine;
	// Buffer de lectura de la UART
	unsigned char rx_buffer[256];
	// Datos leidos de la UART
	int rx_length = 0;

	printf("Configurando UART");
	// Abrimos la UART en modo lectura/escritura/no bloqueante
	uart0_filestream = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY);
	if (uart0_filestream == -1)
	{
		print_error();
		// Error, no pudimos abrir el puerto serie
		printf(ANSI_COLOR_RED);
		printf("ERROR - No se pudo abrir la UART\n");
		printf(ANSI_COLOR_RESET);
		return 1;
	}
	print_ok();

	// Configuración de la UART: 115200 baudios, 8bits, 1bit de parada, paridad impar
	struct termios options;
	tcgetattr(uart0_filestream, &options);
	options.c_cflag = B115200 | CS8 | CLOCAL | CREAD | PARENB | PARODD;
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	tcflush(uart0_filestream, TCIFLUSH);
	tcsetattr(uart0_filestream, TCSANOW, &options);

	uint32_t version;
	printf("Comprobando archivo de version");
	// Abrimos el archivo de imagen
	static const char verfilename[] = "image.ver";
	FILE *verfile = fopen ( verfilename, "r" );

	if ( verfile == NULL )
	{
		print_error();
		printf(ANSI_COLOR_RED);
		printf("ERROR - No se pudo abrir el archivo de version\n");
		perror ( verfilename );
		printf(ANSI_COLOR_RESET);
		return 1;
	}
	else
	{
		if (fgets ( line, 50 * sizeof(char), verfile ) != NULL) // Esto deberia cumplirse siempre
		{
			version = atoi(line);
			printf(". v%u", version);
		}
		else
		{
			print_error();
			printf(ANSI_COLOR_RED);
			printf("ERROR - Archivo de version no valido\n");
			printf(ANSI_COLOR_RESET);
			return 1;
		}
		fclose(verfile);
	}
	print_ok();


	printf("Comprobando archivo de imagen");
	// Abrimos el archivo de imagen
	static const char filename[] = "image.hex";
	FILE *file = fopen ( filename, "r" );

	if ( file == NULL )
	{
		print_error();
		printf(ANSI_COLOR_RED);
		printf("ERROR - No se pudo abrir el archivo de imagen\n");
		perror ( filename );
		printf(ANSI_COLOR_RESET);
		return 1;
	}

	// Comprobamos que el binario sea correcto
	while ( fgets ( line, 50 * sizeof(char), file ) != NULL )
	{
		if(!parse_line(line, &hexLine))
		{
			printf(ANSI_COLOR_RED);
			printf("ERROR - Encontrada linea no valida en el archivo de imagen\n");
			printf(ANSI_COLOR_RESET);
			return 1;
		}

		uint16_t sum = hexLine.numBytes;
		sum += (uint8_t)hexLine.offset;
		sum += (uint8_t)((hexLine.offset) >> 8);
		sum += hexLine.type;
		for (uint8_t i=0; i<hexLine.numBytes; i++)
			sum += hexLine.data[i];

		uint8_t calculated_checksum = 0x100 - (0xFF & sum);
		if (calculated_checksum != hexLine.checksum)
		{
			printf(ANSI_COLOR_RED);
			printf("ERROR - El checksum no concuerda para la linea: \n");
			printf(ANSI_COLOR_RESET);
			print_hex_line(hexLine);
			printf("El checksum obtenido es %X y el esperado era %02X\n", calculated_checksum, hexLine.checksum);
			return 1;
		}
	}
	rewind(file);
	print_ok();

	printf("Reiniciando nodo maestro");
	if (wiringPiSetup() == -1)
	{
		print_error();
		printf(ANSI_COLOR_RED);
		printf("ERROR - No se pudo configurar WiringPi\n");
		printf(ANSI_COLOR_RESET);
		return 1;
	}
	pinMode(GPIO_RESET_PIN, OUTPUT);
	// Generamos un pulso en el pin de reset del procesador
	digitalWrite(GPIO_RESET_PIN, 0);
	delay(500);
	digitalWrite(GPIO_RESET_PIN, 1);
	print_ok();

	printf("Esperando datos desde la UART\n");
	while (1)
	{
		rx_length = 0;

		// Esperamos a un input desde la UART
		while((rx_length = read(uart0_filestream, (void*)rx_buffer, 255)) <= 0);

		// Datos recibidos
		rx_buffer[rx_length] = '\0';
		//printf("%i bytes read : 0x%X\n", rx_length, rx_buffer[0]);
		printf("0x%X: ", rx_buffer[0]);

		// Seleccionamos accion en función del primer dato del buffer
		switch (rx_buffer[0])
		{
			case 0xA0: // Peticion de linea del HexFile
			{
				// Obtenemos un uint32 a partir de los uint8 de entrada
				// El +1 es porque line index es 0-based mientras que last_line_index es 1-based
				uint32_t line_index = *((uint32_t*)&rx_buffer[1]) + 1;

				// Recolocamos el puntero al fichero en función de la linea que nos piden
				if ((int32_t)(line_index - last_line_index) > 0) // Nos piden una linea posterior a la ultima que leimos
				{
					for (uint32_t i = line_index - last_line_index; i > 0; i--)
						fgets ( line, 50 * sizeof(char), file );

				}
				else // Nos piden una anterior
				{
					printf(ANSI_COLOR_MAGENTA);
					rewind(file); // Reiniciamos puntero

					// Avanzamos tantas lineas como nos piden
					for (uint32_t i = line_index; i > 0; i--)
						fgets ( line, 50 * sizeof(char), file );
				}

				// Actualizamos el contador de ultima linea pedida
				last_line_index = line_index;

				if(!parse_line(line, &hexLine))
				{
					printf(ANSI_COLOR_RED);
					printf("ERROR - Encontrada linea no valida en el archivo de imagen\n");
					printf(ANSI_COLOR_RESET);
					break;
				}

				printf("%s:%04u ", filename, line_index);
				printf(ANSI_COLOR_RESET);

				if (uart0_filestream != -1)
				{
					int count = write(uart0_filestream, &hexLine, sizeof(hexLine));

					if (count < 0) // No se ha enviado bien, mostramos un error
					{
						printf(ANSI_COLOR_RED);
						printf("ERROR - No se pudo escribir en la UART durante el envio de una linea\n");
						printf(ANSI_COLOR_RESET);
					}
					else // Se ha enviado correctamente, mostramos la linea enviada
						print_hex_line(hexLine);
				}
				else
				{
					// Error, no pudimos abrir el puerto serie
					printf(ANSI_COLOR_RED);
					printf("ERROR - No se pudo acceder a la UART durante el envio de una linea\n");
					printf(ANSI_COLOR_RESET);
					return 1;
				}
				break;
			}
			case 0xB0: // Peticion de version
			{
				// Escribimos sizeof(hexLine) bytes porque es lo que el buffer DMA del nodo espera
				int count = write(uart0_filestream, &version, sizeof(hexLine));

				if (count < 0) // No se ha enviado bien, mostramos un error
				{
					printf(ANSI_COLOR_RED);
					printf("ERROR - No se pudo escribir en la UART durante el envio de identificador de version\n");
					printf(ANSI_COLOR_RESET);
				}
				else // Se ha enviado correctamente, mostramos la linea enviada
					printf("Version enviada: v%u\n", version);

				break;
			}
			case 0xFF: // Legacy: Reinicio del puntero de archivo
				rewind(file);
			case 0xF0: // Legacy: Envio de la sigiente linea por UART
			{
				if ( fgets ( line, 50 * sizeof(char), file ) != NULL )
				{
					if(!parse_line(line, &hexLine))
					{
						printf(ANSI_COLOR_RED);
						printf("ERROR - Encontrada linea no valida en el archivo de imagen\n");
						printf(ANSI_COLOR_RESET);
						break;
					}

					if (uart0_filestream != -1)
					{
						int count = write(uart0_filestream, &hexLine, sizeof(hexLine));

						if (count < 0) // No se ha enviado bien, mostramos un error
						{
							printf(ANSI_COLOR_RED);
							printf("ERROR - No se pudo escribir en la UART durante el envio de una linea\n");
							printf(ANSI_COLOR_RESET);
						}
						else // Se ha enviado correctamente, mostramos la linea enviada
							print_hex_line(hexLine);
					}
					else
					{
						// Error, no pudimos abrir el puerto serie
						printf(ANSI_COLOR_RED);
						printf("ERROR - No se pudo acceder a la UART durante el envio de una linea\n");
						printf(ANSI_COLOR_RESET);
						return 1;
					}
				}
				break;
			}
			default:
				write(uart0_filestream, &version, sizeof(hexLine));
				printf(ANSI_COLOR_RED);
				printf("Codigo no identificado.\n");
				printf(ANSI_COLOR_RESET);
				break;
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

void print_hex_line(hex_line l)
{
	printf(ANSI_COLOR_YELLOW ":" ANSI_COLOR_RESET);
	printf(ANSI_COLOR_GREEN "%02X" ANSI_COLOR_RESET, l.numBytes);
	printf(ANSI_COLOR_BLUE "%04X" ANSI_COLOR_RESET, l.offset);
	printf(ANSI_COLOR_RED "%02X" ANSI_COLOR_RESET, l.type);
	for (uint8_t i=0; i<l.numBytes; i++)
		printf(ANSI_COLOR_CYAN "%02X" ANSI_COLOR_RESET, l.data[i]);
	printf(ANSI_COLOR_MAGENTA "%02X" ANSI_COLOR_RESET "\n", l.checksum);
}

void print_ok()
{
	printf(ANSI_COLOR_GREEN " OK\n" ANSI_COLOR_RESET);
}

void print_error()
{
	printf(ANSI_COLOR_GREEN " ERROR\n" ANSI_COLOR_RESET);
}
