#pragma once
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

#include <vector>
#include "platform/util/StdString.h"
#include "platform/os.h"
#include "platform/threads/mutex.h"
#include "client.h"
#include "json/json.h"

struct PVREpgEntry
{
	int         iBroadcastId;
	std::string strEventId;
	std::string strTitle;
	int         iChannelId;
	time_t      startTime;
	time_t      endTime;
	std::string strPlotOutline;
	std::string strPlot;
	std::string strIconPath;
	int         iGenreType;
	int         iGenreSubType;
	std::string strTimeshiftStream;
	//  time_t      firstAired;
	//  int         iParentalRating;
	//  int         iStarRating;
	//  bool        bNotify;
	//  int         iSeriesNumber;
	//  int         iEpisodeNumber;
	//  int         iEpisodePartNumber;
	//  std::string strEpisodeName;
};

struct PVRChannel
{
	bool                    bRadio;
	int                     iUniqueId;
	int                     iChannelNumber;
	int                     iSubChannelNumber;
	int                     iEncryptionSystem;
	std::string             strChannelName;
	std::string             strChannelId;
	std::string             strIconPath;
	std::string             strStreamURL;
	std::vector<PVREpgEntry> epg;
};

class PVRData
{
	private:
		mutable PLATFORM::CMutex m_channelsLock;
		std::string m_LiveStreamUrl;

		int RPC(const std::string& command, const std::string& arguments, std::string& json_response);
		int JSONRPC(const std::string& command, const std::string& arguments, Json::Value& json_response);

		bool LoadPersistentData(void);
		bool SavePersistentData(void);
		int FillChannels(void);
		bool FillEpgForChannels(time_t iStart, time_t iEnd);
	protected:
		int CheckResponseStatus(Json::Value response, std::string& errorMessage);
		bool TryToPairDevice(void);
		bool TryToLoginDevice(void);
		bool ApiCall(const std::string& command, const std::string& arguments, Json::Value& json_response);
	public:
		PVRData(void);
		virtual ~PVRData(void);

		virtual int GetChannelsAmount(void);
		virtual PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio);
		virtual bool GetChannel(const PVR_CHANNEL &channel, PVRChannel &myChannel);
		virtual const char *GetLiveStreamURL(const PVR_CHANNEL &channel);

		virtual int GetChannelGroupsAmount(void);
		virtual PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio);
		virtual PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group);

		virtual PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd);

		virtual int GetRecordingsAmount(void);
		virtual PVR_ERROR GetRecordings(ADDON_HANDLE handle);

		virtual int GetTimersAmount(void);
		virtual PVR_ERROR GetTimers(ADDON_HANDLE handle);

		virtual std::string GetSettingsFile(std::string filename, bool user = false) const;
	private:
		std::vector<PVRChannel> m_channels;
		time_t m_iEpgStart;
		time_t m_iEpgStop;
		CStdString m_strDefaultIcon;
		CStdString m_strDefaultMovie;
};
