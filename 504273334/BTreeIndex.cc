/*
 * Copyright (C) 2008 by The Regents of the University of California
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL).
 *
 * @author Junghoo "John" Cho <cho AT cs.ucla.edu>
 * @date 3/24/2008
 */
 
#include "BTreeIndex.h"
#include "BTreeNode.h"

using namespace std;

const int MAX_KEY_NUM = 85;
const int PAGE_SIZE = PageFile::PAGE_SIZE;

/*
 * BTreeIndex constructor
 */
BTreeIndex::BTreeIndex()
{
    rootPid = -1;
}


/* 
 *write the current root and height into file
*/
RC BTreeIndex::updateRH()
{
	char temp [PAGE_SIZE];
	memset(temp, -1, PAGE_SIZE);

	char * idx = temp;

	memcpy(idx, &rootPid, sizeof(int));
	idx += sizeof(int);
	memcpy(idx, &treeHeight, sizeof(int));

	if(pf.write(0,temp)) return RC_FILE_WRITE_FAILED;

	return 0;
}

/*
 * Open the index file in read or write mode.
 * Under 'w' mode, the index file should be created if it does not exist.
 * @param indexname[IN] the name of the index file
 * @param mode[IN] 'r' for read, 'w' for write
 * @return error code. 0 if no error
 */
RC BTreeIndex::open(const string& indexname, char mode)
{	
	//if new file, initialize first page to have two ints
	//first int is rootpid, which is -1 for emtpy tree
	//second int is height, which is 0 for empty tree
	if (pf.endPid() == 0 && mode == 'w') {
		//open file
		if (pf.open(indexname, 'w'))
			return RC_FILE_OPEN_FAILED;		
		
		//temporary buffer
		char temp [PAGE_SIZE];
		memset(temp, -1, PAGE_SIZE);
		char * idx = temp;
		
		//fill first ints into buffer
		treeHeight = 0;
		rootPid = -1;
		memcpy(idx, &rootPid, sizeof(int));
		idx += sizeof(int);
		memcpy(idx, &treeHeight, sizeof(int));

		//write buffer to file
		if (pf.write(0, temp))
			return RC_FILE_WRITE_FAILED;
	} else {
		//open file
		if (pf.open(indexname, 'r'))
			return RC_FILE_OPEN_FAILED;

		//read the first page to initiate height and root
		char temp [PAGE_SIZE];
		if (pf.read(0,temp))
			return RC_FILE_READ_FAILED;

		//get height and root
		char * idx = temp;
		memcpy(&rootPid, idx, sizeof(int));
		idx += sizeof(int);
		memcpy(&treeHeight, idx, sizeof(int));

	}

	return 0;
}

/*
 * Close the index file.
 * @return error code. 0 if no error
 */
RC BTreeIndex::close()
{
    return pf.close();
}


/*
 * Get the parent node of the page ID
 * @param key[IN] the first key of child node
 * @param child[IN] the child node pageId
 * @param searchPid[IN] pid of current node to search in
 * @return -1 if not found. PageId if no error
 */
PageId BTreeIndex::getParent(const int& key, const PageId& child, const PageId& searchPid, int tHeight) {
	
	//emtpry tree
	if(rootPid == -1) return -1;

	//temp variables
	PageId tempId;
	BTNonLeafNode tempnode;

	//read page into temporary node and locate child pointer
	tempnode.read(searchPid, pf);
	tempnode.locateChildPtr(key, tempId);
	
	//check if child pointer found, if not recursively call
	if (tempId == child) return searchPid; 
	else if (tHeight == treeHeight-1) return -1;
	else return getParent(key, child, tempId, tHeight+1);
}


/*
 * Get the child node for the key
 * @param key[IN] the searchkey
 * @param searchPid[IN] pid of current node to search in
 * @return -1 if not found. PageId if no error
 */
