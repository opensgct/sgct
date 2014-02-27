/*************************************************************************
Copyright (c) 2012-2013 Miroslav Andel
All rights reserved.

For conditions of distribution and use, see copyright notice in sgct.h
*************************************************************************/

#include "../include/sgct/NetworkManager.h"
#include "../include/sgct/MessageHandler.h"
#include "../include/sgct/ClusterManager.h"
#include "../include/sgct/SGCTMutexManager.h"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <time.h>

#define MESSAGE_HANDLER_MAX_SIZE 8192
#define COMBINED_MESSAGE_MAX_SIZE 8224 //MESSAGE_HANDLER_MAX_SIZE + 32

sgct::MessageHandler * sgct::MessageHandler::mInstance = NULL;

sgct::MessageHandler::MessageHandler(void)
{
    //nothrow makes sure that a null pointer is returned upon failiure
	mParseBuffer	= new (std::nothrow) char[MESSAGE_HANDLER_MAX_SIZE];
	mCombinedBuffer = new (std::nothrow) char[COMBINED_MESSAGE_MAX_SIZE];
	headerSpace		= new (std::nothrow) unsigned char[ sgct_core::SGCTNetwork::mHeaderSize ];

	if( !headerSpace || !mCombinedBuffer || !headerSpace)
	{
		fprintf(stderr, "Fatal error while allocating memory for MessageHandler!\n");
		return;
	}

#ifdef __SGCT_DEBUG__
	mLevel = NOTIFY_DEBUG;
#else
	mLevel = NOTIFY_WARNING;
#endif

	mRecBuffer.reserve(MESSAGE_HANDLER_MAX_SIZE);
	mBuffer.reserve(MESSAGE_HANDLER_MAX_SIZE);

	for(unsigned int i=0; i<sgct_core::SGCTNetwork::mHeaderSize; i++)
		headerSpace[i] = sgct_core::SGCTNetwork::SyncByte;
	mBuffer.insert(mBuffer.begin(), headerSpace, headerSpace+sgct_core::SGCTNetwork::mHeaderSize);

    mLocal = true;
	mShowTime = true;
	mLogToFile = false;

	setLogPath(NULL);
}

sgct::MessageHandler::~MessageHandler(void)
{
    if(mParseBuffer)
		delete [] mParseBuffer;
    mParseBuffer = NULL;

	if(mCombinedBuffer)
		delete [] mCombinedBuffer;
	mCombinedBuffer = NULL;

	if(headerSpace)
		delete [] headerSpace;
    headerSpace = NULL;

	mBuffer.clear();
	mRecBuffer.clear();
}

void sgct::MessageHandler::decode(const char * receivedData, int receivedlength, int clientIndex)
{
	SGCTMutexManager::instance()->lockMutex( SGCTMutexManager::DataSyncMutex );
		mRecBuffer.clear();
		mRecBuffer.insert(mRecBuffer.end(), receivedData, receivedData + receivedlength);
		mRecBuffer.push_back('\0');
		print("\n[client %d]: %s [end]\n", clientIndex, &mRecBuffer[0]);
    SGCTMutexManager::instance()->unlockMutex( SGCTMutexManager::DataSyncMutex );
}

void sgct::MessageHandler::printv(const char *fmt, va_list ap)
{
	//prevent writing to console simultaneously
	SGCTMutexManager::instance()->lockMutex( SGCTMutexManager::ConsoleMutex );
	mParseBuffer[0] = '\0';

#if (_MSC_VER >= 1400) //visual studio 2005 or later
    vsprintf_s(mParseBuffer, MESSAGE_HANDLER_MAX_SIZE, fmt, ap);	// And Converts Symbols To Actual Numbers
#else
    vsprintf(mParseBuffer, fmt, ap);
#endif
    va_end(ap);		// Results Are Stored In Text

    //print local
	if( getShowTime() )
	{
#if (_MSC_VER >= 1400) //visual studio 2005 or later
		sprintf_s( mCombinedBuffer, COMBINED_MESSAGE_MAX_SIZE, "%s| %s", getTimeOfDayStr(), mParseBuffer );
#else
		sprintf( mCombinedBuffer, "%s| %s", getTimeOfDayStr(), mParseBuffer );
#endif
		std::cerr << mCombinedBuffer;

		if(mLogToFile)
			logToFile( mCombinedBuffer );
	}
	else
	{
		std::cerr << mParseBuffer;
		if(mLogToFile)
			logToFile( mParseBuffer );
	}

	SGCTMutexManager::instance()->unlockMutex( SGCTMutexManager::ConsoleMutex );

    //if client send to server
    sendMessageToServer(mParseBuffer);
}

