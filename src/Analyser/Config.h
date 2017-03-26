#pragma once

#ifdef DEBUG
#define AN_ASSERT( expr ) { if( (expr) == false ) (*((char*)0) = 0); }
#else
#define AN_ASSERT( expr )
#endif // DEBUG


typedef char		int8;
typedef short		int16;
typedef int			int32;
typedef long long	int64;

typedef unsigned char		uint8;
typedef unsigned short		uint16;
typedef unsigned int		uint32;
typedef unsigned long long	uint64;

#ifdef _WIN64
typedef uint64		ptr_int;
#else
typedef uint32		ptr_int;
#endif
