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

  virtual bool cellString(int x, int y, const std::string& str);

private:
  void *implementation;
  int w, h;
};

#endif
