#ifndef COOPY_SHEETSCHEMA
#define COOPY_SHEETSCHEMA

#include <coopy/ColumnInfo.h>
#include <coopy/ColumnRef.h>
#include <coopy/RefCount.h>
#include <coopy/Dbg.h>

#include <vector>
#include <string>

namespace coopy {
  namespace store {
    class SheetSchema;
    class SimpleSheetSchema;
  }
}

class coopy::store::SheetSchema : public RefCount {
public:
  virtual ~SheetSchema() {
  }

  virtual ColumnInfo getColumnInfo(int x) const {
    return ColumnInfo();
  }

  virtual int headerHeight() const {
    return 0;
  }

  virtual int getColumnCount() const {
    return -1;
  }

  virtual int getColumnIndexByName(const char *name) const {
    for (int i=0; i<getColumnCount(); i++) {
      ColumnInfo info = getColumnInfo(i);
      if (info.getName()==name) {
	return i;
      }
    }
    return -1;
  }

  virtual bool providesPrimaryKeys() const {
    return false;
  }

  virtual std::string getSheetName() const {
    return "";
  }

  virtual bool hasSheetName() const {
    return true;
  }

  virtual SheetSchema *clone() const;

  virtual bool isGuess() const {
    return false;
  }

  virtual bool addedHeader() {
    fprintf(stderr, "Don't know what to do with header\n");
    COOPY_ASSERT(1==0);
  }

  virtual bool deleteColumn(const ColumnRef& column) {
    return false;
  }

  virtual ColumnRef insertColumn(const ColumnRef& column, 
				 const ColumnInfo& info) {
    fprintf(stderr, "Don't know how to insert column in schema\n");
    return ColumnRef();
  }

  virtual bool modifyColumn(const ColumnRef& column, 
			    const ColumnInfo& info) {
    fprintf(stderr, "Don't know how to modify column in schema\n");
    return false;
  }

  virtual bool setHeaderHeight(int hh) {
    return false;
  }

  virtual std::string toString() const {
    return "unknown-schema";
  }
};

class coopy::store::SimpleSheetSchema : public SheetSchema {
private:
  std::string sheetName;
  std::vector<std::string> columns;
  std::vector<ColumnType> kinds;
  int hh;
  bool guessed;
  bool hasName;
public:
  SimpleSheetSchema() {
    hh = 0;
    guessed = false;
    hasName = true;
  }

  bool copy(const SheetSchema& ss) {
    sheetName = ss.getSheetName();
    hh = ss.headerHeight();
    int ct = ss.getColumnCount();
    columns.clear();
    kinds.clear();
    for (int i=0; i<ct; i++) {
      ColumnInfo c = ss.getColumnInfo(i);
      columns.push_back(c.getName());
      kinds.push_back(c.getColumnType());
    }
    guessed = ss.isGuess();
    return true;
  }

  virtual bool addedHeader() {
    hh++;
  }  

  virtual bool setHeaderHeight(int hh) {
    this->hh = hh;
    return true;
  }

  virtual int headerHeight() const {
    return hh;
  }

  void setSheetName(const char *name) {
    sheetName = name;
  }

  bool addColumn(const char *name) {
    columns.push_back(name);
    kinds.push_back(ColumnType());
    return true;
  }

  bool addColumn(const char *name, const ColumnType& kind) {
    columns.push_back(name);
    kinds.push_back(kind);
    return true;
  }

  virtual ColumnInfo getColumnInfo(int x) const {
    return ColumnInfo(columns[x],kinds[x]);
  }

  virtual int getColumnCount() const {
    return columns.size();
  }

  virtual bool providesPrimaryKeys() const {
    return false;
  }

  virtual std::string getSheetName() const {
    return sheetName;
  }

  virtual bool isGuess() const {
    return guessed;
  }

  void setGuess(bool flag) {
    guessed = flag;
  }

  void setHasSheetName(bool flag) {
    hasName = flag;
  }

  virtual bool hasSheetName() const {
    return hasName;
  }

  virtual bool deleteColumn(const ColumnRef& column) {
    columns.erase(columns.begin()+column.getIndex());
    kinds.erase(kinds.begin()+column.getIndex());
    return true;
  }

  virtual ColumnRef insertColumn(const ColumnRef& column, 
				 const ColumnInfo& info) {
    int index = column.getIndex();
    if (index==-1) {
      return addColumn(info.getName().c_str());
    }
    columns.insert(columns.begin()+column.getIndex(),info.getName().c_str());
    kinds.insert(kinds.begin()+column.getIndex(),info.getColumnType());
    return true;
  }

  virtual bool modifyColumn(const ColumnRef& column, 
			    const ColumnInfo& info) {
    
    columns[column.getIndex()] = info.getName();
    return true;
  }

  virtual std::string toString() const {
    std::string r = "";
    r += "sheet ";
    r += getSheetName();
    r += " header height ";
    int h = headerHeight();
    if (h>0) {
      r += ">0";
    } else if (h==0) {
      r += "=0";
    } else {
      r += "<0";
    }
    return r;
  }
};

#endif
