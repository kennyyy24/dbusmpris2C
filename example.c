//#############################################################################
//
//            Example code for dbusmpris2C (control of VLC through dbus)
//        Vincent Crocher - 2016
//
//#############################################################################
#include <stdio.h>
#include <unistd.h>
#include "MPRIS2rcDBus.h"


int main()
{
	//Create stucture
	MPRIS2rc VLCRC;
	
	//Open connection to first instance of vlc
	MPRIS2rc_Open(&VLCRC, "vlc", 0); //For now only "vlc" and "omx" (for omxplayer) are supported
	 
	//Action examples 
	MPRIS2rc_Play(&VLCRC);
	sleep(5);
	MPRIS2rc_Pause(&VLCRC);
	sleep(2);
	MPRIS2rc_Play(&VLCRC);
	MPRIS2rc_Faster(&VLCRC);
	sleep(5);
	MPRIS2rc_Faster(&VLCRC);
	sleep(5);
	MPRIS2rc_NormalSpeed(&VLCRC);
	MPRIS2rc_Stop(&VLCRC);
	sleep(5);
	MPRIS2rc_Play(&VLCRC);
	sleep(3);
	MPRIS2rc_Toggle(&VLCRC);


	//Quit player
	MPRIS2rc_Close(&VLCRC);
	
	return 0;
}