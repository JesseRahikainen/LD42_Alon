#ifndef MATH_UTIL_H
#define MATH_UTIL_H

#include "vector2.h"
#include "vector3.h"
#include <stdint.h>
#include <stdbool.h>

#define M_PI_F 3.14159265359f
#define M_TWO_PI_F ( 2.0f * M_PI_F )

#define DEG_TO_RAD( d ) ( ( ( d ) * ( M_PI_F / 180.0f ) ) )
#define RAD_TO_DEG( d ) ( ( ( d ) * ( 180.0f / M_PI_F ) ) )

#define MIN( a, b ) ( ((a)>(b))?(b):(a) )
#define MAX( a, b ) ( ((a)>(b))?(a):(b) )

int isPowerOfTwo( int x );
float lerp( float from, float to, float t );

float radianRotLerp( float from, float to, float t );
float degreeRotLerp( float from, float to, float t );
float degreeRotDiff( float from, float to );
float degreeRotWrap( float deg );
float spineDegRotToEngineDegRot( float spineDeg );
float engineDegRotToSpineDegRot( float engineDeg );

uint8_t lerp_uint8_t( uint8_t from, uint8_t to, float t );
float inverseLerp( float from, float to, float val );
float clamp( float min, float max, float val );
float randFloat( float min, float max );
float randFloatVar( float mid, float var );
float sign( float val );
Vector3* vec2ToVec3( const Vector2* vec2, float z, Vector3* out );
float jerkLerp( float t );
void closestPtToSegment( const Vector2* segOne, const Vector2* segTwo, const Vector2* pos, Vector2* outPos, float* outParam );
float sqDistPointSegment( const Vector2* segOne, const Vector2* segTwo, const Vector2* pos );
float wrap( float min, float max, float val );
float signed2DTriArea( const Vector2* a, const Vector2* b, const Vector2* c );

#endif // inclusion guard