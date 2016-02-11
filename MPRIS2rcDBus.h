//#############################################################################
//
//                       MPRIS2 Controller (through DBus)
//        Vincent Crocher - 2015 - 2016
//
//#############################################################################

#include <stdio.h>
#define _XOPEN_SOURCE //For putenv
#include <stdlib.h> //For putenv
#include <dbus/dbus.h>

#include "ProcessIds.h"

//Properties access VLC:
	//Get volume: dbus-send --print-reply=double --session --reply-timeout=500 --dest=org.mpris.MediaPlayer2.vlc /org/mpris/MediaPlayer2 org.freedesktop.DBus.Properties.Get string:"org.mpris.MediaPlayer2.Player" string:"Volume"
	//Set volume: dbus-send --print-reply=double --session --reply-timeout=500 --dest=org.mpris.MediaPlayer2.vlc /org/mpris/MediaPlayer2 org.freedesktop.DBus.Properties.Set string:"org.mpris.MediaPlayer2.Player" string:"Volume" variant:double:2.0
	//Set rate: dbus-send --print-reply=double --session --reply-timeout=500 --dest=org.mpris.MediaPlayer2.vlc /org/mpris/MediaPlayer2 org.freedesktop.DBus.Properties.Set string:"org.mpris.MediaPlayer2.Player" string:"Rate" variant:double:1.

//Properties access OMX (! different than vlc):
	//Set rate (doesn't seem to work on sound) dbus-send --print-reply=double --session --reply-timeout=500 --dest=org.mpris.MediaPlayer2.omxplayer /org/mpris/MediaPlayer2 org.freedesktop.DBus.Properties.Rate double:2
	//Set volume: dbus-send --print-reply=double --session --reply-timeout=500 --dest=org.mpris.MediaPlayer2.omxplayer /org/mpris/MediaPlayer2 org.freedesktop.DBus.Properties.Volume double:5.
	//Set position (in MICROs, here ex for 10s): dbus-send --print-reply=int64 --session --reply-timeout=500 --dest=org.mpris.MediaPlayer2.omxplayer /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.SetPosition objpath:/null int64:10000000

typedef struct MPRIS2rcs
{
	DBusConnection *Connection;
	DBusMessage *Message;
	DBusError Error;
	char BusDestination[1024];
	const char * BusPath;
	const char * BusPlayerInterface;
	const char * BusPropertiesInterface;
	const char * BusMainInterface;
	int IsPlaying;
	double Rate;
} MPRIS2rc;


void MPRIS2rc_SendMessage(MPRIS2rc *rc);
double MPRIS2rc_SendMessageWithDoubleIntReply(MPRIS2rc *rc);
int MPRIS2rc_Open(MPRIS2rc *rc, const char *player, int instance_nb);
int MPRIS2rc_Close(MPRIS2rc *rc);
int MPRIS2rc_IsPlaying(MPRIS2rc *rc);
int MPRIS2rc_Toggle(MPRIS2rc *rc);
void MPRIS2rc_Play(MPRIS2rc *rc);
void MPRIS2rc_Pause(MPRIS2rc *rc);
void MPRIS2rc_Stop(MPRIS2rc *rc);
float MPRIS2rc_GetRate(MPRIS2rc *rc);
float MPRIS2rc_SetRate(MPRIS2rc *rc, double r);
float MPRIS2rc_Faster(MPRIS2rc *rc);
float MPRIS2rc_Slower(MPRIS2rc *rc);
float MPRIS2rc_NormalSpeed(MPRIS2rc *rc);
float MPRIS2rc_GetPosition(MPRIS2rc *rc);
float MPRIS2rc_SetPosition(MPRIS2rc *rc, float pos_in_ms);
void MPRIS2rc_Seek(MPRIS2rc *rc, float relative_pos_in_ms);
float MPRIS2rc_GetDuration(MPRIS2rc *rc);
float MPRIS2rc_GetVolume(MPRIS2rc *rc);
float MPRIS2rc_SetVolume(MPRIS2rc *rc, double volume);
void MPRIS2rc_Mute(MPRIS2rc *rc);
void MPRIS2rc_Unmute(MPRIS2rc *rc);
void MPRIS2rc_SetLoopNone(MPRIS2rc *rc);
void MPRIS2rc_SetLoopTrack(MPRIS2rc *rc);
void MPRIS2rc_SetLoopPlaylist(MPRIS2rc *rc);




