/*
 * This file produced from ./encodings.pl on Mon Dec  4 09:01:30 2023
 */
#ifdef TDS_ICONV_ENCODING_TABLES

static const TDS_ENCODING canonic_charsets[] = {
	{        "ISO-8859-1",	1, 1,   0},	/*   0 */
	{             "UTF-8",	1, 4,   1},	/*   1 */
	{           "UCS-2LE",	2, 2,   2},	/*   2 */
	{           "UCS-2BE",	2, 2,   3},	/*   3 */
	{          "UTF-16LE",	2, 4,   4},	/*   4 */
	{          "UTF-16BE",	2, 4,   5},	/*   5 */
	{           "UCS-4LE",	4, 4,   6},	/*   6 */
	{           "UCS-4BE",	4, 4,   7},	/*   7 */
	{          "UTF-32LE",	4, 4,   8},	/*   8 */
	{          "UTF-32BE",	4, 4,   9},	/*   9 */
	{         "ARMSCII-8",	1, 1,  10},	/*  10 */
	{             "BIG-5",	1, 2,  11},	/*  11 */
	{        "BIG5-HKSCS",	1, 2,  12},	/*  12 */
	{               "C99",	1, 1,  13},	/*  13 */
	{           "CHINESE",	1, 1,  14},	/*  14 */
	{                "CN",	1, 1,  15},	/*  15 */
	{             "CN-GB",	1, 2,  16},	/*  16 */
	{    "CN-GB-ISOIR165",	1, 1,  17},	/*  17 */
	{            "CP1133",	1, 1,  18},	/*  18 */
	{            "CP1250",	1, 1,  19},	/*  19 */
	{            "CP1251",	1, 1,  20},	/*  20 */
	{            "CP1252",	1, 1,  21},	/*  21 */
	{            "CP1253",	1, 1,  22},	/*  22 */
	{            "CP1254",	1, 1,  23},	/*  23 */
	{            "CP1255",	1, 1,  24},	/*  24 */
	{            "CP1256",	1, 1,  25},	/*  25 */
	{            "CP1257",	1, 1,  26},	/*  26 */
	{            "CP1258",	1, 1,  27},	/*  27 */
	{            "CP1361",	1, 2,  28},	/*  28 */
	{             "CP437",	1, 1,  29},	/*  29 */
	{             "CP850",	1, 1,  30},	/*  30 */
	{             "CP862",	1, 1,  31},	/*  31 */
	{             "CP866",	1, 1,  32},	/*  32 */
	{             "CP874",	1, 1,  33},	/*  33 */
	{             "CP932",	1, 2,  34},	/*  34 */
	{             "CP936",	1, 2,  35},	/*  35 */
	{             "CP949",	1, 2,  36},	/*  36 */
	{             "CP950",	1, 2,  37},	/*  37 */
	{            "EUC-JP",	1, 3,  38},	/*  38 */
	{            "EUC-KR",	1, 2,  39},	/*  39 */
	{            "EUC-TW",	1, 4,  40},	/*  40 */
	{           "GB18030",	1, 4,  41},	/*  41 */
	{  "GEORGIAN-ACADEMY",	1, 1,  42},	/*  42 */
	{       "GEORGIAN-PS",	1, 1,  43},	/*  43 */
	{                "HZ",	1, 1,  44},	/*  44 */
	{       "ISO-2022-CN",	1, 4,  45},	/*  45 */
	{   "ISO-2022-CN-EXT",	1, 4,  46},	/*  46 */
	{       "ISO-2022-JP",	1, 1,  47},	/*  47 */
	{     "ISO-2022-JP-1",	1, 1,  48},	/*  48 */
	{     "ISO-2022-JP-2",	1, 1,  49},	/*  49 */
	{       "ISO-2022-KR",	1, 2,  50},	/*  50 */
	{       "ISO-8859-10",	1, 1,  51},	/*  51 */
	{       "ISO-8859-13",	1, 1,  52},	/*  52 */
	{       "ISO-8859-14",	1, 1,  53},	/*  53 */
	{       "ISO-8859-15",	1, 1,  54},	/*  54 */
	{       "ISO-8859-16",	1, 1,  55},	/*  55 */
	{        "ISO-8859-2",	1, 1,  56},	/*  56 */
	{        "ISO-8859-3",	1, 1,  57},	/*  57 */
	{        "ISO-8859-4",	1, 1,  58},	/*  58 */
	{        "ISO-8859-5",	1, 1,  59},	/*  59 */
	{        "ISO-8859-6",	1, 1,  60},	/*  60 */
	{        "ISO-8859-7",	1, 1,  61},	/*  61 */
	{        "ISO-8859-8",	1, 1,  62},	/*  62 */
	{        "ISO-8859-9",	1, 1,  63},	/*  63 */
	{         "ISO-IR-14",	1, 1,  64},	/*  64 */
	{        "ISO-IR-149",	1, 1,  65},	/*  65 */
	{        "ISO-IR-159",	1, 1,  66},	/*  66 */
	{        "ISO-IR-166",	1, 1,  67},	/*  67 */
	{         "ISO-IR-87",	1, 1,  68},	/*  68 */
	{              "JAVA",	1, 1,  69},	/*  69 */
	{     "JISX0201-1976",	1, 1,  70},	/*  70 */
	{            "KOI8-R",	1, 1,  71},	/*  71 */
	{           "KOI8-RU",	1, 1,  72},	/*  72 */
	{            "KOI8-T",	1, 1,  73},	/*  73 */
	{            "KOI8-U",	1, 1,  74},	/*  74 */
	{               "MAC",	1, 1,  75},	/*  75 */
	{         "MACARABIC",	1, 1,  76},	/*  76 */
	{  "MACCENTRALEUROPE",	1, 1,  77},	/*  77 */
	{       "MACCROATIAN",	1, 1,  78},	/*  78 */
	{       "MACCYRILLIC",	1, 1,  79},	/*  79 */
	{          "MACGREEK",	1, 1,  80},	/*  80 */
	{         "MACHEBREW",	1, 1,  81},	/*  81 */
	{        "MACICELAND",	1, 1,  82},	/*  82 */
	{        "MACROMANIA",	1, 1,  83},	/*  83 */
	{           "MACTHAI",	1, 1,  84},	/*  84 */
	{        "MACTURKISH",	1, 1,  85},	/*  85 */
	{        "MACUKRAINE",	1, 1,  86},	/*  86 */
	{         "MULELAO-1",	1, 1,  87},	/*  87 */
	{          "NEXTSTEP",	1, 1,  88},	/*  88 */
	{            "ROMAN8",	1, 1,  89},	/*  89 */
	{              "SJIS",	1, 2,  90},	/*  90 */
	{              "TCVN",	1, 1,  91},	/*  91 */
	{          "US-ASCII",	1, 1,  92},	/*  92 */
	{             "UTF-7",	1, 4,  93},	/*  93 */
	{            "VISCII",	1, 1,  94},	/*  94 */
};

