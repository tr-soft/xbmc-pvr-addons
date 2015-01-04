/*
 *      Copyright (C) 2011 Pulse-Eight
 *      http://www.pulse-eight.com/
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301  USA
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <cstdlib>
#include <cstdio>

#include "tinyxml/XMLUtils.h"
#include "PVRData.h"
#include "DialogLogin.h"
#include "client.h"
#include "utils.h"
#include "./uri.h"

using namespace std;
using namespace ADDON;
using namespace PLATFORM;

#ifdef TARGET_WINDOWS
#pragma warning(disable:4005) // Disable "warning C4005: '_WINSOCKAPI_' : macro redefinition"
#include <winsock2.h>
#pragma warning(default:4005)
#define close(s) closesocket(s) 
#else
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#endif

#define SEND_RQ(MSG) \
	/*cout<<send_str;*/ \
	send(sock,MSG,strlen(MSG),0);

PLATFORM::CMutex communication_mutex;

/*
API
- /api/epg?time=2014-01-01%2013:00&detail=1&duration=900&channels=nova
- /api/playlist
- /api/event-timeshift?eventId=...[&format=m3u8]
- /api/get-pvr
- /api/record-event?eventId=...
- /api/delete-record?recordId=...
- /api/time/

*/

unsigned int GetStringFNV1Hash32(std::string name)
{
	unsigned int result = 2166136261; // FNV_offset_basis
	for (int i = 0; i < name.length(); i++)
	{
		result *= 16777619; // FNV_Prime
		result = result ^ name[i];
	}
	return (result);
}

int PVRData::RPC(const std::string& command, const std::string& arguments, std::string& json_response)
{
	PLATFORM::CLockObject critsec(communication_mutex);
	std::string url = g_szBaseURL + command;
	int retval = 1;
	void* hFile = XBMC->OpenFile(url.c_str(), 0);
	if (hFile != NULL)
	{
		std::string result;
		result.clear();
		char buffer[1024];
		while (XBMC->ReadFileString(hFile, buffer, 1023))
			result.append(buffer);
		json_response = result;
		retval = 0;

		XBMC->CloseFile(hFile);
	}
	else
		XBMC->Log(LOG_ERROR, "can not open %s for write", url.c_str());
	return retval;
}

int PVRData::JSONRPC(const std::string& command, const std::string& arguments, Json::Value& json_response)
{
	std::string response, cmd = command;
	int retval = 1;

	if (g_szPhpSession != "")
		cmd += "?PHPSESSID=" + g_szPhpSession;

	if (arguments.length() > 0)
	{
		if (cmd.find("?") == std::string::npos)
			cmd += "?";
		else
			cmd += "&";
		cmd += arguments;
	}
	retval = RPC(cmd, "", response);

	if (retval == 0)
	{
		if (response.length() != 0)
		{
			Json::Reader reader;

			bool parsingSuccessful = reader.parse(response, json_response);

			if (!parsingSuccessful)
			{
				XBMC->Log(LOG_DEBUG, "Failed to parse %s: \n%s\n",
					response.c_str(),
					reader.getFormatedErrorMessages().c_str());
				return 1;
			}
		}
		else
		{
			XBMC->Log(LOG_DEBUG, "Empty response");
			return 2;
		}
	}

	return retval;
}

bool PVRData::LoadPersistentData(void)
{
	TiXmlDocument xmlDoc;

	string strSettingsFile = GetSettingsFile("persistent.xml", true);

	if (!xmlDoc.LoadFile(strSettingsFile))
	{
		XBMC->Log(LOG_ERROR, "invalid demo data (no/invalid data file found at '%s')", strSettingsFile.c_str());
		return false;
	}

	TiXmlElement *pRootElement = xmlDoc.RootElement();
	if (pRootElement)
	{
		if (strcmp(pRootElement->Value(), "settings") != 0)
		{
			XBMC->Log(LOG_ERROR, "invalid demo data (no <demo> tag found)");
			return false;
		}

		/* load channels */
		TiXmlNode *pNode = NULL;
		while ((pNode = pRootElement->IterateChildren(pNode)) != NULL)
		{
			CStdString strTmp1, strTmp2;

			if (pNode->Type() == TiXmlNode::TINYXML_ELEMENT)
			{
				TiXmlElement *e = (TiXmlElement*)pNode;
				std::string sName = e->Attribute("name");
				std::string sValue = e->Attribute("value");

				if (sName == "registereddeviceid")
					g_szRegisteredDeviceId = sValue;
				else if (sName == "registeredpassword")
					g_szRegisteredPassword = sValue;
			}
		}
	}
	return (true);
}

