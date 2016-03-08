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

//operator less than overloaded for InteractionTuple so it can be stored in a set
bool operator<(const InteractionTuple & lhs, const InteractionTuple & rhs) {
	if (lhs.context < rhs.context) return true;
	else if (rhs.context < lhs.context) return false;
	else if (lhs.from < rhs.from) return true;
	else if (rhs.from < lhs.from) return false;
	else if (lhs.to < rhs.to) return true;
	else return false;
}

IntelWeb::IntelWeb() {}
IntelWeb::~IntelWeb() {
	close();
}
bool IntelWeb::createNew(const std::string& filePrefix, unsigned int maxDataItems) {
	close();
	return initiator_events.createNew(filePrefix + "-initiator.dmm", (unsigned int) maxDataItems*(4.0 / 3.0)) && 
		receiver_events.createNew(filePrefix + "-receiver.dmm", (unsigned int)maxDataItems*(4.0 / 3.0));
}
bool IntelWeb::openExisting(const std::string& filePrefix) {
	close();
	return initiator_events.openExisting(filePrefix + "-initiator.dmm") && receiver_events.openExisting(filePrefix + "-receiver.dmm");
}
void IntelWeb::close() {
	initiator_events.close();
	receiver_events.close();
}

bool IntelWeb::ingest(const std::string& telemetryFile) {
	// Open the file for input
	std::ifstream inf(telemetryFile);
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

		initiator_events.insert(initiator, receiver, context);
		receiver_events.insert(receiver, initiator, context);
	}
	return true;
}

unsigned int IntelWeb::crawl(const std::vector<std::string>& indicators, unsigned int minPrevalenceToBeGood, std::vector<std::string>& badEntitiesFound, std::vector<InteractionTuple>& interactions) {
	unsigned int numBadEntities = 0;
	std::unordered_map<std::string, unsigned int> prevalences; //stores prevalences of each entity
	std::unordered_map<std::string, int> state; //stores whether entity shouldn't be processed (0), needs to be processed (1), or has already been processed (2)

	std::queue<std::string> badEntitiesToBeProcessed; //store bad entities that need to be processed (ie searched for associations)
	std::set<InteractionTuple> interactionsSet; //set of all bad interactions (for efficient insertion and collision prevention)

	for (std::vector<std::string>::const_iterator it = indicators.begin(); it != indicators.end(); it++) {
		state[*it] = 1; //set state indicating it needs to be processed
		badEntitiesToBeProcessed.push(*it);
	}

	while (!badEntitiesToBeProcessed.empty()) {
		std::string key = badEntitiesToBeProcessed.front(); badEntitiesToBeProcessed.pop(); 
		state[key] = 2; //set state so this key isn't accessed again
		vector<MultiMapTuple> associations_i, associations_r; //initiator and receiver associations
		
		//go through all of this key's associations and add potential bad entities (ie. entities that haven't been processed yet or have too low prevalence)
		DiskMultiMap::Iterator it_i = initiator_events.search(key), it_r = receiver_events.search(key);
		unsigned int numAssociations = 0;
		while (it_i.isValid()) { //associations where key is initiator
			numAssociations++;
			associations_i.push_back(*it_i);
			++it_i;
		}
		while (it_r.isValid()) { //associations where key is receiver
			numAssociations++;
			associations_r.push_back(*it_r);
			++it_r;
		}
		prevalences[key] = numAssociations;
		if (numAssociations >= minPrevalenceToBeGood || numAssociations == 0) continue; //this key has enough prevalence to be skipped or the key doesn't have any associations
		
		badEntitiesFound.push_back(key);
		numBadEntities++;
		
		for (int i = 0; i < associations_i.size(); i++) {
			MultiMapTuple &mmt = associations_i[i];
			if (state[mmt.value] == 0 && prevalences[mmt.value] < minPrevalenceToBeGood) {
				badEntitiesToBeProcessed.push(mmt.value); //add it to the process queue
				state[mmt.value] = 1; //set state indicating this value needs to be processed
			}
			InteractionTuple bad_interaction(key, mmt.value, mmt.context);
			interactionsSet.insert(bad_interaction); //inserting to set will prevent duplicates
		}
		for (int i = 0; i < associations_r.size(); i++) {
			MultiMapTuple &mmt = associations_r[i];
			if (state[mmt.value] == 0 && prevalences[mmt.value] < minPrevalenceToBeGood) {
				badEntitiesToBeProcessed.push(mmt.value); //add it to the process queue
				state[mmt.value] = 1; //set state indicating this value needs to be processed
			}
			InteractionTuple bad_interaction(mmt.value, key, mmt.context);
			interactionsSet.insert(bad_interaction); //inserting to set will prevent duplicates
		}
	}

	std::sort(badEntitiesFound.begin(), badEntitiesFound.end());
	for (std::set<InteractionTuple>::const_iterator it = interactionsSet.begin(); it != interactionsSet.end(); it++) {
		interactions.push_back(*it); //in-order traversal through std::set of interactions will result in sorted order
	}

	return numBadEntities;
}

bool IntelWeb::purge(const std::string& entity) {
	bool purged = false;
	DiskMultiMap::Iterator it_i, it_r;
	do {
		it_i = initiator_events.search(entity);
		MultiMapTuple mmt = *it_i;
		initiator_events.erase(mmt.key, mmt.value, mmt.context);
		receiver_events.erase(mmt.value, mmt.key, mmt.context); //receiver events have key and value swapped
		purged = true;
	} while (it_i.isValid());

	do {
		it_i = receiver_events.search(entity);
		MultiMapTuple mmt = *it_i;
		receiver_events.erase(mmt.key, mmt.value, mmt.context);
		initiator_events.erase(mmt.value, mmt.key, mmt.context); //initiator events have key and value swapped
		purged = true;
	} while (it_i.isValid());

	return purged;
}
/*
int main() {
	IntelWeb a;
	a.createNew("a", 100);
	a.ingest("jan-telemetry.txt");
	a.ingest("feb-telemetry.txt");
	
	vector<string> ind;
	ind.push_back("http://www.virus.com");
	ind.push_back("disk-eater.exe");
	ind.push_back("datakill.exe");
	ind.push_back("http://www.stealthyattack.com");
	vector<string> be;
	vector<InteractionTuple> bi;

	a.crawl(ind, 12, be, bi);
	cout << be.size();
	cout << bi.size();
}*/