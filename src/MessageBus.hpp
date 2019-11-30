//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc BoulÃ© 
//
//This code is by Jerry Sievert, and was modified by Marc Boulé
//https://github.com/JerrySievert/CharredDesert/blob/v1.0/src/model/MessageBus.hpp 
//License CC0 1.0 Universal
//https://github.com/JerrySievert/CharredDesert/blob/v1.0/LICENSE.txt
//Downloaded by Marc Boulé on sept 4th 2019
//***********************************************************************************************

#ifndef MESSAGE_BUS_HPP
#define MESSAGE_BUS_HPP


#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <cstdint>

struct MessageBase {
	int id;
	char name[6];
};


template <typename T>
struct MessageBus {
	std::mutex memberMutex;
	std::unordered_map<int, T> memberData;
	std::unordered_map<int, bool> members;
	int busId = 0;


	void send(T* message) {// id of sender is included in message, this method will delete the given message after copying it
		std::lock_guard<std::mutex> lock(memberMutex);
		if (members[message->id]) {
			memberData[message->id] = *message;
		} 
		delete message;
	}


	void receive(T* message) {// id of sender we want to receive from must be in message->id, other fields will be filled by this method as the receive mechanism
		std::lock_guard<std::mutex> lock(memberMutex);
		if (members[message->id]) {
			*message = memberData[message->id];
		} 	
	}


	std::vector<MessageBase>* surveyValues() {// caller must delete returned value when non null. Does not use type T since don't want all the data, just the header info (id and name)
		std::lock_guard<std::mutex> lock(memberMutex);

		std::vector<MessageBase> *data = new std::vector<MessageBase>();
		for (const auto &e : memberData) {
			MessageBase newMessage;
			newMessage.id = e.second.id;
			for (int i = 0; i < 6; i++) {
				newMessage.name[i] = e.second.name[i];
			}
			data->push_back(newMessage);
		}

		return data;
	}


	int registerMember() {
		std::lock_guard<std::mutex> lock(memberMutex);

		int newId = busId;
		busId++;
		members[newId] = true;
		return newId;
	}


	void deregisterMember(int id) {
		std::lock_guard<std::mutex> lock(memberMutex);

		if (members[id]) {
			members.erase(id);
			memberData.erase(id);
		} 
	}
};

#endif