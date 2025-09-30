#include "quakedef.h"
#include "download.h"
#include "vgui_int.h"
#include "client.h"


DownloadManager TheDownloadManager;

//--------------------------------------------------------------------------------------------------------------
static char * CloneString(const char *original)
{
	char *newString = new char[strlen(original) + 1];
	strcpy(newString, original);

	return newString;
}

char *DL_SharedVarArgs(char *format, ...)
{
	va_list list;
	static int curstring;
	static char string[4096];

	va_start(list, format);
	curstring = (curstring + 1) % 4;
	vsnprintf(&string[1024 * curstring], 1024, format, list);
	return &string[1024 * curstring];
}

void DownloadManager::ClearDownloads()
{
	for (int i = 0; i<m_downloadedMaps.Count(); ++i)
	{
		delete[] m_downloadedMaps[i];
	}
	m_downloadedMaps.RemoveAll();
}

//--------------------------------------------------------------------------------------------------------------
void DownloadManager::Reset()
{
	// ask the active request to bail
	// must be m_activeRequest.Tail();
	if (m_activeRequest.Count())
	{
		FOR_EACH_LL(m_activeRequest, head)
		{
			Con_DPrintf(const_cast<char*>("Aborting download of %s\n"), m_activeRequest[head]->gamePath);
			SteamHTTP()->ReleaseHTTPRequest(m_activeRequest[head]->hOpenResource);
			delete m_activeRequest[head];
		}
		m_activeRequest.RemoveAll();
		StopLoadingProgressBar();
	}

	// clear out any queued requests
	for (int i = 0; i<m_queuedRequests.Count(); ++i)
	{
		delete m_queuedRequests[i];
	}
	m_queuedRequests.RemoveAll();

	m_lastPercent = 0;
	m_totalRequests = 0;
}

void DownloadManager::Stop()
{ 
	Reset(); 
}

void DownloadManager::OnHTTPRequestCompleted(HTTPRequestCompleted_t *pCallback)
{
	char filedir[MAX_PATH];
	FileHandle_t file;
	uint32 unBodySize;
	RequestContext *ctx;
	char *pch;

	if (!m_activeRequest.IsValidIndex(pCallback->m_ulContextValue))
		return;

	ctx = m_activeRequest[pCallback->m_ulContextValue];

	if (pCallback->m_bRequestSuccessful && 
		(pCallback->m_eStatusCode == k_EHTTPStatusCode200OK || pCallback->m_eStatusCode == k_EHTTPStatusCode304NotModified) &&
		SteamHTTP()->GetHTTPResponseBodySize(pCallback->m_hRequest, &unBodySize) && unBodySize)
	{
		CUtlBuffer buf;

		buf.EnsureCapacity(unBodySize);
		buf.SeekPut(CUtlBuffer::SeekType_t::SEEK_HEAD, unBodySize);

		SteamHTTP()->GetHTTPResponseBodyData(pCallback->m_hRequest, (byte*)buf.Base(), unBodySize);

		if (!FS_FileExists(ctx->gamePath))
		{
			strncpy(filedir, ctx->gamePath, MAX_PATH);
			COM_FixSlashes(filedir);
			pch = strrchr(filedir, '/');

			if (pch)
			{
				*pch = 0;
				FS_CreateDirHierarchy(filedir, "GAMEDOWNLOAD");
			}
			else
			{
				FS_CreateDirHierarchy("", "GAMEDOWNLOAD");
			}

			file = FS_OpenPathID(ctx->gamePath, "wb", "GAMEDOWNLOAD");
			if (file)
			{
				FS_Write(buf.Base(), unBodySize, 1, file);
				FS_Close(file);
				Con_DPrintf(const_cast<char*>("Saved %s%s to disk\n"), ctx->baseURL, ctx->gamePath);
			}
		}

		m_activeRequest[pCallback->m_ulContextValue]->error = HTTP_ERROR_NONE;
		m_activeRequest[pCallback->m_ulContextValue]->status = HTTP_DONE;

		SteamHTTP()->ReleaseHTTPRequest(pCallback->m_hRequest);
	}
	else
	{
		Con_DPrintf(const_cast<char*>("Failed HTTP download of %s%s.\n"), ctx->baseURL, ctx->gamePath);
		m_activeRequest[pCallback->m_ulContextValue]->error = HTTP_ERROR_API;
		m_activeRequest[pCallback->m_ulContextValue]->status = HTTP_ERROR;
	}
}

