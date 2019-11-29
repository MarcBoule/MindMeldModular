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

template <typename T>
struct Message {
	std::string id;
	T value;
};

template <typename T>
class MessageBus {
	std::mutex memberMutex;
	std::unordered_map<std::string, Message<T> *> memberData;
	std::unordered_map<std::string, bool> members;
	// this would be better handled with a uuid
	uint64_t busId = 0;

	public:
	
	MessageBus<T>() { }

	void send(std::string id, Message<T> *message) {// id of sender
		std::lock_guard<std::mutex> lock(memberMutex);

		if (members[id]) {
			if (memberData[id]) {
				Message<T> *old = memberData[id];
				memberData.erase(id);

				delete old;
			}

			memberData[id] = message;
		} 
		// else {
			// fprintf(stderr, "Unknown member sent data to the MessageBus: %s\n", id.c_str());
		// }
	}

	Message<T>* receive(std::string id) {// id of sender, not receiver. caller must delete returned value when non null
		std::lock_guard<std::mutex> lock(memberMutex);
		//std::unique_lock<std::mutex> lock(memberMutex, std::try_to_lock);
		Message<T> *message = NULL;
		
		if (members[id]) {
			if (memberData[id]) {
				message = memberData[id];
				memberData.erase(id);
			}
		} 
		// else {
			// fprintf(stderr, "Tried to read data from unknown member from the MessageBus : %s\n", id.c_str());
		// }
		
		return message;
	}

	std::vector<T> *currentValues() {
		std::vector<T> *data = new std::vector<T>();

		std::lock_guard<std::mutex> lock(memberMutex);

		for (const auto &e : memberData) {
			Message<T> *message = (Message<T> *) e.second;
			data->push_back(message->value);
		}

		return data;
	}

	std::string registerMember() {
		std::lock_guard<std::mutex> lock(memberMutex);

		std::string id = std::to_string(busId);
		busId++;

		members[id] = true;

		return id;
	}

	void deregisterMember(std::string id) {
		std::lock_guard<std::mutex> lock(memberMutex);

		if (members[id]) {
			if (memberData[id]) {
				Message<T> *old = memberData[id];
				delete old;
				memberData.erase(id);
			}

			members.erase(id);
		} 
		// else {
			// fprintf(stderr, "Unknown member tried to deregister: %s\n", id.c_str());
		// }
	}
};

#endif