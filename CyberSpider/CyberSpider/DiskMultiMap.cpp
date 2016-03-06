#include "DiskMultiMap.h"

DiskMultiMap::Iterator::Iterator() {
	m_offset = -1;
}
DiskMultiMap::Iterator::Iterator(const std::string& filename, long offset) {
	m_offset = offset;
	m_filename = filename;
	bf.openExisting(filename);
}
DiskMultiMap::Iterator::Iterator(const Iterator& ob) {
	m_offset = ob.m_offset;
	m_filename = ob.m_filename;
	bf.openExisting(ob.m_filename);
}
DiskMultiMap::Iterator::~Iterator() {
	bf.close();
}
bool DiskMultiMap::Iterator::isValid() const { 
	return m_offset != -1 && bf.isOpen(); 
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
	KeyValueContextTuple kvct;
	bf.read(kvct, m_offset);
	return convert(kvct.key, kvct.value, kvct.context);
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
	if (header.kvct_last_erased == -1) {
		kvct_offset = bf.fileLength();
	} else {
		kvct_offset = header.kvct_last_erased;
		bf.read(header.kvct_last_erased, header.kvct_last_erased); //reads new last_erased position from the last_erased position
	}
	KeyValueContextTuple kvct;
	strcpy_s(kvct.key, key.c_str()); strcpy_s(kvct.value, value.c_str()); strcpy_s(kvct.context, context.c_str()); kvct.next = -1; kvct.m_offset = kvct_offset;
	if (!bf.write(kvct, kvct_offset)) return false;

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
			//that key already exists (and kt has the same value)
			long kvct_pos = kt.kvct_pos;
			KeyValueContextTuple prev;
			do {
				bf.read(prev, kvct_pos);
				kvct_pos = prev.next;
			} while (kvct_pos != -1);
			prev.next = kvct_offset;
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
		strcpy_s(kt.key, key.c_str()); kt.next = -1; kt.kvct_pos = kvct_offset; kt.m_offset = kt_offset;
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
		return Iterator(m_filename, kt.kvct_pos);
	}
}

int main()
{
	DiskMultiMap x;
	x.createNew("myhashtable.dat", 100); // empty, with 100 buckets
	x.insert("hmm.exe", "pfft.exe", "m52902");
	x.insert("hmm.exe", "pfft.exe", "m52902");
	x.insert("hmm.exe", "pfft.exe", "m10001");
	x.insert("blah.exe", "bletch.exe", "m0003");
	DiskMultiMap::Iterator it = x.search("hmm.exe");
	if (it.isValid())
	{
		cout << "I found at least 1 item with a key of hmm.exe\n";
		do
		{
			MultiMapTuple m = *it; // get the association
			cout << "The key is: " << m.key << endl;
			cout << "The value is: " << m.value << endl;
			cout << "The context is: " << m.context << endl;
			cout << endl;
			++it; // advance iterator to the next matching item
		} while (it.isValid());
	}
}