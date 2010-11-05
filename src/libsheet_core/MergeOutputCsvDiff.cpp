

/*
  (Planned) changes since 0.2:
  Version number change.
  The special "*" character goes away.
  A "row transform" operation is added.
 */

#include <coopy/MergeOutputCsvDiff.h>
#include <coopy/SheetStyle.h>
#include <coopy/DataSheet.h>

#include <stdio.h>
#include <stdlib.h>

#define WANT_MAP2STRING
#define WANT_VECTOR2STRING
#include <coopy/Stringer.h>
#define ROW_COL "ROW"

using namespace std;
using namespace coopy::store;
using namespace coopy::cmp;

static string encoder(const string& x) {
  //SheetStyle style;
  //string result = DataSheet::encodeCell(x,style);
  //return (result!="")?result:"\"\"";
  string result = stringer_encoder(x);
  if (result.find("=")!=result.npos) {
    if (result[0]!='"') {
      // force quoting of any material containing the "=" symbol
      result = string("\"")+result+string("\"");
    }
  }
  return result;
}

static string cond(const vector<string>& names,
		   const map<string,string>& conds,
		   const map<string,string>& vals,
		   bool act) {
  string c = "";
  string pv = "";
  string v = "";
  bool nontrivial_past = false;
  for (vector<string>::const_iterator it = names.begin();
       it!=names.end();
       it++) {
    string name = *it;
    if (act) {
      //if (conds.find(name)!=conds.end()) {
	//fprintf(stderr,"Missing condition for %s\n", name.c_str());
	//exit(1);
      if (vals.find(name)!=vals.end()) {
	string pval;
	string val = vals.find(name)->second;
	if (conds.find(name)!=conds.end()) {
	  pval = conds.find(name)->second;
	  if (pval!="") {
	    nontrivial_past = true;
	  }
	}
	if (pv!="") pv += ",";
	pv += encoder(pval);
	if (v!="") v += ",";
	v += encoder(val);
	if (c!="") c += ",";
	c += encoder(name);
      }
	//}
    } else {
      if (conds.find(name)!=conds.end()) {
	if (vals.find(name)==vals.end()) {
	  string val = conds.find(name)->second;
	  if (c!="") c += ",";
	  c += encoder(name);
	  if (v!="") v += ",";
	  v += encoder(val);
	}
      }
    }
  }
  if (act) {
    if (pv == "" || !nontrivial_past) {
      return c + " = " + v;
    }
    return c + " = " + pv + " -> " + v;
  }
  return c + " = " + v;
}

static string cond(const vector<string>& names,
		   const map<string,string>& vals,
		   string val_label, string cond_label) {
  SheetStyle style;
  string c = "";
  string v = "";
  size_t ct = 0;
  for (vector<string>::const_iterator it = names.begin();
       it!=names.end();
       it++) {
    string name = *it;
    if (vals.find(name)!=vals.end()) {
      ct++;
      string val = vals.find(name)->second;
      if (c!="") c += " ";
      c += encoder(name);
      if (v!="") v += " ";
      v += encoder(val);
    }
  }
  if (ct==names.size()) {
    c = "*";
    return string("  ") + val_label + " " + v;
  }
  return string("  ") + cond_label + " " + c + "\n  " + val_label + " " + v;
}

static string cond(const vector<string>& names) {
  string c = "";
  for (vector<string>::const_iterator it = names.begin();
       it!=names.end();
       it++) {
    string name = *it;
    if (c!="") c += " ";
    c += encoder(name);
  }
  return c;
}

MergeOutputCsvDiff::MergeOutputCsvDiff() {
  result.setStrict(0);
  result.addField("dtbl",false);
  result.addField("csv",false);
  result.addField("version",false);
  result.addField("0.2",false);
  result.addRecord();
  result.addField("config",false);
  result.addField("empty_tag",false);
  result.addField("*",false);
  result.addRecord();
  result.addField("config",false);
  result.addField("row_tag",false);
  result.addField(ROW_COL,false);
  result.addRecord();
}

bool MergeOutputCsvDiff::mergeDone() {
  SheetStyle style;
  SheetCell c = result.cellSummary(0,0);
  fprintf(out,"%s",result.encode(style).c_str());
}

bool MergeOutputCsvDiff::changeColumn(const OrderChange& change) {
  switch (change.mode) {
  case ORDER_CHANGE_DELETE:
    result.addField("column",false);
    result.addField("delete",false);
    result.addField(ROW_COL,false);
    for (int i=0; i<(int)change.namesAfter.size(); i++) {
      result.addField(change.namesAfter[i].c_str(),false);
    }
    result.addRecord();
    break;
  case ORDER_CHANGE_INSERT:
    result.addField("column",false);
    result.addField("insert",false);
    result.addField(ROW_COL,false);
    for (int i=0; i<(int)change.namesAfter.size(); i++) {
      result.addField(change.namesAfter[i].c_str(),false);
    }
    result.addRecord();
    break;
  case ORDER_CHANGE_MOVE:
    result.addField("column",false);
    result.addField("move",false);
    result.addField(ROW_COL,false);
    for (int i=0; i<(int)change.namesAfter.size(); i++) {
      result.addField(change.namesAfter[i].c_str(),false);
    }
    result.addRecord();
    break;
  default:
    fprintf(stderr,"  Unknown column operation\n\n");
    exit(1);
    break;
  }
  return true;
}

bool MergeOutputCsvDiff::selectRow(const RowChange& change, const char *tag) {
  result.addField("row",false);
  result.addField(tag,false);
  result.addField("*",false);
  for (int i=0; i<(int)change.names.size(); i++) {
    string name = change.names[i];
    if (change.cond.find(name)!=change.cond.end()) {
      result.addField(change.cond.find(name)->second);
    } else {
      result.addField("*",false);
    }
  }
  result.addRecord();
  return true;
}

bool MergeOutputCsvDiff::describeRow(const RowChange& change, const char *tag){
  result.addField("row",false);
  result.addField(tag,false);
  result.addField("*",false);
  for (int i=0; i<(int)change.names.size(); i++) {
    string name = change.names[i];
    if (change.val.find(name)!=change.val.end()) {
      result.addField(change.val.find(name)->second);
    } else {
      result.addField("*",false);
    }
  }
  result.addRecord();
  return true;
}

bool MergeOutputCsvDiff::changeRow(const RowChange& change) {
  switch (change.mode) {
  case ROW_CHANGE_INSERT:
    describeRow(change,"insert");
    break;
  case ROW_CHANGE_DELETE:
    selectRow(change,"delete");
    break;
  case ROW_CHANGE_UPDATE:
    selectRow(change,"select");
    describeRow(change,"update");
    break;
  default:
    fprintf(stderr,"  Unknown row operation\n\n");
    exit(1);
    break;
  }
  return true;
}


bool MergeOutputCsvDiff::declareNames(const vector<string>& names, 
					  bool final) {
  if (!final) {
    result.addField("column",false);
    result.addField("name",false);
    result.addField(ROW_COL,false);
    for (int i=0; i<(int)names.size(); i++) {
      result.addField(names[i].c_str(),false);
    }
    result.addRecord();
  }
  return true;
}

