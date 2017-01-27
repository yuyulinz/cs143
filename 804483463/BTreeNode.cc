#include "BTreeNode.h"
#include <string.h>
#include <stdio.h>
const int PAGE_SIZE = PageFile::PAGE_SIZE;
const int MAX_KEY_NUM = 85;
const int MAX_PAGEID_SIZE = sizeof(int);
using namespace std;

/*
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::read(PageId pid, const PageFile& pf)
{ 
	//initialize buffer to all -1 so that is read as a unset key
	memset(buffer, -1, PAGE_SIZE);
	return pf.read(pid, buffer); 
}
    
/*
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::write(PageId pid, PageFile& pf)
{ return pf.write(pid, buffer); }

/*
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTLeafNode::getKeyCount()
{ 	
	int key, keySize = sizeof(int);
	int entrySize = sizeof(RecordId) + sizeof(int);
	char* current_idx = buffer;
	int i, size = PAGE_SIZE - sizeof(int); //last entry will always be the PageId

	for (i = 0; i < size; i += entrySize, current_idx += entrySize) {
		memcpy(&key, current_idx, keySize);
		if (key == -1)
			break;
	}
	return i/entrySize; 
}

/*
 * Insert a (key, rid) pair to the node.
 * @param key[IN] the key to insert
 * @param rid[IN] the RecordId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTLeafNode::insert(int key, const RecordId& rid)
{ 
	int keySize = sizeof(int), recordSize = sizeof(RecordId);
	int entrySize = keySize + recordSize;
	int keyCount = getKeyCount();
	int eid;

	//initialize  new array to all -1
	char temp [PAGE_SIZE];
	memset(&temp, -1, PAGE_SIZE);
	int nextNode = 0;
	
	//make sure there is enough room in the node to insert
	if (keyCount >= MAX_KEY_NUM)
	  return RC_NODE_FULL;
	
	//find position to insert new entry and attach next node pointer at the end
	locate(key, eid);
	memcpy(&temp, buffer, PAGE_SIZE);

	//idx is where we want to insert
	char* idx = temp + (eid*entrySize);
	
	//size of buffer after insert point
	int suffixSize = (keyCount - eid) * entrySize;

	//put all the suffix into temp buffer
	char tempSuf [suffixSize];
	memcpy(&tempSuf, idx, suffixSize);
	
	//add this key into temp
	memcpy(idx, &key, keySize);
	idx += keySize;
	memcpy(idx, &rid, recordSize);

	//add the suffix back into rest of temp
	idx += recordSize;
	memcpy(idx, &tempSuf, suffixSize);

	//clear old buffer and fill it with new contents
	memset(buffer, -1, PAGE_SIZE);
	memcpy(buffer, temp, PAGE_SIZE);
	return 0;
}

/*
 * Insert the (key, rid) pair to the node
 * and split the node half and half with sibling.
 * The first key of the sibling node is returned in siblingKey.
 * @param key[IN] the key to insert.
 * @param rid[IN] the RecordId to insert.
 * @param sibling[IN] the sibling node to split with. This node MUST be EMPTY when this function is called.
 * @param siblingKey[OUT] the first key in the sibling node after split.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::insertAndSplit(int key, const RecordId& rid, 
                              BTLeafNode& sibling, int& siblingKey)
{ 
	if (sibling.getKeyCount() != 0)
		return RC_FILE_WRITE_FAILED;

	

	int eid = 0; int keyCount = getKeyCount();
	int leftOrRight = 0;

	//find where the key is to be inserted, if already inserted, dont insert and return error
	if (locate(key, eid) == 0)
	  return RC_INVALID_RID;

	//choose which sibling to insert, default left(0), else right(1)
	if (eid > keyCount/2)
	  leftOrRight = 1;
	
	//set the divide based on left or right
	int divide = keyCount/2;
	if (leftOrRight == 1)
	  divide++;
	
	int recordSize = sizeof(RecordId), keySize = sizeof(int);
	int entrySize = recordSize + keySize;

	//idx set to beginning of split
	char* idx = buffer + (divide * entrySize);
	
	//how much to copy
	int toCopy = (keyCount - divide) * entrySize;

	//save the next pointer of the sibling node and copy everything to the right
	//of divide into sibling
	int nextNode = sibling.getNextNodePtr();
	int copy_idx = 0;
	char temp [PAGE_SIZE];
	memcpy(&temp, idx, toCopy);
	idx = temp;
	
	//copynum is number of entries to move to sibling
	int copynum = divide;
        if(leftOrRight == 1)
	  copynum = divide-1;
	
	//copy all sibling entries
	while (copy_idx < copynum) {
		int copied_key; RecordId copied_rid;
		memcpy(&copied_key, idx, keySize);
		idx += keySize;
		memcpy(&copied_rid, idx, recordSize);
		if (sibling.insert(copied_key, copied_rid) != 0)
			return RC_FILE_WRITE_FAILED;
		idx += recordSize;
		copy_idx++;
	}
	sibling.setNextNodePtr(nextNode);

	//delete everything moved to sibling node from current node
	memset(&temp, -1, PAGE_SIZE);
	nextNode = getNextNodePtr();
	int toSave = divide * entrySize;
	memcpy(&temp, buffer, toSave);
	memset(&buffer, -1, PAGE_SIZE);

	//reset pageId pointer
	memcpy(&buffer, temp, toSave);
	setNextNodePtr(nextNode);

	//insert this key either to left of right
	if (leftOrRight == 0) {
	  if (insert(key, rid) != 0)
	    return RC_FILE_WRITE_FAILED;
	} else if (leftOrRight == 1) {
	  if (sibling.insert(key, rid) != 0)
	    return RC_FILE_WRITE_FAILED;
	}

	//set siblingKey to first entry
	RecordId filler;
	if (sibling.readEntry(0, siblingKey, filler) != 0)
	  return RC_FILE_READ_FAILED;

	return 0;
}

/**
 * If searchKey exists in the node, set eid to the index entry
 * with searchKey and return 0. If not, set eid to the index entry
 * immediately after the largest index key that is smaller than searchKey,
 * and return the error code RC_NO_SUCH_RECORD.
 * Remember that keys inside a B+tree node are always kept sorted.
 * @param searchKey[IN] the key to search for.
 * @param eid[OUT] the index entry number with searchKey or immediately
                   behind the largest key smaller than searchKey.
 * @return 0 if searchKey is found. Otherwise return an error code.
 */
