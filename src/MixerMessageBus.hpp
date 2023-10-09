//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc BoulÃ© 
//
//This code is an adaptation by Marc Boulé of Jerry Sievert's MessageBus
//https://github.com/JerrySievert/CharredDesert/blob/v1.0/src/model/MessageBus.hpp 
//License CC0 1.0 Universal
//https://github.com/JerrySievert/CharredDesert/blob/v1.0/LICENSE.txt
//Downloaded by Marc Boulé on sept 4th 2019
//***********************************************************************************************

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <cstdint>


struct MessageBase {
	int64_t id;
	char name[7] = {};
};

struct MixerMessage : MessageBase {
	// GUI elements:
	bool isJr = false;
	char trkGrpAuxLabels[(16 + 4 + 4) * 4] = {};
	int8_t vuColors[1 + 16 + 4 + 4] = {};// room for global, tracks, groups, aux
	int8_t dispColors[1 + 16 + 4 + 4] = {};// room for global, tracks, groups, aux
	// Track move elements:
	union tmU {
		int8_t tmSep[4];// 0 is has move (0=no, 1=yes), 1 is src, 2 is dest, 3 is rotating tag (0 to 15, 15 being arbitrary choice)
		int32_t tmTot = 0;
	};
	tmU tm;
};


struct MixerMessageBus {
	std::mutex memberMutex;
	std::unordered_map<int64_t, MixerMessage> memberData;// first value is a "Module::id + 1" (so that 0 = deregistered, instead of -1)


	void send(int64_t id, const char* masterLabel, const char* trackLabels, const char* auxLabels, const int8_t *_vuColors, const int8_t *_dispColors, bool doTrackMoveInit) {
		std::lock_guard<std::mutex> lock(memberMutex);
		memberData[id].id = id;
		memcpy(memberData[id].name, masterLabel, 6);
		memberData[id].isJr = false;
		memcpy(memberData[id].trkGrpAuxLabels, trackLabels, (16 + 4) * 4);// grabs groups also since contiguous
		memcpy(&memberData[id].trkGrpAuxLabels[(16 + 4) * 4], auxLabels, 4 * 4);
		memberData[id].vuColors[0] = _vuColors[0];
		if (_vuColors[0] >= 5) {
			memcpy(&memberData[id].vuColors[1], &_vuColors[1], 16 + 4 + 4);
		}
		memberData[id].dispColors[0] = _dispColors[0];
		if (_dispColors[0] >= 7) {
			memcpy(&memberData[id].dispColors[1], &_dispColors[1], 16 + 4 + 4);
		}
		if (doTrackMoveInit) {
			memberData[id].tm.tmTot = 0;
		}
	}
	void sendJr(int64_t id, const char* masterLabel, const char* trackLabels, const char* groupLabels, const char* auxLabels, const int8_t *_vuColors, const int8_t *_dispColors, bool doTrackMoveInit) {// does not write to tracks 9-16 and groups 3-4 when jr.
		std::lock_guard<std::mutex> lock(memberMutex);
		memberData[id].id = id;
		memcpy(memberData[id].name, masterLabel, 6);
		memberData[id].isJr = true;
		memcpy(memberData[id].trkGrpAuxLabels, trackLabels, 8 * 4);
		memcpy(&memberData[id].trkGrpAuxLabels[16 * 4], groupLabels, 2 * 4);
		memcpy(&memberData[id].trkGrpAuxLabels[(16 + 4) * 4], auxLabels, 4 * 4);
		memberData[id].vuColors[0] = _vuColors[0];
		if (_vuColors[0] >= 5) {
			memcpy(&memberData[id].vuColors[1], &_vuColors[1], 8);
			memcpy(&memberData[id].vuColors[1 + 16], &_vuColors[1 + 16], 2);
			memcpy(&memberData[id].vuColors[1 + 16 + 4], &_vuColors[1 + 16 + 4], 4);
		}
		memberData[id].dispColors[0] = _dispColors[0];
		if (_dispColors[0] >= 7) {
			memcpy(&memberData[id].dispColors[1], &_dispColors[1], 8);
			memcpy(&memberData[id].dispColors[1 + 16], &_dispColors[1 + 16], 2);
			memcpy(&memberData[id].dispColors[1 + 16 + 4], &_dispColors[1 + 16 + 4], 4);
		}
		if (doTrackMoveInit) {
			memberData[id].tm.tmTot = 0;
		}
	}
	
	void sendTrackMove(int64_t id, int8_t srcTrack, int8_t destTrack) {
		std::lock_guard<std::mutex> lock(memberMutex);
		// not need to write id since that is already there (MixMaster construct has sendToMessageBus() which has send(...))
		memberData[id].tm.tmSep[0] = 1;
		memberData[id].tm.tmSep[1] = srcTrack;
		memberData[id].tm.tmSep[2] = destTrack;
		memberData[id].tm.tmSep[3]++;
		if (memberData[id].tm.tmSep[3] > 15) {
			memberData[id].tm.tmSep[3] = 0;
		}
	}

	void receive(MixerMessage* message) {// id of sender we want to receive from must be in message->id, other fields will be filled by this method as the receive mechanism. If non-existing sender is requested, a blank message with an id of 0 will be returned
		std::lock_guard<std::mutex> lock(memberMutex);
		int64_t id = message->id;
		*message = memberData[id];
	}


	std::vector<MessageBase>* surveyValues() {// caller must delete returned value when non null. Does not use MixerMessage type since don't want all the data, just the header info (id and name)
		std::lock_guard<std::mutex> lock(memberMutex);

		std::vector<MessageBase> *data = new std::vector<MessageBase>();
		for (const auto &e : memberData) {
			if (e.second.id != 0) {
				MessageBase newMessage;
				newMessage.id = e.second.id;
				memcpy(newMessage.name, e.second.name, 6);
				data->push_back(newMessage);
			}
		}

		return data;
	}

	void deregisterMember(int64_t id) {
		std::lock_guard<std::mutex> lock(memberMutex);
		memberData.erase(id);
	}
};
