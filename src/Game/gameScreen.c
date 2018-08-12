#include "gameScreen.h"

#include "../Graphics/graphics.h"
#include "../Graphics/images.h"
#include "../Graphics/spineGfx.h"
#include "../Graphics/camera.h"
#include "../Graphics/debugRendering.h"
#include "../Graphics/imageSheets.h"
#include "../UI/text.h"
#include "../System/platformLog.h"
#include "../System/random.h"
#include "../System/ECPS/entityComponentProcessSystem.h"
#include "../Utils/stretchyBuffer.h"
#include "../Input/input.h"
#include "../Utils/idSet.h"
#include "../UI/button.h"
#include "resources.h"
#include "../sound.h"

#include "gameOverScreen.h"

typedef struct {
	const char* typeName;
	Color clr;
	Color textClr;
} FileType;

FileType fileTypes[] = {
	{ "img", { 0.1f, 0.75f, 0.1f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ "txt", { 0.1f, 0.75f, 0.75f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ "db", { 0.1f, 0.1f, 0.75f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ "ui", { 0.75, 0.5f, 0.1f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ "c", { 0.5f, 0.1f, 0.75f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ "dat", { 0.75f, 0.75f, 0.1f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ "elf", { 0.75f, 0.1f, 0.1f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
	{ "arc", { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.25f, 0.25f, 0.25f, 1.0f } }
};

// matches fileTypes above, for easier access
typedef enum {
	FT_IMG,
	FT_TXT,
	FT_DB,
	FT_UI,
	FT_C,
	FT_DAT,
	FT_ELF,
	FT_ARC,
	NUM_TYPES
} FileTypes;

#define STR_SIZE 128
typedef struct {
	char name[STR_SIZE];
	char description[STR_SIZE];
	int fileType;
	uint8_t sizeBlocks;

	bool isSelected;

	float amtDone;

	EntityID* sbContainedFiles;
} File;

typedef struct {
	char name[STR_SIZE];
	EntityID fileID;
} RequestableFile;

RequestableFile* sbRequestableFiles = NULL;

IDSet fileIDs;
File* sbFiles = NULL;

typedef enum {
	OP_EXPAND,
	OP_ARCHIVE,
	OP_DELETE,
	NUM_OPS
} OpType;

typedef struct {
	OpType type;
	EntityID archiveFile;
	EntityID processingFile;
	EntityID compressFiles[32];
} Operation;

Operation* sbActiveOperations = NULL;

typedef struct {
	uint8_t maxSize;
	EntityID* sbFiles;
} DataDrive;

DataDrive drive = { 18, NULL };

EntityID highlighted = INVALID_ENTITY_ID;

uint32_t archiveNameCount;

typedef struct {
	int requestableFileIdx;
	float timeRequired;
	float timePassed;
	float giveUpTime;
	float giveUpTimePassed;
} UserRequest;

typedef struct {
	const char* userName;
	UserRequest* sbRequests;
	float requestWaitTime;
	int fileRequestsLeft;
} User;

#define MIN_USER_WAIT 10.0f
#define MAX_USER_WAIT 15.0f
#define MIN_USER_REQ 1
#define MAX_USER_REQ 4
#define MAX_LOGGED_IN 4
#define MIN_USER_GIVEUP 10.0f
#define MAX_USER_GIVEUP 15.0f
#define MIN_USER_TIMENEEDED 1.0f
#define MAX_USER_TIMENEEDED 25.0f

float timePassed;
float difficulty;
void updateDifficulty( float dt )
{
	timePassed += dt;
	difficulty = lerp( 1.0f, 0.25f, inverseLerp( 10.0f, 60.0f * 4.0f, timePassed ) );
}

int getDeduction( )
{
	return 2 + (int)( timePassed / 45.0f );
}

float getUserWaitTime( )
{
	return rand_GetRangeFloat( NULL, MIN_USER_WAIT, MAX_USER_WAIT ) * difficulty;
}

float getGiveUpTime( )
{
	return rand_GetRangeFloat( NULL, MIN_USER_GIVEUP, MAX_USER_GIVEUP ) * difficulty;
}

float getTimeNeeded( )
{
	return rand_GetRangeFloat( NULL, MIN_USER_TIMENEEDED, MAX_USER_TIMENEEDED ) * difficulty;
}

static User* sbUsers = NULL;
static int loggedInUsers[MAX_LOGGED_IN];

static bool shouldChoose( size_t total )
{
	return ( ( rand_GetU32( NULL ) ) % (int)total == 0 );
}

static float userLoginTimeLeft;

static uint8_t driveSpaceLeft( )
{
	uint8_t totalSpace = 0;

	for( size_t i = 0; i < sb_Count( drive.sbFiles ); ++i ) {
		EntityID id = drive.sbFiles[i];
		uint16_t idx = idSet_GetIndex( id );
		totalSpace += sbFiles[idx].sizeBlocks;
	}

	return ( drive.maxSize - totalSpace );
}

static void addExistingFileToDrive( EntityID fileID )
{
	uint16_t idx = idSet_GetIndex( fileID );
	assert( driveSpaceLeft( ) >= sbFiles[idx].sizeBlocks );
	sb_Push( drive.sbFiles, fileID );
}

static void removeFileFromDrive( EntityID fileID )
{
	for( size_t i = 0; i < sb_Count( drive.sbFiles ); ++i ) {
		if( drive.sbFiles[i] == fileID ) {
			sb_Remove( drive.sbFiles, i );
			--i;
		}
	}
}

static EntityID createFile(  const char* name, const char* description, int type, uint8_t sizeBlocks )
{
	EntityID newID = idSet_ClaimID( &fileIDs );
	uint16_t idx = idSet_GetIndex( newID );
	if( idx >= sb_Count( sbFiles ) ) {
		size_t add = ( idx + 1 ) - sb_Count( sbFiles );
		assert( add > 0 );
		sb_Add( sbFiles, add );
	}

	File* f = &( sbFiles[idx] );
	memset( f->name, 0, STR_SIZE * sizeof( f->name[0] ) );
	if( name ) strncpy( f->name, name, STR_SIZE - 1 );
	memset( f->description, 0, STR_SIZE * sizeof( f->description[0] ) );
	if( description ) strncpy( f->description, description, STR_SIZE - 1 );
	f->fileType = type;
	f->sizeBlocks = sizeBlocks;
	f->sbContainedFiles = NULL;
	f->amtDone = 1.0f;
	f->isSelected = false;

	if( type != FT_ARC ) {
		RequestableFile rf;
		memset( rf.name, 0, STR_SIZE * sizeof( rf.name[0] ) );
		if( name ) strncpy( rf.name, name, STR_SIZE - 1 );
		rf.fileID = newID;
		sb_Push( sbRequestableFiles, rf );
	}

	return newID;
}

static void destroyFile( EntityID fileID )
{
	uint16_t idx = idSet_GetIndex( fileID );

	// if it has any archived files then destroy them
	for( size_t i = 0; i < sb_Count( sbFiles[idx].sbContainedFiles ); ++i ) {
		destroyFile( sbFiles[idx].sbContainedFiles[i] );
	}
	sb_Release( sbFiles[idx].sbContainedFiles );

	idSet_ReleaseID( &fileIDs, fileID );
	removeFileFromDrive( fileID );
}

static bool mousePressed = false;
static void mouseLeftPressed( void )
{
	mousePressed = true;
}

static void mouseLeftReleased( void )
{
	mousePressed = false;
}

static int shiftPressed = 0;
static void shiftPressedDown( void )
{
	++shiftPressed;
}

static void shiftReleased( void )
{
	--shiftPressed;
}

static bool expandPressed = false;
static void xPressed( void )
{
	expandPressed = true;
}

static bool archivePressed = false;;
static void aPressed( void )
{
	archivePressed = true;
}

static bool deletePressed = false;
static void dPressed( void )
{
	deletePressed = true;
}

static void deselectAllFiles( )
{
	EntityID id = idSet_GetFirstValidID( &fileIDs );
	while( id != INVALID_ENTITY_ID ) {
		uint16_t idx = idSet_GetIndex( id );
		sbFiles[idx].isSelected = false;
		id = idSet_GetNextValidID( &fileIDs, id );
	}
}

static void recalculateArchiveSize( EntityID archive )
{
	uint16_t idx = idSet_GetIndex( archive );
	uint8_t containedSize = 0;
	for( size_t i = 0; i < sb_Count( sbFiles[idx].sbContainedFiles ); ++i ) {
		EntityID childID = sbFiles[idx].sbContainedFiles[i];
		uint16_t childIdx = idSet_GetIndex( childID );
		containedSize += sbFiles[childIdx].sizeBlocks;
	}

	if( ( containedSize % 2 ) == 1 ) {
		++containedSize;
	}
	sbFiles[idx].sizeBlocks = containedSize / 2;
}

static void immediateAddFileToArchive( EntityID archive, EntityID file )
{
	uint16_t idx = idSet_GetIndex( archive );
	sb_Push( sbFiles[idx].sbContainedFiles, file );

	recalculateArchiveSize( archive );
}

static void immediateRemoveFileFromArchive( EntityID archive, EntityID file )
{
	uint16_t idx = idSet_GetIndex( archive );
	uint8_t containedSize = 0;
	for( size_t i = 0; i < sb_Count( sbFiles[idx].sbContainedFiles ); ++i ) {
		if( sbFiles[idx].sbContainedFiles[i] == file ) {
			sb_Remove( sbFiles[idx].sbContainedFiles, i );
			--i;
		}
	}

	recalculateArchiveSize( archive );
}

static uint8_t calculateSizeWithoutFile( EntityID archive, EntityID skipFile )
{
	uint16_t idx = idSet_GetIndex( archive );
	uint8_t containedSize = 0;
	for( size_t i = 0; i < sb_Count( sbFiles[idx].sbContainedFiles ); ++i ) {
		EntityID childID = sbFiles[idx].sbContainedFiles[i];
		if( skipFile == childID ) continue;
		uint16_t childIdx = idSet_GetIndex( childID );
		containedSize += sbFiles[childIdx].sizeBlocks;
	}

	if( ( containedSize % 2 ) == 1 ) {
		++containedSize;
	}
	return ( containedSize / 2 );
}

static uint8_t calculateSizeWithFile( EntityID archive, EntityID addFile )
{
	uint16_t idx = idSet_GetIndex( archive );
	uint8_t containedSize = 0;
	for( size_t i = 0; i < sb_Count( sbFiles[idx].sbContainedFiles ); ++i ) {
		EntityID childID = sbFiles[idx].sbContainedFiles[i];
		uint16_t childIdx = idSet_GetIndex( childID );
		containedSize += sbFiles[childIdx].sizeBlocks;
	}

	uint16_t addIdx = idSet_GetIndex( addFile );
	containedSize += sbFiles[addIdx].sizeBlocks;

	if( ( containedSize % 2 ) == 1 ) {
		++containedSize;
	}
	return ( containedSize / 2 );
}

static void addExpandOperation( EntityID archive )
{
	Operation newOp;
	newOp.type = OP_EXPAND;
	newOp.archiveFile = archive;
	newOp.processingFile = INVALID_ENTITY_ID;

	for( size_t i = 0; i < ARRAYSIZE( newOp.compressFiles ); ++i ) {
		newOp.compressFiles[i] = INVALID_ENTITY_ID;
	}

	sb_Push( sbActiveOperations, newOp );
}

static void addCompressionOperation( EntityID archive )
{
	Operation newOp;
	newOp.type = OP_ARCHIVE;
	newOp.archiveFile = archive;
	newOp.processingFile = INVALID_ENTITY_ID;
	
	int currArcFileIdx = 0;
	EntityID id = idSet_GetFirstValidID( &fileIDs );
	memset( newOp.compressFiles, 0, ARRAYSIZE( newOp.compressFiles ) * sizeof( newOp.compressFiles[0] ) );
	while( ( id != INVALID_ENTITY_ID ) && ( currArcFileIdx < ARRAYSIZE( newOp.compressFiles ) ) ) {
		uint16_t idx = idSet_GetIndex( id );
		if( sbFiles[idx].isSelected ) {
			newOp.compressFiles[currArcFileIdx] = id;
			sbFiles[idx].isSelected = false;
			++currArcFileIdx;
		}

		id = idSet_GetNextValidID( &fileIDs, id );
	}

	// nothing to add, don't bother creating the new archive, destroy it instead
	if( currArcFileIdx <= 0 ) {
		destroyFile( archive );
		return;
	}

	sb_Push( sbActiveOperations, newOp );
}

static void addDeleteOperation( EntityID file )
{
	Operation newOp;
	newOp.type = OP_DELETE;
	newOp.archiveFile = INVALID_ENTITY_ID;
	newOp.processingFile = file;

	for( size_t i = 0; i < ARRAYSIZE( newOp.compressFiles ); ++i ) {
		newOp.compressFiles[i] = INVALID_ENTITY_ID;
	}

	sb_Push( sbActiveOperations, newOp );
}

static bool isFileInDrive( EntityID file )
{
	for( size_t i = 0; i < sb_Count( drive.sbFiles ); ++i ) {
		if( drive.sbFiles[i] == file ) return true;
	}

	return false;
}

static bool isFileBeingOperatedOn( EntityID file )
{
	for( size_t i = 0; i < sb_Count( sbActiveOperations ); ++i ) {
		if( sbActiveOperations[i].archiveFile == file ) return true;
		if( sbActiveOperations[i].processingFile == file ) return true;

		for( size_t x = 0; x < ARRAYSIZE( sbActiveOperations[i].compressFiles ); ++x ) {
			if( sbActiveOperations[i].compressFiles[x] == file ) return true;
		}
	}

	return false;
}

void archiveButtonReleased( int i )
{
	archivePressed = true;
}

void expandButtonReleased( int i )
{
	expandPressed = true;
}

void deleteButtonReleased( int i )
{
	deletePressed = true;
}

void createUser( const char* name )
{
	User newUser;

	newUser.userName = name;
	newUser.sbRequests = NULL;

	sb_Push( sbUsers, newUser );
}

bool isUserLoggedIn( size_t idx )
{
	for( int i = 0; i < MAX_LOGGED_IN; ++i ) {
		if( loggedInUsers[i] == (int)idx ) return true;
	}
	return false;
}

int buttonSystem = -1;
int expandButton = -1;
int archiveButton = -1;
int deleteButton = -1;
int score = 0;
static int gameScreen_Enter( void )
{
	cam_TurnOnFlags( 0, 1 );

	idSet_Init( &fileIDs, 256 );
	
	gfx_SetClearColor( clr( 0.0f, 0.01f, 0.09f, 1.0f ) );

	//Vector2 dataAreaTopLeft = { 575.0f, 64.0f };
	//Vector2 dataAreaSize = { 200.0f, 550.0f };
	archiveButton = btn_Create( vec2( ( 800.0f + 574.0f ) / 2.0f, 600.0f - 120.0f ), vec2( 100.0f, 32.0f ), vec2( 98.0f, 30.0f ),
		"(A)rchive", textFont, CLR_WHITE, VEC2_ZERO, button3x3, -1, CLR_WHITE, 1, 12,
		NULL, archiveButtonReleased );
	btn_SetEnabled( archiveButton, false );
	expandButton = btn_Create( vec2( ( 800.0f + 574.0f ) / 2.0f, 600.0f - 80.0f ), vec2( 100.0f, 32.0f ), vec2( 98.0f, 30.0f ),
		"e(X)tract", textFont, CLR_WHITE, VEC2_ZERO, button3x3, -1, CLR_WHITE, 1, 12,
		NULL, expandButtonReleased );
	btn_SetEnabled( expandButton, false );
	deleteButton = btn_Create( vec2( ( 800.0f + 574.0f ) / 2.0f, 600.0f - 40.0f ), vec2( 100.0f, 32.0f ), vec2( 98.0f, 30.0f ),
		"(D)elete", textFont, CLR_WHITE, VEC2_ZERO, button3x3, -1, CLR_WHITE, 1, 12,
		NULL, deleteButtonReleased );
	btn_SetEnabled( deleteButton, false );

	// we'll want the total space required for all files to be around 1.5 times the size on the disk, max size is 18
	//  files should total 27 blocks
	// 1*3
	// 7*2
	// 10*1
	// for the intial archives things will be randomly positioned, among 4 archives

	EntityID arc;
	arc = createFile( "Arc0", "", FT_ARC, 0 ); // business
	immediateAddFileToArchive( arc, createFile( "Deliver", "Client deliverable", FT_DB, 3 ) );
	immediateAddFileToArchive( arc, createFile( "Clients", "IMPORTANT! DO NOT DELETE!", FT_DB, 2 ) );
	immediateAddFileToArchive( arc, createFile( "TODO", "", FT_TXT, 1 ) );
	addExistingFileToDrive( arc );

	arc = createFile( "Arc1", "", FT_ARC, 0 ); // catagorizer
	immediateAddFileToArchive( arc, createFile( "EDM2088", "Research data", FT_DAT, 3 ) );
	immediateAddFileToArchive( arc, createFile( "Manual", "How to run the an2.4", FT_TXT, 2 ) );
	immediateAddFileToArchive( arc, createFile( "an2.4", "Analytics program", FT_ELF, 1 ) );
	immediateAddFileToArchive( arc, createFile( "ml", "Catagorizer v2.4", FT_C, 1 ) );
	addExistingFileToDrive( arc );

	arc = createFile( "Arc2", "", FT_ARC, 0 ); // scraper
	immediateAddFileToArchive( arc, createFile( "Scraper", "", FT_ELF, 2 ) );
	immediateAddFileToArchive( arc, createFile( "scraperSrc", "Public records scraper source", FT_C, 1 ) );
	//immediateAddFileToArchive( arc, createFile( "scrapedJ2088", "Auto scraped data", FT_DAT, 2 ) );
	immediateAddFileToArchive( arc, createFile( "scrapedF2088", "Auto scraped data", FT_DAT, 1 ) );
	immediateAddFileToArchive( arc, createFile( "scrpr10892", "scraper cache", FT_DAT, 2 ) );
	addExistingFileToDrive( arc );

	arc = createFile( "Arc3", "", FT_ARC, 0 ); // website and hr
	immediateAddFileToArchive( arc, createFile( "Handbook", "Employee handbook", FT_TXT, 2 ) );
	immediateAddFileToArchive( arc, createFile( "Logo", "Official Alon Analytics logo", FT_IMG, 1 ) );
	immediateAddFileToArchive( arc, createFile( "WebSite", "Layout for the company website", FT_UI, 1 ) );
	//immediateAddFileToArchive( arc, createFile( "WebBG", "Background image", FT_IMG, 1 ) );
	immediateAddFileToArchive( arc, createFile( "Copy", "Website copy text", FT_TXT, 1 ) );
	immediateAddFileToArchive( arc, createFile( "WDA2087", "Website analytics data", FT_DAT, 1 ) );
	addExistingFileToDrive( arc );

	// create the files and archive as we go
	/*EntityID ld42Arc = createFile( "LD42_Release", "Content submission archive", FT_ARC, 0, 0, 0 );
	immediateAddFileToArchive( ld42Arc, createFile( "LD42", "LD42 Project Source Code", FT_C, 2, 2, 2 ) );
	immediateAddFileToArchive( ld42Arc, createFile( "Logo", "Alon Analytics offical logo", FT_IMG, 4, 2, 6 ) );
	immediateAddFileToArchive( ld42Arc, createFile( "Alon_Analytics", "LD42 Release", FT_ELF, 6, 6, 6 ) );
	immediateAddFileToArchive( ld42Arc, createFile( "Notes", NULL, FT_TXT, 1, 1, 2 ) );
	addExistingFileToDrive( ld42Arc );*/

	createUser( "0cool" );
	createUser( "4c1d8urn" );
	createUser( "Lightman" );
	createUser( "StephenF" );
	createUser( "sark" );
	createUser( "Newman" );
	createUser( "PuppetMaster" );
	createUser( "mcohen216" );
	createUser( "pgibbons" );
	createUser( "mb" );
	createUser( "samir" );
	createUser( "Bill_L" );
	createUser( "MWaddams" );

	input_BindOnMouseButtonPress( SDL_BUTTON_LEFT, mouseLeftPressed );
	input_BindOnMouseButtonRelease( SDL_BUTTON_LEFT, mouseLeftReleased );

	input_BindOnKeyPress( SDLK_LSHIFT, shiftPressedDown );
	input_BindOnKeyPress( SDLK_RSHIFT, shiftPressedDown );
	input_BindOnKeyRelease( SDLK_LSHIFT, shiftReleased );

	input_BindOnKeyRelease( SDLK_RSHIFT, shiftReleased );

	input_BindOnKeyPress( SDLK_x, xPressed );
	input_BindOnKeyPress( SDLK_a, aPressed );
	input_BindOnKeyPress( SDLK_d, dPressed );

	archiveNameCount = 0;

	userLoginTimeLeft = 2.0f;

	score = 15;

	for( int i = 0; i < MAX_LOGGED_IN; ++i ) {
		loggedInUsers[i] = -1;
	}

	timePassed = 0.0f;
	difficulty = 1.0f;

	return 1;
}

static int gameScreen_Exit( void )
{
	sb_Release( sbRequestableFiles );

	sb_Release( drive.sbFiles );
	sb_Release( sbActiveOperations );

	for( size_t i = 0; i < sb_Count( sbFiles ); ++i ) {
		sb_Release( sbFiles[i].sbContainedFiles );
	}
	sb_Release( sbFiles );

	for( size_t i = 0; i < sb_Count( sbUsers ); ++i ) {
		sb_Release( sbUsers[i].sbRequests );
	}
	sb_Release( sbUsers );

	input_ClearAllKeyBinds( );
	input_ClearAllMouseButtonBinds( );

	btn_DestroyAll( );

	return 1;
}

static void gameScreen_ProcessEvents( SDL_Event* e )
{
}

static void gameScreen_Process( void )
{
}

static void appendTextToStretchyBuffer( const char* txt, char** sb )
{
	while( *txt ) {
		sb_Push( (*sb), *(txt++) );
	}
}

static void appendFileNameToStretchyBuffer( EntityID fileID, char** sb )
{
	uint16_t fileIdx = idSet_GetIndex( fileID );
	appendTextToStretchyBuffer( sbFiles[fileIdx].name, sb );
	sb_Push( *sb, '.' );
	appendTextToStretchyBuffer( fileTypes[sbFiles[fileIdx].fileType].typeName, sb );
}

char* sbDisplayName = NULL;
static void drawFileInfo( EntityID fileID, Vector2 topLeft, Vector2 size, bool highlighted, bool selected )
{
	uint16_t fileIdx = idSet_GetIndex( fileID );

	// don't bother with empty files, these are most likely archives that are just starting up
	if( sbFiles[fileIdx].sizeBlocks <= 0 ) return;

	Color clr = fileTypes[sbFiles[fileIdx].fileType].clr;

	Vector2 bgSize = size;
	bgSize.x *= sbFiles[fileIdx].amtDone;

	// assuming offset is in center
	Vector2 imgSize;
	img_GetSize( gradientImg, &imgSize );
	imgSize.x = bgSize.x / imgSize.x;
	imgSize.y = bgSize.y / imgSize.y;

	Vector2 pos;
	vec2_AddScaled( &topLeft, &bgSize, 0.5f, &pos );

	sb_Clear( sbDisplayName );
	appendFileNameToStretchyBuffer( fileID, &sbDisplayName );

	if( highlighted ) {
		clr.r *= 1.25f;
		clr.g *= 1.25f;
		clr.b *= 1.25f;
	}

	if( selected ) {
		appendTextToStretchyBuffer( "*", &sbDisplayName );
		sb_Insert( sbDisplayName, 0, '*' );
	}

	sb_Push( sbDisplayName, 0 );

	img_Draw_sv_c( gradientImg, 1, pos, pos, imgSize, imgSize, clr, clr, 1 );


	Vector2 textPos = topLeft;
	Vector2 textSize = size;
	textPos.x += 4.0f;
	textSize.x -= 8.0f;
	txt_DisplayTextArea( (uint8_t*)sbDisplayName, textPos, textSize, fileTypes[sbFiles[fileIdx].fileType].textClr, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, displayFont, 0, NULL, 1, 2 );
}

static bool isPointInsideBox( Vector2 point, Vector2 topLeft, Vector2 size )
{
	Vector2 diff;
	vec2_Subtract( &point, &topLeft, &diff );
	return ( ( diff.x >= 0.0f ) && ( diff.x <= size.w ) && ( diff.y >= 0.0f ) && ( diff.y <= size.h ) );
}

static void drawMeter( Vector2 topLeft, Vector2 size, float pctFilled, Color bgColor, Color fillColor )
{
	Vector2 imgSize;
	img_GetSize( whiteImg, &imgSize );

	Vector2 bgSize = size;
	
	// assuming offset is in center
	bgSize.x = bgSize.x / imgSize.x;
	bgSize.y = bgSize.y / imgSize.y;

	Vector2 pos;
	vec2_AddScaled( &topLeft, &size, 0.5f, &pos );

	img_Draw_sv_c( whiteImg, 1, pos, pos, bgSize, bgSize, bgColor, bgColor, 0 );


	Vector2 fgSize = size;
	fgSize.x *= pctFilled;
	vec2_AddScaled( &topLeft, &fgSize, 0.5f, &pos );
	fgSize.x = fgSize.x / imgSize.x;
	fgSize.y = fgSize.y / imgSize.y;

	//vec2_AddScaled( &topLeft, &fgSize, 0.5f, &pos );
	img_Draw_sv_c( whiteImg, 1, pos, pos, fgSize, fgSize, fillColor, fillColor, 1 );
}

char* sbDataDisplay = NULL;
static void gameScreen_Draw( void )
{
	// draw the storage
	Vector2 bgPos = { 400.0f, 300.0f };
	Vector2 imgSize;
	img_GetSize( whiteImg, &imgSize );
	Vector2 imgScale;
	imgScale.x = 350 / imgSize.x;
	imgScale.y = 600 / imgSize.y;
	//img_Draw_sv_c( whiteImg, 1, bgPos, bgPos, imgScale, imgScale, CLR_BLACK, CLR_BLACK, 0 );

	//  draw the storage space dividers
	debugRenderer_AABB( 1, vec2( 250.0f, 12.0f ), vec2( 300.0f, 576.0f ), CLR_WHITE );

	// draw your score
	char scoreTxt[32];
	memset( scoreTxt, 0, sizeof( scoreTxt ) );
	SDL_snprintf( scoreTxt, 31, "%i", score );
	Color scoreClr = CLR_WHITE;
	if( score <= 5 ) {
		scoreClr = CLR_RED;
	} else if( score <= 10 ) {
		scoreClr = clr( 1.0f, 0.5f, 0.0f, 1.0f );
	}
	txt_DisplayString( scoreTxt, vec2( 112.0f, 48.0f ), scoreClr, HORIZ_ALIGN_CENTER, VERT_ALIGN_CENTER, largeFont, 1, 0 );

	// draw the file display
	Vector2 mousePos;
	input_GetMousePosition( &mousePos );
	uint8_t currOffset = 0;
	highlighted = INVALID_ENTITY_ID;
	for( size_t i = 0; i < sb_Count( drive.sbFiles ); ++i ) {
		EntityID fileID = drive.sbFiles[i];
		uint16_t fileIdx = idSet_GetIndex( fileID );
		Vector2 pos;
		pos.x = 250.0f;
		pos.y = 12.0f + ( currOffset * 32.0f );

		Vector2 size;
		size.x = 300.0f;
		size.y = sbFiles[fileIdx].sizeBlocks * 32.0f;

		// do the selection here based on the mouse position and input
		bool over = isPointInsideBox( mousePos, pos, size );
		if( over ) {
			highlighted = fileID;
			if( mousePressed && !isFileBeingOperatedOn( fileID ) ) {
				mousePressed = false;
				if( shiftPressed == 0 ) {
					// if shift isn't pressed clear all current selections
					EntityID deselectID = idSet_GetFirstValidID( &fileIDs );
					while( deselectID != INVALID_ENTITY_ID ) {
						if( deselectID != fileID ) {
							uint16_t idx = idSet_GetIndex( deselectID );
							sbFiles[idx].isSelected = false;
						}

						deselectID = idSet_GetNextValidID( &fileIDs, deselectID );;
					}
				}

				sbFiles[fileIdx].isSelected = !sbFiles[fileIdx].isSelected;
			}
		}

		drawFileInfo( fileID, pos, size, ( highlighted == fileID ), sbFiles[fileIdx].isSelected );

		currOffset += sbFiles[fileIdx].sizeBlocks;
	}

	// draw the info display
	Vector2 dataAreaTopLeft = { 575.0f, 64.0f };
	Vector2 dataAreaSize = { 200.0f, 550.0f };
	
	if( sb_Count( sbDataDisplay ) > 0 ) {
		memset( sbDataDisplay, 0, sb_Count( sbDataDisplay ) * sizeof( sbDataDisplay[0] ) );
	}
	sb_Clear( sbDataDisplay );
	if( highlighted != INVALID_ENTITY_ID ) {
		uint16_t fileIdx = idSet_GetIndex( highlighted );

		appendFileNameToStretchyBuffer( highlighted, &sbDataDisplay );
		sb_Push( sbDataDisplay, '\n' );
		sb_Push( sbDataDisplay, ' ' );
		sb_Push( sbDataDisplay, ' ' );
		sb_Push( sbDataDisplay, ' ' );
		if( sbFiles[fileIdx].description == NULL ) {
			appendTextToStretchyBuffer( "NO DESCRIPTION PROVIDED", &sbDataDisplay );
		} else {
			appendTextToStretchyBuffer( sbFiles[fileIdx].description, &sbDataDisplay );
		}
		sb_Push( sbDataDisplay, '\n' );

		if( sb_Count( sbFiles[fileIdx].sbContainedFiles ) > 0 ) {
			appendTextToStretchyBuffer( "Contents:\n", &sbDataDisplay );
			for( size_t i = 0; i < sb_Count( sbFiles[fileIdx].sbContainedFiles ); ++i ) {
				sb_Push( sbDataDisplay, ' ' ); sb_Push( sbDataDisplay, '+' ); sb_Push( sbDataDisplay, '>' );
				appendFileNameToStretchyBuffer( sbFiles[fileIdx].sbContainedFiles[i], &sbDataDisplay );
				sb_Push( sbDataDisplay, '\n' );
			}
		}
	} else {
		// just display a list of the selected files
		appendTextToStretchyBuffer( "Selected Files:\n", &sbDataDisplay );
		for( size_t i = 0; i < sb_Count( drive.sbFiles ); ++i ) {

			EntityID fileID = drive.sbFiles[i];
			uint16_t fileIdx = idSet_GetIndex( fileID );

			if( !sbFiles[fileIdx].isSelected ) continue;

			sb_Push( sbDataDisplay, ' ' ); sb_Push( sbDataDisplay, ' ' ); sb_Push( sbDataDisplay, ' ' );
			appendFileNameToStretchyBuffer( fileID, &sbDataDisplay );

			// TODO?: Display the deeper structure for achived files
			//  or don't, if they want to make archives of archives then let them get the space gains, but they lose speed since they don't know what contains what
			if( sb_Count( sbFiles[fileIdx].sbContainedFiles ) > 0 ) {
				for( size_t x = 0; x < sb_Count( sbFiles[fileIdx].sbContainedFiles ); ++x ) {
					sb_Push( sbDataDisplay, '\n' );
					sb_Push( sbDataDisplay, ' ' ); sb_Push( sbDataDisplay, ' ' ); sb_Push( sbDataDisplay, ' ' );
					sb_Push( sbDataDisplay, ' ' ); sb_Push( sbDataDisplay, '+' ); sb_Push( sbDataDisplay, '>' );
					appendFileNameToStretchyBuffer( sbFiles[fileIdx].sbContainedFiles[x], &sbDataDisplay );
				}
			}
			sb_Push( sbDataDisplay, '\n' );
		}
	}
	sb_Push( sbDataDisplay, 0 );
	txt_DisplayTextArea( (const uint8_t*)sbDataDisplay, dataAreaTopLeft, dataAreaSize, CLR_LIGHT_GREY, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, textFont, 0, NULL, 1, 10 );

	// handle input stuff
	// display (A)rchive button and file name text field
	int numSelected = 0;
	bool archiveSelected = false;
	for( size_t i = 0; i < sb_Count( drive.sbFiles ); ++i ) {
		EntityID fileID = drive.sbFiles[i];
		uint16_t fileIdx = idSet_GetIndex( fileID );

		if( !sbFiles[fileIdx].isSelected ) continue;

		++numSelected;
		archiveSelected = archiveSelected || ( sbFiles[fileIdx].fileType == FT_ARC );
	}

	if( numSelected > 0 ) {
		if( ( numSelected > 1 ) || !archiveSelected ) {
			btn_SetEnabled( archiveButton, true );
			if( archivePressed ) {
				char fileName[62] = { 0 };
				SDL_snprintf( fileName, 61, "Archive_%i", archiveNameCount );
				++archiveNameCount;
				EntityID newArcFile = createFile( fileName, "Archived files", FT_ARC, 0 );
				addExistingFileToDrive( newArcFile );
				addCompressionOperation( newArcFile );
				deselectAllFiles( );
			}
		} else {
			btn_SetEnabled( archiveButton, false );
		}
		
		btn_SetEnabled( deleteButton, true );
		if( deletePressed ) {
			EntityID fileID = idSet_GetFirstValidID( &fileIDs );
			while( fileID!= INVALID_ENTITY_ID ) {
				uint16_t idx = idSet_GetIndex( fileID );
				if( sbFiles[idx].isSelected ) {
					addDeleteOperation( fileID );
				}

				fileID = idSet_GetNextValidID( &fileIDs, fileID );
			}
			deselectAllFiles( );
		}
	} else {
		btn_SetEnabled( archiveButton, false );
		btn_SetEnabled( deleteButton, false );
	}

	if( archiveSelected ) {
		btn_SetEnabled( expandButton, true );

		if( expandPressed ) {
			EntityID fileID = idSet_GetFirstValidID( &fileIDs );
			while( fileID!= INVALID_ENTITY_ID ) {
				uint16_t idx = idSet_GetIndex( fileID );

				if( sbFiles[idx].isSelected && ( sbFiles[idx].fileType == FT_ARC ) ) {
					sbFiles[idx].isSelected = false;
					addExpandOperation( fileID );
				}

				fileID = idSet_GetNextValidID( &fileIDs, fileID );
			}
			deselectAllFiles( );
		}
	} else {
		btn_SetEnabled( expandButton, false );
	}

	// display users and their requests
	float currTop = 96.0f;
	for( int i = 0; i < MAX_LOGGED_IN; ++i ) {
		if( loggedInUsers[i] < 0 ) continue;
		int idx = loggedInUsers[i];

		float currY = currTop;

		// draw user info

		txt_DisplayString( sbUsers[idx].userName, vec2( 16.0f, currY ), CLR_LIGHT_GREY, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, displayFont, 1, 10 );
		currY += 32.0f;
		for( size_t x = 0; x < sb_Count( sbUsers[idx].sbRequests ); ++x ) {
			UserRequest* req = &( sbUsers[idx].sbRequests[x] );

			EntityID fileID = sbRequestableFiles[req->requestableFileIdx].fileID;
			uint16_t idx = idSet_GetIndex( fileID );

			Vector2 p = vec2( 24.0f, currY );
			Color txtColor;
			clr_Lerp( &CLR_WHITE, &CLR_RED, ( req->giveUpTimePassed / req->giveUpTime ), &txtColor );
			txt_DisplayString( sbFiles[idx].name, p, txtColor, HORIZ_ALIGN_LEFT, VERT_ALIGN_TOP, textFont, 1, 10 );

			p.x -= 2.0f;
			p.y -= 2.0f;
			drawMeter( p, vec2( 150.0f, 18.0f ), ( req->timePassed / req->timeRequired ), clr( 0.1f, 0.1f, 0.1f, 1.0f ), clr( 0.1f, 0.5f, 0.1f, 1.0f ) );

			currY += 20.0f;
		}

		currTop += 100.0f;
	}

	// clean up
	archivePressed = false;
	expandPressed = false;
	deletePressed = false;
}

static void signalUserLogin( const char* user )
{
	snd_Play( loginSnd, 1.0f, rand_GetToleranceFloat( NULL, 1.0f, 0.1f ), 0.0f, 0 );
}

static void signalUserLogout( const char* user )
{
	snd_Play( logoutSnd, 1.0f, rand_GetToleranceFloat( NULL, 1.0f, 0.1f ), 0.0f, 0 );
}

static void signalUserSuccess( const char* user )
{
	snd_Play( requestDoneSnd, 1.0f, rand_GetToleranceFloat( NULL, 1.0f, 0.1f ), 0.0f, 0 );
	++score;
}

static bool signalUserFailure( const char* user )
{
	snd_Play( requestFailSnd, 1.0f, rand_GetToleranceFloat( NULL, 1.0f, 0.1f ), 0.0f, 0 );
	score -= getDeduction( );;
	if( score < 0 ) {
		// FAIL!
		gsmEnterState( &globalFSM, &gameOverScreenState );
		return true;
	}

	return false;;
}

static void signalFail( const char* reason )
{
	snd_Play( opFailedSnd, 1.0f, rand_GetToleranceFloat( NULL, 1.0f, 0.1f ), 0.0f, 0 );
}

static void signalNewRequest( const char* user, const char* fileName )
{
	snd_Play( newRequestSnd, 1.0f, rand_GetToleranceFloat( NULL, 1.0f, 0.1f ), 0.0f, 0 );
}

static void updateUserLogin( float dt )
{
	userLoginTimeLeft -= dt;

	if( userLoginTimeLeft <= 0.0f ) {
		userLoginTimeLeft = getUserWaitTime( );

		// choose a random user that isn't logged in and log them in
		int logInIdx = -1;
		for( int i = 0; ( i < MAX_LOGGED_IN ) && ( logInIdx < 0 ); ++i ) {
			if( loggedInUsers[i] < 0 ) logInIdx = i;
		}

		// no space for logged in users
		if( logInIdx < 0 ) return;

		int total = 0;
		size_t chosen = SIZE_MAX;
		for( size_t i = 0; i < sb_Count( sbUsers ); ++i ) {
			if( !isUserLoggedIn( i ) ) {
				++total;
				if( shouldChoose( total ) ) {
					chosen = i;
				}
			}
		}

		if( chosen != SIZE_MAX ) {
			loggedInUsers[logInIdx] = (int)chosen;

			sbUsers[chosen].requestWaitTime = getUserWaitTime( );
			sbUsers[chosen].fileRequestsLeft = rand_GetRangeS32( NULL, MIN_USER_REQ, MAX_USER_REQ );
			signalUserLogin( sbUsers[chosen].userName );
		}
	}
}

static void updateUserRequests( float dt )
{
	bool someoneLoggedOff = false;
	for( int i = 0; i < MAX_LOGGED_IN; ++i ) {
		if( loggedInUsers[i] < 0 ) continue;
		size_t idx = (size_t)loggedInUsers[i];

		sbUsers[idx].requestWaitTime -= dt;
		if( sbUsers[idx].requestWaitTime <= 0.0f ) {
			sbUsers[idx].requestWaitTime = getUserWaitTime( );

			// if the user has no pending requests and no requests left then they log out
			if( ( sbUsers[idx].fileRequestsLeft <= 0 ) && ( sb_Count( sbUsers[idx].sbRequests ) <= 0 ) ) {
				sb_Clear( sbUsers[idx].sbRequests );
				signalUserLogout( sbUsers[idx].userName );
				loggedInUsers[i] = -1;
				someoneLoggedOff = true;
			} else {
				// add a new file request
				UserRequest request;
			
				// we'll want to have more of a chance to select deleted files, anti-cheese tech so you can't just delete
				//  the biggest files and have the rest sitting there
				request.requestableFileIdx = sbRequestableFiles[0].fileID;
				int total = 0;
				for( size_t f = 0; f < sb_Count( sbRequestableFiles ); ++f ) {
					int weight = 1;
					if( !idSet_IsIDValid( &fileIDs, sbRequestableFiles[f].fileID ) ) {
						weight = 6;
					}

					while( weight > 0 ) {
						++total;
						--weight;
						if( shouldChoose( total ) ) {
							request.requestableFileIdx = f;
						}
					}
				}


				request.timeRequired = getTimeNeeded( );
				request.giveUpTime = getGiveUpTime( );

				request.timePassed = 0.0f;
				request.giveUpTimePassed = 0.0f;

				sb_Push( sbUsers[idx].sbRequests, request );

				--( sbUsers[idx].fileRequestsLeft );

				uint16_t fileIdx = idSet_GetIndex( request.requestableFileIdx );
				signalNewRequest( sbUsers[idx].userName, sbFiles[fileIdx].name );
			}
		}

		// update individual requests
		for( int x = 0; x < (int)sb_Count( sbUsers[idx].sbRequests ); ++x ) {
			UserRequest* req = &( sbUsers[idx].sbRequests[x] );
			if( !isFileInDrive( sbRequestableFiles[req->requestableFileIdx].fileID ) ) {
				req->giveUpTimePassed += dt;
			} else {
				req->timePassed += dt;
			}

			bool removeReq = false;
			if( req->giveUpTimePassed >= req->giveUpTime ) {
				if( signalUserFailure( sbUsers[idx].userName ) ) {
					// game over
					return;
				}
				removeReq = true;
			} else if( req->timePassed >= req->timeRequired ) {
				signalUserSuccess( sbUsers[idx].userName );
				removeReq = true;
			}

			if( removeReq ) {
				sb_Remove( sbUsers[idx].sbRequests, (size_t)x );
				--x;
			}
		}
	}

	// can't find an assurance that qsort keeps the same order for elements, so we'll do a crap sort here
	if( someoneLoggedOff ) {
		for( int a = 0; a < MAX_LOGGED_IN - 1; ++a ) {
			if( loggedInUsers[a] < 0 ) {
				// find next val >= 0 and replace it
				for( int b = a + 1; b < MAX_LOGGED_IN; ++b ) {
					if( loggedInUsers[b] >= 0 ) {
						loggedInUsers[a] = loggedInUsers[b];
						loggedInUsers[b] = -1;
						break;
					}
				}
			}
		}
	}
}

#define ARC_PER_BLOCK_TIME 0.25f
static bool runArchive( Operation* op, float delta )
{
	EntityID arcID = op->archiveFile;
	uint16_t arcIdx = idSet_GetIndex( arcID );
	File* arcFile = &( sbFiles[arcIdx] );

	EntityID currID = op->processingFile;
	if( currID != INVALID_ENTITY_ID ) {
		// compressing file
		uint16_t currIdx = idSet_GetIndex( currID );
		File* currFile = &( sbFiles[currIdx] );

		float totalTime = currFile->sizeBlocks * ARC_PER_BLOCK_TIME;
		float timeSpent = ( 1.0f - currFile->amtDone ) * currFile->sizeBlocks * ARC_PER_BLOCK_TIME;
		timeSpent += delta;

		currFile->amtDone = 1.0f - timeSpent / totalTime;

		arcFile->amtDone = clamp( 0.0f, 1.0f, 1.0f - currFile->amtDone );

		if( currFile->amtDone <= 0.0f ) {
			currFile->amtDone = 0.0f;

			// if after this operation the drive would be over full then fail
			uint8_t spaceLeft = driveSpaceLeft( );
			spaceLeft += currFile->sizeBlocks;
			spaceLeft += arcFile->sizeBlocks;

			if( spaceLeft < calculateSizeWithFile( arcID, currID ) ) {
				signalFail( "Archive would grow too large." );
				return true;
			}

			removeFileFromDrive( currID );
			immediateAddFileToArchive( arcID, currID );
			op->processingFile = INVALID_ENTITY_ID;
		}
	} else {
		// choose file to compress
		size_t firstFound = SIZE_MAX;
		for( size_t i = 0; ( i < ARRAYSIZE( op->compressFiles ) ) && ( firstFound == SIZE_MAX ); ++i ) {
			if( op->compressFiles[i] != INVALID_ENTITY_ID ) {
				firstFound = i;
			}
		}

		if( firstFound == SIZE_MAX ) {
			// no more files to archive, op done
			return true;
		}

		op->processingFile = op->compressFiles[firstFound];
		op->compressFiles[firstFound] = INVALID_ENTITY_ID;
	}

	return false;
}

#define TIME_TO_EXPAND_BLOCK 0.5f
static bool runExpand( Operation* op, float delta )
{
	EntityID arcID = op->archiveFile;
	uint16_t arcIdx = idSet_GetIndex( arcID );
	File* arcFile = &( sbFiles[arcIdx] );

	EntityID currID = op->processingFile;
	
	// we're either currently working on a file, or we're starting a file
	if( currID != INVALID_ENTITY_ID ) {
		uint16_t currIdx = idSet_GetIndex( currID );
		File* currFile = &( sbFiles[currIdx] );

		// file already in drive, we're operating on it
		float totalTime = currFile->sizeBlocks * TIME_TO_EXPAND_BLOCK;
		float timeSpent = currFile->amtDone * currFile->sizeBlocks * TIME_TO_EXPAND_BLOCK;
		timeSpent += delta;

		currFile->amtDone = timeSpent / totalTime;

		if( currFile->amtDone >= 1.0f ) {
			// file done
			currFile->amtDone = 1.0f;
			op->processingFile = INVALID_ENTITY_ID;

			if( sb_Count( arcFile->sbContainedFiles ) <= 0 ) {
				// no more files left
				destroyFile( arcID );
				return true;
			}
		}

		arcFile->amtDone = 1.0f - currFile->amtDone;
	} else {
		// starting up a file
		currID = arcFile->sbContainedFiles[0];
		uint16_t currIdx = idSet_GetIndex( currID );
		File* currFile = &( sbFiles[currIdx] );
		
		//  check to see if there's room for it, if there isn't then fail
		uint8_t spaceLeft = driveSpaceLeft( );
		spaceLeft += arcFile->sizeBlocks;
		spaceLeft -= calculateSizeWithoutFile( arcID, currID );

		if( currFile->sizeBlocks > spaceLeft ) {
			signalFail( "Not enough space to decompress file." );
			arcFile->amtDone = 1.0f;
			return true;
		}

		// remove from archive
		immediateRemoveFileFromArchive( arcID, currID );

		addExistingFileToDrive( currID );
		currFile->amtDone = 0.0f;
		op->processingFile = currID;
	}

	return false;
}

#define TIME_TO_DELETE_BLOCK 0.15f
static bool runDelete( Operation* op, float delta )
{
	EntityID fileID = op->processingFile;
	uint16_t fileIdx = idSet_GetIndex( fileID );
	File* file = &( sbFiles[fileIdx] );

	float totalTime = file->sizeBlocks * TIME_TO_DELETE_BLOCK;
	float timeSpent = ( 1.0f - file->amtDone ) * file->sizeBlocks * TIME_TO_DELETE_BLOCK;
	timeSpent += delta;

	file->amtDone = 1.0f - timeSpent / totalTime;

	if( file->amtDone <= 0.0f ) {
		destroyFile( op->processingFile );
		return true;
	}

	return false;
}

static void gameScreen_PhysicsTick( float delta )
{
	// run all processes
	for( size_t i = 0; i < sb_Count( sbActiveOperations ); ++i ) {
		bool done = false;
		switch( sbActiveOperations[i].type ) {
		case OP_ARCHIVE:
			done = runArchive( &( sbActiveOperations[i] ), delta );
			break;
		case OP_EXPAND:
			done = runExpand( &( sbActiveOperations[i] ), delta );
			break;
		case OP_DELETE:
			done = runDelete( &( sbActiveOperations[i] ), delta );
			break;
		}

		if( done ) {
			sb_Remove( sbActiveOperations, i );
			--i;
		}
	}

	// run all users
	updateUserLogin( delta );
	updateUserRequests( delta );

	updateDifficulty( delta );
}

struct GameState gameScreenState = { gameScreen_Enter, gameScreen_Exit, gameScreen_ProcessEvents,
	gameScreen_Process, gameScreen_Draw, gameScreen_PhysicsTick };