void DownloadManager::OnHTTPRequestCompleted(HTTPRequestCompleted_t *pCallback, bool bIOFailure)
{
	if (pCallback)
	{
		if (!bIOFailure)
			OnHTTPRequestCompleted(pCallback);
	}
}

DownloadManager::DownloadManager()
{
	this->m_HTTPRequestCompleted.m_mapAPICalls.SetLessFunc(DefLessFunc(SteamAPICall_t));
	this->m_HTTPRequestCompleted.m_pObj = this;
	this->m_HTTPRequestCompleted.m_Func = &DownloadManager::OnHTTPRequestCompleted;

	this->m_lastPercent = 0;
	this->m_totalRequests = 0;
}

DownloadManager::~DownloadManager()
{
	Reset();

	ClearDownloads();

	for (int i = 0; i < this->m_HTTPRequestCompleted.m_mapAPICalls.MaxElement(); i++)
	{
		if (i != m_HTTPRequestCompleted.m_mapAPICalls.LeftChild(i))
			SteamAPI_UnregisterCallResult(&m_HTTPRequestCompleted, m_HTTPRequestCompleted.m_mapAPICalls.Element(i));
	}

	m_HTTPRequestCompleted.m_mapAPICalls.RemoveAll();

	// остальные блоки с Purge выполняются автоматически
}

//--------------------------------------------------------------------------------------------------------------
bool DownloadManager::HasMapBeenDownloadedFromServer(const char *serverMapName)
{
	if (!serverMapName)
		return false;

	for (int i = 0; i<m_downloadedMaps.Count(); ++i)
	{
		const char *oldServerMapName = m_downloadedMaps[i];
		if (oldServerMapName && !stricmp(serverMapName, oldServerMapName))
		{
			return true;
		}
	}
	return false;
}

//--------------------------------------------------------------------------------------------------------------
// Checks download status, and updates progress bar
void DownloadManager::CheckActiveDownload()
{
	CUtlVector<int> vecDeleteList{};
	int i;
	bool bCompletedRequest;
	float flPercentDone, statusText, flTotalPercent;
	float flNewPercent;

	bCompletedRequest = 0;
	flTotalPercent = 0.0;
	flPercentDone = 0.0;

	FOR_EACH_LL(m_activeRequest, i)
	{
		// check active request for completion / error / progress update
		switch (m_activeRequest[i]->status)
		{
		case HTTP_DONE:
			if (m_activeRequest[i]->hOpenResource)
			{
				SetSecondaryProgressBarText(m_activeRequest[i]->gamePath);
				vecDeleteList.InsertBefore(i);
			}
			bCompletedRequest = true;
			break;
		case HTTP_ERROR:
			vecDeleteList.InsertBefore(i);
			bCompletedRequest = true;
			break;
		case HTTP_FETCH:
			SetSecondaryProgressBarText(m_activeRequest[i]->gamePath);
			if (m_activeRequest[i]->hOpenResource)
			{
				flNewPercent = 0.0;
				SteamHTTP()->GetHTTPDownloadProgressPct(m_activeRequest[i]->hOpenResource, &flNewPercent);
				flTotalPercent = flTotalPercent + 1.0;
				flPercentDone += 0.01 * flNewPercent;
			}
			break;
		}
	}

	if (flTotalPercent > 0.0)
		flPercentDone = flPercentDone / flTotalPercent * 100.0;

	if ((float)m_lastPercent != flPercentDone)
	{
		m_lastPercent = flPercentDone;
		statusText = flPercentDone * 0.01;
		SetSecondaryProgressBar(statusText);
	}

	for (int i = 0; i < vecDeleteList.Size(); i++)
		m_activeRequest.Free(vecDeleteList[i]);

	if (bCompletedRequest && !m_activeRequest.Count() && !m_queuedRequests.Size())
	{
		StopLoadingProgressBar();
		Cbuf_AddText(const_cast<char*>("retry\n"));
	}
}

int DownloadManager::GetQueueSize()
{
	return m_queuedRequests.Count();
}

//--------------------------------------------------------------------------------------------------------------
void DownloadManager::MarkMapAsDownloadedFromServer(const char *serverMapName)
{
	if (!serverMapName)
		return;

	if (HasMapBeenDownloadedFromServer(serverMapName))
		return;

	m_downloadedMaps.AddToTail(CloneString(serverMapName));


	return;
}

void CL_HTTPStop_f()
{
	TheDownloadManager.Stop();
}