static const CHARACTER_SET_ALIAS iconv_aliases[] = {
	{                    "646",   92 },
	{                    "850",   30 },
	{                    "862",   31 },
	{                    "866",   32 },
	{         "ANSI_X3.4-1968",   92 },
	{         "ANSI_X3.4-1986",   92 },
	{                 "ARABIC",   60 },
	{              "ARMSCII-8",   10 },
	{                  "ASCII",   92 },
	{               "ASMO-708",   60 },
	{                  "BIG-5",   11 },
	{               "BIG-FIVE",   11 },
	{                   "BIG5",   11 },
	{             "BIG5-HKSCS",   12 },
	{              "BIG5HKSCS",   12 },
	{                "BIGFIVE",   11 },
	{                    "C99",   13 },
	{                "CHINESE",   14 },
	{                     "CN",   15 },
	{                "CN-BIG5",   11 },
	{                  "CN-GB",   16 },
	{         "CN-GB-ISOIR165",   17 },
	{                 "CP1133",   18 },
	{                 "CP1250",   19 },
	{                 "CP1251",   20 },
	{                 "CP1252",   21 },
	{                 "CP1253",   22 },
	{                 "CP1254",   23 },
	{                 "CP1255",   24 },
	{                 "CP1256",   25 },
	{                 "CP1257",   26 },
	{                 "CP1258",   27 },
	{                 "CP1361",   28 },
	{                  "CP367",   92 },
	{                  "CP437",   29 },
	{                "CP65001",    1 },
	{                  "CP819",    0 },
	{                  "CP850",   30 },
	{                  "CP862",   31 },
	{                  "CP866",   32 },
	{                  "CP874",   33 },
	{                  "CP932",   34 },
	{                  "CP936",   35 },
	{                  "CP949",   36 },
	{                  "CP950",   37 },
	{                "CSASCII",   92 },
	{                 "CSBIG5",   11 },
	{                "CSEUCKR",   39 },
	{    "CSEUCPKDFMTJAPANESE",   38 },
	{                "CSEUCTW",   40 },
	{               "CSGB2312",   16 },
	{    "CSHALFWIDTHKATAKANA",   70 },
	{             "CSHPROMAN8",   89 },
	{               "CSIBM866",   32 },
	{      "CSISO14JISC6220RO",   64 },
	{   "CSISO159JISX02121990",   66 },
	{            "CSISO2022CN",   45 },
	{            "CSISO2022JP",   47 },
	{           "CSISO2022JP2",   49 },
	{            "CSISO2022KR",   50 },
	{          "CSISO57GB1988",   15 },
	{        "CSISO58GB231280",   14 },
	{        "CSISO87JISX0208",   68 },
	{            "CSISOLATIN1",    0 },
	{            "CSISOLATIN2",   56 },
	{            "CSISOLATIN3",   57 },
	{            "CSISOLATIN4",   58 },
	{            "CSISOLATIN5",   63 },
	{            "CSISOLATIN6",   51 },
	{       "CSISOLATINARABIC",   60 },
	{     "CSISOLATINCYRILLIC",   59 },
	{        "CSISOLATINGREEK",   61 },
	{       "CSISOLATINHEBREW",   62 },
	{                "CSKOI8R",   71 },
	{          "CSKSC56011987",   65 },
	{            "CSMACINTOSH",   75 },
	{    "CSPC850MULTILINGUAL",   30 },
	{     "CSPC862LATINHEBREW",   31 },
	{             "CSSHIFTJIS",   90 },
	{            "CSUNICODE11",    3 },
	{        "CSUNICODE11UTF7",   93 },
	{               "CSVISCII",   94 },
	{               "CYRILLIC",   59 },
	{               "ECMA-114",   60 },
	{               "ECMA-118",   61 },
	{               "ELOT_928",   61 },
	{                 "EUC-CN",   16 },
	{                 "EUC-JP",   38 },
	{                 "EUC-KR",   39 },
	{                 "EUC-TW",   40 },
	{                  "EUCCN",   16 },
	{                  "EUCJP",   38 },
	{                  "EUCKR",   39 },
	{                  "EUCTW",   40 },
	{"EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE",   38 },
	{                "GB18030",   41 },
	{                 "GB2312",   16 },
	{                    "GBK",   35 },
	{             "GB_1988-80",   15 },
	{             "GB_2312-80",   14 },
	{       "GEORGIAN-ACADEMY",   42 },
	{            "GEORGIAN-PS",   43 },
	{                  "GREEK",   61 },
	{                 "GREEK8",   61 },
	{                 "HEBREW",   62 },
	{              "HP-ROMAN8",   89 },
	{                     "HZ",   44 },
	{             "HZ-GB-2312",   44 },
	{             "IBM-CP1133",   18 },
	{                 "IBM367",   92 },
	{                 "IBM437",   29 },
	{                 "IBM819",    0 },
	{                 "IBM850",   30 },
	{                 "IBM862",   31 },
	{                 "IBM866",   32 },
	{            "ISO-2022-CN",   45 },
	{        "ISO-2022-CN-EXT",   46 },
	{            "ISO-2022-JP",   47 },
	{          "ISO-2022-JP-1",   48 },
	{          "ISO-2022-JP-2",   49 },
	{            "ISO-2022-KR",   50 },
	{             "ISO-8859-1",    0 },
	{            "ISO-8859-10",   51 },
	{            "ISO-8859-13",   52 },
	{            "ISO-8859-14",   53 },
	{            "ISO-8859-15",   54 },
	{            "ISO-8859-16",   55 },
	{             "ISO-8859-2",   56 },
	{             "ISO-8859-3",   57 },
	{             "ISO-8859-4",   58 },
	{             "ISO-8859-5",   59 },
	{             "ISO-8859-6",   60 },
	{             "ISO-8859-7",   61 },
	{             "ISO-8859-8",   62 },
	{             "ISO-8859-9",   63 },
	{             "ISO-CELTIC",   53 },
	{             "ISO-IR-100",    0 },
	{             "ISO-IR-101",   56 },
	{             "ISO-IR-109",   57 },
	{             "ISO-IR-110",   58 },
	{             "ISO-IR-126",   61 },
	{             "ISO-IR-127",   60 },
	{             "ISO-IR-138",   62 },
	{              "ISO-IR-14",   64 },
	{             "ISO-IR-144",   59 },
	{             "ISO-IR-148",   63 },
	{             "ISO-IR-149",   65 },
	{             "ISO-IR-157",   51 },
	{             "ISO-IR-159",   66 },
	{             "ISO-IR-165",   17 },
	{             "ISO-IR-166",   67 },
	{             "ISO-IR-179",   52 },
	{             "ISO-IR-199",   53 },
	{             "ISO-IR-203",   54 },
	{             "ISO-IR-226",   55 },
	{              "ISO-IR-57",   15 },
	{              "ISO-IR-58",   14 },
	{               "ISO-IR-6",   92 },
	{              "ISO-IR-87",   68 },
	{              "ISO646-CN",   15 },
	{              "ISO646-JP",   64 },
	{              "ISO646-US",   92 },
	{              "ISO8859-1",    0 },
	{             "ISO8859-10",   51 },
	{             "ISO8859-15",   54 },
	{              "ISO8859-2",   56 },
	{              "ISO8859-4",   58 },
	{              "ISO8859-5",   59 },
	{              "ISO8859-6",   60 },
	{              "ISO8859-7",   61 },
	{              "ISO8859-8",   62 },
	{              "ISO8859-9",   63 },
	{       "ISO_646.IRV:1991",   92 },
	{             "ISO_8859-1",    0 },
	{            "ISO_8859-10",   51 },
	{       "ISO_8859-10:1992",   51 },
	{            "ISO_8859-13",   52 },
	{            "ISO_8859-14",   53 },
	{       "ISO_8859-14:1998",   53 },
	{            "ISO_8859-15",   54 },
	{       "ISO_8859-15:1998",   54 },
	{            "ISO_8859-16",   55 },
	{       "ISO_8859-16:2000",   55 },
	{        "ISO_8859-1:1987",    0 },
	{             "ISO_8859-2",   56 },
	{        "ISO_8859-2:1987",   56 },
	{             "ISO_8859-3",   57 },
	{        "ISO_8859-3:1988",   57 },
	{             "ISO_8859-4",   58 },
	{        "ISO_8859-4:1988",   58 },
	{             "ISO_8859-5",   59 },
	{        "ISO_8859-5:1988",   59 },
	{             "ISO_8859-6",   60 },
	{        "ISO_8859-6:1987",   60 },
	{             "ISO_8859-7",   61 },
	{        "ISO_8859-7:1987",   61 },
	{             "ISO_8859-8",   62 },
	{        "ISO_8859-8:1988",   62 },
	{             "ISO_8859-9",   63 },
	{        "ISO_8859-9:1989",   63 },
	{                   "JAVA",   69 },
	{                "JIS0208",   68 },
	{          "JISX0201-1976",   70 },
	{      "JIS_C6220-1969-RO",   64 },
	{         "JIS_C6226-1983",   68 },
	{              "JIS_X0201",   70 },
	{              "JIS_X0208",   68 },
	{         "JIS_X0208-1983",   68 },
	{         "JIS_X0208-1990",   68 },
	{              "JIS_X0212",   66 },
	{         "JIS_X0212-1990",   66 },
	{       "JIS_X0212.1990-0",   66 },
	{                  "JOHAB",   28 },
	{                     "JP",   64 },
	{                 "KOI8-R",   71 },
	{                "KOI8-RU",   72 },
	{                 "KOI8-T",   73 },
	{                 "KOI8-U",   74 },
	{                 "KOREAN",   65 },
	{               "KSC_5601",   65 },
	{         "KS_C_5601-1987",   65 },
	{         "KS_C_5601-1989",   65 },
	{                     "L1",    0 },
	{                     "L2",   56 },
	{                     "L3",   57 },
	{                     "L4",   58 },
	{                     "L5",   63 },
	{                     "L6",   51 },
	{                     "L7",   52 },
	{                     "L8",   53 },
	{                 "LATIN1",    0 },
	{                 "LATIN2",   56 },
	{                 "LATIN3",   57 },
	{                 "LATIN4",   58 },
	{                 "LATIN5",   63 },
	{                 "LATIN6",   51 },
	{                 "LATIN7",   52 },
	{                 "LATIN8",   53 },
	{                    "MAC",   75 },
	{              "MACARABIC",   76 },
	{       "MACCENTRALEUROPE",   77 },
	{            "MACCROATIAN",   78 },
	{            "MACCYRILLIC",   79 },
	{               "MACGREEK",   80 },
	{              "MACHEBREW",   81 },
	{             "MACICELAND",   82 },
	{              "MACINTOSH",   75 },
	{               "MACROMAN",   75 },
	{             "MACROMANIA",   83 },
	{                "MACTHAI",   84 },
	{             "MACTURKISH",   85 },
	{             "MACUKRAINE",   86 },
	{                "MS-ANSI",   21 },
	{                "MS-ARAB",   25 },
	{                "MS-CYRL",   20 },
	{                  "MS-EE",   19 },
	{               "MS-GREEK",   22 },
	{                "MS-HEBR",   24 },
	{                "MS-TURK",   23 },
	{               "MS_KANJI",   90 },
	{              "MULELAO-1",   87 },
	{               "NEXTSTEP",   88 },
	{                     "R8",   89 },
	{                 "ROMAN8",   89 },
	{              "SHIFT-JIS",   90 },
	{              "SHIFT_JIS",   90 },
	{                   "SJIS",   90 },
	{                   "TCVN",   91 },
	{              "TCVN-5712",   91 },
	{             "TCVN5712-1",   91 },
	{        "TCVN5712-1:1993",   91 },
	{                "TIS-620",   67 },
	{                 "TIS620",   67 },
	{               "TIS620-0",   67 },
	{          "TIS620.2529-1",   67 },
	{          "TIS620.2533-0",   67 },
	{          "TIS620.2533-1",   67 },
	{                "UCS-2BE",    3 },
	{                "UCS-2LE",    2 },
	{                "UCS-4BE",    7 },
	{                "UCS-4LE",    6 },
	{                    "UHC",   36 },
	{            "UNICODE-1-1",    3 },
	{      "UNICODE-1-1-UTF-7",   93 },
	{             "UNICODEBIG",    3 },
	{          "UNICODELITTLE",    2 },
	{                     "US",   92 },
	{               "US-ASCII",   92 },
	{               "UTF-16BE",    5 },
	{               "UTF-16LE",    4 },
	{               "UTF-32BE",    9 },
	{               "UTF-32LE",    8 },
	{                  "UTF-7",   93 },
	{                  "UTF-8",    1 },
	{                   "UTF7",   93 },
	{                   "UTF8",    1 },
	{                 "VISCII",   94 },
	{            "VISCII1.1-1",   94 },
	{             "WINBALTRIM",   26 },
	{           "WINDOWS-1250",   19 },
	{           "WINDOWS-1251",   20 },
	{           "WINDOWS-1252",   21 },
	{           "WINDOWS-1253",   22 },
	{           "WINDOWS-1254",   23 },
	{           "WINDOWS-1255",   24 },
	{           "WINDOWS-1256",   25 },
	{           "WINDOWS-1257",   26 },
	{           "WINDOWS-1258",   27 },
	{            "WINDOWS-874",   33 },
	{                  "X0201",   70 },
	{                  "X0208",   68 },
	{                  "X0212",   66 },
	{                   "big5",   11 },
	{                 "cp1250",   19 },
	{                 "cp1251",   20 },
	{                 "cp1252",   21 },
	{                 "cp1253",   22 },
	{                 "cp1254",   23 },
	{                 "cp1255",   24 },
	{                 "cp1256",   25 },
	{                 "cp1257",   26 },
	{                 "cp1258",   27 },
	{                  "cp437",   29 },
	{                  "cp850",   30 },
	{                  "cp862",   31 },
	{                  "cp866",   32 },
	{                  "cp874",   33 },
	{                  "eucJP",   38 },
	{                  "eucKR",   39 },
	{                  "eucTW",   40 },
	{                 "hp15CN",   14 },
	{                  "iso81",    0 },
	{                 "iso815",   54 },
	{                  "iso82",   56 },
	{                  "iso83",   57 },
	{                  "iso84",   58 },
	{                  "iso85",   59 },
	{                  "iso86",   60 },
	{                  "iso87",   61 },
	{                  "iso88",   62 },
	{               "iso88591",    0 },
	{              "iso885915",   54 },
	{               "iso88592",   56 },
	{               "iso88593",   57 },
	{               "iso88594",   58 },
	{               "iso88595",   59 },
	{               "iso88596",   60 },
	{               "iso88597",   61 },
	{               "iso88598",   62 },
	{               "iso88599",   63 },
	{                  "iso89",   63 },
	{                  "roma8",   89 },
	{                 "roman8",   89 },
	{                   "sjis",   90 },
	{                  "thai8",   67 },
	{                 "tis620",   67 },
	{                   "utf8",    1 },
	{NULL,	0}
};