/*###################### GENERIC FUNCTIONS #######################*/

//!Convenient function to actually send a prepared Message
void MPRIS2rc_SendMessage(MPRIS2rc *rc)
{
	//Send the Message
	dbus_connection_send(rc->Connection, rc->Message, NULL);
	if ( dbus_error_is_set(&rc->Error) )
	{
		fprintf(stderr, "MPRIS2rc_SendMessage: Error: %s\n", rc->Error.message);
		dbus_error_free(&rc->Error);
	}

	//Flush needed (or recommended) if not using bus loop
	dbus_connection_flush(rc->Connection);
	if ( dbus_error_is_set(&rc->Error) )
	{
		fprintf(stderr, "MPRIS2rc_SendMessage: Error: %s\n", rc->Error.message);
		dbus_error_free(&rc->Error);
	}

	//Why not ?
	dbus_message_unref (rc->Message);
	if ( dbus_error_is_set(&rc->Error) )
	{
		fprintf(stderr, "MPRIS2rc_SendMessage: Error: %s\n", rc->Error.message);
		dbus_error_free(&rc->Error);
	}
}

//!Convenient function to actually send a prepared Message and get a DOUBLE or a INT reply
double MPRIS2rc_SendMessageWithDoubleIntReply(MPRIS2rc *rc)
{
	DBusPendingCall* pending; //Reply handler

	//Send the Message
	dbus_connection_send_with_reply(rc->Connection, rc->Message, &pending, -1); //-1 for default timeout
	if ( dbus_error_is_set(&rc->Error) )
	{
		fprintf(stderr, "MPRIS2rc_SendMessageWithDoubleIntReply: Error: %s\n", rc->Error.message);
		dbus_error_free(&rc->Error);
		return -1;
	}

	//Flush needed (or recommended) if not using bus loop
	dbus_connection_flush(rc->Connection);
	if ( dbus_error_is_set(&rc->Error) )
	{
		fprintf(stderr, "MPRIS2rc_SendMessageWithDoubleIntReply: Error: %s\n", rc->Error.message);
		dbus_error_free(&rc->Error);
		return -1;
	}

	//Why not ?
	dbus_message_unref (rc->Message);
	if ( dbus_error_is_set(&rc->Error) )
	{
		fprintf(stderr, "MPRIS2rc_SendMessageWithDoubleIntReply: Error: %s\n", rc->Error.message);
		dbus_error_free(&rc->Error);
		return -1;
	}


	//Reply:
	//Block until we receive a reply
	dbus_pending_call_block(pending);

	//Get the reply message
	DBusMessage* msg;
	msg = dbus_pending_call_steal_reply(pending);
	if (NULL == msg)
	{
		fprintf(stderr, "MPRIS2rc_SendMessageWithDoubleIntReply: Reply NULL: %s\n", rc->Error.message);
		dbus_error_free(&rc->Error);
		return -1;
	}
	//Free the pending message handle
	dbus_pending_call_unref(pending);

	//Process reply value
	DBusMessageIter args;
	double double_val;
	double val=-1.0;
	if (!dbus_message_iter_init(msg, &args))
	{
		//Free reply msg
		dbus_message_unref(msg);
		fprintf(stderr, "MPRIS2rc_SendMessageWithDoubleIntReply: Error: No reply !\n");
		return -1;
	}

	if (DBUS_TYPE_DOUBLE == dbus_message_iter_get_arg_type(&args) )
	{
		dbus_message_iter_get_basic(&args, &double_val);
		val=double_val;
	}
	else if (DBUS_TYPE_INT64 == dbus_message_iter_get_arg_type(&args))
	{
		dbus_int64_t int64_val;
		dbus_message_iter_get_basic(&args, &int64_val);
		val=(double)int64_val;
	}
	else if (DBUS_TYPE_INT32 == dbus_message_iter_get_arg_type(&args))
	{
		dbus_int32_t int32_val;
		dbus_message_iter_get_basic(&args, &int32_val);
		val=(double)int32_val;
	}
	else if (DBUS_TYPE_INT16 == dbus_message_iter_get_arg_type(&args))
	{
		dbus_int32_t int16_val;
		dbus_message_iter_get_basic(&args, &int16_val);
		val=(double)int16_val;
	}
	else if (DBUS_TYPE_VARIANT == dbus_message_iter_get_arg_type(&args))
	{
		DBusMessageIter variant;
		dbus_message_iter_recurse(&args, &variant);
		if (DBUS_TYPE_INT64 == dbus_message_iter_get_arg_type(&variant))
		{
			dbus_int64_t int64_val;
			dbus_message_iter_get_basic(&variant, &int64_val);
			val=(double)int64_val;
		}
		else if (DBUS_TYPE_DOUBLE == dbus_message_iter_get_arg_type(&variant))
		{
			dbus_message_iter_get_basic(&variant, &double_val);
			val=double_val;
		}
		else
		{
			//Free reply msg
			dbus_message_unref(msg);
			fprintf(stderr, "MPRIS2rc_SendMessageWithDoubleIntReply: Error: Reply is not a DOUBLE nor INT!\n");
			return -1;
		}
	}
	else
	{
		//Free reply msg
		dbus_message_unref(msg);
		fprintf(stderr, "MPRIS2rc_SendMessageWithDoubleIntReply: Error: Reply is not a DOUBLE nor INT!\n");
		return -1;
	}

	//Free reply msg
	dbus_message_unref(msg);

	return val;
}

