#ifndef DISKMULTIMAP_H_
#define DISKMULTIMAP_H_

#include <string>
#include <cstring>
#include <functional>
#include "MultiMapTuple.h"
#include "BinaryFile.h"

class DiskMultiMap {
private:
	//structs are defined before rest of class
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
		bool cached;
		DiskMultiMap::ValueContextTuple m_vct;
		MultiMapTuple convert(ValueContextTuple vct) const {
			MultiMapTuple m;
			m.key = std::string(key);
			m.value = std::string(vct.value);
			m.context = std::string(vct.context);
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
	BinaryFile bf;
	std::hash<std::string> hash;
	DiskHeader header;
};

#endif // DISKMULTIMAP_H_