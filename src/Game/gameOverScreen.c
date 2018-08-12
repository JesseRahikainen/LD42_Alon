#include "gameOverScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"
#include "../UI/text.h"
#include "../System/random.h"
#include "../UI/button.h"
#include "titleScreen.h"

#include "resources.h"

static const char* gameOverTexts[] = {
	"Due to lack of performance employee has been returned to the MPI to serve out the rest of their debt.",
	"Due to lack of performance employee has been docked pay. Restoration of nutrional IV will be restored when performance improves.",
	"Due to lack of performance employee has been has been fired and all company equipment will be repossessed. Company surgeons should be done by midnight."
};
static int chosen;

static void tryAgain( int i )
{
	gsmEnterState( &globalFSM, &titleScreenState );
}

static int gameOverScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );
	
	gfx_SetClearColor( clr( 0.0f, 0.01f, 0.09f, 1.0f ) );

	chosen = rand_GetRangeS32( NULL, 0, 2 );

	btn_Create( vec2( 400.0f, 475.0f ), vec2( 200.0f, 50.0f ), vec2( 196.0f, 46.0f ), "Try Again",
		displayFont, CLR_WHITE, VEC2_ZERO, button3x3, -1, CLR_WHITE, 1, 0,
		NULL, tryAgain );

	return 1;
}

static int gameOverScreen_Exit( void )
{
	btn_DestroyAll( );
	return 1;
}

static void gameOverScreen_ProcessEvents( SDL_Event* e )
{
}

static void gameOverScreen_Process( void )
{
}

static void gameOverScreen_Draw( void )
{
	txt_DisplayString( "Employee Penalized", vec2( 400.0f, 250.0f ), CLR_WHITE, HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER, largeFont, 1, 0 );
	txt_DisplayTextArea( (const uint8_t*)gameOverTexts[chosen], vec2( 100.0f, 300.0f ), vec2( 600.0f, 400.0f ), CLR_LIGHT_GREY, HORIZ_ALIGN_CENTER, VERT_ALIGN_TOP, displayFont, 0, NULL, 1, 0 );
}

static void gameOverScreen_PhysicsTick( float dt )
{
}

struct GameState gameOverScreenState = { gameOverScreen_Enter, gameOverScreen_Exit, gameOverScreen_ProcessEvents,
	gameOverScreen_Process, gameOverScreen_Draw, gameOverScreen_PhysicsTick };