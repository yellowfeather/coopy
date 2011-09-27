
#include <stdio.h>
#include <getopt.h>

#include <coopy/CsvFile.h>
#include <coopy/CsvTextBook.h>
#include <coopy/MergeOutputIndex.h>
#include <coopy/BookCompare.h>
#include <coopy/PolyBook.h>
#include <coopy/SheetPatcher.h>

using namespace coopy::store;
using namespace coopy::cmp;
using namespace std;

Patcher *createTool(string mode, string version="") {
  return Patcher::createByName(mode.c_str(),version.c_str());
}

int main(int argc, char *argv[]) {
  std::string output = "-";
  std::string mode = "tdiff";
  std::string parent_file = "";
  std::string version = "";
  std::string map_file = "";

  CompareFlags flags;

  std::vector<std::string> ids;
  bool verbose = false;
  bool equality = false;
  bool apply = false;
  bool named = false;
  bool help = false;
  bool omitHeader = false;
  bool ordered = false;
  bool orderSet = false;
  bool bias = false;

  while (true) {
    int option_index = 0;
    static struct option long_options[] = {
      {"format-csv", 0, 0, 'c'},
      {"format-sql", 0, 0, 's'},
      {"format-raw", 0, 0, 'r'},
      {"format-tdiff", 0, 0, 't'},
      {"format-index", 0, 0, 'i'},
      {"format", 1, 0, 'f'},

      {"apply", 0, 0, 'a'},

      {"equals", 0, 0, 'e'},
      {"index", 0, 0, 'i'},
      {"verbose", 0, 0, 'v'},

      {"id", 1, 0, 'k'},
      {"bid", 1, 0, 'b'},

      {"named", 0, 0, 'd'},

      {"fixed-columns", 0, 0, 'F'},
      {"ignore-case", 0, 0, 'A'},

      {"ordered", 0, 0, '1'},
      {"unordered", 0, 0, '0'},

      {"map", 1, 0, 'm'},

      {"omit-format-name", 0, 0, 'O'},

      {"table", 1, 0, 'T'},

      {"output", 1, 0, 'o'},
      {"version", 1, 0, 'V'},
      {"parent", 1, 0, 'p'},
      {"list-formats", 0, 0, 'l'},
      {"help", 0, 0, 'h'},
      {0, 0, 0, 0}
    };

    int c = getopt_long(argc, argv, "",
			long_options, &option_index);
    if (c==-1) break;
    switch (c) {
    case 'c':
      mode = "csv";
      break;
    case 's':
      mode = "sql";
      break;
    case 'h':
      help = true;
      break;
    case 't':
      mode = "tdiff";
      break;
    case 'r':
      mode = "raw";
      break;
    case 'v':
      verbose = true;
      break;
    case 'e':
      equality = true;
      break;
    case 'i':
      mode = "index";
      break;
    case 'd':
      named = true;
      break;
    case '0':
      ordered = false;
      orderSet = true;
      break;
    case '1':
      ordered = true;
      orderSet = true;
      break;

    case 'k':
      ids.push_back(optarg);
      break;
    case 'b':
      ids.push_back(optarg);
      bias = true;
      break;


    case 'm':
      map_file = optarg;
      break;

    case 'O':
      omitHeader = true;
      break;

    case 'f':
      mode = optarg;
      break;
    case 'o':
      output = optarg;
      break;
    case 'p':
      parent_file = optarg;
      break;
    case 'V':
      version = optarg;
      break;
    case 'l':
      PolyBook::showFormats();
      return 0;
      break;
    case 'a':
      apply = true;
      break;

    case 'F':
      flags.fixed_columns = true;
      break;

    case 'A':
      flags.ignore_case = true;
      break;

    case 'T':
      flags.tables.insert(optarg);
      break;

    default:
      fprintf(stderr, "Unrecognized option\n");
      return 1;
    }
  }

  if (optind<argc-3) {
    fprintf(stderr, "Options not understood\n");
    return 1;
  }
  argc -= optind;
  argv += optind;

  if (argc<2||help) {
    printf("Show the difference between two tables/databases/spreadsheets. Call as:\n");
    printf("  ssdiff [--output <filename>] local.csv remote.csv\n");
    printf("\n");

    printf("To list supported input formats:\n");
    printf("  ssdiff --list-formats\n");
    printf("\n");

    printf("The difference can be shown in different output formats:\n");
    printf("  ssdiff --format tdiff local.csv modified.csv  # default format\n");
    printf("  ssdiff --format raw local.csv modified.csv    # verbose format for debugging\n");
    printf("  ssdiff --format sql local.csv modified.csv    # SQL format\n");
    printf("  ssdiff --format hilite local.csv modified.csv # colorful format, best used\n");
    printf("                                                # with --output out.xls\n");
    printf("  ssdiff --format review local.csv modified.csv # hilite, with column for\n");
    printf("                                                # selecting/rejecting changes\n");
    printf("  ssdiff --format csv local.csv modified.csv    # CSV-compatible diff format\n");
    printf("  ssdiff --format index local.csv modified.csv  # table showing matching rows\n");
    printf("\n");

    printf("The default matching algorithm is very general.  Generality can be traded for\n");
    printf("speed with the following options:\n");
    printf("  ssdiff --id COL1 --id COL2 local.csv modified.csv # declare primary key\n");
    printf("  ssdiff --bid COL1 --bid COL2 local.csv modified.csv # concentrate comparison\n");
    printf("  ssdiff --named local.csv modified.csv   # trust names of columns\n");
    printf("  ssdiff --unordered local.csv modified.csv   # do not worry about row order\n");
    printf("  ssdiff --fixed-columns local.csv modified.csv   # no column modifications\n");
    printf("\n");

    printf("For multi-table input, you can narrow the diff down using:\n");
    printf("  ssdiff --table TABLE1 --table TABLE2 local.mdb modified.mdb\n");
    printf("\n");

    printf("It is possible to immediately apply a difference as a patch:\n");
    printf("  ssdiff --apply local.xls modified.csv   # modifies local.xls\n");
    return 1;
  }

  BookCompare cmp;
  cmp.setVerbose(verbose);

  PolyBook _pivot;
  PolyBook *pivot;
  PolyBook _local;
  PolyBook *local = &_local;
  PolyBook _remote;
  PolyBook *remote = &_remote;
  
  if (!_local.read(argv[0])) {
    fprintf(stderr,"Failed to read %s\n", argv[0]);
    return 1;
  }
  flags.local_uri = argv[0];
  if (!_remote.read(argv[1])) {
    fprintf(stderr,"Failed to read %s\n", argv[1]);
    return 1;
  }
  flags.remote_uri = argv[1];
  if (parent_file!="") {
    if (!_pivot.read(parent_file.c_str())) {
      fprintf(stderr,"Failed to read %s\n", parent_file.c_str());
      return 1;
    }
    flags.pivot_uri = argv[1];
    pivot = &_pivot;
  } else {
    pivot = &_local;
    flags.pivot_sides_with_local = true;
  }

  if (equality) {
    if (*local == *remote) {
      return 0;
    }
    return 1;
  }

  PolyBook mapping;
  if (map_file!="") {
    if (!mapping.read(map_file.c_str())) {
	fprintf(stderr,"Failed to read %s\n", map_file.c_str());
	return 1;
      }
      flags.mapping_book = &mapping;
  }

  if (ids.size()>0) {
    flags.ids = ids;
    flags.trust_ids = !bias;
    flags.bias_ids = bias;
  }
  flags.trust_column_names = named;
  if (orderSet) {
    flags.use_order = ordered;
  }

  if (omitHeader) {
    flags.omit_format_name = true;
  }

  Patcher *diff = createTool(mode,version);
  if (diff==NULL) {
    fprintf(stderr,"Patch mode not recognized\n");
    return 1;
  }
  PolyBook obook;
  if (diff->needOutputBook()) {
    if (!obook.attach(output.c_str())) {
      delete diff; diff = NULL;
      return 1;
    }
    diff->attachOutputBook(obook);
  }
  PolyBook tbook;
  if (diff->outputStartsFromInput()) {
    if (output!="-") {
      if (!_local.write(output.c_str())) {
	delete diff; diff = NULL;
	return 1;
      }
      if (!_local.read(argv[0])) {
	fprintf(stderr,"Failed to read %s\n", argv[0]);
	return 1;
      }
      if (!tbook.read(output.c_str())) {
	fprintf(stderr,"Failed to read %s\n", output.c_str());
	return 1;
      }
    } else {
      tbook.take(new CsvTextBook(true));
      Property p;
      tbook.copy(_local,p);
    }
    diff->attachBook(tbook);
  } else {
    diff->attachBook(*local);
  }

  if (apply) {
    SheetPatcher *apply_diff = SheetPatcher::createForApply();
    COOPY_ASSERT(apply_diff!=NULL);
    apply_diff->showSummary(diff);
    diff = apply_diff;
    diff->attachBook(*local);
  }
  if (!diff->startOutput(output,flags)) {
    fprintf(stderr,"Patch output failed\n");
    delete diff;
    diff = NULL;
    return 1;
  }
  cmp.compare(*pivot,*local,*remote,*diff,flags);
  diff->stopOutput(output,flags);
  if (diff->needOutputBook()) {
    obook.flush();
  }
  if (diff->outputStartsFromInput()) {
    if (!tbook.write(output.c_str())) {
      fprintf(stderr,"Failed to write %s\n", output.c_str());
      return 1;
    }
  }
  if (apply) {
    if (diff->getChangeCount()>0) {
      if (!local->inplace()) {
	if (!local->write(argv[0])) {
	  fprintf(stderr,"Failed to write %s\n", argv[0]);
	  return 1;
	}
      }
    }
  }
  delete diff;
  diff = NULL;

  return 0;
}