RC BTLeafNode::locate(int searchKey, int& eid)
{ 
	int numKeys = getKeyCount();
	//empty node should insert at eid 0
	if (numKeys == 0) {
		eid = 0;
		return RC_NO_SUCH_RECORD;
	}

	int keySize = sizeof(int), recordSize = sizeof(RecordId);
	int entrySize = keySize + recordSize;	
	int key;
	int i;
	char * current_idx = buffer;
	int max = (numKeys*entrySize);
	for (i = 0; i < max ; i += entrySize, current_idx += entrySize) {
		//get the search key and recordID
		memcpy(&key, current_idx, keySize);
		if (key == searchKey) {
			eid = i/entrySize;
			return 0;
		}
		//searchKey not found
		else if (key >= searchKey) {
			eid = i/entrySize;
			return RC_NO_SUCH_RECORD;
		}
	}
	eid = i/entrySize;
	return  RC_NO_SUCH_RECORD;	
}

/*
 * Read the (key, rid) pair from the eid entry.
 * @param eid[IN] the entry number to read the (key, rid) pair from
 * @param key[OUT] the key from the entry
 * @param rid[OUT] the RecordId from the entry
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::readEntry(int eid, int& key, RecordId& rid)
{ 
	if (eid < 0 || eid >= MAX_KEY_NUM || eid>=getKeyCount()) //max key is 84
		return RC_NO_SUCH_RECORD;

	int recordSize = sizeof(RecordId);
	int keySize = sizeof(int);
	int entrySize = keySize + recordSize;

	//get the key
	char * idx = buffer + entrySize*eid;
	memcpy(&key, idx, keySize);
	idx += keySize;
	//get the rid
	memcpy(&rid, idx, recordSize);

	return 0;
}

/*
 * Return the pid of the next slibling node.
 * @return the PageId of the next sibling node 
 */
PageId BTLeafNode::getNextNodePtr()
{ 
	int entrySize = sizeof(RecordId) + sizeof(int);
	char* temp = buffer + (MAX_KEY_NUM * entrySize);
	int key;
	memcpy(&key, temp, MAX_PAGEID_SIZE);
	return key;
}

