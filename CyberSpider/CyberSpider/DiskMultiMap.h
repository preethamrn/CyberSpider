#ifndef DISKMULTIMAP_H_
#define DISKMULTIMAP_H_

#include <string>
#include <cstring>
#include <functional>
#include "MultiMapTuple.h"
#include "BinaryFile.h"

class DiskMultiMap {
public:
	class Iterator {
	public:
		Iterator();
		Iterator(BinaryFile* file, long offset);
		// You may add additional constructors
		bool isValid() const;
		Iterator& operator++();
		MultiMapTuple operator*();

	private:
		long m_offset;
		BinaryFile* bf;
		MultiMapTuple convert(char key[], char value[], char context[]) {
			MultiMapTuple m;
			m.key = std::string(key);
			m.value = std::string(value);
			m.context = std::string(context);
			return m;
		}
	};

	DiskMultiMap();
	~DiskMultiMap();
	bool createNew(const std::string& filename, unsigned int numBuckets);
	bool openExisting(const std::string& filename);
	void close();
	bool insert(const std::string& key, const std::string& value, const std::string& context);
	Iterator search(const std::string& key);
	int erase(const std::string& key, const std::string& value, const std::string& context);

private:
	struct KeyValueContextTuple {
		char key[128], value[128], context[128];
		long next;
		long m_offset;
		///remember to initialize offset and key, value, context when inserting
	};
	struct KeyTuple {
		char key[128];
		long kvct_pos; //KeyValueContextTuple position
		long next;
		long m_offset;
		///remember to initialize offsets key when inserting
	};
	struct DiskHeader {
		unsigned int numBuckets;
		long kvct_last_erased, kt_last_erased;
	};
	BinaryFile bf;
	std::hash<std::string> hash;
	std::string m_filename;
	DiskHeader header;
	// Your private member declarations will go here
};

#endif // DISKMULTIMAP_H_