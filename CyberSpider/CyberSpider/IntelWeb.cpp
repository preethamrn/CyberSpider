#include "IntelWeb.h"
#include "DiskMultiMap.h"
#include "MultiMapTuple.h"
#include "InteractionTuple.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <queue>
#include <set>
#include <algorithm>
#include <unordered_map>

IntelWeb::IntelWeb() {}
IntelWeb::~IntelWeb() {
	close();
}
bool IntelWeb::createNew(const std::string& filePrefix, unsigned int maxDataItems) {
	close();
	telemetryLogFilename = filePrefix + ".dat";
	telemetryLog.open(telemetryLogFilename, std::ios::trunc | std::ios::in | std::ios::out);
	return telemetryLog && 
		initiator_events.createNew(filePrefix + "-initiator.dmm", (unsigned int) maxDataItems*(4.0 / 3.0)) && 
		receiver_events.createNew(filePrefix + "-receiver.dmm", (unsigned int)maxDataItems*(4.0 / 3.0));
}
bool IntelWeb::openExisting(const std::string& filePrefix) {
	close();
	telemetryLogFilename = filePrefix + ".dat";
	telemetryLog.open(telemetryLogFilename, std::ios::app | std::ios::in | std::ios::out);
	return telemetryLog && initiator_events.openExisting(filePrefix + "-initiator.dmm") && receiver_events.openExisting(filePrefix + "-receiver.dmm");
}
void IntelWeb::close() {
	initiator_events.close();
	receiver_events.close();
	telemetryLog.close();
}

bool IntelWeb::ingest(const std::string& telemetryFile) {
	// Open the file for input
	std::ifstream inf(telemetryFile);
	telemetryLog.seekp(0, std::ios::end); //go to the end of file to add new telemetry data
	// Test for failure to open
	if (!inf) {
		std::cerr << "Cannot open telemetry file!" << std::endl;
		return false;
	}

	std::string line;
	while (getline(inf, line)) {
		std::istringstream iss(line);
		std::string context, initiator, receiver;

		if (!(iss >> context >> initiator >> receiver)) {
			std::cerr << "Ignoring badly-formatted input line: " << line << std::endl;
			continue;
		}
		
		char dummy;
		if (iss >> dummy) // succeeds if there a non-whitespace char
			std::cerr << "Ignoring extra data in line: " << line << std::endl;

		telemetryLog << line << std::endl;
		initiator_events.insert(initiator, receiver, context);
		receiver_events.insert(receiver, initiator, context);
	}
	return true;
}

unsigned int IntelWeb::crawl(const std::vector<std::string>& indicators, unsigned int minPrevalenceToBeGood, std::vector<std::string>& badEntitiesFound, std::vector<InteractionTuple>& interactions) {
	unsigned int numBadEntities = 0;
	std::unordered_map<std::string, unsigned int> prevalences; //stores prevalences of each entity
	std::unordered_map<std::string, int> state; //stores whether entity shouldn't be processed (0), needs to be processed (1), or has already been processed (2)

	{ //first pass to initialize the prevalences and states
		telemetryLog.seekg(0, std::ios::beg);
		std::string line;
		while (getline(telemetryLog, line)) {
			std::istringstream iss(line);
			std::string context, key, value;

			{//make sure line is good incase user made any changes which they aren't supposed to
				if (!(iss >> context >> key >> value)) {
					std::cerr << "Ignoring badly-formatted input line: " << line << std::endl;
					continue;
				}
				char dummy;
				if (iss >> dummy) // succeeds if there a non-whitespace char
					std::cerr << "Ignoring extra data in line: " << line << std::endl;
			}

			//store prevalences of all the entities
			if (prevalences.find(key) == prevalences.end()) prevalences[key] = 1;
			else prevalences[key]++;
			if (prevalences.find(value) == prevalences.end()) prevalences[value] = 1;
			else prevalences[value]++;
			//store state of key and value (shouldn't be processed (0), needs to be processed (1), or has already been processed (2))
			state[key] = 0;
			state[value] = 0;
		}
	}

	std::queue<std::string> badEntitiesToBeProcessed; //store bad entities that need to be processed (ie searched for associations)
	std::set<InteractionTuple> interactionsSet; //set of all bad interactions (for efficient insertion and collision prevention)

	for (std::vector<std::string>::const_iterator it = badEntitiesFound.begin(); it != badEntitiesFound.end(); it++) {
		state[*it] = 1; //set entities to need to access
		badEntitiesToBeProcessed.push(*it);
		numBadEntities++;
	}

	while (!badEntitiesToBeProcessed.empty()) {
		std::string key = badEntitiesToBeProcessed.front(); badEntitiesToBeProcessed.pop();
		//checks all the values that this key initiates (ie. key is an initiator)
		DiskMultiMap::Iterator it_i = initiator_events.search(key);
		while (it_i.isValid()) {
			MultiMapTuple mmt = *it_i;
			std::string value = mmt.value;
			std::string context = mmt.context;
			InteractionTuple bad_interaction(key, value, context);
			if (prevalences[value] < minPrevalenceToBeGood && state[value] == 0) {
				//whenever something is added to the queue: increment numBadEntities, change state[value] = 1, push_back to vector of badEntitiesFound
				badEntitiesToBeProcessed.push(value);
				badEntitiesFound.push_back(value);
				numBadEntities++;
				state[value] = 1;
			}
			//add interaction to badInteractions whenever at least one is bad (even if the other has higher prevalence)
			interactionsSet.insert(bad_interaction); //inserting to set will prevent duplicates
			++it_i;
		}
		//checks all the initiators that initiate this key (ie. key is a receiver)
		DiskMultiMap::Iterator it_r = receiver_events.search(key);
		while (it_r.isValid()) {
			MultiMapTuple mmt = *it_r;
			std::string value = mmt.value;
			std::string context = mmt.context;
			InteractionTuple bad_interaction(value, key, context);
			if (prevalences[value] < minPrevalenceToBeGood && state[value] == 0) {
				//whenever something is added to the queue: increment numBadEntities, change state[value] = 1, push_back to vector of badEntitiesFound
				badEntitiesToBeProcessed.push(value);
				badEntitiesFound.push_back(value);
				numBadEntities++;
				state[value] = 1;
			}
			//add interaction to badInteractions whenever at least one is bad (even if the other has higher prevalence)
			interactionsSet.insert(bad_interaction); //inserting to set will prevent duplicates
			++it_r;
		}
		state[key] = 2; //make state[key] = 2 at end of loop
	}

	for (std::set<InteractionTuple>::const_iterator it = interactionsSet.begin(); it != interactionsSet.end(); it++) {
		interactions.push_back(*it); //in-order traversal through std::set of interactions will result in sorted order
	}

	return numBadEntities;
}