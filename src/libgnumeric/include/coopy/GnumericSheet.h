#ifndef COOPY_GNUMERICSHEET
#define COOPY_GNUMERICSHEET

#include <coopy/DataSheet.h>

namespace coopy {
  namespace store {
    class GnumericSheet;
  }
}

class coopy::store::GnumericSheet : public DataSheet {
public:
  GnumericSheet(void *sheet);

  virtual ~GnumericSheet();

  virtual int width() const  { return w; }
  virtual int height() const { return h; }

  virtual std::string cellString(int x, int y) const;

  virtual std::string cellString(int x, int y, bool& escaped) const;

  virtual bool cellString(int x, int y, const std::string& str) {
    return cellString(x,y,str,false);
  }

  virtual bool cellString(int x, int y, const std::string& str, bool escaped);

  virtual ColumnRef moveColumn(const ColumnRef& src, const ColumnRef& base);

  virtual bool deleteColumn(const ColumnRef& column);

  virtual ColumnRef insertColumn(const ColumnRef& base);

  virtual RowRef insertRow(const RowRef& base);

  virtual bool deleteRow(const RowRef& src);

  virtual bool hasDimension() const {
    return false;
  }

  virtual bool forceWidth(int width) {
    w = width;
    return true;
  }

  virtual bool deleteData();

private:
  void *implementation;
  int w, h;
};

#endif
