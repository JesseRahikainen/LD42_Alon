#include "titleScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"
#include "../sound.h"

#include "../UI/text.h"
#include "../UI/button.h"
#include "resources.h"

#include "../gameState.h"

#include "gameScreen.h"
#include "instructionsScreen.h"

static void startGameReleased( int i )
{
	gsmEnterState( &globalFSM, &gameScreenState );
}

static void showInstructionsReleased( int i )
{
	gsmEnterState( &globalFSM, &instructionsScreenState );
}

static int titleScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );
	
	gfx_SetClearColor( clr( 0.0f, 0.01f, 0.09f, 1.0f ) );

	btn_Create( vec2( 400.0f, 400.0f ), vec2( 200.0f, 50.0f ), vec2( 196.0f, 46.0f ), "Begin Work",
		displayFont, CLR_WHITE, VEC2_ZERO, button3x3, -1, CLR_WHITE, 1, 0,
		NULL, startGameReleased );

	btn_Create( vec2( 400.0f, 475.0f ), vec2( 200.0f, 50.0f ), vec2( 196.0f, 46.0f ), "Orientation",
		displayFont, CLR_WHITE, VEC2_ZERO, button3x3, -1, CLR_WHITE, 1, 0,
		NULL, showInstructionsReleased );

	return 1;
}

static int titleScreen_Exit( void )
{
	btn_DestroyAll( );

	return 1;
}

static void titleScreen_ProcessEvents( SDL_Event* e )
{
}

static void titleScreen_Process( void )
{
}

static void titleScreen_Draw( void )
{
	txt_DisplayString( "Welcome to", vec2( 400.0f, 200.0f ), CLR_LIGHT_GREY, HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER, displayFont, 1, 0 );

	txt_DisplayString( "Alon Analytics", vec2( 400.0f, 250.0f ), CLR_WHITE, HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER, largeFont, 1, 0 );
}

static void titleScreen_PhysicsTick( float dt )
{
}

struct GameState titleScreenState = { titleScreen_Enter, titleScreen_Exit, titleScreen_ProcessEvents,
	titleScreen_Process, titleScreen_Draw, titleScreen_PhysicsTick };