void sgct::MessageHandler::logToFile(const char * buffer)
{
	FILE* pFile = NULL;
	bool error = false;

#if (_MSC_VER >= 1400) //visual studio 2005 or later
	errno_t err = fopen_s(&pFile, mFileName, "a");
	if( err != 0 || !pFile ) //error
		error = true;
#else
	pFile = fopen(mFileName, "a");
	if( pFile == NULL )
		error = true;
#endif

	if( error )
	{
		std::cerr << "Failed to open '" << mFileName << "'!" << std::endl;
		return;
	}

	fprintf(pFile, "%s", buffer);
	fclose(pFile);
}

/*!
 Set the log file path/directoy. The nodeId is optional and will be appended on the filename if larger than -1.
 */
void sgct::MessageHandler::setLogPath(const char * path, int nodeId)
{
	time_t now = time(NULL);

#if (_MSC_VER >= 1400) //visual studio 2005 or later
	struct tm timeInfo;
	errno_t err = localtime_s(&timeInfo, &now);
	if( err == 0 )
	{
		if( path == NULL )
        {
			char tmpBuff[64];
			strftime(tmpBuff, 64, "SGCT_log_%Y_%m_%d_T%H_%M_%S", &timeInfo);
            if( nodeId > -1 )
                sprintf_s(mFileName, LOG_FILENAME_BUFFER_SIZE, "%s_node%d.txt", tmpBuff, nodeId);
            else
                sprintf_s(mFileName, LOG_FILENAME_BUFFER_SIZE, "%s.txt", tmpBuff);
        }
		else
		{
			char tmpBuff[64];
			strftime(tmpBuff, 64, "SGCT_log_%Y_%m_%d_T%H_%M_%S", &timeInfo);
            if( nodeId > -1 )
                sprintf_s(mFileName, LOG_FILENAME_BUFFER_SIZE, "%s/%s_node%d.txt", path, tmpBuff, nodeId);
            else
                sprintf_s(mFileName, LOG_FILENAME_BUFFER_SIZE, "%s/%s.txt", path, tmpBuff);
		}
	}
#else
	struct tm * timeInfoPtr;
	timeInfoPtr = localtime(&now);
	if( path == NULL )
    {
		char tmpBuff[64];
        strftime(tmpBuff, 64, "SGCT_log_%Y_%m_%d_T%H_%M_%S", timeInfoPtr);
        if( nodeId > -1 )
            sprintf(mFileName, "%s_node%d.txt", tmpBuff, nodeId);
        else
            sprintf(mFileName, "%s.txt", tmpBuff);
    }
	else
	{
		char tmpBuff[64];
		strftime(tmpBuff, 64, "SGCT_log_%Y_%m_%d_T%H_%M_%S", timeInfoPtr);
        if( nodeId > -1 )
            sprintf(mFileName, "%s/%s_node%d.txt", path, tmpBuff, nodeId);
        else
            sprintf(mFileName, "%s/%s.txt", path, tmpBuff);
	}
#endif
}

/*!
	Print messages to command line and share to master for easier debuging on a cluster.
*/
void sgct::MessageHandler::print(const char *fmt, ...)
{
	if ( fmt == NULL )		// If There's No Text
	{
		*mParseBuffer=0;	// Do Nothing
		return;
	}

	va_list		ap;		// Pointer To List Of Arguments
    va_start(ap, fmt);	// Parses The String For Variables
    printv(fmt, ap);
}

/*!
	Print messages to command line and share to master for easier debuging on a cluster.

	\param nl is the notify level of this message
*/
void sgct::MessageHandler::print(NotifyLevel nl, const char *fmt, ...)
{
	if (nl > getNotifyLevel() || fmt == NULL)		// If There's No Text
	{
		*mParseBuffer=0;	// Do Nothing
		return;
	}

	va_list		ap;		// Pointer To List Of Arguments
    va_start(ap, fmt);	// Parses The String For Variables
    printv(fmt, ap);
}

void sgct::MessageHandler::clearBuffer()
{
	SGCTMutexManager::instance()->lockMutex( SGCTMutexManager::DataSyncMutex );
	mBuffer.clear();
	SGCTMutexManager::instance()->unlockMutex( SGCTMutexManager::DataSyncMutex );
}

