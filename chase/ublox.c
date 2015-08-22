/* ========================================================================== */
/*                                                                            */
/*   ublox.c                                                                  */ 
/*   For Raspberry Pi                                                         */
/*                                                                            */
/*   Description                                                              */
/*   Serial GPS parser, storing results in shared memory                      */
/* ========================================================================== */

// Version 0.1 03/10/2013

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "hab.h"
#include <termios.h>
#include <dirent.h>

char Hex[] = "0123456789ABCDEF";

struct HAB *HAB;


int GPSChecksumOK(unsigned char *Buffer, int Count)
{
  unsigned char XOR, i, c;

  XOR = 0;
  for (i = 1; i < (Count-4); i++)
  {
    c = Buffer[i];
    XOR ^= c;
  }

  return (Buffer[Count-4] == '*') && (Buffer[Count-3] == Hex[XOR >> 4]) && (Buffer[Count-2] == Hex[XOR & 15]);
}

void ProcessGPRMCCommand(unsigned char *Buffer, int GPSIndex)
{
  int i, j, k, IntegerPart;
  double Divider;
  char GPS_Time[9] = "00:00:00";
  unsigned int GPS_Latitude_Minutes, GPS_Longitude_Minutes;
  double GPS_Latitude_Seconds, GPS_Longitude_Seconds;
  char *GPS_LatitudeSign="";
  char *GPS_LongitudeSign="";
  unsigned int GPS_Speed=0;
  unsigned int GPS_Direction=0;


  // $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A

  Divider = 1;

  for (i=7, j=0, k=0; (i<GPSIndex) && (j<8); i++)
  {
    if (Buffer[i] == ',')
    {
      j++;    // Segment index
      k=0;    // Index into target variable
      IntegerPart = 1;
    }
    else
    {
      switch (j)
      {
        case 0:
          // UTC Time
          if (k < 8)
          {
            GPS_Time[k++] = Buffer[i];
            if ((k==2) || (k==5))
            {
              GPS_Time[k++] = ':';
            }
            GPS_Time[k] = 0;
          }
          break;  // Start bit

        case 1:
          // Validity
          if (Buffer[i] == 'A')
          {
            // Message OK
            GPS_Latitude_Minutes = 0;
            GPS_Latitude_Seconds = 0;
            GPS_Longitude_Minutes = 0;
            GPS_Longitude_Seconds = 0;
            GPS_Speed = 0;
            GPS_Direction = 0;
            GPS_LongitudeSign = "-";  // new
          }
          else
          {
            return;
          }
          break;

        case 2:
          // Latitude
          if (k <= 1)
          {
            GPS_Latitude_Minutes = GPS_Latitude_Minutes * 10 + (int)(Buffer[i] - '0');
            Divider = 1;
          }
          else if ( k != 4)
          {
            Divider = Divider * 10;
            GPS_Latitude_Seconds = GPS_Latitude_Seconds  + (double)(Buffer[i] - '0') / Divider;
          }
          k++;
          break;  // Start bit

        case 3:
          // N or S
          if (k < 1)
          {
            // Latitude = GPS_Latitude_Minutes + GPS_Latitude_Seconds * 5 / 30000;
            if (Buffer[i] == 'S')
            {
              GPS_LatitudeSign = "-";
            }
            else
            {
              GPS_LatitudeSign = "";
            }
          }
          break;  // Start bit

        case 4:
          // Longitude
          if (k <= 2)
          {
            GPS_Longitude_Minutes = GPS_Longitude_Minutes * 10 + (int)(Buffer[i] - '0');
            Divider = 1;
          }
          else if ( k != 5)
          {
            Divider = Divider * 10;
            GPS_Longitude_Seconds = GPS_Longitude_Seconds + (double)(Buffer[i] - '0') / Divider;
          }
          /*
          if (k < 10)
          {
            GPS_Longitude[k++] = Buffer[i];
            GPS_Longitude[k] = 0;
          }
          */
          k++;
          break;  // Start bit

        case 5:
          // E or W
          // if (k < 1)
          {
            if (Buffer[i] == 'E')
            {
              GPS_LongitudeSign = "";
            }
          }
          break;  // Start bit

        case 6:
          // Speed
          if (IntegerPart)
          {
            if ((Buffer[i] >= '0') && (Buffer[i] <= '9'))
            {
              GPS_Speed = GPS_Speed * 10;
              GPS_Speed += (unsigned int)(Buffer[i] - '0');
            }
            else
            {
              IntegerPart = 0;
            }
          }
          break;  // Start bit

        case 7:
          // Direction
          if (IntegerPart)
          {
            if ((Buffer[i] >= '0') && (Buffer[i] <= '9'))
            {
              GPS_Direction = GPS_Direction * 10;
              GPS_Direction += (unsigned int)(Buffer[i] - '0');
            }
            else
            {
              IntegerPart = 0;
            }
          }
          break;  // Start bit

        default:
          break;
      }
    }
  }

	// Lock shared memory

	// Update variables
  	strcpy(HAB->GPS_Time, GPS_Time);
  	strcpy(HAB->GPS_LatitudeSign, GPS_LatitudeSign);
  	strcpy(HAB->GPS_LongitudeSign, GPS_LongitudeSign);
	HAB->GPS_Latitude = GPS_Latitude_Minutes + GPS_Latitude_Seconds * 5 / 3;
	HAB->GPS_Longitude = GPS_Longitude_Minutes + GPS_Longitude_Seconds * 5 / 3;
	if (*GPS_LatitudeSign == '-') HAB->GPS_Latitude = -1 * HAB->GPS_Latitude;
	if (*GPS_LongitudeSign == '-') HAB->GPS_Longitude = -1 * HAB->GPS_Longitude;
	HAB->GPS_Speed = GPS_Speed;
  	HAB->GPS_Direction = GPS_Direction;

	// Release shared memory
	
	printf ("%lf, %lf\n", HAB->GPS_Latitude, HAB->GPS_Longitude);
}

