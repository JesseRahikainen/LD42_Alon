#include "resources.h"

#include <stdlib.h>
#include "../Graphics/images.h"
#include "../Graphics/imageSheets.h"
#include "../UI/text.h"
#include "../sound.h"
#include "../UI/button.h"

int whiteImg = -1;
int gradientImg = -1;
int displayFont = -1;
int textFont = -1;
int largeFont = -1;
int* button3x3 = NULL;

int loginSnd = -1;
int logoutSnd = -1;
int requestDoneSnd = -1;
int requestFailSnd = -1;
int opFailedSnd = -1;
int newRequestSnd = -1;

void loadResources( void )
{
	whiteImg = img_Load( "Images/white.png", ST_DEFAULT );
	gradientImg = img_Load( "Images/gradient.png", ST_DEFAULT );
	img_LoadSpriteSheet( "Images/button.ss", ST_DEFAULT, &button3x3 );
	displayFont = txt_LoadFont( "Fonts/kenvector_future_thin.ttf", 24.0f );
	textFont = txt_LoadFont( "Fonts/kenvector_future_thin.ttf", 16.0f );
	largeFont = txt_LoadFont( "Fonts/kenvector_future_thin.ttf", 64.0f );

	loginSnd = snd_LoadSample( "Sounds/login.ogg", 1, false );
	logoutSnd = snd_LoadSample( "Sounds/logout.ogg", 1, false );
	requestDoneSnd = snd_LoadSample( "Sounds/requestDone.ogg", 1, false );
	requestFailSnd = snd_LoadSample( "Sounds/requestFailed.ogg", 1, false );
	opFailedSnd = snd_LoadSample( "Sounds/opFailed.ogg", 1, false );
	newRequestSnd = snd_LoadSample( "Sounds/newRequest.ogg", 1, false );

	// always want the buttons
	btn_Init( );
	btn_RegisterSystem( );
}