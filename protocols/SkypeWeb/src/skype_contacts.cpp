/*
Copyright (c) 2015 Miranda NG project (http://miranda-ng.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "common.h"

WORD CSkypeProto::GetContactStatus(MCONTACT hContact)
{
	return getWord(hContact, "Status", ID_STATUS_OFFLINE);
}

void CSkypeProto::SetContactStatus(MCONTACT hContact, WORD status)
{
	WORD oldStatus = GetContactStatus(hContact);
	if (oldStatus != status)
	{
		setWord(hContact, "Status", status);
		if (status == ID_STATUS_OFFLINE)
			db_unset(hContact, m_szModuleName, "MirVer");
	}
}

void CSkypeProto::SetAllContactsStatus(WORD status)
{
	for (MCONTACT hContact = db_find_first(m_szModuleName); hContact; hContact = db_find_next(hContact, m_szModuleName))
		SetContactStatus(hContact, status);
}

MCONTACT CSkypeProto::GetContactFromAuthEvent(MEVENT hEvent)
{
	DWORD body[3];
	DBEVENTINFO dbei = { sizeof(DBEVENTINFO) };
	dbei.cbBlob = sizeof(DWORD) * 2;
	dbei.pBlob = (PBYTE)&body;

	if (db_event_get(hEvent, &dbei))
		return INVALID_CONTACT_ID;

	if (dbei.eventType != EVENTTYPE_AUTHREQUEST)
		return INVALID_CONTACT_ID;

	if (strcmp(dbei.szModule, m_szModuleName) != 0)
		return INVALID_CONTACT_ID;
	return DbGetAuthEventContact(&dbei);
}

MCONTACT CSkypeProto::FindContact(const char *skypename)
{
	MCONTACT hContact = NULL;
	for (hContact = db_find_first(m_szModuleName); hContact; hContact = db_find_next(hContact, m_szModuleName))
	{
		ptrA cSkypename(getStringA(hContact, SKYPE_SETTINGS_ID));
		if (mir_strcmpi(skypename, cSkypename) == 0)
			break;
	}
	return hContact;
}

MCONTACT CSkypeProto::AddContact(const char *skypename, bool isTemporary)
{
	MCONTACT hContact = FindContact(skypename);
	if (!hContact)
	{
		hContact = (MCONTACT)CallService(MS_DB_CONTACT_ADD, 0, 0);
		CallService(MS_PROTO_ADDTOCONTACT, hContact, (LPARAM)m_szModuleName);

		setString(hContact, SKYPE_SETTINGS_ID, skypename);

		DBVARIANT dbv;
		if (!getTString(SKYPE_SETTINGS_GROUP, &dbv))
		{
			db_set_ts(hContact, "CList", "Group", dbv.ptszVal);
			db_free(&dbv);
		}

		setByte(hContact, "Auth", 1);
		setByte(hContact, "Grant", 1);

		if (isTemporary)
			db_set_b(hContact, "CList", "NotOnList", 1);
	}
	return hContact;
}

void CSkypeProto::LoadContactsAuth(const NETLIBHTTPREQUEST *response)
{
	if (response == NULL)
		return;

	
	JSONROOT root(response->pData);
	if (root == NULL)
		return;

	JSONNODE *items = json_as_array(root), *item, *node;
	for (size_t i = 0; i < json_size(items); i++)
	{
		item = json_at(items, i);
		if (item == NULL)
			break;

		node = json_get(item, "sender");
		ptrA skypename(mir_t2a(ptrT(json_as_string(node))));

		JSONNODE *node = json_get(root, "greeting");
		CMStringA reason = ptrA(mir_t2a(ptrT(json_as_string(node))));

		if (reason == "null")
			reason.Empty();

		MCONTACT hContact = AddContact(skypename);
		if (hContact)
		{
			delSetting(hContact, "Auth");

			PROTORECVEVENT pre = { 0 };
			pre.flags = PREF_UTF;
			pre.timestamp = time(NULL);
			pre.lParam = (DWORD)(sizeof(DWORD) * 2 + mir_strlen(skypename) + reason.GetLength() + 5);

			/*blob is: 0(DWORD), hContact(DWORD), nick(ASCIIZ), firstName(ASCIIZ), lastName(ASCIIZ), id(ASCIIZ), reason(ASCIIZ)*/
			PBYTE pBlob, pCurBlob;
			pCurBlob = pBlob = (PBYTE)mir_calloc(pre.lParam);

			*((PDWORD)pCurBlob) = 0;
			pCurBlob += sizeof(DWORD);
			*((PDWORD)pCurBlob) = (DWORD)hContact;
			pCurBlob += sizeof(DWORD);
			pCurBlob += 3;
			mir_strcpy((char*)pCurBlob, skypename);
			pCurBlob += mir_strlen(skypename) + 1;
			mir_strcpy((char*)pCurBlob, reason);
			pre.szMessage = (char*)pBlob;

			ProtoChainRecv(hContact, PSR_AUTH, 0, (LPARAM)&pre);
		}
	}
	json_delete(items);
}

