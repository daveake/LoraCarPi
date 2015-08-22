#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#include <wiringPi.h>
#include <wiringPiSPI.h>

#include "urlencode.h"
#include "base64.h"

#include "../chase/hab.h"

struct HAB *HAB;

// RFM98
uint8_t currentMode = 0x81;

#define REG_FIFO                    0x00
#define REG_FIFO_ADDR_PTR           0x0D 
#define REG_FIFO_TX_BASE_AD         0x0E
#define REG_FIFO_RX_BASE_AD         0x0F
#define REG_RX_NB_BYTES             0x13
#define REG_OPMODE                  0x01
#define REG_FIFO_RX_CURRENT_ADDR    0x10
#define REG_IRQ_FLAGS               0x12
#define REG_RSSI_PACKET             0x1A
#define REG_RSSI_CURRENT            0x1B
#define REG_DIO_MAPPING_1           0x40
#define REG_DIO_MAPPING_2           0x41
#define REG_MODEM_CONFIG            0x1D
#define REG_MODEM_CONFIG2           0x1E
#define REG_PAYLOAD_LENGTH          0x22
#define REG_IRQ_FLAGS_MASK          0x11
#define REG_HOP_PERIOD              0x24

// MODES
#define RF96_MODE_RX_CONTINUOUS     0x85
#define RF96_MODE_SLEEP             0x80
#define RF96_MODE_STANDBY           0x81

#define PAYLOAD_LENGTH              255

// Modem Config 1
#define EXPLICIT_MODE               0x00
#define IMPLICIT_MODE               0x01

#define ERROR_CODING_4_5            0x02
#define ERROR_CODING_4_6            0x04
#define ERROR_CODING_4_7            0x06
#define ERROR_CODING_4_8            0x08

#define BANDWIDTH_7K8               0x00
#define BANDWIDTH_10K4              0x10
#define BANDWIDTH_15K6              0x20
#define BANDWIDTH_20K8              0x30
#define BANDWIDTH_31K25             0x40
#define BANDWIDTH_41K7              0x50
#define BANDWIDTH_62K5              0x60
#define BANDWIDTH_125K              0x70
#define BANDWIDTH_250K              0x80
#define BANDWIDTH_500K              0x90

// Modem Config 2

#define SPREADING_6                 0x60
#define SPREADING_7                 0x70
#define SPREADING_8                 0x80
#define SPREADING_9                 0x90
#define SPREADING_10                0xA0
#define SPREADING_11                0xB0
#define SPREADING_12                0xC0

#define CRC_OFF                     0x00
#define CRC_ON                      0x04

// POWER AMPLIFIER CONFIG
#define REG_PA_CONFIG               0x09
#define PA_MAX_BOOST                0x8F
#define PA_LOW_BOOST                0x81
#define PA_MED_BOOST                0x8A
#define PA_OFF_BOOST                0x00
#define RFO_MIN                     0x00

// LOW NOISE AMPLIFIER
#define REG_LNA                     0x0C
#define LNA_MAX_GAIN                0x23  // 0010 0011
#define LNA_OFF_GAIN                0x00
#define LNA_LOW_GAIN                0xC0  // 1100 0000

struct TConfig
{
	char Tracker[16];
	char Frequency[8];
	int  SlowMode;
	int EnableHabitat;
	int EnableSSDV;
	int PayloadLength;
};

struct TConfig Config;

extern struct TConfig Config;

void writeRegister(uint8_t reg, uint8_t val)
{
	unsigned char data[2];
	
    // noInterrupts();    // Disable Interrupts
    // digitalWrite(_slaveSelectPin, LOW);
    
	data[0] = reg | 0x80;
	data[1] = val;
	wiringPiSPIDataRW(0, data, 2);
	
    // SPI.transfer(reg | RFM69_SPI_WRITE_MASK); // Send the address with the write mask on
    // SPI.transfer(val); // New value follows

    // digitalWrite(_slaveSelectPin, HIGH);
    // interrupts();     // Enable Interrupts
}

