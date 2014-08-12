#include "common.h"

CToxProto::CToxProto(const char* protoName, const TCHAR* userName) :
	PROTO<CToxProto>(protoName, userName)
{
	tox = tox_new(TOX_ENABLE_IPV6_DEFAULT);

	ptrA dataPath(getStringA("DataPath"));
	if (dataPath)
	{
		LoadToxData(dataPath);
	}

	tox_callback_friend_request(tox, OnFriendRequest, this);
	tox_callback_friend_message(tox, OnFriendMessage, this);
	tox_callback_friend_action(tox, OnAction, this);
	tox_callback_name_change(tox, OnFriendNameChange, this);
	tox_callback_status_message(tox, OnStatusMessageChanged, this);
	tox_callback_user_status(tox, OnUserStatusChanged, this);
	tox_callback_read_receipt(tox, OnReadReceipt, this);
	tox_callback_connection_status(tox, OnConnectionStatusChanged, this);

	CreateProtoService(PS_CREATEACCMGRUI, &CToxProto::OnAccountManagerInit);
}

CToxProto::~CToxProto()
{
	ptrA dataPath(getStringA("DataPath"));
	if (dataPath)
	{
		SaveToxData(dataPath);
	}

	tox_kill(tox);
}

MCONTACT __cdecl CToxProto::AddToList(int flags, PROTOSEARCHRESULT* psr)
{
//	MCONTACT hContact = AddContact(psr->id, psr->nick);

	return NULL;
}

MCONTACT __cdecl CToxProto::AddToListByEvent(int flags, int iContact, HANDLE hDbEvent) { return 0; }

int __cdecl CToxProto::Authorize(HANDLE hDbEvent) { return 0; }
int __cdecl CToxProto::AuthDeny(HANDLE hDbEvent, const PROTOCHAR* szReason) { return 0; }
int __cdecl CToxProto::AuthRecv(MCONTACT hContact, PROTORECVEVENT*) { return 0; }
int __cdecl CToxProto::AuthRequest(MCONTACT hContact, const PROTOCHAR* szMessage) { return 0; }

HANDLE __cdecl CToxProto::ChangeInfo(int iInfoType, void* pInfoData) { return 0; }

HANDLE __cdecl CToxProto::FileAllow(MCONTACT hContact, HANDLE hTransfer, const PROTOCHAR* szPath) { return 0; }
int __cdecl CToxProto::FileCancel(MCONTACT hContact, HANDLE hTransfer) { return 0; }
int __cdecl CToxProto::FileDeny(MCONTACT hContact, HANDLE hTransfer, const PROTOCHAR* szReason) { return 0; }
int __cdecl CToxProto::FileResume(HANDLE hTransfer, int* action, const PROTOCHAR** szFilename) { return 0; }

DWORD_PTR __cdecl CToxProto::GetCaps(int type, MCONTACT hContact)
{
	switch(type)
	{
	case PFLAGNUM_1:
		return PF1_IM | PF1_AUTHREQ | PF1_BASICSEARCH | PF1_ADDSEARCHRES;
	case PFLAGNUM_2:
		return PF2_ONLINE | PF2_SHORTAWAY | PF2_LIGHTDND;
	case PFLAGNUM_4:
		return PF4_IMSENDUTF | PF4_SUPPORTTYPING;
	case PFLAG_UNIQUEIDTEXT:
		return (INT_PTR)"Tox ID";
	case PFLAG_UNIQUEIDSETTING:
		return (DWORD_PTR)TOX_SETTING_ID;
	case PFLAG_MAXLENOFMESSAGE:
		return TOX_MAX_MESSAGE_LENGTH;
	}

	return 0;
}
int __cdecl CToxProto::GetInfo(MCONTACT hContact, int infoType) { return 0; }

HANDLE __cdecl CToxProto::SearchBasic(const PROTOCHAR* id)
{
	//if ( !IsOnline()) return 0;

	this->ForkThread(&CToxProto::SearchByUidAsync, (void*)id);

	return (HANDLE)TOX_SEARCH_BYUID;
}

