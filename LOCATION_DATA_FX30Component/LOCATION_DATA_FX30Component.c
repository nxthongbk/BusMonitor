#include "legato.h"
#include "le_avdata_interface.h"
#include "le_avc_interface.h"
#include "interfaces.h"

#define ASSET_NAME				 "Room"

#define INSTANCE_COUNT           1

#define START_AVC                                                        //undefined this, if you have a separate AVC Controller

le_avdata_AssetInstanceRef_t    _assetInstRef[INSTANCE_COUNT];

char*                           _busName[INSTANCE_COUNT];

int32_t                         _longitude[INSTANCE_COUNT];
int32_t                         _latitude[INSTANCE_COUNT];
int32_t                         _speedh[INSTANCE_COUNT];

le_timer_Ref_t                  _tempUpdateTimerRef = NULL;
le_timer_Ref_t                  _tempUpdatelocation = NULL;

int32_t longitude;
int32_t latitude;
int32_t hAccuracy;

uint32_t degree;
uint32_t accuracy;

uint32_t hor_speed;
uint32_t acchor_speed;
int32_t ver_speed;
int32_t accver_speed;

//#ifdef START_AVC
le_timer_Ref_t                  _avcTimerRef;
le_avc_StatusEventHandlerRef_t  _avcEventHandlerRef = NULL;              //reference to AirVantage Controller (AVC) Session handler
//#endif

//--------------------------------------------------------------------------------------------------
/**
 * Pointer to the null terminated string containing the destination phone number.
 */
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * Attempts to use the GPS to find the current latitude, longitude and horizontal accuracy within
 * the given timeout constraints.
 *
 * @return
 *      - LE_OK on success
 *      - LE_UNAVAILABLE if positioning services are unavailable
 *      - LE_TIMEOUT if the timeout expires before successfully acquiring the location
 *
 * @note
 *      Blocks until the location has been identified or the timeout has occurred.
 */
//--------------------------------------------------------------------------------------------------

void GetDevicelocation ()
{
	//Get Location of Device
	le_result_t res;
    le_posCtrl_Request();
	res = le_pos_Get2DLocation(&latitude, &longitude, &hAccuracy);

	if (res == LE_OK)
		{
			LE_INFO("Location: latitude=%d, longitude=%d, hAccuracy=%d", latitude, longitude, hAccuracy);
		}
	else if (res == LE_FAULT)
		{
			LE_INFO("Failed to get the 2D location's data");
		}
	else if (res == LE_OUT_OF_RANGE)
		{
			LE_INFO("One of the retrieved parameter is invalid (set to INT32_MAX)");
		}
	else
		{
			LE_INFO("unknown location error (%d)", res);
		}

	//Get Speed of Device
	le_result_t res_speed;

	res_speed=le_pos_GetMotion(&hor_speed,&acchor_speed,&ver_speed,&accver_speed);
	if(res_speed == LE_OK)
	{
		LE_INFO("hor_speed= %u and ver_speed= %d",hor_speed,ver_speed);
	}
	else if (res_speed == LE_FAULT)
	{
		LE_INFO("Failed to get the speed data");
	}
	else if (res_speed == LE_OUT_OF_RANGE)
	{
		LE_INFO("One of the retrieved parameter is invalid (set to INT32_MAX)");
	}
	else
	{
		LE_INFO("unknown speed error %d ",res_speed);
	}
}

/**
 * Update location of device to AirVantage
 */
//--------------------------------------------------------------------------------------------------

void UpdateDeviceLocation(le_timer_Ref_t  timerRef)
{
	GetDevicelocation();

	int i;

	for (i=0; i< INSTANCE_COUNT; i++)
	{
		 _longitude[i] = longitude ;
		 _latitude[i] = latitude;
		 _speedh[i]= (int32_t)hor_speed;

		LE_INFO("longitude %d ",_longitude[i]);
		LE_INFO("latitude %d ",_latitude[i]);
		LE_INFO("speed %d ",_speedh[i]);


		le_avdata_SetInt(_assetInstRef[i], "longitude", _longitude[i]);
		le_avdata_SetInt(_assetInstRef[i], "latitude", _latitude[i]);
		le_avdata_SetInt(_assetInstRef[i], "speed", _speedh[i]);

		_longitude[i]=0;
		_latitude[i]=0;

	}
}

static void sig_appTermination_cbh(int sigNum)
{
	//#ifdef START_AVC
	LE_INFO("Legato AssetData: Close AVC session");
	le_avc_StopSession();

	if (NULL != _avcEventHandlerRef)
	{
		//unregister the handler
		le_avc_RemoveStatusEventHandler(_avcEventHandlerRef);
	}
	//#endif
}

//#ifdef START_AVC
static void AVsessionHandler
(
	le_avc_Status_t     updateStatus,
	int32_t             totalNumBytes,
	int32_t             progress,
	void*               contextPtr
)
{
	LE_INFO("AVsessionHandler-callback: status %i", updateStatus);
}


COMPONENT_INIT
{
	LE_INFO("Legato AssetData: Start Legato AssetDataApp");

	le_sig_Block(SIGTERM);
    le_sig_SetEventHandler(SIGTERM, sig_appTermination_cbh);

	//#ifdef START_AVC
	//Start AVC Session
	_avcEventHandlerRef = le_avc_AddStatusEventHandler(AVsessionHandler, NULL);    //register a AVC handler
	le_result_t result = le_avc_StartSession();      //Start AVC session. Note: AVC handler must be registered prior starting a session
	if (LE_FAULT == result)
	{
		le_avc_StopSession();
		le_avc_StartSession();
	}

	LE_INFO("Legato AssetData: Started LWM2M session with AirVantage");

	int i = 0;
   //Assign default value to asset data fields
	_assetInstRef[i] = le_avdata_Create(ASSET_NAME);
	_busName[i] = (char *) malloc(16);
	strcpy(_busName[i], "Bus");
	_longitude[i] = 0 ;
	_latitude[i] = 0 ;
	_speedh[i]=0;


	for (i=0; i< INSTANCE_COUNT; i++)
	{
		//set the variable values
		le_avdata_SetString(_assetInstRef[i], "Name", _busName[i]);
        le_avdata_SetInt(_assetInstRef[i], "longitude", _longitude[i]);
		le_avdata_SetInt(_assetInstRef[i], "latitude", _latitude[i]);
		le_avdata_SetInt(_assetInstRef[i], "speed", _speedh[i]);

	}

   //Set timer to update temperature on a regularly basis
	_tempUpdateTimerRef = le_timer_Create("tempUpdateTimer");     //create timer

	le_clk_Time_t      interval = {5, 0 };                      //update temperature every 5 seconds
	le_timer_SetInterval(_tempUpdateTimerRef, interval);
	le_timer_SetRepeat(_tempUpdateTimerRef, 0);                   //set repeat to always

	//set callback function to handle timer expiration event
	le_timer_SetHandler(_tempUpdateTimerRef, UpdateDeviceLocation);

	//start timer
	le_timer_Start(_tempUpdateTimerRef);
}