uint8_t readRegister(uint8_t reg)
{
	unsigned char data[2];
	uint8_t val;
	
    // noInterrupts();    // Disable Interrupts
    // digitalWrite(_slaveSelectPin, LOW);
    
	data[0] = reg & 0x7F;
	data[1] = 0;
	wiringPiSPIDataRW(0, data, 2);
	val = data[1];

    // SPI.transfer(reg & ~RFM69_SPI_WRITE_MASK); // Send the address with the write mask off
    // uint8_t val = SPI.transfer(0); // The written value is ignored, reg value is read
    
    // digitalWrite(_slaveSelectPin, HIGH);
    // interrupts();     // Enable Interrupts
	
    return val;
}


/////////////////////////////////////
//    Method:   Change the mode
//////////////////////////////////////
void setMode(uint8_t newMode)
{
  if(newMode == currentMode)
    return;  
  
  switch (newMode) 
  {
    case RF96_MODE_RX_CONTINUOUS:
      writeRegister(REG_PA_CONFIG, PA_OFF_BOOST);  // TURN PA OFF FOR RECIEVE??
      writeRegister(REG_LNA, LNA_MAX_GAIN);  // LNA_MAX_GAIN);  // MAX GAIN FOR RECIEVE
      writeRegister(REG_OPMODE, newMode);
      currentMode = newMode; 
      printf("Changing to Receive Continuous Mode\n");
      break;
    case RF96_MODE_SLEEP:
      writeRegister(REG_OPMODE, newMode);
      currentMode = newMode; 
      printf("Changing to Sleep Mode\n"); 
      break;
    case RF96_MODE_STANDBY:
      writeRegister(REG_OPMODE, newMode);
      currentMode = newMode; 
      printf("Changing to Standby Mode\n");
      break;
    default: return;
  } 
  
  if(newMode != RF96_MODE_SLEEP)
  {
	// sleep(1);
	delay(100);
  /*
    while(digitalRead(dio5) == 0)
    {
      printf("z");
    } 
  }
  */
	}
	
  printf(" Mode Change Done\n");
  return;
}


void setLoRaMode()
{
	double Frequency;
	unsigned long FrequencyValue;

	printf("Setting LoRa Mode\n");
	setMode(RF96_MODE_SLEEP);
	writeRegister(REG_OPMODE,0x80);
	
	printf("Setting LoRa Mode\n");
	setMode(RF96_MODE_SLEEP);
  
	if (sscanf(Config.Frequency, "%lf", &Frequency))
	{
		FrequencyValue = (unsigned long)(Frequency * 7110656 / 434);
		writeRegister(0x06, (FrequencyValue >> 16) & 0xFF);		// Set frequency
		writeRegister(0x07, (FrequencyValue >> 8) & 0xFF);
		writeRegister(0x08, FrequencyValue & 0xFF);
	}

		/*
	writeRegister(0x06, 0x6C);		// Set frequency
	writeRegister(0x07, 0xA3);
	writeRegister(0x08, 0x33);
	*/
   
	printf("Mode = %d\n", readRegister(REG_OPMODE));
}

/////////////////////////////////////
//    Method:   Setup to receive continuously
//////////////////////////////////////
void startReceiving()
{
	if (Config.SlowMode)
	{
		writeRegister(REG_MODEM_CONFIG, EXPLICIT_MODE | ERROR_CODING_4_8 | BANDWIDTH_20K8);
		writeRegister(REG_MODEM_CONFIG2, SPREADING_11 | CRC_ON);
		writeRegister(0x26, 0x0C);    // 0000 1 1 00
		printf("Set slow mode\n");
	}
	else
	{
		writeRegister(REG_MODEM_CONFIG, IMPLICIT_MODE | ERROR_CODING_4_5 | BANDWIDTH_20K8);
		writeRegister(REG_MODEM_CONFIG2, SPREADING_6);
		writeRegister(0x31, (readRegister(0x31) & 0xF8) | 0x05);
		writeRegister(0x37, 0x0C);
		printf("Set fast mode\n");
	}
	
	writeRegister(0x26, 0x0C);    // 0000 1 1 00
	writeRegister(REG_PAYLOAD_LENGTH, Config.PayloadLength);
	writeRegister(REG_RX_NB_BYTES, Config.PayloadLength);

	writeRegister(REG_HOP_PERIOD,0xFF);
	writeRegister(REG_FIFO_ADDR_PTR, readRegister(REG_FIFO_RX_BASE_AD));   
  
	// Setup Receive Continous Mode
	setMode(RF96_MODE_RX_CONTINUOUS); 
}

