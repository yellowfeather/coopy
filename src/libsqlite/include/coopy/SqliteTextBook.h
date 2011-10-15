#ifndef COOPY_SQLITETEXTBOOK
#define COOPY_SQLITETEXTBOOK

#include <coopy/TextBook.h>
#include <coopy/TextBookFactory.h>

namespace coopy {
  namespace store {

    /**
     *
     * Sqlite-backed table representation.
     *
     */
    namespace sqlite {
      class SqliteTextBook;
      class SqliteTextBookFactory;
    }
  }
}

class coopy::store::sqlite::SqliteTextBook : public TextBook {
public:
  SqliteTextBook(bool textual = false);
  virtual ~SqliteTextBook();

  void clear();

  virtual bool read(const char *fname, bool can_create = true);

  virtual bool save(const char *fname, const char *format, 
		    bool itextual = false);

  virtual bool open(const Property& config);
  
  std::vector<std::string> getNames() {
    return names;
  }

  PolySheet readSheet(const std::string& name);

  virtual bool inplace() const {
    return (!textual)&&(!memory);
  }

  virtual bool addSheet(const SheetSchema& schema);


  virtual std::string desc() const {
    return "SqliteBook";
  }

private:
  void *implementation;
  bool textual;
  bool memory;
  std::vector<std::string> names;

  std::vector<std::string> getNamesSql();

};


class coopy::store::sqlite::SqliteTextBookFactory : public TextBookFactory {
private:
  bool textual;
public:
 SqliteTextBookFactory(bool textual=false) : textual(textual) {
  }

  virtual std::string getName() {
    return textual?"sqlitext":"sqlite";
  }


  virtual TextBook *open(AttachConfig& config, AttachReport& report) {
    if (!textual) return openNormal(config,report);
    return openTextual(config,report);
  }
	
  TextBook *openNormal(AttachConfig& config, AttachReport& report) {
    SqliteTextBook *book = new SqliteTextBook(textual);
    if (book==NULL) return NULL;
    if (!book->open(config.options)) {
      delete book;
      book = NULL;
    }
    if (config.shouldWrite) {
      if (config.prevBook!=NULL) {
	if (!book->copy(*config.prevBook,config.options)) {
	  delete book;
	  book = NULL;
	  report.msg = "data transfer failed";
	  return NULL;
	}
	if (!book->inplace()) {
	  book->save("-",NULL,true);
	}
      }
    }
    if (book!=NULL) {
      report.success = true;
    }
    return book;
  }

  TextBook *openTextual(AttachConfig& config, AttachReport& report) {
    std::string ext = config.ext;
    report.applicable = true;
    if (config.shouldCreate) {
      report.errorCreateNotImplemented("sqlitext");
      return NULL;
    }
    if (config.shouldWrite && !config.shouldRead) {
      if (config.prevBook!=NULL) {
	SqliteTextBook *prev = dynamic_cast<SqliteTextBook *>(config.prevBook);
	if (prev!=NULL) {
	  bool ok = prev->save(config.fname.c_str(),NULL,true);
	  if (!ok) {
	    report.success = false;
	    report.msg = "failed to save file";
	    return NULL;
	  }
	  report.success = true;
	  return prev;
	}
      }
    }
    SqliteTextBook *book = new SqliteTextBook(true);
    if (book==NULL) return book;
    bool ok = false;
    if (book->read(config.fname.c_str(),true)) {
      ok = true;
    }
    if (!ok) {
      delete book;
      book = NULL;
    } else {
      if (config.prevBook!=NULL) {
	book->copy(*config.prevBook,config.options);
	if (config.shouldWrite) {
	  book->save(config.fname.c_str(),NULL,true);
	}
      }
    }
    if (book!=NULL) {
      report.success = true;
    }
    return book;
  }
};

#endif