bool PVRData::SavePersistentData(void)
{
	TiXmlDocument xmlDoc;

	string strSettingsFile = GetSettingsFile("persistent.xml", true);
	if (!XBMC->DirectoryExists(g_strUserPath.c_str()))
		XBMC->CreateDirectory(g_strUserPath.c_str());

	TiXmlDeclaration * decl = new TiXmlDeclaration("1.0", "", "");
	if (decl)
	{
		xmlDoc.LinkEndChild(decl);

		TiXmlElement *root = new TiXmlElement("settings");
		if (root)
		{
			xmlDoc.LinkEndChild(root);

			TiXmlElement *e = new TiXmlElement("setting");
			if (e)
			{
				e->SetAttribute("name", "registereddeviceid");
				e->SetAttribute("value", g_szRegisteredDeviceId);
			}
			root->LinkEndChild(e);
			
			e = new TiXmlElement("setting");
			if (e)
			{
				e->SetAttribute("name", "registeredpassword");
				e->SetAttribute("value", g_szRegisteredPassword);
			}
			root->LinkEndChild(e);
		}
		xmlDoc.SaveFile(strSettingsFile);
		return (true);
	}

	return (false);
}

int PVRData::FillChannels(void)
{
	CLockObject lock(m_channelsLock);

	m_channels.clear();

	Json::Value channels;
	if (ApiCall("playlist", "", channels))
	{
		if (channels.isObject() && (!channels.isNull()))
		{
			Json::Value channelsArray = channels["channels"];
			if (channelsArray.isArray())
			{
				for (int i = 0; i < (int)channelsArray.size(); i++)
				{
					Json::Value channel = channelsArray[i];
					if (channel.isObject() && (!channel.isNull()))
					{
						PVRChannel pvrChannel;
						pvrChannel.iChannelNumber = i + 1;
						pvrChannel.strChannelId = channel["id"].asString();
						pvrChannel.iUniqueId = i + 1;
						pvrChannel.strChannelName = channel["name"].asString();
						pvrChannel.bRadio = channel["type"].asString() == "radio";
						pvrChannel.strIconPath = channel["logoUrl"].asString();
						pvrChannel.strStreamURL = channel["url"].asString();

						m_channels.push_back(pvrChannel);
					}
				}
			}
		}
	}
	return (0);
}