void CL_HTTPCancel_f()
{
	TheDownloadManager.Reset();
	TheDownloadManager.ClearDownloads();
}

void CL_QueueHTTPDownload(const char *filename)
{
	TheDownloadManager.Queue(cl.downloadUrl, filename);
}

int CL_GetDownloadQueueSize()
{
	return TheDownloadManager.GetQueueSize();
}

int CL_CanUseHTTPDownload()
{
	if (cl.downloadUrl[0])
	{

		const char *serverMapName = DL_SharedVarArgs(const_cast<char*>("%s:%s"), cls.servername, cl.levelname);

		return !TheDownloadManager.HasMapBeenDownloadedFromServer(serverMapName);
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------------------
void CL_MarkMapAsUsingHTTPDownload(void)
{
	const char *serverMapName = DL_SharedVarArgs(const_cast<char*>("%s:%s"), cls.servername, cl.levelname);
	TheDownloadManager.MarkMapAsDownloadedFromServer(serverMapName);
}

//--------------------------------------------------------------------------------------------------------------
void DownloadManager::Queue(const char *baseURL, const char *gamePath)
{
	if (!strcmp("local", cls.servername))
		return;

	++m_totalRequests;

	RequestContext *rc = new RequestContext;
	m_queuedRequests.AddToTail(rc);

	memset(rc, 0, sizeof(RequestContext));

	rc->status = HTTP_CONNECTING;

	_snprintf(rc->baseURL, BufferSize, "%s", baseURL);
	_snprintf(rc->gamePath, BufferSize, "%s", gamePath);
	_snprintf(rc->serverURL, BufferSize, "%s", NET_AdrToString(cls.netchan.remote_address));

}

//--------------------------------------------------------------------------------------------------------------
// Starts a new download if there are queued requests
void DownloadManager::StartNewDownload()
{
	char fullURL[2048];
	SteamAPICall_t hSteamAPICall;

	if (!m_queuedRequests.Count() || !SteamHTTP())
		return;

	while (m_queuedRequests.Count() && m_activeRequest.Count() <= 4)
	{
		int idx = m_activeRequest.AddToTail(m_queuedRequests[0]);
		m_queuedRequests.Remove(0);

		ContinueLoadingProgressBar("Http", m_totalRequests - m_queuedRequests.Count(), 0.0f);
		SetLoadingProgressBarStatusText("#GameUI_VerifyingAndDownloading");

		SetSecondaryProgressBarText(m_activeRequest[idx]->gamePath);
		SetSecondaryProgressBar(0.0);
		Con_DPrintf(const_cast<char*>("Requesting HTTP download of %s%s.\n"), m_activeRequest[idx]->baseURL, m_activeRequest[idx]->gamePath);
		_snprintf(fullURL, sizeof(fullURL), "%s%s", m_activeRequest[idx]->baseURL, m_activeRequest[idx]->gamePath);
		fullURL[sizeof(fullURL) - 1] = 0;

		COM_FixSlashes(fullURL);

		m_activeRequest[idx]->hOpenResource = SteamHTTP()->CreateHTTPRequest(k_EHTTPMethodGET, fullURL);

		SteamHTTP()->SetHTTPRequestHeaderValue(m_activeRequest[idx]->hOpenResource, "Cache-Control", "no-cache");
		SteamHTTP()->SetHTTPRequestHeaderValue(m_activeRequest[idx]->hOpenResource, "User-Agent", "Half-Life");
		SteamHTTP()->SetHTTPRequestContextValue(m_activeRequest[idx]->hOpenResource, idx | ((uint64)idx >> 31) << 32);

		if (SteamHTTP()->SendHTTPRequest(m_activeRequest[idx]->hOpenResource, &hSteamAPICall))
		{
			m_HTTPRequestCompleted.m_mapAPICalls.Insert(&hSteamAPICall, 1);
			SteamAPI_RegisterCallResult(&m_HTTPRequestCompleted, hSteamAPICall);
			m_activeRequest[idx]->status = HTTP_FETCH;
		}
		else
		{
			SteamHTTP()->ReleaseHTTPRequest(m_activeRequest[idx]->hOpenResource);
			m_activeRequest[idx]->error = HTTP_ERROR_API;
			m_activeRequest[idx]->status = HTTP_ERROR;
		}
	}
}

void DownloadManager::Update()
{
	CheckActiveDownload();
	StartNewDownload();
}

void CL_HTTPUpdate()
{
	TheDownloadManager.Update();
}
