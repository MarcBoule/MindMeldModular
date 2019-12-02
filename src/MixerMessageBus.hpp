//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc BoulÃ© 
//
//This code is an adaptation by Marc Boulé of Jerry Sievert's MessageBus
//https://github.com/JerrySievert/CharredDesert/blob/v1.0/src/model/MessageBus.hpp 
//License CC0 1.0 Universal
//https://github.com/JerrySievert/CharredDesert/blob/v1.0/LICENSE.txt
//Downloaded by Marc Boulé on sept 4th 2019
//***********************************************************************************************

#ifndef MIXER_MESSAGE_BUS_HPP
#define MIXER_MESSAGE_BUS_HPP


#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <cstdint>


struct MessageBase {
	int id;
	char name[6];
};

struct MixerMessage : MessageBase {
	bool isJr;
	char trkGrpAuxLabels[(16 + 4 + 4) * 4];
};


struct MixerMessageBus {
	std::mutex memberMutex;
	std::unordered_map<int, MixerMessage> memberData;
	int busId = 1;// 0 is reserved for no data (unseen member)


	void send(int id, bool isJr, char* masterLabel, char* trackLabels, char* groupLabels, char* auxLabels) {
		std::lock_guard<std::mutex> lock(memberMutex);
		memberData[id].id = id;
		memcpy(memberData[id].name, masterLabel, 6);
		memcpy(memberData[id].trkGrpAuxLabels, trackLabels, (isJr ? 8 : 16) * 4);
		if (isJr) {
			snprintf(&memberData[id].trkGrpAuxLabels[8 * 4], 8 * 4, "-09--10--11--12--13--14--15--16-");
		}
		memcpy(&memberData[id].trkGrpAuxLabels[16 * 4], groupLabels, (isJr ? 2 : 4) * 4);
		if (isJr) {
			snprintf(&memberData[id].trkGrpAuxLabels[(16 + 2) * 4], 2 * 4, "GRP3GRP4");
		}
		memcpy(&memberData[id].trkGrpAuxLabels[(16 + 4) * 4], auxLabels, 4 * 4);
	}


	void receive(MixerMessage* message) {// id of sender we want to receive from must be in message->id, other fields will be filled by this method as the receive mechanism. If non-existing sender is requested, a blank message with an id of 0 will be returned
		std::lock_guard<std::mutex> lock(memberMutex);
		int id = message->id;
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


	int registerMember() {
		std::lock_guard<std::mutex> lock(memberMutex);
		int newId = busId;
		busId++;
		return newId;
	}


	void deregisterMember(int id) {
		std::lock_guard<std::mutex> lock(memberMutex);
		memberData.erase(id);
	}
};

#endif