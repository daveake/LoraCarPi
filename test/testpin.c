
#include <wiringPi.h>
#include "stdio.h"

main ()
{
  wiringPiSetup();

  pinMode (0, INPUT);
	pullUpDnControl(0, PUD_UP);

	if (digitalRead(0))
	{
		printf("UP\n");
	}
	else
	{
		printf("DOWN\n");
	}
}

