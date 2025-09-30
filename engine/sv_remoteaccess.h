#ifndef _SV_REMOTEACCESS_H_
#define _SV_REMOTEACCESS_H_

#include "IGameServerData.h"
#include "utlbuffer.h"
#include "utllinkedlist.h"

class CServerRemoteAccess: public IGameServerData {
public:
	struct DataResponse_t {
		CUtlBuffer packet;
	};

private:
	CUtlLinkedList<DataResponse_t, int> m_ResponsePackets;
	int m_iBytesSent;
	int m_iBytesReceived;

public:
	CServerRemoteAccess();

	virtual ~CServerRemoteAccess() {}

	virtual void WriteDataRequest(const void *buffer, int bufferSize);
	virtual int ReadDataResponse(void *data, int len);

	void SendMessageToAdminUI(const char *message);
	void RequestValue(int requestID, const char *variable);
	void SetValue(const char *variable, const char *value);
	void ExecCommand(const char *cmdString);
	bool LookupValue(const char *variable, CUtlBuffer &value);
	const char* LookupStringValue(const char *variable);
	void GetUserBanList(CUtlBuffer &value);
	void GetPlayerList(CUtlBuffer &value);
	void GetMapList(CUtlBuffer &value);
};

extern CServerRemoteAccess g_ServerRemoteAccess;
extern void NotifyDedicatedServerUI(const char *message);

#endif