PageId BTreeIndex::getChild(const int& key, const PageId& searchPid, int tHeight) {
	
	//empty tree
	if(rootPid == -1) return -1;

	//temp variables
	PageId tempId;
	BTNonLeafNode tempnode;

	//read page into temporary node and search for child pointer
	tempnode.read(searchPid, pf);
	tempnode.locateChildPtr(key, tempId);

	//recursive call until a height right before leaf nodes
	if (tHeight == treeHeight-1) return tempId;
	else return getChild(key, tempId, tHeight+1);
}

RC BTreeIndex::insertNonLeafNode(const PageId& Parent, const int& key, const PageId& PID, int tHeight) {
	int rc;

	BTNonLeafNode tempNode;
	tempNode.read(Parent, pf);

	if(tHeight == 1 && tempNode.getKeyCount() >= MAX_KEY_NUM) {
		//make new node
		BTNonLeafNode sibling;
		PageId siblingPid = pf.endPid();

		//split root into two
		int midKey;
		rc = tempNode.insertAndSplit(key, PID, sibling, midKey);
		if(rc) return rc;

		//write new node to disk
		rc = sibling.write(siblingPid, pf);
		if(rc) return rc;

		//write udpated node into disk
		rc = tempNode.write(Parent, pf);
		if(rc) return rc;

		//make new root
		BTNonLeafNode newRoot;
		PageId newRootPid = pf.endPid();

		//initialize new root
		rc = newRoot.initializeRoot(Parent, midKey, siblingPid);
		if(rc) return rc;

		//write new root to disk
		rc = newRoot.write(newRootPid, pf);
		if(rc) return rc;

		//update new root and height
		rootPid = newRootPid;
		treeHeight++;
		updateRH();

		return 0;
	} else if (tempNode.getKeyCount() >= MAX_KEY_NUM) {  //not root but still full
		
		//make new sibling
		BTNonLeafNode sibling;
		PageId siblingPid = pf.endPid();

		//split into two 
		int midKey;
		rc = tempNode.insertAndSplit(key, PID, sibling, midKey);
		if(rc) return rc;

		//write sibling to disk
		rc = sibling.write(siblingPid, pf);
		if(rc) return rc;

		rc = tempNode.write(Parent, pf);
		if(rc) return rc;

		//get the first key of tempNode
		int childKey = tempNode.getFirstKey();

		//use first key to get parent
		PageId gParent = getParent(childKey, Parent, rootPid, 1);
		
		//insert overflow key to parent
		rc = insertNonLeafNode(gParent, midKey, siblingPid, tHeight-1);
		if(rc) return rc;

		return 0;
	} else { //not full
		//simple insert no overflow
		rc = tempNode.insert(key, PID);
		if(rc) return rc;

		rc = tempNode.write(Parent, pf);
		if(rc) return rc;

		return 0;
	}
}

/*
 * Insert (key, RecordId) pair to the index.
 * @param key[IN] the key for the value inserted into the index
 * @param rid[IN] the RecordId for the record being inserted into the index
 * @return error code. 0 if no error
 */
