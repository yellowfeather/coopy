#ifndef COOPY_MERGEOUTPUTFILTER
#define COOPY_MERGEOUTPUTFILTER

#include <coopy/MergeOutput.h>

#include <string>
#include <list>
#include <map>

namespace coopy {
  namespace cmp {
    class MergeOutputFilter;
    class SheetUnit;
    class RowUnit;
  }
}

class coopy::cmp::SheetUnit {
public:
  std::string sheet_name;
  NameChange name0, name1;
  bool have_name0, have_name1;
  std::list<OrderChange> orders;
  std::list<PoolChange> pools;
  //std::list<RowUnit *> rows;

  SheetUnit() {
    have_name0 = have_name1 = false;
  }
  
  SheetUnit(const std::string& name) : sheet_name(name) {
    have_name0 = have_name1 = false;
  }
};

class coopy::cmp::RowUnit {
public:
  std::string sheet_name;
  RowChange change;

  RowUnit(const std::string& sheet_name, const RowChange& change) :
    sheet_name(sheet_name),
    change(change) {
  }
};

/*
  changeConfig: pass through
  changePool, changeColumn, changeName: cache verbatim per sheet
    order - changename1 changecolumns changename2 changepools
  changeRow: stack + release
 */
class coopy::cmp::MergeOutputFilter : public MergeOutput {
private:
  std::string sheet_name;
  std::string last_sheet_name;
  Patcher *chain;
  std::map<std::string,SheetUnit> sheet_units;
  std::list<RowUnit> rows;
  std::list<std::string> sheet_order;
  std::map<std::string, int> started_sheets;
  std::map<std::string, int> desired_sheets;

  SheetUnit& getSheetUnit() {
    std::map<std::string,SheetUnit>::iterator it = sheet_units.find(sheet_name);
    if (it!=sheet_units.end()) {
      return it->second;
    }
    sheet_order.push_back(sheet_name);
    return sheet_units[sheet_name] = SheetUnit(sheet_name);
  }

public:
  MergeOutputFilter(Patcher *next) {
    chain = next;
  }

  virtual bool setSheet(const char *name) { 
    sheet_name = name;
    return true; 
  }

  virtual bool changeConfig(const ConfigChange& change) { 
    return chain->changeConfig(change);
  }

  virtual bool changeColumn(const OrderChange& change) { 
    if (!isActiveTable()) return false;
    getSheetUnit().orders.push_back(change);
    return true; 
  }

  virtual bool changeRow(const RowChange& change) { 
    if (!isActiveTable()) return false;
    rows.push_back(RowUnit(sheet_name,change));
    return true;
  }

  virtual bool changePool(const PoolChange& change) { 
    if (!isActiveTable()) return false;
    getSheetUnit().pools.push_back(change);
    return true; 
  }

  virtual bool changeName(const NameChange& change) { 
    if (!isActiveTable()) return false;
    SheetUnit& unit = getSheetUnit();
    if (change.final) {
      unit.name1 = change;
      unit.have_name1 = true;
    } else {
      unit.name0 = change;
      unit.have_name0 = true;
    }
    return true;
  }

  virtual bool mergeStart();

  virtual bool mergeDone() {
    return true;
  }

  virtual bool mergeAllDone();

  bool emitPreamble(const SheetUnit& preamble);

  bool emitRow(const RowUnit& row);

  bool isActiveTable() {
    if (desired_sheets.size()==0) return true;
    return desired_sheets.find(sheet_name)!=desired_sheets.end();
  }
};

#endif
