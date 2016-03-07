#include "IntelWeb.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

IntelWeb::IntelWeb() {}
IntelWeb::~IntelWeb() {
	close();
}
bool IntelWeb::createNew(const std::string& filePrefix, unsigned int maxDataItems) {
	close();
	telemetryLogFilename = filePrefix + ".dat";
	telemetryLog.open(telemetryLogFilename, std::ios::trunc | std::ios::in | std::ios::out);
	return telemetryLog && events.createNew(filePrefix + ".dmm", maxDataItems*(4.0 / 3.0));
}
bool IntelWeb::openExisting(const std::string& filePrefix) {
	close();
	telemetryLogFilename = filePrefix + ".dat";
	telemetryLog.open(telemetryLogFilename, std::ios::app | std::ios::in | std::ios::out);
	return telemetryLog && events.openExisting(filePrefix + ".dmm");
}
void IntelWeb::close() {
	events.close();
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
		std::string context, key, value;

		if (!(iss >> context >> key >> value)) {
			std::cerr << "Ignoring badly-formatted input line: " << line << std::endl;
			continue;
		}
		
		char dummy;
		if (iss >> dummy) // succeeds if there a non-whitespace char
			std::cerr << "Ignoring extra data in line: " << line << std::endl;

		telemetryLog << line << std::endl;
		events.insert(key, value, context);
	}
	return true;
}

unsigned int IntelWeb::crawl(const std::vector<std::string>& indicators, unsigned int minPrevalenceToBeGood, std::vector<std::string>& badEntitiesFound, std::vector<InteractionTuple>& interactions) {
	unsigned int numBadEntities = 0;
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

		DiskMultiMap::Iterator it = events.search(key); ///DEBUGGING: TODO: all the logic for crawling

		///queue: store keys that need to be processed later (push to this queue whenever an interaction finds a new bad entity with prevalence < minPrevalenceToBeGood [also, numBadEntities++])
		///unordered_map: store key = (accessed / need to access/ key is good (ie. don't access))
		///unordered_map: store interactions that are already in vector to prevent duplicates (or do some ordering in post);
		///		NOTE: need functions that check if string is website, file, or context!!!
		///unordered_map: store prevalences of all the entities (do this in first pass. remaining processing in second pass)
	}
	return numBadEntities;
}