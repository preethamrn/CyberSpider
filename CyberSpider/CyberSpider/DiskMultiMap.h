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
		Iterator(BinaryFile* file, BinaryFile::Offset offset, const std::string& k);
		bool isValid() const;
		Iterator& operator++();
		MultiMapTuple operator*();

	private:
		BinaryFile::Offset m_offset;
		BinaryFile* bf;
		std::string key;
		MultiMapTuple convert(const char key[], const char value[], const char context[]) const {
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
	struct ValueContextTuple {
		char value[128], context[128];
		BinaryFile::Offset next;
		BinaryFile::Offset m_offset;
	};
	struct KeyTuple {
		char key[128];
		BinaryFile::Offset vct_pos; //KeyValueContextTuple position
		BinaryFile::Offset next;
		BinaryFile::Offset m_offset;
	};
	struct DiskHeader {
		unsigned int numBuckets;
		BinaryFile::Offset vct_last_erased, kt_last_erased;
	};
	BinaryFile bf;
	std::hash<std::string> hash;
	DiskHeader header;
};

#endif // DISKMULTIMAP_H_