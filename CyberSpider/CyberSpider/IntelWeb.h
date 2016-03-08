#ifndef INTELWEB_H_
#define INTELWEB_H_

#include "InteractionTuple.h"
#include "DiskMultiMap.h"
#include <fstream>
#include <string>
#include <vector>

class IntelWeb {
public:
	IntelWeb();
	~IntelWeb();
	bool createNew(const std::string& filePrefix, unsigned int maxDataItems);
	bool openExisting(const std::string& filePrefix);
	void close();
	bool ingest(const std::string& telemetryFile);
	unsigned int crawl(const std::vector<std::string>& indicators,
		unsigned int minPrevalenceToBeGood,
		std::vector<std::string>& badEntitiesFound,
		std::vector<InteractionTuple>& interactions
		);
	bool purge(const std::string& entity);

private:
	DiskMultiMap initiator_events, receiver_events;
	//initiator_events: stores mapping from initiator to all the receivers
	//receiver_events: stores mapping from receivers to all its initiators
	std::string telemetryLogFilename;
	std::fstream telemetryLog;

};

bool operator<(const InteractionTuple& lhs, const InteractionTuple & rhs) {
	if (lhs.context < rhs.context) return true;
	else if (rhs.context < lhs.context) return false;
	else if (lhs.from < rhs.from) return true;
	else if (rhs.from < lhs.from) return false;
	else if (lhs.to < rhs.to) return true;
	else return false;
}

#endif // INTELWEB_H_