/*----------------------------------------------------------------*/



/*####################### MAIN FUNCTIONS #########################*/

//!Open DBus, initialize structure and play
int MPRIS2rc_Open(MPRIS2rc *rc, const char *player, int instance_nb)
{
	//Omx has different behavior (request to get DBus address)
	char dbus_addr[1024], export_bus[1024], PlayerProcessName[15];
	FILE *tmpbusfile;
	switch(player[0])
	{
		//vlc
		case 'v':
			//Process name
			sprintf(PlayerProcessName, "vlc");
			//Init bus parameters (constant)
			rc->BusPath = "/org/mpris/MediaPlayer2";
			rc->BusPlayerInterface = "org.mpris.MediaPlayer2.Player";
			rc->BusPropertiesInterface = "org.freedesktop.DBus.Properties";
			rc->BusMainInterface = "org.mpris.MediaPlayer2";
			//Default destination
			sprintf(rc->BusDestination, "org.mpris.MediaPlayer2.vlc");
			break;

		//omxplayer
		case 'o':
			//Process name
			sprintf(PlayerProcessName, "omxplayer.bin");
			//Init bus parameters (constant)
			rc->BusPath = "/org/mpris/MediaPlayer2";
			rc->BusPlayerInterface = "org.mpris.MediaPlayer2.Player";
			rc->BusPropertiesInterface = "org.freedesktop.DBus.Properties";
			rc->BusMainInterface = "org.mpris.MediaPlayer2";
			//Default destination
			sprintf(rc->BusDestination, "org.mpris.MediaPlayer2.omxplayer");

			//Get DBUS address from tmp file of OMXPLAYER: omx doesn't use the default session address...
			tmpbusfile=popen("cat /tmp/omxplayerdbus.${USER}", "r");
			fscanf(tmpbusfile, "%s", dbus_addr);
			printf("Omxplayer DBUS address: %s\n", dbus_addr);
			pclose(tmpbusfile);
			//Set as the DBUS SESSION NAME
			sprintf(export_bus, "DBUS_SESSION_BUS_ADDRESS=%s", dbus_addr);
			putenv(export_bus);
			break;

		//other: error
		default:
			printf("MPRIS2rc_Open:: Error, unknown player.\n");
			return -1;
	}

	//Then prepare Connection:
	dbus_error_init(&rc->Error);
	//Connect
	rc->Connection = dbus_bus_get(DBUS_BUS_SESSION, &rc->Error);
	if ( dbus_error_is_set(&rc->Error) )
	{
		fprintf(stderr, "MPRIS2rc_Open: Error connecting to the daemon bus: %s", rc->Error.message);
		dbus_error_free(&rc->Error);
		return -1;
	}


	//Init bus:
	//Get process PID
	int instance_pid=GetProcessPID(PlayerProcessName, instance_nb);
	//Use default if requested instance doesn't exist
	if(instance_pid<1 || instance_nb<1) //Use default even for first instance
	{
		printf("MPRIS2rcDBus::Open : WARNING: Using default instance (1st one).\n");
	}
	else
	{
		printf("MPRIS2rcDBus::Open : Using requested instance (Nb: %d, PID: %d).\n", instance_nb, instance_pid);
		//Append instance to bus destination
		sprintf(rc->BusDestination, "%s.instance%d", rc->BusDestination, instance_pid); //Take hand directly on the instance we are interested in
	}


	//Set playing:
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPlayerInterface,
	"Play");
	if (rc->Message == NULL) {
		fprintf(stderr, "Cannot allocate DBus Message!\n");
	}
	//Send
	MPRIS2rc_SendMessage(rc);

	//and assume playing
	rc->IsPlaying=1;

	//assume rate is 1.0
	rc->Rate=1.0;

	return 0;
}

