#include "IntelWeb.h"
#include <string>
#include <vector>

IntelWeb::IntelWeb() {}
IntelWeb::~IntelWeb() {
	close();
}
bool IntelWeb::createNew(const std::string& filePrefix, unsigned int maxDataItems) {
	close();
	return creation.createNew(filePrefix + "-creation.dmmp", maxDataItems*(4.0 / 3.0)) &&
		download.createNew(filePrefix + "-download.dmmp", maxDataItems*(4.0 / 3.0)) &&
		contact.createNew(filePrefix + "-conact.dmmp", maxDataItems*(4.0 / 3.0));
}
bool IntelWeb::openExisting(const std::string& filePrefix) {
	close();
	return creation.openExisting(filePrefix + "-creation.dmmp") &&
		download.openExisting(filePrefix + "-download.dmmp") &&
		contact.openExisting(filePrefix + "-conact.dmmp");
}
void IntelWeb::close() {
	creation.close();
	download.close();
	contact.close();
}

bool IntelWeb::ingest(const std::string& telemetryFile) {
	return true;
}

unsigned int IntelWeb::crawl(const std::vector<std::string>& indicators, unsigned int minPrevalenceToBeGood, 
	std::vector<std::string>& badEntitiesFound, std::vector<InteractionTuple>& interactions) {
	
}