/*!
Set the notify level for displaying messages\n
This function is mutex protected/thread safe
*/
void sgct::MessageHandler::setNotifyLevel( NotifyLevel nl )
{
	SGCTMutexManager::instance()->lockMutex( SGCTMutexManager::SharedVariableMutex );
	mLevel = nl;
	SGCTMutexManager::instance()->unlockMutex( SGCTMutexManager::SharedVariableMutex );
}

/*!
Get the notify level for displaying messages\n
This function is mutex protected/thread safe
*/
sgct::MessageHandler::NotifyLevel sgct::MessageHandler::getNotifyLevel()
{
	NotifyLevel tmpNL;
	SGCTMutexManager::instance()->lockMutex( SGCTMutexManager::SharedVariableMutex );
	tmpNL = mLevel;
	SGCTMutexManager::instance()->unlockMutex( SGCTMutexManager::SharedVariableMutex );
	return tmpNL;
}

/*!
Set if time of day should be displayed with each print message.\n
This function is mutex protected/thread safe
*/
void sgct::MessageHandler::setShowTime( bool state )
{
	SGCTMutexManager::instance()->lockMutex( SGCTMutexManager::SharedVariableMutex );
	mShowTime = state;
	SGCTMutexManager::instance()->unlockMutex( SGCTMutexManager::SharedVariableMutex );
}

/*!
Get if time of day should be displayed with each print message.\n
This function is mutex protected/thread safe
*/
bool sgct::MessageHandler::getShowTime()
{
	bool tmpBool;
	SGCTMutexManager::instance()->lockMutex( SGCTMutexManager::SharedVariableMutex );
	tmpBool = mShowTime;
	SGCTMutexManager::instance()->unlockMutex( SGCTMutexManager::SharedVariableMutex );
	return tmpBool;
}

/*!
Set if log to file should be enabled
*/
void sgct::MessageHandler::setLogToFile( bool state )
{
	mLogToFile = state;
}

/*!
Get the time of day string
*/
const char * sgct::MessageHandler::getTimeOfDayStr()
{
	time_t now = time(NULL);
#if (_MSC_VER >= 1400) //visual studio 2005 or later
	struct tm timeInfo;
	errno_t err = localtime_s(&timeInfo, &now);
	if( err == 0 ) 
		strftime(mTimeBuffer, TIME_BUFFER_SIZE, "%X", &timeInfo);
#else
	struct tm * timeInfoPtr;
	timeInfoPtr = localtime(&now);
	strftime(mTimeBuffer, TIME_BUFFER_SIZE, "%X", timeInfoPtr);
#endif

	return mTimeBuffer;
}

char * sgct::MessageHandler::getMessage()
{
	return &mBuffer[0];
}

void sgct::MessageHandler::printDebug(NotifyLevel nl, const char *fmt, ...)
{
#ifdef __SGCT_DEBUG__
    if (nl > getNotifyLevel() || fmt == NULL)
    {
        *mParseBuffer = 0;
        return;
    }

	va_list ap;
    va_start(ap, fmt);	// Parses The String For Variables
    printv(fmt, ap);
#endif
}

void sgct::MessageHandler::printIndent(NotifyLevel nl, unsigned int indentation, const char* fmt, ...)
{
    if (nl > getNotifyLevel() || fmt == NULL)
    {
        *mParseBuffer = 0;
        return;
    }

	 va_list ap;

    if (indentation > 0) {
        const std::string padding(indentation, ' ');
        const std::string fmtString = std::string(fmt);
        const std::string fmtComplete = padding + fmtString;

        const char *fmtIndented = fmtComplete.c_str();
        va_start(ap, fmt);	// Parses The String For Variables
        printv(fmtIndented, ap);
    }
    else {
        va_start(ap, fmt);	// Parses The String For Variables
        printv(fmt, ap);
    }
}

void sgct::MessageHandler::sendMessageToServer(const char * str)
{
	if( str == NULL)
		return;

	//if client send to server
    if(!mLocal)
    {
        SGCTMutexManager::instance()->lockMutex( SGCTMutexManager::DataSyncMutex );
        if(mBuffer.empty())
            mBuffer.insert(mBuffer.begin(), headerSpace, headerSpace+sgct_core::SGCTNetwork::mHeaderSize);
        mBuffer.insert(mBuffer.end(), str, str + strlen(str));
        SGCTMutexManager::instance()->unlockMutex( SGCTMutexManager::DataSyncMutex );
    }
}
