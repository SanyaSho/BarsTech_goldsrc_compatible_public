#ifndef _DOWNLOAD_H_
#define _DOWNLOAD_H

#include <UtlVector.h>
#include <UtlRBTree.h>
#include "utlbuffer.h"
#include "utllinkedlist.h"
#include "steam/isteamhttp.h"
#include "steam/steam_api.h"

//--------------------------------------------------------------------------------------------------------------
/**
* Status of the download thread, as set in RequestContext::status.
*/
enum HTTPStatus
{
	HTTP_CONNECTING = 0,///< This is set in the main thread before the download thread starts.
	HTTP_FETCH,			///< The download thread sets this when it starts reading data.
	HTTP_DONE,			///< The download thread sets this if it has read all the data successfully.
	HTTP_ABORTED,		///< The download thread sets this if it aborts because it's RequestContext::shouldStop has been set.
	HTTP_ERROR			///< The download thread sets this if there is an error connecting or downloading.  Partial data may be present, so the main thread can check.
};

//--------------------------------------------------------------------------------------------------------------
/**
* Error encountered in the download thread, as set in RequestContext::error.
*/
enum HTTPError
{
	HTTP_ERROR_NONE = 0,
	HTTP_ERROR_ZERO_LENGTH_FILE,
	HTTP_ERROR_CONNECTION_CLOSED,
	HTTP_ERROR_INVALID_URL,			///< InternetCrackUrl failed
	HTTP_ERROR_INVALID_PROTOCOL,	///< URL didn't start with http:// or https://
	HTTP_ERROR_CANT_BIND_SOCKET,
	HTTP_ERROR_CANT_CONNECT,
	HTTP_ERROR_NO_HEADERS,			///< Cannot read HTTP headers
	HTTP_ERROR_FILE_NONEXISTENT,
	HTTP_ERROR_API,
	HTTP_ERROR_MAX
};

enum { BufferSize = 1024 };

typedef struct {

	HTTPStatus		status;					///< Download thread status
	HTTPError		error;					///< Detailed error info

	char			baseURL[BufferSize];	///< Base URL (including http://).  Set by main thread.
	char			gamePath[BufferSize];	///< Game path to be appended to base URL.  Set by main thread.
	char			serverURL[BufferSize];	///< Server URL (IP:port, loopback, etc).  Set by main thread, and used for HTTP Referer header.

	// Used purely by the download thread - internal data -------------------
	HTTPRequestHandle			hOpenResource;			///< Handle created by InternetOpen

} RequestContext;

// попытка воссоздать шаблон класса 
template<class T, class P>
class CMultipleCallResults : public CCallbackBase
{
public:
	typedef void (T::*func_t)(P*, bool);

	CMultipleCallResults()
	{
		m_pObj = NULL;
		m_Func = NULL;
		m_iCallback = P::k_iCallback;
	}

	void Run(void *pvParam)
	{
		(m_pObj->*m_Func)((P *)pvParam, false);
	}

	void Run(void *pvParam, bool bIOFailure, SteamAPICall_t hSteamAPICall)
	{
		if (m_mapAPICalls.Remove(hSteamAPICall))
		{
			(m_pObj->*m_Func)((P *)pvParam, bIOFailure);
		}
	}

	int GetCallbackSizeBytes()
	{
		return sizeof(P);
	}

//	CCallbackBase baseclass;
	CUtlRBTree<SteamAPICall_t, int> m_mapAPICalls;
	T *m_pObj;
	func_t m_Func;
};

// инкапсуляция?
class DownloadManager
{
public:
	DownloadManager();
	~DownloadManager();

	void OnHTTPRequestCompleted(HTTPRequestCompleted_t *pCallback);
	void OnHTTPRequestCompleted(HTTPRequestCompleted_t *pCallback, bool bIOFailure);

	void Reset();
	void Stop();
	void ClearDownloads();

	bool HasMapBeenDownloadedFromServer(const char *serverMapName);
	void MarkMapAsDownloadedFromServer(const char *serverMapName);

	void CheckActiveDownload();
	int GetQueueSize();

	void Queue(const char *baseURL, const char *gamePath);
	void StartNewDownload();
	void Update();

private:

	// по размерам вроде бьются, так что пусть останется как есть
	CMultipleCallResults<DownloadManager, HTTPRequestCompleted_t> m_HTTPRequestCompleted;	

	typedef CUtlVector< RequestContext * > RequestVector;
	RequestVector m_queuedRequests;		///< these are requests waiting to be spawned
	CUtlLinkedList<RequestContext*, short unsigned int> m_activeRequest;	///< this is the active request being downloaded in another thread

	int m_lastPercent;					///< last percent value the progress bar was updated with (to avoid spamming it)
	int m_totalRequests;				///< Total number of requests (used to set the top progress bar)

	typedef CUtlVector< char * > StrVector;
	StrVector m_downloadedMaps;			///< List of maps for which we have already tried to download assets.
};


void CL_HTTPStop_f();
void CL_HTTPCancel_f();
void CL_QueueHTTPDownload(const char *filename);
int CL_GetDownloadQueueSize();
int CL_CanUseHTTPDownload();
void CL_MarkMapAsUsingHTTPDownload(void);
void CL_HTTPUpdate();

#endif