bool PVRData::FillEpgForChannels(time_t iStart, time_t iEnd)
{
	time_t iLastEndTime = m_iEpgStart + 1;

	for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
	{
		PVRChannel &myChannel = m_channels.at(iChannelPtr);
		myChannel.epg.clear();
	}

	bool bFirstRun = true;
	int iUniqueId = 1;
	while (iStart < iEnd)
	{
		struct tm *tm = localtime(&iStart);
		char tmpBuff[64];
		std::sprintf(tmpBuff, "%04d-%02d-%02d%%20%02d:%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);

		std::string startTime = tmpBuff;

		int iDuration = (iEnd - iStart) / 60; // total minutes of EPG
		if (iDuration > 600)
			iDuration = 600;
		iStart += iDuration * 60;

		std::sprintf(tmpBuff, "%d", iDuration);
		std::string duration = tmpBuff;

		int iAddition = 0;
		Json::Value wholeEpg;
		if (ApiCall("epg", "time=" + startTime + "&duration=" + duration + "&detail=1", wholeEpg))
		{
			Json::Value &channels = wholeEpg["channels"];
			if (channels.isObject() && (!channels.isNull()))
			{
				for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
				{
					PVRChannel &myChannel = m_channels.at(iChannelPtr);

					Json::Value &chnl = channels[myChannel.strChannelId];
					if (chnl.isArray() && (!chnl.isNull()))
					{
						for (int i = bFirstRun ? 0 : 1; (i < (int)chnl.size()) && (iLastEndTime < iEnd); i++)
						{
							Json::Value &entry = chnl[i];
							if (entry.isObject() && (!entry.isNull()))
							{
								PVREpgEntry tag;

								struct tm tm;
								memset(&tm, 0, sizeof(struct tm));

								std::sscanf(entry["startTime"].asCString(), "%d-%d-%d %d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min);
								tm.tm_year -= 1900;
								tm.tm_mon--;
								tm.tm_sec = 0;
								tag.startTime = mktime(&tm);

								std::sscanf(entry["endTime"].asCString(), "%d-%d-%d %d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min);
								tm.tm_year -= 1900;
								tm.tm_mon--;
								tm.tm_sec = 0;
								tag.endTime = mktime(&tm);

								if (myChannel.epg.size() > 0)
								{
									PVREpgEntry &last = myChannel.epg[myChannel.epg.size() - 1];
									if (last.endTime < tag.startTime)
									{
										// dira v EPG
										PVREpgEntry tmp;

										tmp.startTime = last.endTime;
										tmp.endTime = tag.startTime;
										tmp.iBroadcastId = iUniqueId++;
										tmp.strTitle = "<no programme>";
										tmp.iChannelId = myChannel.iChannelNumber;
										tmp.strPlotOutline = "";
										tmp.strPlot = "";
										tmp.strIconPath = "";
										tmp.iGenreType = 0;
										tmp.iGenreSubType = 0;

										myChannel.epg.push_back(tmp);
									}
								}

								tag.strEventId = entry["eventId"].asString();
								//tag.iBroadcastId = iUniqueId++;
								tag.iBroadcastId = GetStringFNV1Hash32(tag.strEventId);
								tag.strTitle = entry["title"].asString();
								tag.iChannelId = myChannel.iChannelNumber;
								tag.strPlotOutline = "<unread>";
								tag.strPlot = entry["description"].asString();
								tag.strIconPath = "";
								tag.iGenreType = 16;
								tag.iGenreSubType = 0;

								iLastEndTime = tag.endTime;

								myChannel.epg.push_back(tag);
							}
						}
					}
				}
			}
		}
		bFirstRun = false;
	}
	return (true);
}

int PVRData::CheckResponseStatus(Json::Value response, std::string& errorMessage)
{
	if (response.isObject() && (!response.isNull()))
	{
		Json::Value status = response["status"];
		if (status.isInt() && status.asInt() != 1)
		{
			Json::Value errorMsg = response["error"];
			if (errorMsg.isString() && (!errorMsg.isNull()))
				errorMessage = errorMsg.asString();
			return (status.asInt());
		}
		return (1);
	}
	return (0);
}

bool PVRData::TryToPairDevice(void)
{
	Json::Value pairResult;

	CDialogLogin loginDialog(XBMC, GUI, "", "");
	if (loginDialog.DoModal() == 1)
	{
		int iTimeout = 100;
		std::string serial = GUI->GetInfoLabel("$INFO[Network.MacAddress]", 0);
		while ((serial.length() > 0) && (serial.find(":") == std::string::npos) && (iTimeout-- > 0))
		{
#ifdef TARGET_WINDOWS
			Sleep(1000);
#else
			sleep(1);
#endif
			serial = GUI->GetInfoLabel("$INFO[Network.MacAddress]", 0);
		}
		std::string friendlyName = GUI->GetInfoLabel("$INFO[System.FriendlyName]", 0);

		if ((JSONRPC("create-pairing", "username=" + uri::encode(uri::QUERY_TRAITS, loginDialog.Username) + 
			"&password=" + uri::encode(uri::QUERY_TRAITS, loginDialog.Password) +
			"&type=xbmc&product=" + uri::encode(uri::QUERY_TRAITS, friendlyName) +
			"&serial=" + uri::encode(uri::QUERY_TRAITS, serial),
			pairResult) == 0) && (pairResult.isObject() && (!pairResult.isNull())))
		{
			std::string errMsg = "";
			if (CheckResponseStatus(pairResult, errMsg))
			{
				char tmpBuff[32];
				int deviceId = pairResult["deviceId"].asInt();
				std::sprintf(tmpBuff, "%d", deviceId);
				g_szRegisteredDeviceId = tmpBuff;
				g_szRegisteredPassword = pairResult["password"].asString();

				SavePersistentData();
				return (true);
			}
		}
	}
	return (false);
}

bool PVRData::TryToLoginDevice(void)
{
	if ((g_szRegisteredDeviceId != "") && (g_szRegisteredPassword != ""))
	{
		Json::Value loginResult;
		if ((JSONRPC("device-login", "deviceId=" + uri::encode(uri::QUERY_TRAITS, g_szRegisteredDeviceId) + "&password=" + 
			uri::encode(uri::QUERY_TRAITS, g_szRegisteredPassword), loginResult) == 0) && (loginResult.isObject() && (!loginResult.isNull())))
		{
			std::string errMsg = "";
			if (CheckResponseStatus(loginResult, errMsg))
			{
				g_szPhpSession = loginResult["PHPSESSID"].asString();
				return (true);
			}
		}
	}
	return (false);
}

bool PVRData::ApiCall(const std::string& command, const std::string& arguments, Json::Value& json_response)
{
	if (JSONRPC(command, arguments, json_response) == 0)
	{
		std::string errMsg = "";
		if (CheckResponseStatus(json_response, errMsg))
		{
			return (true);
		}
		else
		{
			if (errMsg == "not logged")
			{
				if ((g_szRegisteredDeviceId == "") || (g_szRegisteredPassword == ""))
				{
					// nezname zarizeni, provedeme sparovani
					if (!TryToPairDevice())
						return (false);
				}

				if (!TryToLoginDevice())
				{
					// nepovedlo se prihlasit sparovane zarizeni, zkusime proto znovu zarizeni sparovat a prihlasit se
					g_szRegisteredDeviceId = "";
					g_szRegisteredPassword = "";

					if (!TryToPairDevice())
						return (false);

					// sparovani se povedlo
					if (!TryToLoginDevice())
						return (false); // prihlaseni ani pote neproslo, koncime s chybou
				}

				if (JSONRPC(command, arguments, json_response) == 0)
				{
					if (CheckResponseStatus(json_response, errMsg))
						return (true);
				}
			}
		}
	}
	return (false);
}

PVRData::PVRData(void)
{
	m_iEpgStart = -1;
	m_iEpgStop = -1;
	m_strDefaultIcon = "http://www.royalty-free.tv/news/wp-content/uploads/2011/06/cc-logo1.jpg";
	m_strDefaultMovie = "";
	m_LiveStreamUrl = "";

	LoadPersistentData();
}

PVRData::~PVRData(void)
{
	m_channels.clear();
}

std::string PVRData::GetSettingsFile(std::string filename, bool user) const
{
	string settingFile = user ? g_strUserPath : g_strClientPath;
	if (settingFile.at(settingFile.size() - 1) == '\\' ||
		settingFile.at(settingFile.size() - 1) == '/')
		settingFile.append(filename);
	else
		settingFile.append("/" + filename);
	return settingFile;
}

int PVRData::GetChannelsAmount(void)
{
	return m_channels.size();
}

PVR_ERROR PVRData::GetChannels(ADDON_HANDLE handle, bool bRadio)
{
	if (m_channels.empty())
		FillChannels();

	for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
	{
		PVRChannel &channel = m_channels.at(iChannelPtr);
		if (channel.bRadio == bRadio)
		{
			PVR_CHANNEL xbmcChannel;
			memset(&xbmcChannel, 0, sizeof(PVR_CHANNEL));

			xbmcChannel.iUniqueId = channel.iUniqueId;
			xbmcChannel.bIsRadio = channel.bRadio;
			xbmcChannel.iChannelNumber = channel.iChannelNumber;
			xbmcChannel.iSubChannelNumber = channel.iSubChannelNumber;
			strncpy(xbmcChannel.strChannelName, channel.strChannelName.c_str(), sizeof(xbmcChannel.strChannelName) - 1);

			CStdString stream;
			if (channel.bRadio)
				stream.Format("pvr://stream/radio/%i.ts", channel.iUniqueId);
			else
				stream.Format("pvr://stream/tv/%i.ts", channel.iUniqueId);
			strncpy(xbmcChannel.strStreamURL, stream.c_str(), stream.length());

			xbmcChannel.iEncryptionSystem = channel.iEncryptionSystem;
			strncpy(xbmcChannel.strIconPath, channel.strIconPath.c_str(), sizeof(xbmcChannel.strIconPath) - 1);
			xbmcChannel.bIsHidden = false;

			PVR->TransferChannelEntry(handle, &xbmcChannel);
		}
	}

	return PVR_ERROR_NO_ERROR;
}

bool PVRData::GetChannel(const PVR_CHANNEL &channel, PVRChannel &myChannel)
{
	for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
	{
		PVRChannel &thisChannel = m_channels.at(iChannelPtr);
		if (thisChannel.iUniqueId == (int)channel.iUniqueId)
		{
			myChannel.iUniqueId = thisChannel.iUniqueId;
			myChannel.bRadio = thisChannel.bRadio;
			myChannel.iChannelNumber = thisChannel.iChannelNumber;
			myChannel.iSubChannelNumber = thisChannel.iSubChannelNumber;
			myChannel.iEncryptionSystem = thisChannel.iEncryptionSystem;
			myChannel.strChannelName = thisChannel.strChannelName;
			myChannel.strIconPath = thisChannel.strIconPath;
			myChannel.strStreamURL = thisChannel.strStreamURL;

			return true;
		}
	}

	return false;
}

const char *PVRData::GetLiveStreamURL(const PVR_CHANNEL &channel)
{
	PVRChannel mychannel;
	if (GetChannel(channel, mychannel))
	{
		PLATFORM::CLockObject critsec(communication_mutex);
		std::string url = mychannel.strStreamURL;
		std::string server = "", path = "";
		int port = 80;

		int iServerPos = url.find("://");
		if (iServerPos > 0)
		{
			int iStreamPathPos = url.find("/", iServerPos + 3);
			if (iStreamPathPos > 0)
			{
				server = url.substr(iServerPos + 3, iStreamPathPos - iServerPos - 3);
				int iColonPos = server.find(":");
				if (iColonPos > 0)
				{
					server = server.substr(0, iColonPos - 1);
					port = strtol(server.substr(iColonPos, server.length() - iColonPos - 1).c_str(), NULL, 10);
				}
				path = url.substr(iStreamPathPos, url.length() - iStreamPathPos);
			}
		}

		std::string buffer = "";
		std::string message = "", actualLine = "";
		char content_header[100];

		buffer.append("GET " + path + " HTTP/1.1\r\n");
		std::sprintf(content_header, "Host: %s:%d\r\n", server.c_str(), port);
		buffer.append(content_header);
		buffer.append("Accept: */*\r\n");
		buffer.append("Accept-Charset: UTF-8,*;q=0.8\r\n");
		buffer.append("\r\n");

#ifdef TARGET_WINDOWS
		{
			WSADATA  WsaData;
			WSAStartup(0x0101, &WsaData);
		}
#endif

		sockaddr_in sin;
		int sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock != -1)
		{
			sin.sin_family = AF_INET;
			sin.sin_port = htons((unsigned short)port);

			struct hostent * host_addr = gethostbyname(server.c_str());
			if (host_addr != NULL)
			{
				sin.sin_addr.s_addr = *((int*)*host_addr->h_addr_list);

				if (connect(sock, (const struct sockaddr *)&sin, sizeof(sockaddr_in)) != -1)
				{
					SEND_RQ(buffer.c_str());

					char c1[1];
					int l, line_length;
					bool loop = true;
					bool bHeader = false;

					while (loop)
					{
						l = recv(sock, c1, 1, 0);
						if (l < 0) 
							loop = false;
						if (c1[0] == '\n')
						{
							message += actualLine + "\n";
							if (line_length == 0)
								loop = false;
							line_length = 0;
							if (message.find("302 Found") != std::string::npos)
								bHeader = true;
							if (bHeader)
							{
								if (actualLine.find("Location: ") == 0)
									m_LiveStreamUrl = actualLine.substr(10, actualLine.length() - 10);
							}
							actualLine = "";
						}
						else if (c1[0] != '\r')
						{
							actualLine += c1[0];
							line_length++;
						}
					}

					message = "";
					if (bHeader)
					{
						char p[1024];
						while ((l = recv(sock, p, 1023, 0)) > 0)  {
							p[l] = '\0';
							message += p;
						}
					}
					close(sock);
				}
			}
		}
	}

	return m_LiveStreamUrl.c_str();
}

int PVRData::GetChannelGroupsAmount(void)
{
	return 0;
}

PVR_ERROR PVRData::GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
	return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVRData::GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
	return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVRData::GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
	if ((m_iEpgStart < iStart) || (m_iEpgStop < iEnd))
	{
		m_iEpgStart = iStart;
		m_iEpgStop = iEnd;
		FillEpgForChannels(iStart, iEnd);
	}

	for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
	{
		PVRChannel &myChannel = m_channels.at(iChannelPtr);
		if (myChannel.iUniqueId != (int)channel.iUniqueId)
			continue;

		for (int i = 0; i < (int)myChannel.epg.size(); i++)
		{
			PVREpgEntry &myTag = myChannel.epg.at(i);

			EPG_TAG tag;
			memset(&tag, 0, sizeof(EPG_TAG));

			tag.iUniqueBroadcastId = myTag.iBroadcastId;
			tag.strTitle = myTag.strTitle.c_str();
			tag.iChannelNumber = myTag.iChannelId;
			tag.startTime = myTag.startTime;
			tag.endTime = myTag.endTime;
			tag.strPlotOutline = myTag.strPlotOutline.c_str();
			tag.strPlot = myTag.strPlot.c_str();
			tag.strIconPath = myTag.strIconPath.c_str();
			tag.iGenreType = myTag.iGenreType;
			tag.iGenreSubType = myTag.iGenreSubType;

			tag.strRecordingId = myTag.strEventId.c_str();

			PVR->TransferEpgEntry(handle, &tag);
		}
	}

	PVR->TriggerRecordingUpdate();

	return PVR_ERROR_NO_ERROR;
}

int PVRData::GetRecordingsAmount(void)
{
	return 0;
}

PVR_ERROR PVRData::GetRecordings(ADDON_HANDLE handle)
{
#if NOT_IMPLEMENTED_NOW
	for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
	{
		PVRChannel &myChannel = m_channels.at(iChannelPtr);

		if (!myChannel.bRadio)
		{
			PVR_RECORDING xbmcRecording;
			memset(&xbmcRecording, 0, sizeof(PVR_RECORDING));

			if (myChannel.epg.size() > 1)
			{
				PVREpgEntry &entry = myChannel.epg.at(1);

				xbmcRecording.iDuration = entry.endTime - entry.startTime;
				xbmcRecording.iGenreType = entry.iGenreType;
				xbmcRecording.iGenreSubType = entry.iGenreSubType;
				xbmcRecording.recordingTime = entry.startTime;

				strncpy(xbmcRecording.strChannelName, myChannel.strChannelName.c_str(), sizeof(xbmcRecording.strChannelName) - 1);
				strncpy(xbmcRecording.strPlotOutline, entry.strPlotOutline.c_str(), sizeof(xbmcRecording.strPlotOutline) - 1);
				strncpy(xbmcRecording.strPlot, entry.strPlot.c_str(), sizeof(xbmcRecording.strPlot) - 1);
				strncpy(xbmcRecording.strRecordingId, entry.strEventId.c_str(), sizeof(xbmcRecording.strRecordingId) - 1);
				strncpy(xbmcRecording.strTitle, entry.strTitle.c_str(), sizeof(xbmcRecording.strTitle) - 1);

				strcpy(xbmcRecording.strStreamURL, "");
				strcpy(xbmcRecording.strDirectory, "");

				PVR->TransferRecordingEntry(handle, &xbmcRecording);
			}
		}
	}

	return PVR_ERROR_NO_ERROR;
#endif
	return PVR_ERROR_NOT_IMPLEMENTED;
}

int PVRData::GetTimersAmount(void)
{
	return 0;
}

PVR_ERROR PVRData::GetTimers(ADDON_HANDLE handle)
{
#if NOT_IMPLEMENTED_NOW
	int i = 0;
	for (std::vector<PVRDemoTimer>::iterator it = m_timers.begin(); it != m_timers.end(); it++)
	{
		PVRDemoTimer &timer = *it;

		PVR_TIMER xbmcTimer;
		memset(&xbmcTimer, 0, sizeof(PVR_TIMER));

		xbmcTimer.iClientIndex = ++i;
		xbmcTimer.iClientChannelUid = timer.iChannelId;
		xbmcTimer.startTime = timer.startTime;
		xbmcTimer.endTime = timer.endTime;
		xbmcTimer.state = timer.state;

		strncpy(xbmcTimer.strTitle, timer.strTitle.c_str(), sizeof(timer.strTitle) - 1);
		strncpy(xbmcTimer.strSummary, timer.strSummary.c_str(), sizeof(timer.strSummary) - 1);

		PVR->TransferTimerEntry(handle, &xbmcTimer);
	}

	return PVR_ERROR_NO_ERROR;
#endif
	return PVR_ERROR_NOT_IMPLEMENTED;
}