/*
 * Set the pid of the next slibling node.
 * @param pid[IN] the PageId of the next sibling node 
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::setNextNodePtr(PageId pid)
{ 
	int entrySize = sizeof(RecordId) + sizeof(int);
	char* temp = buffer + (MAX_KEY_NUM * entrySize);
	memcpy(temp, &pid, MAX_PAGEID_SIZE);
	return 0;
}

/*
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::read(PageId pid, const PageFile& pf)
{  
  memset(buffer, -1, PAGE_SIZE);
  return pf.read(pid, buffer); 
}
    
/*
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::write(PageId pid, PageFile& pf)
{ return pf.write(pid, buffer); }

/*
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTNonLeafNode::getKeyCount()
{
  int key, keySize = sizeof(int), pageSize = sizeof(PageId);
  int entrySize = sizeof(PageId) + sizeof(int);
  char* current_idx = buffer + sizeof(PageId); //first entry is always extra PageId
  int i, size = PAGE_SIZE;
  for (i = pageSize; i < size; i += entrySize, current_idx += entrySize) {
    memcpy(&key, current_idx, keySize);
    if (key == -1)
      break;
  }
  return i/entrySize;
}


/*
 * Insert a (key, pid) pair to the node.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTNonLeafNode::insert(int key, PageId pid)
{ 

  int keySize = sizeof(int), pageSize = sizeof(PageId);
  int entrySize = keySize + pageSize;
  int keyCount = getKeyCount();
  int eid;

  //initialize new array to all -1
  char temp [PAGE_SIZE];
  memset(&temp, -1, PAGE_SIZE);
  int nextNode = 0;

  //make sure there is enough room in the node to insert
  if (keyCount >= MAX_KEY_NUM)
    return RC_NODE_FULL;

  //find position to insert new entry and attach next node pointer at the end
  int findkey;
  int i;
  char * current_idx = buffer + pageSize;
  int maxSize = (keyCount*entrySize)+pageSize;
  for (i = pageSize; i < maxSize ; i += entrySize, current_idx+= entrySize) {
    //get the search key and recordID
    memcpy(&findkey, current_idx, keySize);
    //searchKey not found
    if (findkey >= key) {
      
      break;
    }
  }
  eid = i;

  //copy buffer into temp
  memcpy(&temp, buffer, PAGE_SIZE);

  //idx is where we want to insert
  char* idx = temp + eid;

  //size of buffer after insert point
  int suffixSize = (keyCount*entrySize)+pageSize - eid;

  //put all the suffix into temp buffer
  char tempSuf [suffixSize];
  memcpy(&tempSuf, idx, suffixSize);

  //add this key into temp
  memcpy(idx, &key, keySize);
  idx += keySize;
  memcpy(idx, &pid, pageSize);

  //add suffix back into temp
  idx += pageSize;
  memcpy(idx, &tempSuf, suffixSize);

  //clear old buffer and fill it with new contents
  memset(buffer, -1, PAGE_SIZE);
  memcpy(buffer, temp, PAGE_SIZE);
  return 0;

 }

/*
 * Insert the (key, pid) pair to the node
 * and split the node half and half with sibling.
 * The middle key after the split is returned in midKey.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @param sibling[IN] the sibling node to split with. This node MUST be empty when this function is called.
 * @param midKey[OUT] the key in the middle after the split. This key should be inserted to the parent node.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::insertAndSplit(int key, PageId pid, BTNonLeafNode& sibling, int& midKey)
{
  if (sibling.getKeyCount() != 0)
    return RC_FILE_WRITE_FAILED;


  int eid = 0; int keyCount = getKeyCount();
  int leftOrRight = 0;

  //find where the key is inserted
  int keySize = sizeof(int);
  int pageSize = sizeof(int);
  int entrySize = keySize + pageSize;

  int findkey;
  int i;
  char * current_idx = buffer + pageSize;
  int maxSize = (keyCount*entrySize)+pageSize;
  for (i = pageSize; i < maxSize ; i += entrySize, current_idx+= entrySize) {
    //get the search key and recordID
    memcpy(&findkey, current_idx, keySize);
    if (findkey >= key || findkey == -1) {      
      break;
    }
  }
  eid = i;

  //if eid is greater than 44 nodes
  if (eid > (((keyCount/2)+1)*entrySize)+pageSize)
    leftOrRight = 1;

  int divide = keyCount/2 + 1;
  if (leftOrRight == 1)
    divide++;

  //idx is beginning of copy
  char* idx = buffer + pageSize + (divide*entrySize);

  //tocopy is how much to copy
  int toCopy = (keyCount - divide)*entrySize;

  //create temp buffer and copy the last half
  char temp [PAGE_SIZE];
  memcpy(&temp, idx, toCopy);

  //delete copied data
  memset(idx, -1, toCopy);

  //get the first pageid for sibling
  //insert node first if needed
  if (leftOrRight == 0) {
  	if (insert(key, pid))
  		return RC_FILE_WRITE_FAILED;
  	idx = idx + keySize;
  } else {
  	idx = idx - pageSize;
  }
  
  //idx now has the pageid for sibling
  //initialize sibling node with root
  PageId p1;
  PageId p2;
  int k;
  memcpy(&p1, idx, pageSize);
  char * temphead = temp;
  memcpy(&k, temphead, keySize);
  temphead = temphead + keySize;
  memcpy(&p2, temphead, pageSize);
  if (sibling.initializeRoot(p1, k, p2)){
  	return RC_FILE_WRITE_FAILED;
  }

  //copy rest of temp buffer to sibling.
  temphead += pageSize;
  for(i = entrySize; i < toCopy; i += entrySize) {
  	memcpy(&k, temphead, keySize);
  	memcpy(&p1, temphead+keySize, pageSize);

  	if(sibling.insert(k, p1)){
  		return RC_FILE_WRITE_FAILED;
  	}
  	temphead += entrySize;
  }

  //take midkey out of left node
  idx = idx - pageSize;
  memcpy(&midKey, idx, keySize);
  
  //delete the midkey
  memset(idx, -1, entrySize);

  //inser to right side if right side
  if (leftOrRight == 1) {
  	if (sibling.insert(key, pid))
  		return RC_FILE_WRITE_FAILED;
  }
  return 0;
}

/*
 * Given the searchKey, find the child-node pointer to follow and
 * output it in pid.
 * @param searchKey[IN] the searchKey that is being looked up.
 * @param pid[OUT] the pointer to the child node to follow.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::locateChildPtr(int searchKey, PageId& pid)
{ 
  
  int numKeys = getKeyCount();
  //empty node should insert at eid 0
  if (numKeys == 0) {
    pid = 0;
    return RC_INVALID_PID;
  }

  int keySize = sizeof(int), pageSize = sizeof(PageId);
  int entrySize = keySize + pageSize;
  int key;
  PageId tempPid;
  int i;
  char * current_idx = buffer;
  int maxSize = numKeys*entrySize + pageSize;
  for (i = 0; i < maxSize-pageSize; i += entrySize, current_idx+= entrySize) {
    memcpy(&tempPid, current_idx, pageSize);
    memcpy(&key, current_idx+pageSize, keySize);
    if (key > searchKey) {
      pid = tempPid;
      return 0;
    }
  }
  
  //key is greater than all
  memcpy(&tempPid, current_idx, pageSize);
  pid = tempPid;
  return  0; 
  
}

/*
 * Initialize the root node with (pid1, key, pid2).
 * @param pid1[IN] the first PageId to insert
 * @param key[IN] the key that should be inserted between the two PageIds
 * @param pid2[IN] the PageId to insert behind the key
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::initializeRoot(PageId pid1, int key, PageId pid2)
{ 
  int keySize = sizeof(int);
  int pageIdSize = sizeof(PageId);
  //make sure not already initialized
  int keyCount = getKeyCount();
  if(keyCount > 0)
    return RC_INVALID_ATTRIBUTE; //error code added by leon

  //create temp buffer
  char temp [PAGE_SIZE];
  memset(&temp, -1, PAGE_SIZE);

  //pointer to beginnign of temp
  char* idx = temp; 

  //copy three inputs into temp
  memcpy(idx, &pid1, pageIdSize);
  idx += pageIdSize;
  memcpy(idx, &key, keySize);
  idx += keySize;
  memcpy(idx, &pid2, pageIdSize);

  //copy into buffer;
  memcpy(buffer, &temp, PAGE_SIZE);

  return 0; 
}


int BTNonLeafNode::getFirstKey(){
  if(getKeyCount() == 0) return -1;
  char* idx = buffer + sizeof(int);
  int firstkey;
  memcpy(&firstkey, idx, sizeof(int));
  return firstkey;
}