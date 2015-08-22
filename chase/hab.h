struct HAB
{
	// batc data
	int batc_status;
	char batc[20];
	
	// GPS data
	char GPS_Time[9];
	unsigned int GPS_Latitude_Minutes, GPS_Longitude_Minutes;
	float GPS_Latitude_Seconds, GPS_Longitude_Seconds;
	char GPS_LatitudeSign[2];
	char GPS_LongitudeSign[2];
	float GPS_Longitude, GPS_Latitude;
	unsigned int GPS_Altitude;
	unsigned int GPS_Satellites;
	unsigned int GPS_Speed;
	unsigned int GPS_Direction;
	
	// Chase car upload
	int car_status;
	
	// HAB data
	char HAB_Payload[12];
	int HAB_status;
	char HAB_Time[9];
	float HAB_Latitude, HAB_Longitude;
	unsigned int HAB_Altitude;
	unsigned int HAB_Satellites;
	unsigned int HAB_Speed;
	unsigned int HAB_Direction;
	float HAB_AscentRate;

	// Radio data
	int CurrentRSSI;
	int PacketRSSI;
};

