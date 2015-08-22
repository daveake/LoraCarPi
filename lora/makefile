lora: lora.o urlencode.o base64.o
	cc -o lora lora.o urlencode.o base64.o -lm -lwiringPi -lwiringPiDev -lcurl

lora.o: lora.c
	gcc -c lora.c

urlencode.o: urlencode.c
	gcc -c urlencode.c

base64.o: base64.c
	gcc -c base64.c
