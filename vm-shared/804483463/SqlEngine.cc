/**
 * Copyright (C) 2008 by The Regents of the University of California
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL).
 *
 * @author Junghoo "John" Cho <cho AT cs.ucla.edu>
 * @date 3/24/2008
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include "Bruinbase.h"
#include "SqlEngine.h"

using namespace std;

// external functions and variables for load file and sql command parsing 
extern FILE* sqlin;
int sqlparse(void);


RC SqlEngine::run(FILE* commandline)
{
  fprintf(stdout, "Bruinbase> ");

  // set the command line input and start parsing user input
  sqlin = commandline;
  sqlparse();  // sqlparse() is defined in SqlParser.tab.c generated from
               // SqlParser.y by bison (bison is GNU equivalent of yacc)

  return 0;
}

RC SqlEngine::select(int attr, const string& table, const vector<SelCond>& cond)
{
 // printf("start select\n");
  RecordFile rf;   // RecordFile containing the table
  RecordId   rid;  // record cursor for table scanning
  RecordId   rid_min;
  RecordId   rid_max;

  RC     rc = 0;
  int    key;     
  string value;
  int    count;
  int    diff;
  BTreeIndex b_idx;
  SelCond condition;

  bool useIndex = false; 
  bool EqualCond = false;

  //min and max are INCLUDED in the key search, so start AND end on them
  int key_min = -1;
  int key_max = -1;
  int key_equal_val = -1;
  int key_noteq_val;  

  char* value_equal_val = NULL;
  char* value_noteq_val = NULL;


  // check the conditions for traversing the index/table
  for (unsigned i = 0; i < cond.size(); i++) {
    condition = cond[i];
    int val = atoi(condition.value);

    switch (condition.attr) {
    //key attribute
    case 1:
      switch (condition.comp) {

        case SelCond::NE:
        key_noteq_val = val;
          useIndex = false;
          break;
       
        case SelCond::EQ:
          useIndex = true;
          EqualCond = true;
          //check mult eq
          if (key_equal_val != -1 && key_equal_val != val)
            return rc;
          key_equal_val = val;
          break;



        //else figure out the correct range of values to search
        case SelCond::LT:
          useIndex = true;
          if (val < key_max || key_max == -1)
            key_max = val;
          break;
        case SelCond::GT:
          useIndex = true;
          if (val > key_min || key_min == -1)
            key_min = val + 1;
          break;
        case SelCond::LE:
          useIndex = true;
          if (val < key_max || key_max == -1)
            key_max = val + 1;
          break;
        case SelCond::GE:
          useIndex = true;
          if (val > key_min || key_min == -1)        
            key_min = val;
          break;
      }
      break;
    //value attribute
    //check if there are multiple equality statements, if so invalid query
    case 2:
      char* val = condition.value;
      useIndex = false;
      if(SelCond::EQ == condition.comp) {
          //multiple equality statements is invalid
          if (value_equal_val != NULL && strcmp(value_equal_val, val) != 0)
            return rc;
          value_equal_val = val;
      }
      break;

    }
  } 
  //printf("keymax is %d    ", key_max);
  //printf("keymin is %d    ", key_min);

  // open the table file
  if ((rc = rf.open(table + ".tbl", 'r')) < 0) {
    fprintf(stderr, "Error: table %s does not exist\n", table.c_str());
    return rc;
  }




  //open BTreeIndex to find valid tuples
  if(useIndex){
   // printf("using index");
    count = 0;

    //open index table
    if ((rc = b_idx.open(table + ".idx", 'r')) < 0){
      fprintf(stderr, "Error: index %s does not exist\n", table.c_str() );
      goto exit_select;
    }

    //if key_min, is higher than key_max, then not possible
    if (key_min > key_max && key_max != -1)
      return rc;

    //equal condition or not
    if(EqualCond == true){


      //locate rid using key
      IndexCursor CID;
      rc = b_idx.locate(key_equal_val, CID);
      if (rc == 0){
        rc = b_idx.readForward(CID, key, rid);
        if (rc != 0) return rc;
      } else {
        return rc;
      }

      // read the tuple
      if ((rc = rf.read(rid, key, value)) < 0) {
        fprintf(stderr, "Error: while reading a tuple from table %s\n", table.c_str());
        return rc;
      }

      // check the conditions on the tuple
      for (unsigned i = 0; i < cond.size(); i++) {
        // compute the difference between the tuple value and the condition value
        switch (cond[i].attr) {
          case 1:
            diff = key - atoi(cond[i].value);
            break;
          case 2:
            diff = strcmp(value.c_str(), cond[i].value);
            break;
        }

         // skip the tuple if any condition is not met
        switch (cond[i].comp) {
        case SelCond::EQ:
          if (diff != 0) goto exit_select_i;
          break;
        case SelCond::NE:
          if (diff == 0) goto exit_select_i;
          break;
        case SelCond::GT:
          if (diff <= 0) goto exit_select_i;
          break;
        case SelCond::LT:
          if (diff >= 0) goto exit_select_i;
          break;
        case SelCond::GE:
          if (diff < 0) goto exit_select_i;
          break;
        case SelCond::LE:
          if (diff > 0) goto exit_select_i;
          break;
        }
      }
//printf("1");
      // the condition is met for the tuple. 
      // increase matching tuple counter
      count++;  
      // print the tuple 
      switch (attr) {
      case 1:  // SELECT key
        fprintf(stdout, "%d\n", key);
        break;
      case 2:  // SELECT value
        fprintf(stdout, "%s\n", value.c_str());
        break;
      case 3:  // SELECT *
        fprintf(stdout, "%d '%s'\n", key, value.c_str());
        break;
      }

      if (attr == 4) {
        fprintf(stdout, "%d\n", count);
      }

      exit_select_i:;

    } else {

      int key_min_i;
      string value_mi;

      int key_max_i;
      string value_ma;

      if(key_min == -1) key_min = 1;
      IndexCursor cid_min;
      b_idx.locate(key_min, cid_min);
      rc = b_idx.readForward(cid_min, key_min_i, rid_min);
      if (rc != 0) {
        if (cid_min.pid == 2 && cid_min.eid == 0){
          cid_min.pid = 3;
          b_idx.readForward(cid_min, key_min_i, rid_min);
        }
        else return rc;
      }
      
      //printf("key_min_i is %d       ", key_min_i);

      if(key_max != -1){
        IndexCursor cid_max;
        b_idx.locate(key_max, cid_max);
        rc = b_idx.readForward(cid_max, key_max_i, rid_max);
        if (rc != 0) return rc;
      }


      while(key_max == -1 || key_min_i < key_max_i){

        // read the tuple
        if ((rc = rf.read(rid_min, key_min_i, value_mi)) < 0) {
          fprintf(stderr, "Error1: while reading a tuple from table %s\n", table.c_str());
          return rc;
        }

        for (unsigned i = 0; i < cond.size(); i++) {
          // compute the difference between the tuple value and the condition value
          switch (cond[i].attr) {
            case 1:
              diff = key_min_i - atoi(cond[i].value);
              break;
            case 2:
              diff = strcmp(value_mi.c_str(), cond[i].value);
              break;
          }

          // skip the tuple if any condition is not met
          switch (cond[i].comp) {
            case SelCond::EQ:
              if (diff != 0) goto next_key;
              break;
            case SelCond::NE:
              if (diff == 0) goto next_key;
              break;
            case SelCond::GT:
              if (diff <= 0) goto next_key;
              break;
            case SelCond::LT:
              if (diff >= 0) goto next_key;
              break;
            case SelCond::GE:
              if (diff < 0) goto next_key;
              break;
            case SelCond::LE:
              if (diff > 0) goto next_key;
              break;
          }
        }
//printf("2");
        // the condition is met for the tuple. 
        // increase matching tuple counter
        count++;
        // print the tuple 
        switch (attr) {
          case 1:  // SELECT key
            fprintf(stdout, "%d\n", key_min_i);
            break;
          case 2:  // SELECT value
            fprintf(stdout, "%s\n", value_mi.c_str());
            break;
          case 3:  // SELECT *
            fprintf(stdout, "%d '%s'\n", key_min_i, value_mi.c_str());
            break;
        }

        next_key:
        rc = b_idx.readForward(cid_min, key_min_i, rid_min);
        if (rc != 0) {
          //printf("broke");
          break;
        }
      }

      if (attr == 4) {
          fprintf(stdout, "%d\n", count);
      }

    }

    rc = b_idx.close();

  } else {
    
    //printf("not using index\n");

    // scan the table file from the beginning
    rid.pid = rid.sid = 0;
    count = 0;
    while (rid < rf.endRid()) {
      // read the tuple
      if ((rc = rf.read(rid, key, value)) < 0) {
        fprintf(stderr, "Error: while reading a tuple from table %s\n", table.c_str());
        goto exit_select;
      }

      //printf("next\n");
      // check the conditions on the tuple
      for (unsigned i = 0; i < cond.size(); i++) {
        // compute the difference between the tuple value and the condition value
        switch (cond[i].attr) {
          case 1:
            diff = key - atoi(cond[i].value);
            break;
          case 2:
            diff = strcmp(value.c_str(), cond[i].value);
            break;
        }

        // skip the tuple if any condition is not met
        switch (cond[i].comp) {
        case SelCond::EQ:
          if (diff != 0) goto next_tuple;
          break;
        case SelCond::NE:
          if (diff == 0) goto next_tuple;
          break;
        case SelCond::GT:
          if (diff <= 0) goto next_tuple;
          break;
        case SelCond::LT:
          if (diff >= 0) goto next_tuple;
          break;
        case SelCond::GE:
          if (diff < 0) goto next_tuple;
          break;
        case SelCond::LE:
          if (diff > 0) goto next_tuple;
          break;
        }
      }
      // the condition is met for the tuple. 
      // increase matching tuple counter
      count++;
//printf("3");
      // print the tuple 
      switch (attr) {
      case 1:  // SELECT key
        fprintf(stdout, "%d\n", key);
        break;
      case 2:  // SELECT value
        fprintf(stdout, "%s\n", value.c_str());
        break;
      case 3:  // SELECT *
        fprintf(stdout, "%d '%s'\n", key, value.c_str());
        break;
      }

      // move to the next tuple
      next_tuple:
      ++rid;
    }
  

    // print matching tuple count if "select count(*)"
    if (attr == 4) {
      fprintf(stdout, "%d\n", count);
    }

  }
    
  
  

  // close the table file and return
  exit_select: 
  rf.close();
  return rc;
}

RC SqlEngine::load(const string& table, const string& loadfile, bool index)
{
    //open the new file for RecordFile
    RecordFile newRecord;
    int rc;
    if ((rc = newRecord.open(table + ".tbl", 'w'))) {
        fprintf(stderr, "Error creating record file for table with error number %d\n", rc);
        return rc;
    }

    //create the index
    BTreeIndex b_idx;
    if (index)
        b_idx.open(table + ".idx", 'w');

    fstream file;
    string line;
    //open the loadfile
    file.open(loadfile.c_str(), fstream::in);
    if (file.fail())
        return RC_FILE_OPEN_FAILED;

    //parse loadfile and insert into RecordFile
    while (getline(file, line, '\n') && file.good()) {
        int key;
        string value;
        RecordId rid;
        if (parseLoadLine(line, key, value) == 0) {
            if (newRecord.append(key, value, rid) == 0) {
                //insert into index
                if (index) {
                    if(key == 4733) printf("FOUND IT");
                    if (b_idx.insert(key, rid) != 0) {
                        fprintf(stderr, "failed to write to index.\n");
                        return RC_FILE_WRITE_FAILED;                        
                    }
                }
            }
            else
              return RC_INVALID_ATTRIBUTE;
        } else
            return RC_INVALID_ATTRIBUTE;
    }

    //check for file close failure
    file.close();
    rc = b_idx.close();
    if (file.fail() || newRecord.close() < 0 || rc)
        return RC_FILE_CLOSE_FAILED;
    return 0;
}

RC SqlEngine::parseLoadLine(const string& line, int& key, string& value)
{
    const char *s;
    char        c;
    string::size_type loc;
    
    // ignore beginning white spaces
    c = *(s = line.c_str());
    while (c == ' ' || c == '\t') { c = *++s; }

    // get the integer key value
    key = atoi(s);

    // look for comma
    s = strchr(s, ',');
    if (s == NULL) { return RC_INVALID_FILE_FORMAT; }

    // ignore white spaces
    do { c = *++s; } while (c == ' ' || c == '\t');
    
    // if there is nothing left, set the value to empty string
    if (c == 0) { 
        value.erase();
        return 0;
    }

    // is the value field delimited by ' or "?
    if (c == '\'' || c == '"') {
        s++;
    } else {
        c = '\n';
    }

    // get the value string
    value.assign(s);
    loc = value.find(c, 0);
    if (loc != string::npos) { value.erase(loc); }

    return 0;
}