//!Quit MPRIS2 and close socket
int MPRIS2rc_Close(MPRIS2rc *rc)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusMainInterface, //Different interface
	"Quit");
	if (rc->Message == NULL) {
		fprintf(stderr, "Cannot allocate DBus Message!\n");
	}

	printf("Closing player.\n");

	//Send
	MPRIS2rc_SendMessage(rc);

	//Close DBus Connection
	dbus_connection_unref(rc->Connection);

	return 0;
}

/*----------------------------------------------------------------*/



/*###################### PLAYING CONTROLS ########################*/

//!Return 1 if is playing, 0 otherwise (based on internal state)
int MPRIS2rc_IsPlaying(MPRIS2rc *rc)
{
	return rc->IsPlaying;
}

//!Play->Pause or Pause->Play
int MPRIS2rc_Toggle(MPRIS2rc *rc)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPlayerInterface,
	"PlayPause");
	if (rc->Message == NULL) {
		fprintf(stderr, "Cannot allocate DBus Message!\n");
	}

	//Send
	MPRIS2rc_SendMessage(rc);

	//Keep state
	if(rc->IsPlaying==1)
		rc->IsPlaying=0;
	else
		rc->IsPlaying=1;

	return rc->IsPlaying;
}
//!Set Pause if was playing
void MPRIS2rc_Play(MPRIS2rc *rc)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPlayerInterface,
	"Play");
	if (rc->Message == NULL) {
		fprintf(stderr, "Cannot allocate DBus Message!\n");
	}

	//Send
	MPRIS2rc_SendMessage(rc);

	//Keep state
	rc->IsPlaying=1;
}
//!Set Play if was paused
void MPRIS2rc_Pause(MPRIS2rc *rc)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPlayerInterface,
	"Pause");
	if (rc->Message == NULL) {
		fprintf(stderr, "Cannot allocate DBus Message!\n");
	}

	//Send
	MPRIS2rc_SendMessage(rc);

	//Keep state
	rc->IsPlaying=0;
}

//!Stop playing
void MPRIS2rc_Stop(MPRIS2rc *rc)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPlayerInterface,
	"Stop");
	if (rc->Message == NULL) {
		fprintf(stderr, "Cannot allocate DBus Message!\n");
	}

	//Send
	MPRIS2rc_SendMessage(rc);

	//Not playing
	rc->IsPlaying=0;
}

/*----------------------------------------------------------------*/



