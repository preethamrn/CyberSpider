#include "DiskMultiMap.h"
#include "BinaryFile.h"
#include <cstring>
#include <string>

DiskMultiMap::Iterator::Iterator() {
	m_offset = -1;
}
DiskMultiMap::Iterator::Iterator(BinaryFile* file, long offset, std::string k) {
	m_offset = offset;
	bf = file;
	key = k;
}
bool DiskMultiMap::Iterator::isValid() const { 
	return m_offset != -1 && bf->isOpen();
}
DiskMultiMap::Iterator& DiskMultiMap::Iterator::operator++() {
	if (isValid()) {
		//if the iterator is valid, go to the next one (otherwise it will just return this iterator without changes which isn't valid)
		ValueContextTuple vct;
		bf->read(vct, m_offset);
		m_offset = vct.next;
	}
	return *this;
}
MultiMapTuple DiskMultiMap::Iterator::operator*() {
	if (!isValid()) return MultiMapTuple(); //if the iterator isn't valid, return an empty multimap
	ValueContextTuple vct;
	bf->read(vct, m_offset);
	return convert(key.c_str(), vct.value, vct.context);
}

DiskMultiMap::DiskMultiMap() {
	header.numBuckets = 0;
	header.vct_last_erased = -1;
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
	long vct_offset = -1;
	if (header.vct_last_erased == -1) {
		vct_offset = bf.fileLength();
	} else {
		vct_offset = header.vct_last_erased;
		bf.read(header.vct_last_erased, header.vct_last_erased); //reads new last_erased position from the last_erased position
	}
	ValueContextTuple vct;
	strcpy_s(vct.value, value.c_str()); strcpy_s(vct.context, context.c_str()); vct.next = -1; vct.m_offset = vct_offset;
	if (!bf.write(vct, vct_offset)) return false;

	unsigned int pos = (hash(key) % header.numBuckets);
	KeyTuple kt; kt.m_offset = -1;
	long kt_offset = -1;
	bf.read(kt_offset, sizeof(DiskHeader) + pos*sizeof(long));
	if (kt_offset != -1) {
		//if there is already a KeyTuple at that hash
		do {
			bf.read(kt, kt_offset);
		} while (strcmp(kt.key, key.c_str()) && (kt_offset = kt.next) != -1);
		if (kt_offset != -1) {
			//that key already exists in the KeyTuple kt
			long vct_pos = kt.vct_pos;
			ValueContextTuple prev;
			do {
				bf.read(prev, vct_pos);
				vct_pos = prev.next;
			} while (vct_pos != -1);
			prev.next = vct_offset; //pushing to back of list
			if (!bf.write(prev, prev.m_offset)) return false;
		}
	}
	if (kt_offset == -1) {
		//look for a reusable position or end of the file for kt_offset
		if (header.kt_last_erased == -1) {
			kt_offset = bf.fileLength();
		} else {
			kt_offset = header.kt_last_erased;
			bf.read(header.kt_last_erased, header.kt_last_erased); //reads new last_erased position from the last_erased position
		}
		if (kt.m_offset != -1) {
			//there is already a KeyTuple at that hash
			kt.next = kt_offset;
			if(!bf.write(kt, kt.m_offset)) return false;
		} else {
			//there are no KeyTuples at that hash
			if (!bf.write(kt_offset, sizeof(DiskHeader) + pos*sizeof(long))) return false;
		}
		strcpy_s(kt.key, key.c_str()); kt.next = -1; kt.vct_pos = vct_offset; kt.m_offset = kt_offset;
		if(!bf.write(kt, kt_offset)) return false;
	}
	
	return true;
}

DiskMultiMap::Iterator DiskMultiMap::search(const std::string& key) {
	long offset = -1;
	unsigned int pos = hash(key) % header.numBuckets;
	bf.read(offset, sizeof(DiskHeader) + pos*sizeof(long));
	KeyTuple kt;
	while(offset != -1 && bf.read(kt, offset) && strcmp(kt.key, key.c_str())) {
		offset = kt.next;
	}
	if (offset == -1) return Iterator();
	else {
		return Iterator(&bf, kt.vct_pos, kt.key);
	}
}

int DiskMultiMap::erase(const std::string& key, const std::string& value, const std::string& context) {
	long offset = -1;
	unsigned int pos = hash(key) % header.numBuckets;
	bf.read(offset, sizeof(DiskHeader) + pos*sizeof(long));
	KeyTuple kt, prev_kt; prev_kt.m_offset = -1;
	while (offset != -1) {
		bf.read(kt, offset);
		if (!strcmp(kt.key, key.c_str())) break;
		else prev_kt = kt;
		offset = kt.next;
	}
	if (offset == -1) return 0;
	ValueContextTuple prev, curr;
	long vct_offset = kt.vct_pos;
	int num_deleted = 0; //number of deleted items to be returned
	//update kt associated with vct so it keeps pointing to correct head of linked list
	//this only checks for the nodes that need to be erased from the start of the linked list
	while (vct_offset != -1 && bf.read(curr, vct_offset) && !strcmp(curr.value, value.c_str()) && !strcmp(curr.context, context.c_str())) {
		vct_offset = curr.next; kt.vct_pos = vct_offset;
		bf.write(kt, kt.m_offset);
		bf.write(header.vct_last_erased, curr.m_offset);
		header.vct_last_erased = curr.m_offset;
		num_deleted++;
	}
	if (vct_offset == -1) { //ie. all the nodes from start to end match the key, value, and context
		//update kt_last_erased and erase this kt
		if (prev_kt.m_offset != -1) {
			prev_kt.next = kt.next;
			bf.write(prev_kt, prev_kt.m_offset);
		} else {
			//since prev_kt hasn't been updated, we know kt is at the head of the bucket
			bf.write(kt.next, sizeof(DiskHeader) + pos*sizeof(long)); //update the bucket pointer
		}
		bf.write(header.kt_last_erased, kt.m_offset);
		header.kt_last_erased = kt.m_offset;
	}
	//erase and update remaining nodes of linked list that match
	while(vct_offset != -1) {
		bf.read(curr, vct_offset);
		//check whether curr needs to be deleted
		if (!strcmp(curr.value, value.c_str()) && !strcmp(curr.context, context.c_str())) {
			prev.next = curr.next;
			bf.write(prev, prev.m_offset);
			bf.write(header.vct_last_erased, curr.m_offset);
			header.vct_last_erased = curr.m_offset;
			num_deleted++;
		} else {
			prev = curr;
		}
		vct_offset = curr.next;
	}
	bf.write(header, 0);
	return num_deleted;
}