//[{"username":"echo123", "firstname" : "Echo \/ Sound Test Service", "lastname" : null, "avatarUrl" : null, "mood" : null, "richMood" : null, "displayname" : null, "country" : null, "city" : null},...]
void CSkypeProto::LoadContactsInfo(const NETLIBHTTPREQUEST *response)
{
	if (response == NULL)
		return;

	JSONROOT root(response->pData);
	if (root == NULL)
		return;

	JSONNODE *items = json_as_array(root), *item, *node;
	for (size_t i = 0; i < json_size(items); i++)
	{
		item = json_at(items, i);
		if (item == NULL)
			break;

		node = json_get(item, "username");
		ptrA skypename(mir_t2a(ptrT(json_as_string(node))));
		MCONTACT hContact = AddContact(skypename);
		if (hContact)
		{
			UpdateProfileFirstName(item, hContact);
			UpdateProfileLastName(item, hContact);
			UpdateProfileDisplayName(item, hContact);
			UpdateProfileCountry(item, hContact);
			UpdateProfileCity(item, hContact);
			UpdateProfileStatusMessage(item, hContact);
			UpdateProfileAvatar(item, hContact);
		}
	}
	json_delete(items);
}

//[{"skypename":"echo123", "authorized" : true, "blocked" : false, ...},...]
// other properties is exists but empty
void CSkypeProto::LoadContactList(const NETLIBHTTPREQUEST *response)
{
	if (response == NULL)
		return;

	JSONROOT root(response->pData);
	if (root == NULL)
		return;

	LIST<char> skypenames(1);

	JSONNODE *items = json_as_array(root), *item, *node;
	for (size_t i = 0; i < json_size(items); i++)
	{
		item = json_at(items, i);
		if (item == NULL)
			break;

		node = json_get(item, "skypename");
		ptrA skypename(mir_t2a(ptrT(json_as_string(node))));
		MCONTACT hContact = AddContact(skypename);
		if (hContact)
		{
			node = json_get(item, "authorized");
			if (json_as_bool(node))
			{
				delSetting(hContact, "Auth");
				delSetting(hContact, "Grant");
			}
			else
				setByte(hContact, "Grant", 1);

			node = json_get(item, "blocked");
			bool isBlocked = json_as_bool(node);
			if (isBlocked)
			{
				db_set_dw(hContact, "Ignore", "Mask1", 127);
				db_set_b(hContact, "CList", "Hidden", 1);
				setByte(hContact, "IsBlocked", 1);
			}

			skypenames.insert(mir_strdup(skypename));
		}
	}
	json_delete(items);

	ptrA token(getStringA("TokenSecret"));
	if (skypenames.getCount() > 0)
	{
		PushRequest(new GetContactsInfoRequest(token, skypenames), &CSkypeProto::LoadContactsInfo);

		for (int i = 0; i < skypenames.getCount(); i++)
			mir_free(skypenames[i]);
		skypenames.destroy();
	}
	PushRequest(new GetContactsAuthRequest(token), &CSkypeProto::LoadContactsAuth);
}

INT_PTR CSkypeProto::OnRequestAuth(WPARAM hContact, LPARAM)
{
	if (hContact == INVALID_CONTACT_ID)
		return 1;

	ptrA token(getStringA("TokenSecret"));
	ptrA skypename(getStringA(hContact, SKYPE_SETTINGS_ID));
	PushRequest(new AddContactRequest(token, skypename));
	return 0;
}

INT_PTR CSkypeProto::OnGrantAuth(WPARAM hContact, LPARAM)
{
	if (hContact == INVALID_CONTACT_ID)
		return 1;

	ptrA token(getStringA("TokenSecret"));
	ptrA skypename(getStringA(hContact, SKYPE_SETTINGS_ID));
	PushRequest(new AuthAcceptRequest(token, skypename));
	return 0;
}

int CSkypeProto::OnContactDeleted(MCONTACT hContact, LPARAM)
{
	if (hContact && !isChatRoom(hContact))
		PushRequest(new DeleteContactRequest(TokenSecret, db_get_sa(hContact, m_szModuleName, SKYPE_SETTINGS_ID)));
	return 0;
}

INT_PTR CSkypeProto::BlockContact(WPARAM hContact, LPARAM)
{
	if (IDYES == MessageBox(NULL, TranslateT("Are you sure?"), TranslateT("Warning"), MB_YESNOCANCEL | MB_ICONQUESTION))
		SendRequest(new BlockContactRequest(TokenSecret, ptrA(db_get_sa(hContact, m_szModuleName, SKYPE_SETTINGS_ID))), &CSkypeProto::OnBlockContact, (void *)hContact);
	return 0;
}

void CSkypeProto::OnBlockContact(const NETLIBHTTPREQUEST *response, void *p)
{
	MCONTACT hContact = (MCONTACT)p;
	if (response == NULL) 
		return;
	db_set_dw(hContact, "Ignore", "Mask1", 127);
	db_set_b(hContact, "CList", "Hidden", 1);
}

INT_PTR CSkypeProto::UnblockContact(WPARAM hContact, LPARAM)
{
	SendRequest(new UnblockContactRequest(TokenSecret, ptrA(db_get_sa(hContact, m_szModuleName, SKYPE_SETTINGS_ID))), &CSkypeProto::OnUnblockContact, (void *)hContact);
	return 0;
}

void CSkypeProto::OnUnblockContact(const NETLIBHTTPREQUEST *response, void *p)
{
	MCONTACT hContact = (MCONTACT)p;
	if (response == NULL) 
		return;
	db_set_dw(hContact, "Ignore", "Mask1", 0);
	db_set_b(hContact, "CList", "Hidden", 0);
	db_set_b(hContact, m_szModuleName, "IsBlocked", 0);
}