RC BTreeIndex::insert(int key, const RecordId& rid)
{
	int rc;

	//if empty tree, create root node
	//root always points to 2 and 3 at first
	if (pf.endPid() == 1) {
		
		//create rootnode, initialize, write to first page
		BTNonLeafNode rootnode;
		rc = rootnode.initializeRoot(2,key,3);
		if(rc) return rc;
		rc = rootnode.write(1, pf);
		if(rc) return rc;

		//create right leafnode, initialize, write to page
		BTLeafNode leafright;
		rc = leafright.insert(key, rid);
		if(rc) return rc;
		rc = leafright.setNextNodePtr(-1);
		if(rc) return rc;
		rc = leafright.write(3, pf);
		if(rc) return rc;
		

		BTLeafNode leafleft;
		rc = leafleft.setNextNodePtr(3);
		if(rc) return rc;
		rc = leafleft.write(2, pf);
		if(rc) return rc;

		rootPid = 1;
		treeHeight = 2; // height will never be 1 or just root
		rc = updateRH();
		if(rc) return rc;


	} else {	//not empty
		
		//run getchild to get where key must be inserted
		PageId childPid = getChild(key, rootPid, 1);
		
		//read into childNode
		BTLeafNode childNode;
		rc = childNode.read(childPid, pf);
		if(rc) return rc;

		int keycount = childNode.getKeyCount();
		if(keycount == MAX_KEY_NUM) {
			//create sibling and find next page
			BTLeafNode siblingNode;
			PageId siblingPid = pf.endPid();

			//set next pointer for child and sibling
			PageId nextNode = childNode.getNextNodePtr();
			rc = siblingNode.setNextNodePtr(nextNode);
			if(rc) return rc;
			rc = childNode.setNextNodePtr(siblingPid);
			if(rc) return rc;

			//split childNode
			int siblingKey;
			rc = childNode.insertAndSplit(key, rid, siblingNode, siblingKey);

			//write both nodes into disk
			rc = childNode.write(childPid, pf);
			if(rc) return rc;
			rc = siblingNode.write(siblingPid, pf);
			if(rc) return rc;

			//get first key of childPid
			int childKey;
			RecordId childRid;
			childNode.readEntry(0, childKey, childRid);

			//get parent
			PageId parentPid = getParent(childKey, childPid, rootPid, 1);

			//insert siblingKey into parent
			rc = insertNonLeafNode(parentPid, siblingKey, siblingPid, treeHeight - 1);
			if(rc) return rc;

		} else if ( keycount < MAX_KEY_NUM) {
			rc = childNode.insert(key, rid);
			if(rc) return rc;

			rc = childNode.write(childPid, pf);
			if(rc) return rc;
		}
	}
    return 0;
}

/**
 * Run the standard B+Tree key search algorithm and identify the
 * leaf node where searchKey may exist. If an index entry with
 * searchKey exists in the leaf node, set IndexCursor to its location
 * (i.e., IndexCursor.pid = PageId of the leaf node, and
 * IndexCursor.eid = the searchKey index entry number.) and return 0.
 * If not, set IndexCursor.pid = PageId of the leaf node and
 * IndexCursor.eid = the index entry immediately after the largest
 * index key that is smaller than searchKey, and return the error
 * code RC_NO_SUCH_RECORD.
 * Using the returned "IndexCursor", you will have to call readForward()
 * to retrieve the actual (key, rid) pair from the index.
 * @param key[IN] the key to find
 * @param cursor[OUT] the cursor pointing to the index entry with
 *                    searchKey or immediately behind the largest key
 *                    smaller than searchKey.
 * @return 0 if searchKey is found. Othewise an error code
 */
RC BTreeIndex::locate(int searchKey, IndexCursor& cursor)
{
	PageId root = rootPid;
	PageId childPid;
	childPid = getChild(searchKey, root, 1);

	cursor.pid = childPid;

	BTLeafNode templeaf;
	templeaf.read(childPid, pf);

	return templeaf.locate(searchKey, cursor.eid);
}

/*
 * Read the (key, rid) pair at the location specified by the index cursor,
 * and move foward the cursor to the next entry.
 * @param cursor[IN/OUT] the cursor pointing to an leaf-node index entry in the b+tree
 * @param key[OUT] the key stored at the index cursor location.
 * @param rid[OUT] the RecordId stored at the index cursor location.
 * @return error code. 0 if no error
 */
RC BTreeIndex::readForward(IndexCursor& cursor, int& key, RecordId& rid)
{
	int rc;

	//store pid and eid
	PageId mPid = cursor.pid;
	int mEid = cursor.eid;

	//create buffer
	char temp [PAGE_SIZE];
	char* idx = temp;

	//create temporary node
	BTLeafNode tempnode;
	rc = tempnode.read(mPid, pf);
	if(rc) return rc;

	rc = tempnode.readEntry(mEid, key, rid);
	if(rc) return rc;

	
	if(mEid == 85) {
		PageId tempId;
		tempId = tempnode.getNextNodePtr();
		cursor.pid = tempId;
		cursor.eid = 0;
	} else {
		cursor.eid++;
	}

    return 0;
}
