#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <dirent.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stropts.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <wiringPi.h>


#include "hab.h"

struct HAB *HAB;


int main(int argc, char **argv)
{
    key_t key;
    int   id, LoopCount;
	char Command[512], UTCTime[10];
	char *Vehicle, *Password;

	printf("Raspberry Pi Chase Car Uploader\r\n\n");

	printf("Shared memory = %d bytes\n", sizeof(struct HAB));

    key = ftok("/home/pi/key",3);

    printf ("key = %d\n", key);

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
		printf("shmat failed\n");
		exit(1);
	}
	
	wiringPiSetup();

	// Set up pin chase car on/off switch
	pinMode (0, INPUT);
	pullUpDnControl(0, PUD_UP);

	Vehicle = "M6RPI_chase";
	Password = "aurora";
	
	LoopCount = 22;
	
	while(1)
 	{
		if (digitalRead(0))
		{
			// disable chase car upload
			HAB->car_status = 0;
		}
		else if (!HAB->car_status)
		{
			HAB->car_status = 1;
			LoopCount = 25;
		}
		
		HAB->batc_status = !digitalRead(6);
		
		// if (HAB->car_status >= 0)
		// {
		// 	HAB->car_status++;
		// }

		if (++LoopCount >= 25)
		{
			LoopCount = 0;
			
			if (!HAB->car_status)
			{
				// Disabled
				printf("Chase car upload disabled\n");
			}
			else if (HAB->GPS_Satellites >= 4)
			{
				UTCTime[0] = HAB->GPS_Time[0];
				UTCTime[1] = HAB->GPS_Time[1];
				UTCTime[2] = HAB->GPS_Time[3];
				UTCTime[3] = HAB->GPS_Time[4];
				UTCTime[4] = HAB->GPS_Time[6];
				UTCTime[5] = HAB->GPS_Time[7];
				UTCTime[6] = '\0';
		
				sprintf(Command,
						"wget -O wget.txt \"http://spacenear.us/tracker/track.php?vehicle=%s&time=%s&lat=%10.7lf&lon=%9.7lf&speed=%1.0lf&alt=%d&heading=%d&pass=%s\"",	
						Vehicle, UTCTime, HAB->GPS_Latitude, HAB->GPS_Longitude, (double)(HAB->GPS_Speed) * 13.0 / 7.0, HAB->GPS_Altitude, HAB->GPS_Direction, Password);
				printf("%s\n", Command);
				system(Command);
				// HAB->car_status = 0;
			}
			else
			{
				printf("No GPS Lock\n");
			}
		}
		
		sleep(1);
   	}
}


