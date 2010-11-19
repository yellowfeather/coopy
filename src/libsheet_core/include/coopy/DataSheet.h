#ifndef COOPY_DATASHEET
#define COOPY_DATASHEET

#include <coopy/Dbg.h>
#include <coopy/SheetStyle.h>
#include <coopy/SheetSchema.h>
#include <coopy/RowRef.h>
#include <coopy/ColumnRef.h>
#include <coopy/RefCount.h>
#include <coopy/SheetCell.h>

#include <string>
#include <vector>

namespace coopy {
  namespace store {
    class DataSheet;
    class SheetRow;
  }
}

class coopy::store::DataSheet : public RefCount {
public:
  virtual ~DataSheet() {}

  virtual int width() const = 0;
  virtual int height() const = 0;

  virtual std::string cellString(int x, int y) const = 0;

  virtual std::string cellString(int x, int y, bool& escaped) const {
    escaped = false;
    return cellString(x,y);
  }

  virtual SheetCell cellSummary(int x, int y) const {
    SheetCell c;
    c.text = cellString(x,y,c.escaped);
    return c;
  }

  virtual bool cellString(int x, int y, const std::string& str) {
    return false;
  }

  virtual bool cellString(int x, int y, const std::string& str, bool escaped) {
    return cellString(x,y,str);
  }

  virtual bool cellSummary(int x, int y, const SheetCell& c) {
    return cellString(x,y,c.text,c.escaped);
  }

  virtual bool canEscape() const {
    return false;
  }

  virtual SheetCell getCell(int x, int y) const {
    return cellSummary(x,y);
  }

  virtual bool setCell(int x, int  y, const SheetCell& c) {
    return cellSummary(x,y,c);
  }

  std::string encode(const SheetStyle& style) const;

  std::string toString() const {
    SheetStyle style;
    return encode(style);
  }

  static std::string encodeCell(const SheetCell& str, 
				const SheetStyle& style);

  //static std::string encodeCell(const std::string& str, 
  //				const SheetStyle& style);

  // remove any cached values, e.g. in remote connections.
  virtual bool clearCache() { return true; }

  virtual SheetSchema *getSchema() const {
    return 0 /*NULL*/;
  }

  virtual bool deleteColumn(const ColumnRef& column) {
    return false;
  }

  // insert a column before base; if base is invalid insert after all columns
  virtual ColumnRef insertColumn(const ColumnRef& base) {
    return ColumnRef(); // invalid column
  }

  // move a column before base; if base is invalid move after all columns
  virtual ColumnRef moveColumn(const ColumnRef& src, const ColumnRef& base) {
    return ColumnRef(); // invalid column
  }

  virtual bool deleteRow(const RowRef& src) {
    return false;
  }

  // insert a row before base; if base is invalid insert after all rows
  virtual RowRef insertRow(const RowRef& base) {
    return RowRef(); // invalid row
  }

  // move a row before base; if base is invalid move after all rows
  virtual RowRef moveRow(const RowRef& src, const RowRef& base) {
    return RowRef(); // invalid column
  }

  virtual bool copyData(const DataSheet& src);

  virtual bool canWrite() { return true; }

  virtual bool canResize() { return false; }

  virtual bool resize(int w, int h) {
    return false;
  }

  virtual Poly<SheetRow> insertRow();

  virtual bool applySchema(const SheetSchema& ss) {
    return false;
  }

  virtual std::string getDescription() const {
    return "data";
  }

  virtual std::vector<std::string> getNestedDescription() const {
    std::string name = getDescription();
    std::vector<std::string> v;
    v.push_back(name);
    return v;
  }

  virtual std::string desc() const {
    std::vector<std::string> v = getNestedDescription();
    std::string output;
    for (int i=0; i<(int)v.size(); i++) {
      if (output!="") {
	output += ".";
      }
      output += v[i];
    }
    return output;
  }
};

class coopy::store::SheetRow : public RefCount {
private:
  int y;
  DataSheet *sheet;
public:
  SheetRow() {
    y = -1;
    sheet = 0 /*NULL*/;
  }

  // only relevant if not subclassed
  SheetRow(DataSheet *sheet, int y) : sheet(sheet), y(y) {
  }

  virtual SheetCell getCell(int x) const {
    return sheet->cellSummary(x,y);
  }
  
  virtual bool setCell(int x, const SheetCell& c) {
    return sheet->cellSummary(x,y,c);
  }

  virtual bool flush() {
    return true;
  }
};

#endif
