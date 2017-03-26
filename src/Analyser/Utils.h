#pragma once

#include <Windows.h>
#include "Config.h"

////////////////////////////////////////////////////////////
//// LOG

enum LogType
{
	E_LOG_INFO				= 0,
	E_LOG_WARNING			= 1,
	E_LOG_ERROR				= 2,
	E_LOG_INFO_VERBOSE		= 3,
	E_LOG_WARNING_VERBOSE	= 4,
	E_LOG_ERROR_VERBOSE		= 5
};

enum LogLevel
{
	E_DEFAULT = 0,
	E_VERBOSE = 1
};

struct LogStream
{
	virtual void Log( LogType eLevel, const char* const ) = 0;
};

void SetLogStream( LogStream* pStream );
void Log( LogType eLevel, const char* sMessage... );
void LogWithLocation( LogType eLevel, const char* sFilename, int iLine, const char* sMessage... );
void SetLogLevel( LogLevel eLevel );
LogLevel GetLogLevel();

#define LOG_ERROR(msg, ...) LogWithLocation( E_LOG_ERROR, __FILE__, __LINE__, msg, __VA_ARGS__ );

