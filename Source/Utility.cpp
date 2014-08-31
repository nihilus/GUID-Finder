
// ****************************************************************************
// File: Utility.cpp
// Desc: Utility functions
//
// ****************************************************************************
#include "stdafx.h"


// ****************************************************************************
// Func: GetTimeSamp()
// Desc: Get elapsed factional seconds
//
// ****************************************************************************
ALIGN(32) TIMESTAMP GetTimeStamp() 
{
	LARGE_INTEGER tLarge;
	QueryPerformanceCounter(&tLarge);

	static ALIGN(16) TIMESTAMP s_ClockFreq;
	if(s_ClockFreq == 0.0)
	{
		LARGE_INTEGER tLarge;
		QueryPerformanceFrequency(&tLarge);
		s_ClockFreq = (TIMESTAMP) tLarge.QuadPart; 
	}
	
	return((TIMESTAMP) tLarge.QuadPart / s_ClockFreq);
}


// ****************************************************************************
// Func: Log()
// Desc: Send text to a log file.
//
// ****************************************************************************
ALIGN(32) void Log(FILE *pLogFile, const char *format, ...)
{
	if(pLogFile && format)
	{
		// Format string
		va_list vl;
        char	str[2048] = {0};

		va_start(vl, format);
		_vsnprintf(str, (sizeof(str) - 1), format, vl);
		va_end(vl);

		// Out to file
		qfputs(str, pLogFile);
        qflush(pLogFile);
	}
}


// Get size of stream file
ALIGN(32) long qfsize(FILE *fp)
{
	long psave, endpos;
	long result = -1;

	if((psave = qftell(fp)) != -1L)
	{
		if(qfseek(fp, 0, SEEK_END) == 0)
		{
			if((endpos = qftell(fp)) != -1L)
			{
				qfseek(fp, psave, SEEK_SET);
				result = endpos;
			}
		}
	}

	return(result);
}

// Common hash function
ALIGN(32) UINT DJBHash(const BYTE *pData, int iSize)
{
	register UINT uHash = 5381;

	for(int i = 0; i < iSize; i++)
	{
		uHash = (((uHash << 5) + uHash) + (UINT) *pData);
		pData++;
	}

	return(uHash);
}