HANDLE __cdecl CToxProto::SearchByEmail(const PROTOCHAR* email) { return 0; }
HANDLE __cdecl CToxProto::SearchByName(const PROTOCHAR* nick, const PROTOCHAR* firstName, const PROTOCHAR* lastName) { return 0; }
HWND __cdecl CToxProto::SearchAdvanced(HWND owner) { return 0; }
HWND __cdecl CToxProto::CreateExtendedSearchUI(HWND owner) { return 0; }

int __cdecl CToxProto::RecvContacts(MCONTACT hContact, PROTORECVEVENT*) { return 0; }
int __cdecl CToxProto::RecvFile(MCONTACT hContact, PROTOFILEEVENT*) { return 0; }
int __cdecl CToxProto::RecvMsg(MCONTACT hContact, PROTORECVEVENT*) { return 0; }
int __cdecl CToxProto::RecvUrl(MCONTACT hContact, PROTORECVEVENT*) { return 0; }

int __cdecl CToxProto::SendContacts(MCONTACT hContact, int flags, int nContacts, MCONTACT* hContactsList) { return 0; }
HANDLE __cdecl CToxProto::SendFile(MCONTACT hContact, const PROTOCHAR* szDescription, PROTOCHAR** ppszFiles) { return 0; }

int __cdecl CToxProto::SendMsg(MCONTACT hContact, int flags, const char* msg)
{
	if (!IsOnline())
	{
		return 1;
	}

	std::string toxId(getStringA(hContact, TOX_SETTING_ID));
	std::vector<uint8_t> clientId = HexStringToData(toxId);

	uint32_t number = tox_get_friend_number(tox, clientId.data());
	
	int messageId = tox_send_message(tox, number, (uint8_t*)msg, strlen(msg));
	
	return messageId;
}

int __cdecl CToxProto::SendUrl(MCONTACT hContact, int flags, const char* url) { return 0; }

int __cdecl CToxProto::SetApparentMode(MCONTACT hContact, int mode) { return 0; }

int __cdecl CToxProto::SetStatus(int iNewStatus)
{
	if (iNewStatus == this->m_iDesiredStatus)
		return 0;

	int old_status = this->m_iStatus;
	this->m_iDesiredStatus = iNewStatus;

	if (iNewStatus == ID_STATUS_OFFLINE)
	{
		// logout
		isTerminated = true;
		m_iStatus = m_iDesiredStatus = ID_STATUS_OFFLINE;

		ProtoBroadcastAck(NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)old_status, m_iStatus);

		return 0;
	}
	else
	{
		if (old_status == ID_STATUS_OFFLINE/* && !this->IsOnline()*/)
		{
			m_iStatus = ID_STATUS_CONNECTING;

			connectionThread = ForkThreadEx(&CToxProto::ConnectionThread, 0, NULL);
		}
		else
		{
			// set tox status
			tox_set_user_status(tox, MirandaToToxStatus(iNewStatus));

			ProtoBroadcastAck(NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)old_status, m_iStatus);

			return 0;
		}
	}

	ProtoBroadcastAck(NULL, ACKTYPE_STATUS, ACKRESULT_SUCCESS, (HANDLE)old_status, m_iStatus);

	return 0;
}

HANDLE __cdecl CToxProto::GetAwayMsg(MCONTACT hContact) { return 0; }
int __cdecl CToxProto::RecvAwayMsg(MCONTACT hContact, int mode, PROTORECVEVENT* evt) { return 0; }
int __cdecl CToxProto::SetAwayMsg(int iStatus, const PROTOCHAR* msg) { return 0; }

int __cdecl CToxProto::UserIsTyping(MCONTACT hContact, int type) { return 0; }

int __cdecl CToxProto::OnEvent(PROTOEVENTTYPE iEventType, WPARAM wParam, LPARAM lParam)
{
	switch (iEventType)
	{
	case EV_PROTO_ONLOAD:
		return OnModulesLoaded(wParam, lParam);
	}

	return 1;
}