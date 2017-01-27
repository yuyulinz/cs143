/**
 * Copyright (C) 2008 by The Regents of the University of California
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL).
 *
 * @author Junghoo "John" Cho <cho AT cs.ucla.edu>
 * @date 3/24/2008
 */
 
#include "Bruinbase.h"
#include "SqlEngine.h"
#include "BTreeNode.h"
#include "BTreeIndex.h"
#include <cstdio>
#include <stdio.h>

/*
int main(void)
{
	BTreeIndex indexTree;
	indexTree.open("btreetest2", 'w');

	int key1 = 1;
	int key2 = 2;
	int key3 = 3;
	int key4 = 4;
	int key5 = 5;
	int key6 = 6;
	int key7 = 7;
	int key8 = 8;
	int key9 = 9;
	int key10 = 10;
	int key11 = 11;
	int key12 = 12;
	int key13 = 13;
	int key14 = 14;
	int key15 = 15;
	int key16 = 16;
	int key17 = 17;
	int key18 = 18;
	int key19 = 19;
	int key20 = 20;
	int key21 = 21;
	int key22 = 22;	
	int key23 = 23;
	int key24 = 24;
	int key25 = 25;
	int key26 = 26;
	int key27 = 27;
	int key28 = 28;
	int key29 = 29;
	int key30 = 30;
	int key31 = 31;
	int key32 = 32;
	int key33 = 33;
	int key34 = 34;
	int key35 = 35;
	int key36 = 36;
	int key37 = 37;
	int key38 = 38;
	int key39 = 39;
	int key40 = 40;
	int key41 = 41;
	int key42 = 42;
	int key43 = 43;
	int key44 = 44;
	int key45 = 45;
	int key46 = 46;
	int key47 = 47;
	int key48 = 48;
	int key49 = 49;
	int key50 = 50;
	int key51 = 51;
	int key52 = 52;
	int key53 = 53;
	int key54 = 54;
	int key55 = 55;
	int key56 = 56;
	int key57 = 57;
	int key58 = 58;
	int key59 = 59;
	int key60 = 60;
	int key61 = 61;
	int key62 = 62;
	int key63 = 63;
	int key64 = 64;
	int key65 = 65;
	int key66 = 66;
	int key67 = 67;
	int key68 = 68;
	int key69 = 69;
	int key70 = 70;
	int key71 = 71;
	int key72 = 72;
	int key73 = 73;
	int key74 = 74;
	int key75 = 75;
	int key76 = 76;
	int key77 = 77;
	int key78 = 78;
	int key79 = 79;
	int key80 = 80;
	int key81 = 81;
	int key82 = 82;
	int key83 = 83;
	int key84 = 84;
	int key85 = 85;
	int key86 = 86;



	RecordId rid1 = {1,2};
int i;

for(i = 1;i < 86; i++){
	indexTree.insert(i,rid1);
}

int key;
IndexCursor tempc;
RecordId rid;

//test nonleaf node insert
	RecordId rid2 = {2,3};
	for(i = 86; i <=172; i++){
		indexTree.insert(i,rid2);
	}

	
	for(i = 1; i <=172; i++){
		indexTree.locate(i,tempc);
		printf("pid: %d, eid: %d\n", tempc.pid, tempc.eid );
		int RC = indexTree.readForward(tempc, key, rid);
		printf("record is %d, %d\n", rid.pid, rid.sid);		
		printf("cursor is now at %d, %d, RC is %d\n", tempc.pid, tempc.eid, RC);
	}
	printf("\n\n");
	

	PageId child = 6;
	PageId searchPid = 1;
	PageId parent = indexTree.getParent(160, child, searchPid, 1);
	printf("parent for 160 is:%d\n", parent );

	PageId childPid = indexTree.getChild(40, searchPid, 1);
	printf("child of 40 is: %d\n", childPid);

	child = 5;
	parent = indexTree.getParent(110,child,searchPid,1);
	printf("parent for 110 is:%d\n", parent);

	childPid = indexTree.getChild(171, searchPid, 1);
	printf("child of 171 is: %d\n", childPid);	

	indexTree.locate(345, tempc);
	printf("locate cursor to be %d, %d\n", tempc.pid, tempc.eid);		
	int RC = indexTree.readForward(tempc, key, rid);
	printf("cursor is %d, %d, RC is %d\n", tempc.pid, tempc.eid, RC);
	printf("record is %d, %d\n", rid.pid, rid.sid);

	indexTree.locate(172, tempc);
	RC = indexTree.readForward(tempc, key, rid);
	printf("cursor is %d, %d, RC is %d\n", tempc.pid, tempc.eid, RC);
	printf("record is %d, %d\n", rid.pid, rid.sid);	
	remove("btreetest2");

	PageId child = 1;
	PageId searchPid = 90;
	PageId parent = indexTree.getParent(1,child,searchPid,1);
	printf("parent:%d\n", parent );

	searchPid = 90;
	PageId childPid = indexTree.getChild(3656, searchPid, 1);
	printf("child: %d\n", childPid);


	IndexCursor tempc;
	indexTree.locate(3656,tempc);

	printf("pid: %d, eid: %d\n",tempc.pid, tempc.eid );
	
}
*/
int main() {
  // run the SQL engine taking user commands from standard input (console).
  SqlEngine::run(stdin);

  return 0;
}