void ProcessGPGGACommand(unsigned char *Buffer, int GPSIndex)
{
  int i, j, k, IntegerPart;
  unsigned int Satellites, Altitude, GotAltitude;

  // $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
  //                                               =====  <-- altitude in field 8
  IntegerPart = 1;
  Satellites = 0;
  Altitude = 0;
  GotAltitude = 0;

  for (i=7, j=0, k=0; (i<GPSIndex) && (j<9); i++)
  {
    if (Buffer[i] == ',')
    {
      j++;    // Segment index
      k=0;    // Index into target variable
      IntegerPart = 1;
    }
    else
    {
      if (j == 6)
      {
        // Satellite Count
        if ((Buffer[i] >= '0') && (Buffer[i] <= '9'))
        {
          Satellites = Satellites * 10;
          Satellites += (unsigned int)(Buffer[i] - '0');
        }
      }
      else if (j == 8)
      {
        // Altitude
        if ((Buffer[i] >= '0') && (Buffer[i] <= '9') && IntegerPart)
        {
          Altitude = Altitude * 10;
          Altitude += (unsigned int)(Buffer[i] - '0');
	  GotAltitude = 1;
        }
        else
        {
          IntegerPart = 0;
        }
      }
    }
  }

  if (GotAltitude)
  // if ((Satellites >= 4) && GotAltitude)
  {
	// Lock shared memory

	HAB->GPS_Satellites = Satellites;

	if ((Altitude > 0) || (HAB->GPS_Altitude < 50))
        {
		// Fill in values
		// HAB->GPS_Satellites = Satellites;
		HAB->GPS_Altitude = Altitude;
		sprintf(HAB->batc, " %s %ldm ", HAB->GPS_Time, HAB->GPS_Altitude);
	}
   }

   // Release locked memory
}

void SendUBX(int fd, unsigned char *MSG, int len)
{
    write(fd, MSG, len);
	// bcm8235_i2cbb_puts(bb, MSG, len);
}

void InitUBlox(int fd)
{
    // Send navigation configuration command
    unsigned char setNav[] = {0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0x05, 0x00, 0xFA, 0x00, 0xFA, 0x00, 0x64, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0xDC};
    SendUBX(fd, setNav, sizeof(setNav));

}