void setupRFM98()
{
	if (wiringPiSetup() < 0)
	{
		fprintf(stderr, "Failed to open wiringPi\n");
		exit(1);
	}
	
	// initialize the pins
	pinMode(5, INPUT);
	pinMode(6, INPUT);

	if (wiringPiSPISetup(0, 500000) < 0)
	{
		fprintf(stderr, "Failed to open SPI port.  Try loading spi library with 'gpio load spi'");
		exit(1);
	}

	// LoRa mode 
	setLoRaMode();
  
	startReceiving();
}

int receiveMessage(unsigned char *message)
{
	int i, Bytes, currentAddr;

  int x = readRegister(REG_IRQ_FLAGS);
  printf("Message status = %02Xh\n", x);
  
  // clear the rxDone flag
  // writeRegister(REG_IRQ_FLAGS, 0x40); 
  writeRegister(REG_IRQ_FLAGS, 0xFF); 
   
  // check for payload crc issues (0x20 is the bit we are looking for
  if((x & 0x20) == 0x20)
  {
    printf("CRC Failure %02Xh!!\n", x);
    // reset the crc flags
    writeRegister(REG_IRQ_FLAGS, 0x20); 
  }
  else{
    currentAddr = readRegister(REG_FIFO_RX_CURRENT_ADDR);
    Bytes = readRegister(REG_RX_NB_BYTES);
	printf ("%d bytes in packet\n", Bytes);

	HAB->PacketRSSI = readRegister(REG_RSSI_PACKET) - 137;
	printf("RSSI = %d\n", HAB->PacketRSSI);
	 /*
    Serial.print("Packet! RX Current Addr: ");
    Serial.println(currentAddr);
    Serial.print("Number of bytes received: ");
    Serial.println(Bytes);
    Serial.print("Headers so far: ");
    Serial.println(readRegister(0x15));
    Serial.print("Packets so far: ");
    Serial.println(readRegister(0x17));
    Serial.print("Modem status isr: ");
    Serial.println(readRegister(0x18));
	*/
	
    writeRegister(REG_FIFO_ADDR_PTR, currentAddr);   
    // now loop over the fifo getting the data
    for(i = 0; i < Bytes; i++)
    {
      message[i] = (unsigned char)readRegister(REG_FIFO);
    }
    message[Bytes] = '\0';
	
    // writeRegister(REG_FIFO_ADDR_PTR, 0);  // currentAddr);   
  } 
  
  return Bytes;
}

static char *decode_callsign(char *callsign, uint32_t code)
{
	char *c, s;
	
	*callsign = '\0';
	
	/* Is callsign valid? */
	if(code > 0xF423FFFF) return(callsign);
	
	for(c = callsign; code; c++)
	{
		s = code % 40;
		if(s == 0) *c = '-';
		else if(s < 11) *c = '0' + s - 1;
		else if(s < 14) *c = '-';
		else *c = 'A' + s - 14;
		code /= 40;
	}
	*c = '\0';
	
	return(callsign);
}

void ConvertStringToHex(unsigned char *Target, unsigned char *Source, int Length)
{
	const char Hex[16] = "0123456789ABCDEF";
	int i;
	
	for (i=0; i<Length; i++)
	{
		*Target++ = Hex[Source[i] >> 4];
		*Target++ = Hex[Source[i] & 0x0F];
	}

	*Target++ = '\0';
}

void UploadTelemetryPacket(char *Telemetry)
{
	CURL *curl;
	CURLcode res;
	char PostFields[200];
 
	/* In windows, this will init the winsock stuff */ 
	curl_global_init(CURL_GLOBAL_ALL);
 
	/* get a curl handle */ 
	curl = curl_easy_init();
	if (curl)
	{
		/* First set the URL that is about to receive our POST. This URL can
		   just as well be a https:// URL if that is what should receive the
           data. */ 
		curl_easy_setopt(curl, CURLOPT_URL, "http://habitat.habhub.org/transition/payload_telemetry");
    
		// sprintf(Command, "wget -O habitat.txt \"http://habitat.habhub.org/transition/payload_telemetry\" --post-data \"callsign=LoRa&string=%s&string_type=ascii&metadata={}\"", Telemetry);

		/* Now specify the POST data */ 
		sprintf(PostFields, "callsign=%s&string=%s&string_type=ascii&metadata={}", Config.Tracker, Telemetry);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, PostFields);
 
		/* Perform the request, res will get the return code */ 
		res = curl_easy_perform(curl);
    
		/* Check for errors */ 
		if(res != CURLE_OK)
		{
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}
		
		/* always cleanup */ 
		curl_easy_cleanup(curl);
	}
  
	curl_global_cleanup();
}
		
