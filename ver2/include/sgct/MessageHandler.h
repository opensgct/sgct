/*************************************************************************
Copyright (c) 2012-2013 Miroslav Andel
All rights reserved.

For conditions of distribution and use, see copyright notice in sgct.h 
*************************************************************************/

#ifndef _MESSAGE_HANDLER
#define _MESSAGE_HANDLER

#include <stddef.h> //get definition for NULL
#include <stdarg.h>
#include <vector>

#define TIME_BUFFER_SIZE 9

namespace sgct //simple graphics cluster toolkit
{

class MessageHandler
{
public:
	/*!
		Different notify levels for messages
	*/
	enum NotifyLevel { NOTIFY_ERROR = 0, NOTIFY_IMPORTANT, NOTIFY_VERSION_INFO, NOTIFY_INFO, NOTIFY_WARNING, NOTIFY_DEBUG, NOTIFY_ALL };
	
	/*! Get the MessageHandler instance */
	static MessageHandler * instance()
	{
		if( mInstance == NULL )
		{
			mInstance = new MessageHandler();
		}

		return mInstance;
	}

	/*! Destroy the MessageHandler */
	static void destroy()
	{
		if( mInstance != NULL )
		{
			delete mInstance;
			mInstance = NULL;
		}
	}

	void decode(const char * receivedData, int receivedlength, int clientIndex);
	void print(const char *fmt, ...);
	void print(NotifyLevel nl, const char *fmt, ...);
    void printDebug(NotifyLevel nl, const char *fmt, ...);
    void printIndent(NotifyLevel nl, unsigned int indentation, const char* fmt, ...);
	void sendMessageToServer(const char *fmt);
    void setSendFeedbackToServer( bool state ) { mLocal = !state; }
    void clearBuffer();
	void setNotifyLevel( NotifyLevel nl );
	NotifyLevel getNotifyLevel();
	void setShowTime( bool state );
	bool getShowTime();
	const char * getTimeOfDayStr();
	inline std::size_t getDataSize() { return mBuffer.size(); }

	char * getMessage();

private:
	MessageHandler(void);
	~MessageHandler(void);

	// Don't implement these, should give compile warning if used
	MessageHandler( const MessageHandler & tm );
	const MessageHandler & operator=(const MessageHandler & rhs );
	void printv(const char *fmt, va_list ap);

private:
	static MessageHandler * mInstance;

	char * mParseBuffer;
	
	NotifyLevel mLevel;
	std::vector<char> mBuffer;
	std::vector<char> mRecBuffer;
	unsigned char  * headerSpace;
	bool mLocal;
	bool mShowTime;
	char mTimeBuffer[TIME_BUFFER_SIZE];
};

}

#endif
