#include "DiskMultiMap.h"

DiskMultiMap::Iterator::Iterator() {
	m_offset = -1;
}
DiskMultiMap::Iterator::Iterator(const std::string filename, long offset) {
	m_offset = offset;
	bf.openExisting(filename);
}
DiskMultiMap::Iterator::~Iterator() {
	bf.close();
}
bool DiskMultiMap::Iterator::isValid() const { 
	return m_offset != 1 || !bf.isOpen(); 
}
DiskMultiMap::Iterator& DiskMultiMap::Iterator::operator++() {
	if (!isValid()) return Iterator(); //if iterator isn't valid 
	KeyValueContextTuple k;
	bf.read(k, m_offset);
	m_offset = k.next;
	return *this;
}
MultiMapTuple DiskMultiMap::Iterator::operator*() {
	if (!isValid()) return MultiMapTuple();
	KeyValueContextTuple k;
	bf.read(k, m_offset);
	return convert(k.key, k.value, k.context);
}

DiskMultiMap::DiskMultiMap() {
	header.numBuckets = 0;
	header.kvct_last_erased = -1;
	header.kt_last_erased = -1;
}
DiskMultiMap::~DiskMultiMap() {
	bf.close();
}

bool DiskMultiMap::createNew(const std::string& filename, unsigned int numBuckets) {
	if (bf.isOpen()) bf.close();

	if (bf.createNew(filename)) {
		m_filename = filename;

		header.numBuckets = numBuckets;
		bf.write(header, 0);

		for (int i = 0; i < numBuckets; i++) {
			bf.write(long(-1), sizeof(header) + i*sizeof(long));
		}
		return true;
	}
	else return false;
}
bool DiskMultiMap::openExisting(const std::string& filename) {
	if (bf.isOpen()) bf.close();

	if (bf.openExisting(filename)) {
		m_filename = filename;
		bf.read(header, 0);
		return true;
	}
	else return false;
}
void DiskMultiMap::close() {
	if(bf.isOpen()) bf.close();
}
bool DiskMultiMap::insert(const std::string& key, const std::string& value, const std::string& context) {
	if (!bf.isOpen()) return false;
	if (key.length() > 120 || value.length() > 120 || context.length() > 120) return false;
	long kvct_offset = -1;
	if (header.kvct_last_erased != -1) {
		kvct_offset = bf.fileLength();
	} else {
		kvct_offset = header.kvct_last_erased;
		bf.read(header.kvct_last_erased, header.kvct_last_erased); //reads new last_erased position from the last_erased position
	}
	KeyValueContextTuple kvct;
	strcpy_s(kvct.key, key.c_str()); strcpy_s(kvct.value, value.c_str()); strcpy_s(kvct.context, context.c_str()); kvct.next = -1;
	if (!bf.write(kvct, kvct_offset)) return false;

	unsigned int pos = (hash(key) % header.numBuckets);
	KeyTuple kt;
	long kt_offset = -1;
	bf.read(kt_offset, sizeof(unsigned int) + (2 + pos)*sizeof(long));
	if (kt_offset != -1) {
		//if there is already a keytuple at that hash
		do {
			bf.read(kt, kt_offset);
			kt_offset = kt.next;
		} while (kt_offset != -1 && !strcmp(kt.key, key.c_str));		
	}
	if (kt_offset == -1) {
		kt_offset = bf.fileLength();
	}
	
	///TODO:
	///look for a reusable position for kt_offset (instead of just adding to the end)
	///modify the previous node from -1 to the new kt_offset
	

	strcpy_s(kt.key, key.c_str()); kt.next = -1;
	if(kt.kvct_pos == -1) kt.kvct_pos = kvct_offset;
	else {
		///TODO: if this isn't the first node for the key, go to the end and modify from -1 to the new kvct_offset
	}

	
}
int main() {
	DiskMultiMap dmmp;
	cout << dmmp.openExisting("testfile.dmmp");

}