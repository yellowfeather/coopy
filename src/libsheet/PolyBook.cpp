#include <coopy/PolyBook.h>
#include <coopy/ShortTextBook.h>
#include <coopy/SqliteTextBook.h>
#include <coopy/CsvTextBook.h>
#include <coopy/CsvFile.h>
#include <coopy/FormatSniffer.h>
#include <coopy/Dbg.h>

#include <stdlib.h>
#include <stdio.h>

#include <map>

using namespace std;
using namespace coopy::store;
using namespace coopy::format;

extern TextBook *readHelper(const char *fname,
			    const char *ext,
			    const char *data);

bool PolyBook::read(const char *fname) {
  clear();
  string name = fname;
  if (name.length()>=4) {
    string ext = name.substr(name.rfind('.'),name.length());
    for (size_t i=0; i<ext.length(); i++) {
      ext[i] = tolower(ext[i]);
    }
    dbg_printf("Extension %s\n", ext.c_str());
    if (ext==".book") {
      dbg_printf("Trying %s out as CsvTextBook\n", ext.c_str());
      CsvTextBook *book0 = new CsvTextBook();
      if (!book0->read(fname)) {
	delete book0;
      } else {
	book = book0;
      }
      if (book!=NULL) return true;
    }
    book = readHelper(fname,ext.c_str(),NULL);
    if (book!=NULL) return true;
    FormatSniffer sniffer;
    sniffer.open(fname);
    Format f = sniffer.getFormat();
    if (f.id==FORMAT_BOOK_SQLITE) {
      dbg_printf("Trying %s out as Sqlite\n", ext.c_str());
      SqliteTextBook *book0 = new SqliteTextBook();
      if (!book0->read(fname)) {
	delete book0;
      } else {
	book = book0;
      }
    }
    if (book==NULL) {
      dbg_printf("Trying %s out as CSV\n", ext.c_str());
      ShortTextBook *b = new ShortTextBook();
      if (b==NULL) {
	fprintf(stderr,"Failed to allocate ShortTextBook\n");
	exit(1);
      }
      if (CsvFile::read(fname,b->sheet)!=0) {
	delete b;
	b = NULL;
      } else {
	book = b;
      }
    }
  }
  return book!=NULL;
}


class Namer {
public:
  map<string,int> existing;
  string name(string input) {
    string sane = "";
    for (size_t i=0; i<input.length(); i++) {
      char ch = input[i];
      if (ch=='"') {
	sane += '"';
      }
      sane += ch;
    }
    if (existing.find(sane)!=existing.end()) {
      char buf[256];
      int at = 0;
      do {
	sprintf(buf,"%d",at);
	at++;
      } while (existing.find(sane+buf)==existing.end());
      sane += buf;
    }
    existing[sane] = 1;
    return sane;
  }
};

class Valuer {
public:
  string name(string input) {
    string sane = "";
    for (size_t i=0; i<input.length(); i++) {
      char ch = input[i];
      if (ch=='\'') {
	sane += '\'';
      }
      sane += ch;
    }
    return sane;
  }
};

bool PolyBook::write(const char *fname, const char *format) {
  if (book==NULL) {
    fprintf(stderr,"Nothing to write\n");
    return false;
  }
  if (book->save(fname,format)) {
    return true;
  }
  if (format!=NULL) {
    if (string(format)!=""&&string(format)!="-") {
      fprintf(stderr,"Setting output format is not yet supported\n");
      exit(1);
    }
  }
  vector<string> names = getNames();
  string name = fname;
  size_t eid = name.rfind(".");
  string ext = ".csv";
  if (eid!=string::npos) {
    name.substr(eid);
  }
  for (size_t i=0; i<ext.length(); i++) {
    ext[i] = tolower(ext[i]);
  }
  if (ext==".sql") {
    FILE *fout = fopen(fname,"w");
    if (fout==NULL) {
      fprintf(stderr,"Could not open %s for writing\n", fname);
      return false;
    }
    for (size_t i=0; i<names.size(); i++) {
      PolySheet sheet = readSheet(names[i]);
      if (!sheet.isValid()) { 
	fprintf(stderr,"Could not access sheet %s\n", names[i].c_str());
	return false;
      }
      printf("Pumping out %s\n", names[i].c_str());
      Namer namer;
      Namer namer2;
      Valuer val;
      string table = namer.name(names[i]);
      fprintf(fout,"CREATE TABLE \"%s\" (\n", table.c_str());
      if (sheet.height()>=1) {
	for (int x=0; x<sheet.width(); x++) {
	  fprintf(fout,"   \"%s\"%s\n", namer2.name(sheet.cellString(x,0)).c_str(),
		  (x==sheet.width()-1)?"":",");
	}
      }
      fprintf(fout,");\n");
      for (int y=1; y<sheet.height(); y++) {
	fprintf(fout, "INSERT INTO \"%s\" VALUES ( ", table.c_str());
	for (int x=0; x<sheet.width(); x++) {
	  fprintf(fout,"\'%s\'%s ", val.name(sheet.cellString(x,y)).c_str(),
		  (x==sheet.width()-1)?"":",");
	}
	fprintf(fout, ");\n");
      }
    }
    fclose(fout);
    return true;
  }

  if (names.size()!=1) {
    fprintf(stderr,"Unsupported number of sheets during write: %d\n",
	    (int)names.size());
    return false;
  }
  PolySheet sheet = readSheet(names[0]);
  if (!sheet.isValid()) { 
    fprintf(stderr,"Could not access sheet %s\n", names[0].c_str());
    return false;
  }
  return CsvFile::write(sheet,fname)==0;
}

