#include "Utils.h"

#include <cstdio>
#include <TlHelp32.h>

////////////////////////////////////////////////////////////
//// LOG

static LogStream*	g_pLogStream = NULL;
static LogLevel		g_eLogLevel = E_DEFAULT;

void SetLogStream( LogStream* pStream )
{
	g_pLogStream = pStream;
}

void Log( LogType eType, const char* sMessage... )
{
	if( g_eLogLevel == E_DEFAULT && eType >= E_LOG_INFO_VERBOSE )
		return;
	char sFormatedMessage[ 4096 ];

	va_list oArgs;
	va_start( oArgs, sMessage );
	vsprintf_s( sFormatedMessage, 4096 - 1, sMessage, oArgs );
	sFormatedMessage[ 4096 - 1 ] = '\0';
	va_end( oArgs );

	if( g_pLogStream )
		g_pLogStream->Log( eType, sFormatedMessage );
	else
		fprintf( stderr, "%s", sFormatedMessage );
}


void LogWithLocation( LogType eType, const char* sFilename, int iLine, const char* sMessage... )
{
	if( g_eLogLevel == E_DEFAULT && eType >= E_LOG_INFO_VERBOSE )
		return;

	char sFormatedMessage[ 4096 ];

	va_list oArgs;
	va_start( oArgs, sMessage );
	int iBufferSize = vsprintf_s( sFormatedMessage, 4096 - 1, sMessage, oArgs );
	sFormatedMessage[ 4096 - 1 ] = '\0';
	va_end( oArgs );

	sprintf_s( sFormatedMessage + iBufferSize, 4096 - 1 - iBufferSize, "File %s, L.%d\n", sFilename, iLine );
	sFormatedMessage[ 4096 - 1 ] = '\0';

	if( g_pLogStream )
		g_pLogStream->Log( eType, sFormatedMessage );
	else
		fprintf( stderr, "%s", sFormatedMessage );
}

void SetLogLevel( LogLevel eLevel )
{
	g_eLogLevel = eLevel;
}

LogLevel GetLogLevel()
{
	return g_eLogLevel;
}