/*####################### SPEED CONTROLS #########################*/
//!Return playing rate retrieved from the player and update internal rate value
float MPRIS2rc_GetRate(MPRIS2rc *rc)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPropertiesInterface,
	"Get");
	if (rc->Message == NULL) {
		fprintf(stderr, "Cannot allocate DBus Message!\n");
	}

	//Message arguments
	const char *arg1 = rc->BusPlayerInterface;
	const char *arg2 = "Rate";

	//Apppend the two string args
	dbus_message_append_args(rc->Message, DBUS_TYPE_STRING, &arg1, DBUS_TYPE_STRING, &arg2, DBUS_TYPE_INVALID);

	//Send and get reply
	double ret=MPRIS2rc_SendMessageWithDoubleIntReply(rc);
	if(ret>0)
	{
		rc->Rate=ret;
	}
	
	return ret;
}
//!Set Playing rate
float MPRIS2rc_SetRate(MPRIS2rc *rc, double r)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPropertiesInterface,
	"Set");
	if (rc->Message == NULL) {
		fprintf(stderr, "MPRIS2rc_SetRate: Cannot allocate DBus Message!\n");
	}

	//No reply needed
	dbus_message_set_no_reply(rc->Message, TRUE);

	//Message arguments
	const char *arg1 = rc->BusPlayerInterface;
	const char *arg2 = "Rate";
	//bound the rate to apply
	r = r > 0.05 ? r : 0.05;
	r = r < 10 ? r : 10;
	double arg3 = r;
	rc->Rate = r;
	printf("Set playing rate to %f\n", rc->Rate);

	//Variant type not supported by append_args so use iter...
	DBusMessageIter iter, variant;
	dbus_message_iter_init_append(rc->Message, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &arg1);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &arg2);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, DBUS_TYPE_DOUBLE_AS_STRING, &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_DOUBLE, &arg3);
	dbus_message_iter_close_container(&iter, &variant);

	//Send
	MPRIS2rc_SendMessage(rc);

	return rc->Rate;
}
//!Play 10% faster (through SetRate)
float MPRIS2rc_Faster(MPRIS2rc *rc)
{
	//Increase rate by 10%
	MPRIS2rc_SetRate(rc, rc->Rate*1.1);

	return rc->Rate;
}
//!Play 10% slower (through SetRate)
float MPRIS2rc_Slower(MPRIS2rc *rc)
{
	//Decrease rate by 10%
	MPRIS2rc_SetRate(rc, rc->Rate*0.9);

	return rc->Rate;
}
//!Play at normal speed (through SetRate)
float MPRIS2rc_NormalSpeed(MPRIS2rc *rc)
{
	//Rate to 1
	MPRIS2rc_SetRate(rc, 1.0);

	return rc->Rate;
}

/*----------------------------------------------------------------*/