void UploadImagePacket(char *EncodedCallsign, char *EncodedEncoding, char *EncodedData)
{
	CURL *curl;
	CURLcode res;
	char PostFields[1000];
 
	/* In windows, this will init the winsock stuff */ 
	curl_global_init(CURL_GLOBAL_ALL);
 
	/* get a curl handle */ 
	curl = curl_easy_init();
	if(curl)
	{
		/* First set the URL that is about to receive our POST. This URL can
		   just as well be a https:// URL if that is what should receive the
           data. */ 
		curl_easy_setopt(curl, CURLOPT_URL, "http://www.sanslogic.co.uk/ssdv/data.php");
    
		/* Now specify the POST data */ 
		sprintf(PostFields, "callsign=%s&encoding=%s&packet=%s", EncodedCallsign, EncodedEncoding, EncodedData);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, PostFields);
 
		/* Perform the request, res will get the return code */ 
		res = curl_easy_perform(curl);
    
		/* Check for errors */ 
		if(res != CURLE_OK)
		{
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}
		
		/* always cleanup */ 
		curl_easy_cleanup(curl);
	}
  
	curl_global_cleanup();
}

void ReadString(FILE *fp, char *keyword, char *Result, int Length, int NeedValue)
{
	char line[100], *token, *value;
 
	fseek(fp, 0, SEEK_SET);
	*Result = '\0';

	while (fgets(line, sizeof(line), fp) != NULL)
	{
		token = strtok(line, "=");
		if (strcmp(keyword, token) == 0)
		{
			value = strtok(NULL, "\n");
			strcpy(Result, value);
			return;
		}
	}

	if (NeedValue)
	{
		printf("Missing value for '%s' in configuration file\n", keyword);
		exit(1);
	}
}

int ReadInteger(FILE *fp, char *keyword, int NeedValue)
{
	char Temp[32];

	ReadString(fp, keyword, Temp, sizeof(Temp), NeedValue);

	return atoi(Temp);
}

int ReadBoolean(FILE *fp, char *keyword, int NeedValue)
{
	char Temp[32];

	ReadString(fp, keyword, Temp, sizeof(Temp), NeedValue);

	return (*Temp == '1') || (*Temp == 'Y') || (*Temp == 'y') || (*Temp == 't') || (*Temp == 'T');
}
	
void LoadConfigFile(struct TConfig *Config)
{
	FILE *fp;
	char *filename = "lora.txt";

	if ((fp = fopen(filename, "r")) == NULL)
	{
		printf("\nFailed to open config file %s (error %d - %s).\nPlease check that it exists and has read permission.\n", filename, errno, strerror(errno));
		exit(1);
	}

	ReadString(fp, "tracker", Config->Tracker, sizeof(Config->Tracker), 1);
	printf ("Tracker = '%s'\n", Config->Tracker);

	Config->Frequency[0] = '\0';
	ReadString(fp, "frequency", Config->Frequency, sizeof(Config->Frequency), 0);
	if (*Config->Frequency)
	{
		printf("Frequency set to %s\n", Config->Frequency);
	}
	
	Config->SlowMode = 0;
	Config->SlowMode = ReadBoolean(fp, "slow", 0);
	Config->PayloadLength = Config->SlowMode ? 80 : 255;

	fclose(fp);
}

