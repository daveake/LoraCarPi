#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>   /* File control definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stropts.h>
#include <sys/ioctl.h> 
#include <time.h>

#include "../chase/hab.h"

struct HAB *HAB;

void ProcessLine(char *Line)
{
	static unsigned int PreviousAltitude=0;
	static long Then=0;
	long Now;
	struct tm tm;
	float Climb, Period;
	
	int Counter, FieldCount; 

	printf("%s\n", Line);
	
	FieldCount = sscanf(Line+1, "%11[^,],%d,%8[^,],%f,%f,%u",
						&(HAB->HAB_Payload),
						&Counter,
						&(HAB->HAB_Time),
						&(HAB->HAB_Latitude),
						&(HAB->HAB_Longitude),
						&(HAB->HAB_Altitude));
						
	HAB->HAB_status = FieldCount == 6;
	
	strptime(HAB->HAB_Time, "%H:%M:%S", &tm);
	Now = tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;
	
	if ((Then > 0) && (Now > Then))
	{
		Climb = (float)HAB->HAB_Altitude - (float)PreviousAltitude;
		Period = HAB->HAB_AscentRate = (float)Now - (float)Then;
		HAB->HAB_AscentRate = Climb / Period;
	}
	else
	{
		HAB->HAB_AscentRate = 0;
	}
	
	PreviousAltitude = HAB->HAB_Altitude;
	Then = Now;
}

void ProcessChar(char Character)
{
	static char Line[80];
	static int Count=0;

	if (Character == '$')
	{
		Count=0;
		Line[Count++] = Character;
	}
	else if (Count>=79)
	{
		Count = 0;
	}
	else if (Character == '\n')
	{
		Line[Count] = '\0';
		ProcessLine(Line);
		Count = 0;
	}
	else if (Count > 0)
	{
		Line[Count++] = Character;
	}
}

void Connect(char *IPAddress)
{
    int i, sockfd = 0, n = 0;
    char recvBuff[1024];
    struct sockaddr_in serv_addr; 

    memset(recvBuff, '0',sizeof(recvBuff));
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return;
    } 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(7300);

    if(inet_pton(AF_INET, IPAddress, &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return;
    } 

    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
       printf("\n Error : Connect Failed \n");
       return;
    } 

    while ( (n = read(sockfd, recvBuff, sizeof(recvBuff)-1)) > 0)
    {
        recvBuff[n] = 0;
		for (i=0; i<n; i++)
		{
			ProcessChar(recvBuff[i]);
		}
    } 
}

int main(int argc, char *argv[])
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
	
    if(argc != 2)
    {
        printf("\n Usage: %s <ip of server> \n",argv[0]);
        return 1;
    } 

	while (1)
	{
		HAB->HAB_status = 0;
		printf("Connecting ...\n");
		Connect(argv[1]);
	}

	HAB->HAB_status = 0;
}