/*###################### POSITION CONTROLS ########################*/
//!Get (absolute) playing position (in ms)
float MPRIS2rc_GetPosition(MPRIS2rc *rc)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPropertiesInterface,
	"Get");
	if (rc->Message == NULL) {
		fprintf(stderr, "Cannot allocate DBus Message!\n");
	}
	
	//Message arguments
	const char *arg1 = rc->BusPlayerInterface;
	const char *arg2 = "Position";

	//Apppend the two string args
	dbus_message_append_args(rc->Message, DBUS_TYPE_STRING, &arg1, DBUS_TYPE_STRING, &arg2, DBUS_TYPE_INVALID);
	
	//Send and get reply (and convert from us to ms)
	double ret=MPRIS2rc_SendMessageWithDoubleIntReply(rc);
	if(ret==-1)
		return -1;
	else
		return (ret/1000.);
}
//!Set playing position (absolute)
/* !!!!!!!!!!!!!!! OMXPLAYER SPECIFIC AT THE MOMENT !!!!!!!!!!!!!!!*/
float MPRIS2rc_SetPosition(MPRIS2rc *rc, float pos_in_ms)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPlayerInterface,
	"SetPosition");
	if (rc->Message == NULL) {
		fprintf(stderr, "Cannot allocate DBus Message!\n");
	}

	//Message arguments
	printf("Request position %.3fms\n", pos_in_ms);
	dbus_int64_t pos_in_us = (int)(pos_in_ms*1000.);
	const char *track_path = "/null"; //track needs to be retrieved for vlc (through a call to org.mpris.MediaPlayer2.TrackList property: Tracks)
	dbus_message_append_args (rc->Message,
							  DBUS_TYPE_OBJECT_PATH, &track_path,
							  DBUS_TYPE_INT64, &pos_in_us,
							  DBUS_TYPE_INVALID);


	//Send and get reply (and convert from us to ms)
	double ret=MPRIS2rc_SendMessageWithDoubleIntReply(rc);
	if(ret==-1)
		return -1;
	else
		return (ret/1000.);
}
//!Set playing position (relative)
void MPRIS2rc_Seek(MPRIS2rc *rc, float relative_pos_in_ms)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPlayerInterface,
	"Seek");
	if (rc->Message == NULL) {
		fprintf(stderr, "Cannot allocate DBus Message!\n");
	}

	//No reply needed
	dbus_message_set_no_reply(rc->Message, TRUE);

	//Message arguments
	printf("Request relative position %.3fms\n", relative_pos_in_ms);
	dbus_int64_t pos_in_us = (int)(relative_pos_in_ms*1000.);
	dbus_message_append_args (rc->Message,
							   DBUS_TYPE_INT64, &pos_in_us,
							   DBUS_TYPE_INVALID);

	//Send
	MPRIS2rc_SendMessage(rc);
}
//!Get track duration (in ms)
/* !!!!!!!!!!!!!!! OMXPLAYER SPECIFIC AT THE MOMENT !!!!!!!!!!!!!!!*/
float MPRIS2rc_GetDuration(MPRIS2rc *rc)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPropertiesInterface,
	"Duration");
	if (rc->Message == NULL) {
		fprintf(stderr, "Cannot allocate DBus Message!\n");
	}

	//Send and get reply (and convert from us to ms)
	double ret=MPRIS2rc_SendMessageWithDoubleIntReply(rc);
	if(ret==-1)
		return -1;
	else
		return (ret/1000.);
}

/*----------------------------------------------------------------*/



/*####################### VOLUME CONTROLS ########################*/
//!Return current volume (1.0 is normal)
float MPRIS2rc_GetVolume(MPRIS2rc *rc)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPropertiesInterface,
	"Get");
	if (rc->Message == NULL) {
		fprintf(stderr, "Cannot allocate DBus Message!\n");
	}

	//Message arguments
	const char *arg1 = rc->BusPlayerInterface;
	const char *arg2 = "Volume";

	//Apppend the two string args
	dbus_message_append_args(rc->Message, DBUS_TYPE_STRING, &arg1, DBUS_TYPE_STRING, &arg2, DBUS_TYPE_INVALID);

	//Send and get reply
	return MPRIS2rc_SendMessageWithDoubleIntReply(rc);
}
//!Set the current volume (1.0 is normal)
float MPRIS2rc_SetVolume(MPRIS2rc *rc, double volume)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPropertiesInterface,
	"Set");
	if (rc->Message == NULL) {
		fprintf(stderr, "Cannot allocate DBus Message!\n");
	}
	
	//Message arguments
	const char *arg1 = rc->BusPlayerInterface;
	const char *arg2 = "Volume";

	//Variant type not supported by append_args so use iter...
	DBusMessageIter iter, variant;
	dbus_message_iter_init_append(rc->Message, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &arg1);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &arg2);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, DBUS_TYPE_DOUBLE_AS_STRING, &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_DOUBLE, &volume);
	dbus_message_iter_close_container(&iter, &variant);

	//Send and get reply
	return MPRIS2rc_SendMessageWithDoubleIntReply(rc);
}
//!Mute
/* !!!!!!!!!!!!!!! OMXPLAYER SPECIFIC AT THE MOMENT !!!!!!!!!!!!!!!*/
void MPRIS2rc_Mute(MPRIS2rc *rc)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPropertiesInterface,
	"Mute");
	if (rc->Message == NULL) {
		fprintf(stderr, "Cannot allocate DBus Message!\n");
	}

	//Send and get reply
	MPRIS2rc_SendMessage(rc);
}
//!Unmute
/* !!!!!!!!!!!!!!! OMXPLAYER SPECIFIC AT THE MOMENT !!!!!!!!!!!!!!!*/
void MPRIS2rc_Unmute(MPRIS2rc *rc)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPropertiesInterface,
	"Unmute");
	if (rc->Message == NULL) {
		fprintf(stderr, "Cannot allocate DBus Message!\n");
	}

	//Send and get reply
	MPRIS2rc_SendMessage(rc);
}