void ProcessLine(char *Line)
{
	static unsigned int PreviousAltitude=0;
	static long Then=0;
	long Now;
	struct tm tm;
	float Climb, Period;
	
	int Counter, FieldCount; 

	printf("%s\n", Line);
	
	FieldCount = sscanf(Line+2, "%11[^,],%d,%8[^,],%f,%f,%u",
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

void InitHAB(void)
{
    key_t key;
	int id;

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
}

	
int main(int argc, char **argv)
{
	unsigned char Message[257], Command[200], Telemetry[100], filename[100], *dest, *src;
	int Bytes, ImageNumber, PreviousImageNumber, PacketNumber, PreviousPacketNumber;
	uint32_t CallsignCode, PreviousCallsignCode;
	pthread_t CurlThread;
	FILE *fp;
	
	printf("**** LoRa Receiver/Gateway by daveake ****\n");

	InitHAB();
	
	LoadConfigFile(&Config);
	
	PreviousImageNumber = -1;
	PreviousCallsignCode = 0;
	PreviousPacketNumber = 0;
	
	fp = NULL;
	
	/*
	if (pthread_create(&GPSThread, NULL, GPSLoop, &GPS))
	{
		fprintf(stderr, "Error creating GPS thread\n");
		return 1;
	}
	*/
	
	setupRFM98();
	
	while (1)
	{
		if (digitalRead(6))
		{
			Bytes = receiveMessage(Message+1);
			
			// printf ("Data available - %d bytes\n", Bytes);
			// printf ("Line = '%s'\n", Message);

			// Telemetry?
			// $$....
			if (Message[1] == '$')
			{
				printf("Telemetry='%s'\n", Message+1);
				
				ProcessLine(Message+1);
				
				// UploadTelemetryPacket(Message+1);
			}
			else if (Message[1] == 0x66)
			{
				char Callsign[7], *FileMode, *EncodedCallsign, *EncodedEncoding, *Base64Data, *EncodedData, HexString[513], Command[1000];
				int output_length;
				
				Message[0] = 0x55;
				
				CallsignCode = Message[2]; CallsignCode <<= 8;
				CallsignCode |= Message[3]; CallsignCode <<= 8;
				CallsignCode |= Message[4]; CallsignCode <<= 8;
				CallsignCode |= Message[5];
				
				decode_callsign(Callsign, CallsignCode);
				
				printf("SSDV Packet, Callsign %s, Image %d, Packet %d\n", Callsign, Message[6], Message[7] * 256 + Message[8]);
				
				ImageNumber = Message[6];
				PacketNumber = Message[8];
	
				// Create new file ?
				if ((ImageNumber != PreviousImageNumber) || (PacketNumber <= PreviousPacketNumber) || (CallsignCode != PreviousCallsignCode))
				{
					// New image so new file
					FileMode = "wb";
				}
				else
				{
					FileMode = "ab";
				}

				PreviousImageNumber = ImageNumber;
				PreviousPacketNumber = PacketNumber;
				PreviousCallsignCode = CallsignCode;

				// Save to file
				
				sprintf(filename, "%s_%d.bin", Callsign, ImageNumber);

				if (fp = fopen(filename, FileMode))
				{
					printf("Write to file\n");
					fwrite(Message, 1, 256, fp); 
					fclose(fp);
				}
				
				// Upload to server
				EncodedCallsign = url_encode(Callsign); 
				EncodedEncoding = url_encode("hex"); 

				// Base64Data = base64_encode(Message, 256, &output_length);
				// printf("output_length=%d, byte=%02Xh\n", output_length, Base64Data[output_length]);
				// Base64Data[output_length] = '\0';
				// printf ("Base64Data '%s'\n", Base64Data);
				ConvertStringToHex(HexString, Message, 256);
				EncodedData = url_encode(HexString); 

				UploadImagePacket(EncodedCallsign, EncodedEncoding, EncodedData);
				// sprintf(Command, "wget -O ssdv.txt \"http://www.sanslogic.co.uk/ssdv/data.php\" --post-data \"callsign=%s&encoding=%s&packet=%s\"", EncodedCallsign, EncodedEncoding, EncodedData);
				// printf("%s\n", Command);
				// system(Command);
				
				free(EncodedCallsign);
				free(EncodedEncoding);
				// free(Base64Data);
				free(EncodedData);
			}
			else
			{
				printf("Packet type is %02Xh\n", Message[0]);
			}
		}
		else
		{
			// printf ("Nothing\n");
			delay(10);
		}
		HAB->CurrentRSSI = readRegister(REG_RSSI_CURRENT)-137;
		// printf("%d\n", HAB->CurrentRSSI);
 	}

	return 0;
}