void ProcessLine(int fd, unsigned char *Buffer, int Count)
{
    if (GPSChecksumOK(Buffer, Count))
    {
        if ((Buffer[1] == 'G') && (Buffer[2] == 'P') && (Buffer[3] == 'R') && (Buffer[4] == 'M') && (Buffer[5] == 'C'))
        {
            ProcessGPRMCCommand(Buffer, Count);
			printf("%s", Buffer);
            // printf("Time=%s, Lat=%s%7.5lf, Long=%s%7.5lf, Speed=%u, Direction=%d, ",
            // HAB->GPS_Time,
            // HAB->GPS_LatitudeSign, HAB->GPS_Latitude,
            // HAB->GPS_LongitudeSign, HAB->GPS_Longitude,
            // (HAB->GPS_Speed * 13) / 7, HAB->GPS_Direction);
        }
        else if ((Buffer[1] == 'G') && (Buffer[2] == 'P') && (Buffer[3] == 'G') && (Buffer[4] == 'G') && (Buffer[5] == 'A'))
        {
            ProcessGPGGACommand(Buffer, Count);
			printf("%s", Buffer);
        }
        else if ((Buffer[1] == 'G') && (Buffer[2] == 'P') && (Buffer[3] == 'G') && (Buffer[4] == 'S') && (Buffer[5] == 'V'))
        {
            // Disable GSV
            // printf("Disabling GSV\r\n");
            // unsigned char setGSV[] = { 0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x39 };
            // SendUBX(fd, setGSV, sizeof(setGSV));
        }
        else if ((Buffer[1] == 'G') && (Buffer[2] == 'P') && (Buffer[3] == 'G') && (Buffer[4] == 'L') && (Buffer[5] == 'L'))
        {
            // Disable GLL
            // printf("Disabling GLL\r\n");
            // unsigned char setGLL[] = { 0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x2B };
            // SendUBX(fd, setGLL, sizeof(setGLL));
        }
        else if ((Buffer[1] == 'G') && (Buffer[2] == 'P') && (Buffer[3] == 'G') && (Buffer[4] == 'S') && (Buffer[5] == 'A'))
        {
            // Disable GSA
            // printf("Disabling GSA\r\n");
            // unsigned char setGSA[] = { 0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x32 };
            // SendUBX(fd, setGSA, sizeof(setGSA));
        }
        else if ((Buffer[1] == 'G') && (Buffer[2] == 'P') && (Buffer[3] == 'V') && (Buffer[4] == 'T') && (Buffer[5] == 'G'))
        {
            // Disable VTG
            // printf("Disabling VTG\r\n");
            // unsigned char setVTG[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x05, 0x47};
            // SendUBX(fd, setVTG, sizeof(setVTG));
        }
        else
        {
            printf("Unknown NMEA sentence %c%c%c%c%c\r\n", Buffer[1], Buffer[2], Buffer[3], Buffer[4], Buffer[5]);
        }
    }
    else
    {
       printf("Bad checksum\r\n");
    }
}


// *****************************************************************************
// example for reading device ID
// address is the 7 bit address
// *****************************************************************************
int main(int argc, char **argv)
{
	int fd;
	struct termios options;
	unsigned char Line[100];
	int id, Length;
	key_t key;


    key = ftok("/home/pi/key",3);
    id = shmget(key, 300, IPC_CREAT);

    if (id == -1)
    {
        printf("shmget failed\n");
		exit(1);
	}

	printf("Identifier = %d\n", id);

    HAB = (struct HAB *)shmat(id, NULL, 0);

    if ((char *)HAB == (char *)(-1))
    {
		printf("shmat failed with error %d\n", errno);
		exit(1);
	}
	
	// Serial port
    /* open the port */
    if ((fd = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
    {
		printf("Cannot open serial port\n");
		exit(1);
	}
	printf("Serial handle = %d\n", fd);
    fcntl(fd, F_SETFL, 0);
    tcgetattr(fd, &options);
    options.c_lflag &= ~ECHO;
    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 10;
    options.c_cflag &= ~CSTOPB;
    cfsetispeed(&options, B9600);
    cfsetospeed(&options, B9600);
    tcsetattr(fd, TCSAFLUSH, &options);

	Length = 0;

    // InitUBlox();
	
    while (1)
    {
        int i;
		unsigned char Character;

		read(fd, &Character, 1);

        if (Character == '$')
		{
			Line[0] = Character;
			Length = 1;
		}
        else if (Length > 90)
        {
           	Length = 0;
        }
        else if ((Length > 0) && (Character != '\r'))
        {
			Line[Length++] = Character;
            if (Character == '\n')
            {
				Line[Length] = '\0';
                ProcessLine(fd, Line, Length);
				Length = 0;
            }
		}
	}

	exit(0);
}