/*----------------------------------------------------------------*/


/*####################### PLAYLIST CONTROLS ########################*/
/* !!!!!!!!!!!!!!!  VLC SPECIFIC AT THE MOMENT !!!!!!!!!!!!!!!!!!!*/
//!Set loop playback to NONE (no loop)
void MPRIS2rc_SetLoopNone(MPRIS2rc *rc)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPropertiesInterface,
	"Set");
	if (rc->Message == NULL) {
		fprintf(stderr, "MPRIS2rc_SetLoopNone: Cannot allocate DBus Message!\n");
	}

	//No reply needed
	dbus_message_set_no_reply(rc->Message, TRUE);

	//Message arguments
	const char *arg1 = rc->BusPlayerInterface;
	const char *arg2 = "LoopStatus";
	const char *arg3 = "None";

	//Variant type not supported by append_args so use iter...
	DBusMessageIter iter, variant;
	dbus_message_iter_init_append(rc->Message, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &arg1);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &arg2);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING, &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &arg3);
	dbus_message_iter_close_container(&iter, &variant);

	//Send
	MPRIS2rc_SendMessage(rc);
}
//!Set loop playback to TRACK (loop on current track only)
void MPRIS2rc_SetLoopTrack(MPRIS2rc *rc)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPropertiesInterface,
	"Set");
	if (rc->Message == NULL) {
		fprintf(stderr, "MPRIS2rc_SetLoopTrack: Cannot allocate DBus Message!\n");
	}

	//No reply needed
	dbus_message_set_no_reply(rc->Message, TRUE);

	//Message arguments
	const char *arg1 = rc->BusPlayerInterface;
	const char *arg2 = "LoopStatus";
	const char *arg3 = "Track";

	//Variant type not supported by append_args so use iter...
	DBusMessageIter iter, variant;
	dbus_message_iter_init_append(rc->Message, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &arg1);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &arg2);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING, &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &arg3);
	dbus_message_iter_close_container(&iter, &variant);

	//Send
	MPRIS2rc_SendMessage(rc);
}
//!Set loop playback to PLAYLIST (loop on the whole playlist)
void MPRIS2rc_SetLoopPlaylist(MPRIS2rc *rc)
{
	//Prepare Message (method to call) for sending
	rc->Message=NULL;
	rc->Message = dbus_message_new_method_call (
	rc->BusDestination,
	rc->BusPath,
	rc->BusPropertiesInterface,
	"Set");
	if (rc->Message == NULL) {
		fprintf(stderr, "MPRIS2rc_SetLoopPlaylist: Cannot allocate DBus Message!\n");
	}

	//No reply needed
	dbus_message_set_no_reply(rc->Message, TRUE);

	//Message arguments
	const char *arg1 = rc->BusPlayerInterface;
	const char *arg2 = "LoopStatus";
	const char *arg3 = "Playlist";

	//Variant type not supported by append_args so use iter...
	DBusMessageIter iter, variant;
	dbus_message_iter_init_append(rc->Message, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &arg1);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &arg2);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING, &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &arg3);
	dbus_message_iter_close_container(&iter, &variant);

	//Send
	MPRIS2rc_SendMessage(rc);
}

/*----------------------------------------------------------------*/