#endif

enum {
	        TDS_CHARSET_ISO_8859_1 =   0,
	             TDS_CHARSET_UTF_8 =   1,
	           TDS_CHARSET_UCS_2LE =   2,
	           TDS_CHARSET_UCS_2BE =   3,
	          TDS_CHARSET_UTF_16LE =   4,
	          TDS_CHARSET_UTF_16BE =   5,
	           TDS_CHARSET_UCS_4LE =   6,
	           TDS_CHARSET_UCS_4BE =   7,
	          TDS_CHARSET_UTF_32LE =   8,
	          TDS_CHARSET_UTF_32BE =   9,
	         TDS_CHARSET_ARMSCII_8 =  10,
	             TDS_CHARSET_BIG_5 =  11,
	        TDS_CHARSET_BIG5_HKSCS =  12,
	               TDS_CHARSET_C99 =  13,
	           TDS_CHARSET_CHINESE =  14,
	                TDS_CHARSET_CN =  15,
	             TDS_CHARSET_CN_GB =  16,
	    TDS_CHARSET_CN_GB_ISOIR165 =  17,
	            TDS_CHARSET_CP1133 =  18,
	            TDS_CHARSET_CP1250 =  19,
	            TDS_CHARSET_CP1251 =  20,
	            TDS_CHARSET_CP1252 =  21,
	            TDS_CHARSET_CP1253 =  22,
	            TDS_CHARSET_CP1254 =  23,
	            TDS_CHARSET_CP1255 =  24,
	            TDS_CHARSET_CP1256 =  25,
	            TDS_CHARSET_CP1257 =  26,
	            TDS_CHARSET_CP1258 =  27,
	            TDS_CHARSET_CP1361 =  28,
	             TDS_CHARSET_CP437 =  29,
	             TDS_CHARSET_CP850 =  30,
	             TDS_CHARSET_CP862 =  31,
	             TDS_CHARSET_CP866 =  32,
	             TDS_CHARSET_CP874 =  33,
	             TDS_CHARSET_CP932 =  34,
	             TDS_CHARSET_CP936 =  35,
	             TDS_CHARSET_CP949 =  36,
	             TDS_CHARSET_CP950 =  37,
	            TDS_CHARSET_EUC_JP =  38,
	            TDS_CHARSET_EUC_KR =  39,
	            TDS_CHARSET_EUC_TW =  40,
	           TDS_CHARSET_GB18030 =  41,
	  TDS_CHARSET_GEORGIAN_ACADEMY =  42,
	       TDS_CHARSET_GEORGIAN_PS =  43,
	                TDS_CHARSET_HZ =  44,
	       TDS_CHARSET_ISO_2022_CN =  45,
	   TDS_CHARSET_ISO_2022_CN_EXT =  46,
	       TDS_CHARSET_ISO_2022_JP =  47,
	     TDS_CHARSET_ISO_2022_JP_1 =  48,
	     TDS_CHARSET_ISO_2022_JP_2 =  49,
	       TDS_CHARSET_ISO_2022_KR =  50,
	       TDS_CHARSET_ISO_8859_10 =  51,
	       TDS_CHARSET_ISO_8859_13 =  52,
	       TDS_CHARSET_ISO_8859_14 =  53,
	       TDS_CHARSET_ISO_8859_15 =  54,
	       TDS_CHARSET_ISO_8859_16 =  55,
	        TDS_CHARSET_ISO_8859_2 =  56,
	        TDS_CHARSET_ISO_8859_3 =  57,
	        TDS_CHARSET_ISO_8859_4 =  58,
	        TDS_CHARSET_ISO_8859_5 =  59,
	        TDS_CHARSET_ISO_8859_6 =  60,
	        TDS_CHARSET_ISO_8859_7 =  61,
	        TDS_CHARSET_ISO_8859_8 =  62,
	        TDS_CHARSET_ISO_8859_9 =  63,
	         TDS_CHARSET_ISO_IR_14 =  64,
	        TDS_CHARSET_ISO_IR_149 =  65,
	        TDS_CHARSET_ISO_IR_159 =  66,
	        TDS_CHARSET_ISO_IR_166 =  67,
	         TDS_CHARSET_ISO_IR_87 =  68,
	              TDS_CHARSET_JAVA =  69,
	     TDS_CHARSET_JISX0201_1976 =  70,
	            TDS_CHARSET_KOI8_R =  71,
	           TDS_CHARSET_KOI8_RU =  72,
	            TDS_CHARSET_KOI8_T =  73,
	            TDS_CHARSET_KOI8_U =  74,
	               TDS_CHARSET_MAC =  75,
	         TDS_CHARSET_MACARABIC =  76,
	  TDS_CHARSET_MACCENTRALEUROPE =  77,
	       TDS_CHARSET_MACCROATIAN =  78,
	       TDS_CHARSET_MACCYRILLIC =  79,
	          TDS_CHARSET_MACGREEK =  80,
	         TDS_CHARSET_MACHEBREW =  81,
	        TDS_CHARSET_MACICELAND =  82,
	        TDS_CHARSET_MACROMANIA =  83,
	           TDS_CHARSET_MACTHAI =  84,
	        TDS_CHARSET_MACTURKISH =  85,
	        TDS_CHARSET_MACUKRAINE =  86,
	         TDS_CHARSET_MULELAO_1 =  87,
	          TDS_CHARSET_NEXTSTEP =  88,
	            TDS_CHARSET_ROMAN8 =  89,
	              TDS_CHARSET_SJIS =  90,
	              TDS_CHARSET_TCVN =  91,
	          TDS_CHARSET_US_ASCII =  92,
	             TDS_CHARSET_UTF_7 =  93,
	            TDS_CHARSET_VISCII =  94,
	              TDS_NUM_CHARSETS =  95
};

