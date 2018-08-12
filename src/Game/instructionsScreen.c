#include "instructionsScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"
#include "../UI/button.h"
#include "resources.h"
#include "titleScreen.h"

static int instructions;

static void back( int i )
{
	gsmEnterState( &globalFSM, &titleScreenState );
}

static int instructionsScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );
	
	gfx_SetClearColor( clr( 0.0f, 0.01f, 0.09f, 1.0f ) );

	instructions = img_Load( "Images/orientation.png", ST_DEFAULT );

	btn_Create( vec2( 150.0f, 525.0f ), vec2( 200.0f, 50.0f ), vec2( 196.0f, 46.0f ), "Back",
		displayFont, CLR_WHITE, VEC2_ZERO, button3x3, -1, CLR_WHITE, 1, 10,
		NULL, back );

	return 1;
}

static int instructionsScreen_Exit( void )
{
	img_Clean( instructions );

	btn_DestroyAll( );

	return 1;
}

static void instructionsScreen_ProcessEvents( SDL_Event* e )
{
}

static void instructionsScreen_Process( void )
{
}

static Vector2 center = { 400.0f, 300.0f };
static void instructionsScreen_Draw( void )
{
	img_Draw( instructions, 1, center, center, 0 );
}

static void instructionsScreen_PhysicsTick( float dt )
{
}

struct GameState instructionsScreenState = { instructionsScreen_Enter, instructionsScreen_Exit, instructionsScreen_ProcessEvents,
	instructionsScreen_Process, instructionsScreen_Draw, instructionsScreen_PhysicsTick };