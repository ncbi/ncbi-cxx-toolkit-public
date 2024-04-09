/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: /usr/pkg/bin/gperf -m 100 -C -K alias_pos -t -F ,-1 -P -H hash_charset -N charset_lookup -L ANSI-C --enum charset_lookup.gperf  */
/* Computed positions: -k'1,3-11,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 3 "charset_lookup.gperf"

static const struct charset_alias *charset_lookup(register const char *str, register size_t len);
#line 2 "charset_lookup.gperf"
struct charset_alias { short int alias_pos; short int canonic; };
/* maximum key range = 1038, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash_charset (register const char *str, register size_t len)
{
  static const unsigned short asso_values[] =
    {
      1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070,    8,  113, 1070,   34,    7,
        83,  104,   36,    9,   20,   85,    8,   10,  330, 1070,
      1070, 1070, 1070, 1070, 1070,   50,  186,   97,    7,   84,
        66,   32,   73,    7,   17,  186,   27,  183,    9,    7,
       106, 1070,   51,    7,   11,  117,  229,  151,  339,   22,
        10, 1070, 1070, 1070, 1070,   24, 1070,   13,    7,  179,
      1070,    7,    9,   13,   27,    7, 1070,    7, 1070,    8,
         7,    7, 1070, 1070,    8,    8,    7,   10, 1070, 1070,
      1070,   12, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070, 1070,
      1070, 1070, 1070, 1070, 1070, 1070
    };
  register unsigned int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[10]];
      /*FALLTHROUGH*/
      case 10:
        hval += asso_values[(unsigned char)str[9]];
      /*FALLTHROUGH*/
      case 9:
        hval += asso_values[(unsigned char)str[8]];
      /*FALLTHROUGH*/
      case 8:
        hval += asso_values[(unsigned char)str[7]];
      /*FALLTHROUGH*/
      case 7:
        hval += asso_values[(unsigned char)str[6]];
      /*FALLTHROUGH*/
      case 6:
        hval += asso_values[(unsigned char)str[5]];
      /*FALLTHROUGH*/
      case 5:
        hval += asso_values[(unsigned char)str[4]];
      /*FALLTHROUGH*/
      case 4:
        hval += asso_values[(unsigned char)str[3]];
      /*FALLTHROUGH*/
      case 3:
        hval += asso_values[(unsigned char)str[2]];
      /*FALLTHROUGH*/
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

struct stringpool_t
  {
    char stringpool_str32[sizeof("SJIS")];
    char stringpool_str34[sizeof("koi8")];
    char stringpool_str35[sizeof("sjis")];
    char stringpool_str36[sizeof("L1")];
    char stringpool_str37[sizeof("L8")];
    char stringpool_str38[sizeof("L5")];
    char stringpool_str39[sizeof("utf8")];
    char stringpool_str41[sizeof("iso81")];
    char stringpool_str42[sizeof("big5")];
    char stringpool_str43[sizeof("iso88")];
    char stringpool_str44[sizeof("iso15")];
    char stringpool_str45[sizeof("iso85")];
    char stringpool_str47[sizeof("iso89")];
    char stringpool_str48[sizeof("thai8")];
    char stringpool_str49[sizeof("L6")];
    char stringpool_str50[sizeof("roma8")];
    char stringpool_str51[sizeof("866")];
    char stringpool_str53[sizeof("iso815")];
    char stringpool_str56[sizeof("greek8")];
    char stringpool_str57[sizeof("iso_1")];
    char stringpool_str58[sizeof("roman8")];
    char stringpool_str61[sizeof("R8")];
    char stringpool_str63[sizeof("646")];
    char stringpool_str65[sizeof("L4")];
    char stringpool_str67[sizeof("iso86")];
    char stringpool_str71[sizeof("iso88591")];
    char stringpool_str73[sizeof("iso88598")];
    char stringpool_str74[sizeof("LATIN1")];
    char stringpool_str75[sizeof("iso88595")];
    char stringpool_str76[sizeof("LATIN8")];
    char stringpool_str77[sizeof("iso88599")];
    char stringpool_str78[sizeof("LATIN5")];
    char stringpool_str79[sizeof("850")];
    char stringpool_str80[sizeof("ISO8859-1")];
    char stringpool_str82[sizeof("ISO8859-8")];
    char stringpool_str83[sizeof("iso885915")];
    char stringpool_str84[sizeof("ISO8859-5")];
    char stringpool_str85[sizeof("HZ")];
    char stringpool_str86[sizeof("ISO8859-9")];
    char stringpool_str89[sizeof("ISO-8859-1")];
    char stringpool_str91[sizeof("ISO-8859-8")];
    char stringpool_str92[sizeof("ISO8859-15")];
    char stringpool_str93[sizeof("ISO-8859-5")];
    char stringpool_str94[sizeof("iso10")];
    char stringpool_str95[sizeof("ISO-8859-9")];
    char stringpool_str97[sizeof("iso88596")];
    char stringpool_str98[sizeof("iso14")];
    char stringpool_str99[sizeof("iso84")];
    char stringpool_str100[sizeof("LATIN6")];
    char stringpool_str101[sizeof("ISO-8859-15")];
    char stringpool_str105[sizeof("ISO_8859-1")];
    char stringpool_str106[sizeof("ISO8859-6")];
    char stringpool_str107[sizeof("ISO_8859-8")];
    char stringpool_str108[sizeof("CN")];
    char stringpool_str109[sizeof("ISO_8859-5")];
    char stringpool_str111[sizeof("ISO_8859-9")];
    char stringpool_str112[sizeof("L2")];
    char stringpool_str114[sizeof("L7")];
    char stringpool_str115[sizeof("ISO-8859-6")];
    char stringpool_str116[sizeof("iso646")];
    char stringpool_str117[sizeof("ISO_8859-15")];
    char stringpool_str120[sizeof("C99")];
    char stringpool_str121[sizeof("ISO_8859-15:1998")];
    char stringpool_str122[sizeof("ISO-IR-58")];
    char stringpool_str123[sizeof("ISO-8859-16")];
    char stringpool_str125[sizeof("JP")];
    char stringpool_str126[sizeof("US")];
    char stringpool_str129[sizeof("iso88594")];
    char stringpool_str131[sizeof("ISO_8859-6")];
    char stringpool_str132[sizeof("LATIN4")];
    char stringpool_str133[sizeof("L3")];
    char stringpool_str134[sizeof("ISO-IR-159")];
    char stringpool_str135[sizeof("ISO-IR-199")];
    char stringpool_str136[sizeof("ISO-IR-6")];
    char stringpool_str137[sizeof("CP819")];
    char stringpool_str138[sizeof("ISO8859-4")];
    char stringpool_str139[sizeof("ISO_8859-16")];
    char stringpool_str142[sizeof("ISO8859-10")];
    char stringpool_str143[sizeof("ISO-IR-165")];
    char stringpool_str146[sizeof("SHIFT-JIS")];
    char stringpool_str147[sizeof("ISO-8859-4")];
    char stringpool_str148[sizeof("ISO_8859-14:1998")];
    char stringpool_str151[sizeof("ISO-8859-10")];
    char stringpool_str153[sizeof("ISO-IR-101")];
    char stringpool_str155[sizeof("ISO-8859-14")];
    char stringpool_str157[sizeof("ISO-IR-148")];
    char stringpool_str158[sizeof("ISO_8859-16:2000")];
    char stringpool_str159[sizeof("ISO-IR-109")];
    char stringpool_str161[sizeof("ISO-IR-149")];
    char stringpool_str162[sizeof("SHIFT_JIS")];
    char stringpool_str163[sizeof("ISO_8859-4")];
    char stringpool_str164[sizeof("hp15CN")];
    char stringpool_str165[sizeof("ISO-IR-166")];
    char stringpool_str167[sizeof("ISO_8859-10")];
    char stringpool_str168[sizeof("CP949")];
    char stringpool_str170[sizeof("CP866")];
    char stringpool_str171[sizeof("ISO_8859-14")];
    char stringpool_str173[sizeof("ASCII")];
    char stringpool_str176[sizeof("ISO-IR-14")];
    char stringpool_str177[sizeof("862")];
    char stringpool_str180[sizeof("ISO-IR-110")];
    char stringpool_str183[sizeof("GB_1988-80")];
    char stringpool_str187[sizeof("CP850")];
    char stringpool_str189[sizeof("CP950")];
    char stringpool_str192[sizeof("tis620")];
    char stringpool_str193[sizeof("iso82")];
    char stringpool_str195[sizeof("TIS620")];
    char stringpool_str197[sizeof("iso87")];
    char stringpool_str198[sizeof("JIS0208")];
    char stringpool_str203[sizeof("UTF8")];
    char stringpool_str204[sizeof("TIS-620")];
    char stringpool_str207[sizeof("ISO-IR-100")];
    char stringpool_str210[sizeof("ISO-IR-179")];
    char stringpool_str212[sizeof("UTF-8")];
    char stringpool_str213[sizeof("ISO-IR-144")];
    char stringpool_str215[sizeof("CP65001")];
    char stringpool_str216[sizeof("CP1251")];
    char stringpool_str218[sizeof("CP1258")];
    char stringpool_str220[sizeof("CP1255")];
    char stringpool_str221[sizeof("ISO_8859-10:1992")];
    char stringpool_str222[sizeof("ISO646-CN")];
    char stringpool_str223[sizeof("iso88592")];
    char stringpool_str225[sizeof("ISO-IR-138")];
    char stringpool_str226[sizeof("LATIN2")];
    char stringpool_str227[sizeof("iso88597")];
    char stringpool_str228[sizeof("ISO-IR-126")];
    char stringpool_str229[sizeof("CSISO159JISX02121990")];
    char stringpool_str230[sizeof("LATIN7")];
    char stringpool_str231[sizeof("IBM819")];
    char stringpool_str232[sizeof("ISO8859-2")];
    char stringpool_str233[sizeof("macturk")];
    char stringpool_str234[sizeof("iso13")];
    char stringpool_str235[sizeof("iso83")];
    char stringpool_str236[sizeof("ISO8859-7")];
    char stringpool_str237[sizeof("KOI8-T")];
    char stringpool_str238[sizeof("ISO646-US")];
    char stringpool_str239[sizeof("TIS620-0")];
    char stringpool_str240[sizeof("BIG5")];
    char stringpool_str241[sizeof("ISO-8859-2")];
    char stringpool_str242[sizeof("CP1256")];
    char stringpool_str243[sizeof("ELOT_928")];
    char stringpool_str244[sizeof("macgreek")];
    char stringpool_str245[sizeof("ISO-8859-7")];
    char stringpool_str247[sizeof("CSISOLATIN1")];
    char stringpool_str248[sizeof("CP1361")];
    char stringpool_str249[sizeof("BIG-5")];
    char stringpool_str250[sizeof("cp949")];
    char stringpool_str251[sizeof("CSISOLATIN5")];
    char stringpool_str252[sizeof("cp866")];
    char stringpool_str253[sizeof("ascii_8")];
    char stringpool_str255[sizeof("macthai")];
    char stringpool_str256[sizeof("CP936")];
    char stringpool_str257[sizeof("ISO_8859-2")];
    char stringpool_str260[sizeof("GB18030")];
    char stringpool_str261[sizeof("ISO_8859-7")];
    char stringpool_str262[sizeof("TCVN")];
    char stringpool_str264[sizeof("IBM866")];
    char stringpool_str265[sizeof("iso88593")];
    char stringpool_str267[sizeof("CP874")];
    char stringpool_str268[sizeof("LATIN3")];
    char stringpool_str269[sizeof("cp850")];
    char stringpool_str270[sizeof("CP1250")];
    char stringpool_str271[sizeof("cp950")];
    char stringpool_str273[sizeof("CSISOLATIN6")];
    char stringpool_str274[sizeof("CP1254")];
    char stringpool_str275[sizeof("ISO-IR-87")];
    char stringpool_str276[sizeof("ISO-IR-57")];
    char stringpool_str278[sizeof("MS-ANSI")];
    char stringpool_str279[sizeof("CSASCII")];
    char stringpool_str281[sizeof("IBM850")];
    char stringpool_str283[sizeof("ISO-8859-3")];
    char stringpool_str284[sizeof("ISO-IR-157")];
    char stringpool_str291[sizeof("ISO-8859-13")];
    char stringpool_str296[sizeof("CP862")];
    char stringpool_str298[sizeof("cp1251")];
    char stringpool_str299[sizeof("ISO_8859-3")];
    char stringpool_str300[sizeof("cp1258")];
    char stringpool_str301[sizeof("EUCCN")];
    char stringpool_str302[sizeof("cp1255")];
    char stringpool_str304[sizeof("ISO-IR-226")];
    char stringpool_str305[sizeof("CSISOLATIN4")];
    char stringpool_str307[sizeof("ISO_8859-13")];
    char stringpool_str308[sizeof("US-ASCII")];
    char stringpool_str309[sizeof("CSSHIFTJIS")];
    char stringpool_str310[sizeof("EUC-CN")];
    char stringpool_str313[sizeof("CSISO14JISC6220RO")];
    char stringpool_str314[sizeof("UHC")];
    char stringpool_str315[sizeof("ROMAN8")];
    char stringpool_str317[sizeof("KOI8-R")];
    char stringpool_str324[sizeof("cp1256")];
    char stringpool_str327[sizeof("GEORGIAN-PS")];
    char stringpool_str336[sizeof("ISO646-JP")];
    char stringpool_str338[sizeof("cp936")];
    char stringpool_str346[sizeof("CSBIG5")];
    char stringpool_str349[sizeof("cp874")];
    char stringpool_str350[sizeof("JAVA")];
    char stringpool_str352[sizeof("cp1250")];
    char stringpool_str355[sizeof("CN-BIG5")];
    char stringpool_str356[sizeof("cp1254")];
    char stringpool_str357[sizeof("UTF7")];
    char stringpool_str358[sizeof("ISO-IR-127")];
    char stringpool_str360[sizeof("VISCII")];
    char stringpool_str363[sizeof("ECMA-118")];
    char stringpool_str366[sizeof("UTF-7")];
    char stringpool_str367[sizeof("UNICODE-1-1")];
    char stringpool_str368[sizeof("CP1252")];
    char stringpool_str369[sizeof("mac")];
    char stringpool_str370[sizeof("UCS-4LE")];
    char stringpool_str372[sizeof("CP1257")];
    char stringpool_str378[sizeof("cp862")];
    char stringpool_str379[sizeof("CHINESE")];
    char stringpool_str380[sizeof("MAC")];
    char stringpool_str381[sizeof("GEORGIAN-ACADEMY")];
    char stringpool_str382[sizeof("CP932")];
    char stringpool_str384[sizeof("ARMSCII-8")];
    char stringpool_str385[sizeof("CSISOLATINARABIC")];
    char stringpool_str390[sizeof("IBM862")];
    char stringpool_str391[sizeof("ASMO-708")];
    char stringpool_str392[sizeof("KSC_5601")];
    char stringpool_str395[sizeof("KOREAN")];
    char stringpool_str396[sizeof("CP367")];
    char stringpool_str398[sizeof("GB2312")];
    char stringpool_str399[sizeof("CSISOLATIN2")];
    char stringpool_str404[sizeof("JIS_C6220-1969-RO")];
    char stringpool_str406[sizeof("HP-ROMAN8")];
    char stringpool_str407[sizeof("GBK")];
    char stringpool_str408[sizeof("GREEK8")];
    char stringpool_str409[sizeof("MULELAO-1")];
    char stringpool_str410[sizeof("CP1253")];
    char stringpool_str412[sizeof("CP437")];
    char stringpool_str414[sizeof("CSKOI8R")];
    char stringpool_str415[sizeof("EUCJP")];
    char stringpool_str417[sizeof("UCS-2LE")];
    char stringpool_str418[sizeof("CYRILLIC")];
    char stringpool_str419[sizeof("ECMA-114")];
    char stringpool_str420[sizeof("eucJP")];
    char stringpool_str421[sizeof("UTF-16LE")];
    char stringpool_str422[sizeof("MS-CYRL")];
    char stringpool_str423[sizeof("ISO-IR-203")];
    char stringpool_str424[sizeof("EUC-JP")];
    char stringpool_str425[sizeof("mac_cyr")];
    char stringpool_str427[sizeof("GB_2312-80")];
    char stringpool_str429[sizeof("CP1133")];
    char stringpool_str433[sizeof("CN-GB-ISOIR165")];
    char stringpool_str434[sizeof("CSISOLATINCYRILLIC")];
    char stringpool_str435[sizeof("MACTHAI")];
    char stringpool_str439[sizeof("ISO-2022-CN")];
    char stringpool_str440[sizeof("KS_C_5601-1989")];
    char stringpool_str441[sizeof("CSISOLATIN3")];
    char stringpool_str442[sizeof("ISO_8859-8:1988")];
    char stringpool_str443[sizeof("ISO_8859-5:1988")];
    char stringpool_str445[sizeof("ISO-2022-CN-EXT")];
    char stringpool_str446[sizeof("ISO_8859-9:1989")];
    char stringpool_str448[sizeof("MS-EE")];
    char stringpool_str449[sizeof("KOI8-U")];
    char stringpool_str450[sizeof("cp1252")];
    char stringpool_str451[sizeof("UNICODE-1-1-UTF-7")];
    char stringpool_str452[sizeof("ISO-CELTIC")];
    char stringpool_str454[sizeof("cp1257")];
    char stringpool_str455[sizeof("CSISOLATINGREEK")];
    char stringpool_str456[sizeof("ISO-2022-JP-1")];
    char stringpool_str457[sizeof("CSUNICODE11")];
    char stringpool_str458[sizeof("WINDOWS-1251")];
    char stringpool_str459[sizeof("WINDOWS-1258")];
    char stringpool_str460[sizeof("WINDOWS-1255")];
    char stringpool_str462[sizeof("CSISOLATINHEBREW")];
    char stringpool_str464[sizeof("cp932")];
    char stringpool_str465[sizeof("TCVN5712-1")];
    char stringpool_str466[sizeof("CSVISCII")];
    char stringpool_str468[sizeof("CSISO57GB1988")];
    char stringpool_str470[sizeof("ISO_8859-4:1988")];
    char stringpool_str471[sizeof("WINDOWS-1256")];
    char stringpool_str472[sizeof("UNICODELITTLE")];
    char stringpool_str473[sizeof("TIS620.2529-1")];
    char stringpool_str474[sizeof("EUCKR")];
    char stringpool_str475[sizeof("X0201")];
    char stringpool_str476[sizeof("MACINTOSH")];
    char stringpool_str477[sizeof("X0208")];
    char stringpool_str479[sizeof("eucKR")];
    char stringpool_str483[sizeof("EUC-KR")];
    char stringpool_str484[sizeof("JIS_C6226-1983")];
    char stringpool_str485[sizeof("WINDOWS-1250")];
    char stringpool_str487[sizeof("WINDOWS-1254")];
    char stringpool_str490[sizeof("IBM367")];
    char stringpool_str491[sizeof("MS_KANJI")];
    char stringpool_str492[sizeof("cp1253")];
    char stringpool_str493[sizeof("ARABIC")];
    char stringpool_str494[sizeof("cp437")];
    char stringpool_str499[sizeof("EUCTW")];
    char stringpool_str501[sizeof("KOI8-RU")];
    char stringpool_str504[sizeof("eucTW")];
    char stringpool_str506[sizeof("IBM437")];
    char stringpool_str507[sizeof("VISCII1.1-1")];
    char stringpool_str508[sizeof("EUC-TW")];
    char stringpool_str514[sizeof("CN-GB")];
    char stringpool_str515[sizeof("KS_C_5601-1987")];
    char stringpool_str516[sizeof("WINDOWS-874")];
    char stringpool_str517[sizeof("JOHAB")];
    char stringpool_str518[sizeof("ISO_8859-1:1987")];
    char stringpool_str527[sizeof("CSISO2022CN")];
    char stringpool_str529[sizeof("UCS-4BE")];
    char stringpool_str531[sizeof("ISO_8859-6:1987")];
    char stringpool_str532[sizeof("ISO-2022-JP-2")];
    char stringpool_str533[sizeof("TCVN-5712")];
    char stringpool_str534[sizeof("WINDOWS-1252")];
    char stringpool_str536[sizeof("WINDOWS-1257")];
    char stringpool_str537[sizeof("ISO_646.IRV:1991")];
    char stringpool_str538[sizeof("ISO_8859-3:1988")];
    char stringpool_str539[sizeof("CSUNICODE11UTF7")];
    char stringpool_str549[sizeof("CSIBM866")];
    char stringpool_str553[sizeof("ISO-2022-JP")];
    char stringpool_str555[sizeof("WINDOWS-1253")];
    char stringpool_str561[sizeof("JIS_X0201")];
    char stringpool_str563[sizeof("JIS_X0208")];
    char stringpool_str565[sizeof("EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE")];
    char stringpool_str572[sizeof("CSKSC56011987")];
    char stringpool_str576[sizeof("UCS-2BE")];
    char stringpool_str577[sizeof("GREEK")];
    char stringpool_str578[sizeof("MACICELAND")];
    char stringpool_str579[sizeof("JISX0201-1976")];
    char stringpool_str580[sizeof("UTF-16BE")];
    char stringpool_str581[sizeof("UTF-32LE")];
    char stringpool_str582[sizeof("MACCROATIAN")];
    char stringpool_str586[sizeof("UNICODEBIG")];
    char stringpool_str588[sizeof("TIS620.2533-1")];
    char stringpool_str589[sizeof("CSISO58GB231280")];
    char stringpool_str594[sizeof("ISO_8859-2:1987")];
    char stringpool_str596[sizeof("ISO_8859-7:1987")];
    char stringpool_str597[sizeof("MACROMAN")];
    char stringpool_str600[sizeof("X0212")];
    char stringpool_str602[sizeof("CSHPROMAN8")];
    char stringpool_str604[sizeof("CSISO87JISX0208")];
    char stringpool_str609[sizeof("JIS_X0208-1990")];
    char stringpool_str612[sizeof("ISO-2022-KR")];
    char stringpool_str613[sizeof("BIG5HKSCS")];
    char stringpool_str615[sizeof("TIS620.2533-0")];
    char stringpool_str619[sizeof("CSISO2022JP2")];
    char stringpool_str622[sizeof("BIG5-HKSCS")];
    char stringpool_str625[sizeof("CSMACINTOSH")];
    char stringpool_str631[sizeof("CSHALFWIDTHKATAKANA")];
    char stringpool_str641[sizeof("CSISO2022JP")];
    char stringpool_str643[sizeof("MS-HEBR")];
    char stringpool_str657[sizeof("JIS_X0212-1990")];
    char stringpool_str675[sizeof("CSPC862LATINHEBREW")];
    char stringpool_str677[sizeof("HZ-GB-2312")];
    char stringpool_str679[sizeof("JIS_X0208-1983")];
    char stringpool_str681[sizeof("NEXTSTEP")];
    char stringpool_str683[sizeof("CSGB2312")];
    char stringpool_str686[sizeof("JIS_X0212")];
    char stringpool_str690[sizeof("CSEUCKR")];
    char stringpool_str695[sizeof("BIGFIVE")];
    char stringpool_str697[sizeof("MACROMANIA")];
    char stringpool_str700[sizeof("CSISO2022KR")];
    char stringpool_str702[sizeof("HEBREW")];
    char stringpool_str704[sizeof("BIG-FIVE")];
    char stringpool_str715[sizeof("CSEUCTW")];
    char stringpool_str717[sizeof("ANSI_X3.4-1968")];
    char stringpool_str721[sizeof("MS-ARAB")];
    char stringpool_str723[sizeof("MACCYRILLIC")];
    char stringpool_str729[sizeof("ANSI_X3.4-1986")];
    char stringpool_str735[sizeof("CSPC850MULTILINGUAL")];
    char stringpool_str737[sizeof("IBM-CP1133")];
    char stringpool_str740[sizeof("UTF-32BE")];
    char stringpool_str749[sizeof("MS-TURK")];
    char stringpool_str764[sizeof("JIS_X0212.1990-0")];
    char stringpool_str793[sizeof("MACCENTRALEUROPE")];
    char stringpool_str815[sizeof("MACTURKISH")];
    char stringpool_str822[sizeof("MS-GREEK")];
    char stringpool_str827[sizeof("MACARABIC")];
    char stringpool_str868[sizeof("WINBALTRIM")];
    char stringpool_str878[sizeof("MACUKRAINE")];
    char stringpool_str897[sizeof("TCVN5712-1:1993")];
    char stringpool_str911[sizeof("MACGREEK")];
    char stringpool_str1057[sizeof("CSEUCPKDFMTJAPANESE")];
    char stringpool_str1069[sizeof("MACHEBREW")];
  };
static const struct stringpool_t stringpool_contents =
  {
    "SJIS",
    "koi8",
    "sjis",
    "L1",
    "L8",
    "L5",
    "utf8",
    "iso81",
    "big5",
    "iso88",
    "iso15",
    "iso85",
    "iso89",
    "thai8",
    "L6",
    "roma8",
    "866",
    "iso815",
    "greek8",
    "iso_1",
    "roman8",
    "R8",
    "646",
    "L4",
    "iso86",
    "iso88591",
    "iso88598",
    "LATIN1",
    "iso88595",
    "LATIN8",
    "iso88599",
    "LATIN5",
    "850",
    "ISO8859-1",
    "ISO8859-8",
    "iso885915",
    "ISO8859-5",
    "HZ",
    "ISO8859-9",
    "ISO-8859-1",
    "ISO-8859-8",
    "ISO8859-15",
    "ISO-8859-5",
    "iso10",
    "ISO-8859-9",
    "iso88596",
    "iso14",
    "iso84",
    "LATIN6",
    "ISO-8859-15",
    "ISO_8859-1",
    "ISO8859-6",
    "ISO_8859-8",
    "CN",
    "ISO_8859-5",
    "ISO_8859-9",
    "L2",
    "L7",
    "ISO-8859-6",
    "iso646",
    "ISO_8859-15",
    "C99",
    "ISO_8859-15:1998",
    "ISO-IR-58",
    "ISO-8859-16",
    "JP",
    "US",
    "iso88594",
    "ISO_8859-6",
    "LATIN4",
    "L3",
    "ISO-IR-159",
    "ISO-IR-199",
    "ISO-IR-6",
    "CP819",
    "ISO8859-4",
    "ISO_8859-16",
    "ISO8859-10",
    "ISO-IR-165",
    "SHIFT-JIS",
    "ISO-8859-4",
    "ISO_8859-14:1998",
    "ISO-8859-10",
    "ISO-IR-101",
    "ISO-8859-14",
    "ISO-IR-148",
    "ISO_8859-16:2000",
    "ISO-IR-109",
    "ISO-IR-149",
    "SHIFT_JIS",
    "ISO_8859-4",
    "hp15CN",
    "ISO-IR-166",
    "ISO_8859-10",
    "CP949",
    "CP866",
    "ISO_8859-14",
    "ASCII",
    "ISO-IR-14",
    "862",
    "ISO-IR-110",
    "GB_1988-80",
    "CP850",
    "CP950",
    "tis620",
    "iso82",
    "TIS620",
    "iso87",
    "JIS0208",
    "UTF8",
    "TIS-620",
    "ISO-IR-100",
    "ISO-IR-179",
    "UTF-8",
    "ISO-IR-144",
    "CP65001",
    "CP1251",
    "CP1258",
    "CP1255",
    "ISO_8859-10:1992",
    "ISO646-CN",
    "iso88592",
    "ISO-IR-138",
    "LATIN2",
    "iso88597",
    "ISO-IR-126",
    "CSISO159JISX02121990",
    "LATIN7",
    "IBM819",
    "ISO8859-2",
    "macturk",
    "iso13",
    "iso83",
    "ISO8859-7",
    "KOI8-T",
    "ISO646-US",
    "TIS620-0",
    "BIG5",
    "ISO-8859-2",
    "CP1256",
    "ELOT_928",
    "macgreek",
    "ISO-8859-7",
    "CSISOLATIN1",
    "CP1361",
    "BIG-5",
    "cp949",
    "CSISOLATIN5",
    "cp866",
    "ascii_8",
    "macthai",
    "CP936",
    "ISO_8859-2",
    "GB18030",
    "ISO_8859-7",
    "TCVN",
    "IBM866",
    "iso88593",
    "CP874",
    "LATIN3",
    "cp850",
    "CP1250",
    "cp950",
    "CSISOLATIN6",
    "CP1254",
    "ISO-IR-87",
    "ISO-IR-57",
    "MS-ANSI",
    "CSASCII",
    "IBM850",
    "ISO-8859-3",
    "ISO-IR-157",
    "ISO-8859-13",
    "CP862",
    "cp1251",
    "ISO_8859-3",
    "cp1258",
    "EUCCN",
    "cp1255",
    "ISO-IR-226",
    "CSISOLATIN4",
    "ISO_8859-13",
    "US-ASCII",
    "CSSHIFTJIS",
    "EUC-CN",
    "CSISO14JISC6220RO",
    "UHC",
    "ROMAN8",
    "KOI8-R",
    "cp1256",
    "GEORGIAN-PS",
    "ISO646-JP",
    "cp936",
    "CSBIG5",
    "cp874",
    "JAVA",
    "cp1250",
    "CN-BIG5",
    "cp1254",
    "UTF7",
    "ISO-IR-127",
    "VISCII",
    "ECMA-118",
    "UTF-7",
    "UNICODE-1-1",
    "CP1252",
    "mac",
    "UCS-4LE",
    "CP1257",
    "cp862",
    "CHINESE",
    "MAC",
    "GEORGIAN-ACADEMY",
    "CP932",
    "ARMSCII-8",
    "CSISOLATINARABIC",
    "IBM862",
    "ASMO-708",
    "KSC_5601",
    "KOREAN",
    "CP367",
    "GB2312",
    "CSISOLATIN2",
    "JIS_C6220-1969-RO",
    "HP-ROMAN8",
    "GBK",
    "GREEK8",
    "MULELAO-1",
    "CP1253",
    "CP437",
    "CSKOI8R",
    "EUCJP",
    "UCS-2LE",
    "CYRILLIC",
    "ECMA-114",
    "eucJP",
    "UTF-16LE",
    "MS-CYRL",
    "ISO-IR-203",
    "EUC-JP",
    "mac_cyr",
    "GB_2312-80",
    "CP1133",
    "CN-GB-ISOIR165",
    "CSISOLATINCYRILLIC",
    "MACTHAI",
    "ISO-2022-CN",
    "KS_C_5601-1989",
    "CSISOLATIN3",
    "ISO_8859-8:1988",
    "ISO_8859-5:1988",
    "ISO-2022-CN-EXT",
    "ISO_8859-9:1989",
    "MS-EE",
    "KOI8-U",
    "cp1252",
    "UNICODE-1-1-UTF-7",
    "ISO-CELTIC",
    "cp1257",
    "CSISOLATINGREEK",
    "ISO-2022-JP-1",
    "CSUNICODE11",
    "WINDOWS-1251",
    "WINDOWS-1258",
    "WINDOWS-1255",
    "CSISOLATINHEBREW",
    "cp932",
    "TCVN5712-1",
    "CSVISCII",
    "CSISO57GB1988",
    "ISO_8859-4:1988",
    "WINDOWS-1256",
    "UNICODELITTLE",
    "TIS620.2529-1",
    "EUCKR",
    "X0201",
    "MACINTOSH",
    "X0208",
    "eucKR",
    "EUC-KR",
    "JIS_C6226-1983",
    "WINDOWS-1250",
    "WINDOWS-1254",
    "IBM367",
    "MS_KANJI",
    "cp1253",
    "ARABIC",
    "cp437",
    "EUCTW",
    "KOI8-RU",
    "eucTW",
    "IBM437",
    "VISCII1.1-1",
    "EUC-TW",
    "CN-GB",
    "KS_C_5601-1987",
    "WINDOWS-874",
    "JOHAB",
    "ISO_8859-1:1987",
    "CSISO2022CN",
    "UCS-4BE",
    "ISO_8859-6:1987",
    "ISO-2022-JP-2",
    "TCVN-5712",
    "WINDOWS-1252",
    "WINDOWS-1257",
    "ISO_646.IRV:1991",
    "ISO_8859-3:1988",
    "CSUNICODE11UTF7",
    "CSIBM866",
    "ISO-2022-JP",
    "WINDOWS-1253",
    "JIS_X0201",
    "JIS_X0208",
    "EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE",
    "CSKSC56011987",
    "UCS-2BE",
    "GREEK",
    "MACICELAND",
    "JISX0201-1976",
    "UTF-16BE",
    "UTF-32LE",
    "MACCROATIAN",
    "UNICODEBIG",
    "TIS620.2533-1",
    "CSISO58GB231280",
    "ISO_8859-2:1987",
    "ISO_8859-7:1987",
    "MACROMAN",
    "X0212",
    "CSHPROMAN8",
    "CSISO87JISX0208",
    "JIS_X0208-1990",
    "ISO-2022-KR",
    "BIG5HKSCS",
    "TIS620.2533-0",
    "CSISO2022JP2",
    "BIG5-HKSCS",
    "CSMACINTOSH",
    "CSHALFWIDTHKATAKANA",
    "CSISO2022JP",
    "MS-HEBR",
    "JIS_X0212-1990",
    "CSPC862LATINHEBREW",
    "HZ-GB-2312",
    "JIS_X0208-1983",
    "NEXTSTEP",
    "CSGB2312",
    "JIS_X0212",
    "CSEUCKR",
    "BIGFIVE",
    "MACROMANIA",
    "CSISO2022KR",
    "HEBREW",
    "BIG-FIVE",
    "CSEUCTW",
    "ANSI_X3.4-1968",
    "MS-ARAB",
    "MACCYRILLIC",
    "ANSI_X3.4-1986",
    "CSPC850MULTILINGUAL",
    "IBM-CP1133",
    "UTF-32BE",
    "MS-TURK",
    "JIS_X0212.1990-0",
    "MACCENTRALEUROPE",
    "MACTURKISH",
    "MS-GREEK",
    "MACARABIC",
    "WINBALTRIM",
    "MACUKRAINE",
    "TCVN5712-1:1993",
    "MACGREEK",
    "CSEUCPKDFMTJAPANESE",
    "MACHEBREW"
  };
#define stringpool ((const char *) &stringpool_contents)
const struct charset_alias *
charset_lookup (register const char *str, register size_t len)
{
  enum
    {
      TOTAL_KEYWORDS = 375,
      MIN_WORD_LENGTH = 2,
      MAX_WORD_LENGTH = 45,
      MIN_HASH_VALUE = 32,
      MAX_HASH_VALUE = 1069
    };

  static const struct charset_alias wordlist[] =
    {
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1},
#line 273 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str32), 90},
      {-1,-1},
#line 376 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str34), 71},
#line 360 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str35), 90},
#line 229 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str36), 0},
#line 236 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str37), 53},
#line 233 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str38), 63},
#line 363 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str39), 1},
      {-1,-1},
#line 338 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str41), 0},
#line 319 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str42), 11},
#line 346 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str43), 62},
#line 373 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str44), 54},
#line 343 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str45), 59},
      {-1,-1},
#line 357 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str47), 63},
#line 361 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str48), 67},
#line 234 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str49), 51},
#line 358 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str50), 89},
#line 10 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str51), 32},
      {-1,-1},
#line 339 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str53), 54},
      {-1,-1}, {-1,-1},
#line 369 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str56), 61},
#line 375 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str57), 0},
#line 359 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str58), 89},
      {-1,-1}, {-1,-1},
#line 269 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str61), 89},
      {-1,-1},
#line 7 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str63), 92},
      {-1,-1},
#line 232 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str65), 58},
      {-1,-1},
#line 344 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str67), 60},
      {-1,-1}, {-1,-1}, {-1,-1},
#line 347 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str71), 0},
      {-1,-1},
#line 355 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str73), 62},
#line 237 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str74), 0},
#line 352 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str75), 59},
#line 244 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str76), 53},
#line 356 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str77), 63},
#line 241 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str78), 63},
#line 8 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str79), 30},
#line 169 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str80), 0},
      {-1,-1},
#line 177 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str82), 62},
#line 348 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str83), 54},
#line 174 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str84), 59},
#line 113 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str85), 44},
#line 178 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str86), 63},
      {-1,-1}, {-1,-1},
#line 128 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str89), 0},
      {-1,-1},
#line 140 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str91), 62},
#line 171 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str92), 54},
#line 137 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str93), 59},
#line 370 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str94), 51},
#line 141 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str95), 63},
      {-1,-1},
#line 353 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str97), 60},
#line 372 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str98), 53},
#line 342 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str99), 58},
#line 242 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str100), 51},
#line 132 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str101), 54},
      {-1,-1}, {-1,-1}, {-1,-1},
#line 180 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str105), 0},
#line 175 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str106), 60},
#line 203 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str107), 62},
#line 25 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str108), 15},
#line 197 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str109), 59},
      {-1,-1},
#line 205 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str111), 63},
#line 230 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str112), 56},
      {-1,-1},
#line 235 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str114), 52},
#line 138 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str115), 60},
#line 374 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str116), 92},
#line 186 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str117), 54},
      {-1,-1}, {-1,-1},
#line 23 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str120), 13},
#line 187 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str121), 54},
#line 163 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str122), 14},
#line 133 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str123), 55},
      {-1,-1},
#line 220 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str125), 64},
#line 293 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str126), 92},
      {-1,-1}, {-1,-1},
#line 351 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str129), 58},
      {-1,-1},
#line 199 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str131), 60},
#line 240 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str132), 58},
#line 231 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str133), 57},
#line 155 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str134), 66},
#line 159 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str135), 53},
#line 164 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str136), 92},
#line 43 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str137), 0},
#line 173 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str138), 58},
#line 188 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str139), 55},
      {-1,-1}, {-1,-1},
#line 170 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str142), 51},
#line 156 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str143), 17},
      {-1,-1}, {-1,-1},
#line 271 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str146), 90},
#line 136 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str147), 58},
#line 185 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str148), 53},
      {-1,-1}, {-1,-1},
#line 129 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str151), 51},
      {-1,-1},
#line 144 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str153), 56},
      {-1,-1},
#line 131 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str155), 53},
      {-1,-1},
#line 152 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str157), 63},
#line 189 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str158), 55},
#line 145 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str159), 57},
      {-1,-1},
#line 153 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str161), 65},
#line 272 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str162), 90},
#line 195 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str163), 58},
#line 337 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str164), 14},
#line 157 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str165), 67},
      {-1,-1},
#line 181 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str167), 51},
#line 50 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str168), 36},
      {-1,-1},
#line 46 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str170), 32},
#line 184 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str171), 53},
      {-1,-1},
#line 15 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str173), 92},
      {-1,-1}, {-1,-1},
#line 150 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str176), 64},
#line 9 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str177), 31},
      {-1,-1}, {-1,-1},
#line 146 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str180), 58},
      {-1,-1}, {-1,-1},
#line 105 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str183), 15},
      {-1,-1}, {-1,-1}, {-1,-1},
#line 44 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str187), 30},
      {-1,-1},
#line 51 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str189), 37},
      {-1,-1}, {-1,-1},
#line 362 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str192), 67},
#line 340 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str193), 56},
      {-1,-1},
#line 279 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str195), 67},
      {-1,-1},
#line 345 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str197), 61},
#line 208 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str198), 68},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 302 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str203), 1},
#line 278 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str204), 67},
      {-1,-1}, {-1,-1},
#line 143 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str207), 0},
      {-1,-1}, {-1,-1},
#line 158 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str210), 52},
      {-1,-1},
#line 300 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str212), 1},
#line 151 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str213), 59},
      {-1,-1},
#line 42 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str215), 1},
#line 31 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str216), 20},
      {-1,-1},
#line 38 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str218), 27},
      {-1,-1},
#line 35 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str220), 24},
#line 182 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str221), 51},
#line 166 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str222), 15},
#line 349 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str223), 56},
      {-1,-1},
#line 149 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str225), 62},
#line 238 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str226), 56},
#line 354 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str227), 61},
#line 147 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str228), 61},
#line 62 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str229), 66},
#line 243 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str230), 52},
#line 118 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str231), 0},
#line 172 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str232), 56},
#line 381 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str233), 85},
#line 371 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str234), 52},
#line 341 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str235), 57},
#line 176 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str236), 61},
#line 223 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str237), 73},
#line 168 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str238), 92},
#line 280 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str239), 67},
#line 19 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str240), 11},
#line 134 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str241), 56},
#line 36 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str242), 25},
#line 92 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str243), 61},
#line 379 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str244), 80},
#line 139 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str245), 61},
      {-1,-1},
#line 70 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str247), 0},
#line 39 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str248), 28},
#line 17 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str249), 11},
#line 367 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str250), 36},
#line 74 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str251), 63},
#line 332 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str252), 32},
#line 364 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str253), 0},
      {-1,-1},
#line 380 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str255), 84},
#line 49 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str256), 35},
#line 191 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str257), 56},
      {-1,-1}, {-1,-1},
#line 102 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str260), 41},
#line 201 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str261), 61},
#line 274 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str262), 91},
      {-1,-1},
#line 121 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str264), 32},
#line 350 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str265), 57},
      {-1,-1},
#line 47 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str267), 33},
#line 239 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str268), 57},
#line 330 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str269), 30},
#line 30 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str270), 19},
#line 368 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str271), 37},
      {-1,-1},
#line 75 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str273), 51},
#line 34 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str274), 23},
#line 165 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str275), 68},
#line 162 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str276), 15},
      {-1,-1},
#line 259 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str278), 21},
#line 52 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str279), 92},
      {-1,-1},
#line 119 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str281), 30},
      {-1,-1},
#line 135 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str283), 57},
#line 154 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str284), 51},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 130 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str291), 52},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 45 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str296), 31},
      {-1,-1},
#line 321 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str298), 20},
#line 193 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str299), 57},
#line 328 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str300), 27},
#line 97 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str301), 16},
#line 325 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str302), 24},
      {-1,-1},
#line 161 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str304), 55},
#line 73 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str305), 58},
      {-1,-1},
#line 183 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str307), 52},
#line 294 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str308), 92},
#line 85 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str309), 90},
#line 93 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str310), 16},
      {-1,-1}, {-1,-1},
#line 61 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str313), 64},
#line 288 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str314), 36},
#line 270 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str315), 89},
      {-1,-1},
#line 221 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str317), 71},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 326 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str324), 25},
      {-1,-1}, {-1,-1},
#line 108 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str327), 43},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1},
#line 167 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str336), 64},
      {-1,-1},
#line 366 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str338), 35},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1},
#line 53 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str346), 11},
      {-1,-1}, {-1,-1},
#line 333 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str349), 33},
#line 207 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str350), 69},
      {-1,-1},
#line 320 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str352), 19},
      {-1,-1}, {-1,-1},
#line 26 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str355), 11},
#line 324 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str356), 23},
#line 301 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str357), 93},
#line 148 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str358), 60},
      {-1,-1},
#line 303 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str360), 94},
      {-1,-1}, {-1,-1},
#line 91 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str363), 61},
      {-1,-1}, {-1,-1},
#line 299 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str366), 93},
#line 289 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str367), 3},
#line 32 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str368), 21},
#line 377 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str369), 75},
#line 287 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str370), 6},
      {-1,-1},
#line 37 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str372), 26},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 331 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str378), 31},
#line 24 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str379), 14},
#line 245 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str380), 75},
#line 107 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str381), 42},
#line 48 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str382), 34},
      {-1,-1},
#line 14 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str384), 10},
#line 76 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str385), 60},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 120 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str390), 31},
#line 16 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str391), 60},
#line 226 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str392), 65},
      {-1,-1}, {-1,-1},
#line 225 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str395), 65},
#line 40 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str396), 92},
      {-1,-1},
#line 103 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str398), 16},
#line 71 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str399), 56},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 210 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str404), 64},
      {-1,-1},
#line 112 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str406), 89},
#line 104 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str407), 35},
#line 110 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str408), 61},
#line 267 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str409), 87},
#line 33 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str410), 22},
      {-1,-1},
#line 41 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str412), 29},
      {-1,-1},
#line 80 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str414), 71},
#line 98 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str415), 38},
      {-1,-1},
#line 285 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str417), 2},
#line 89 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str418), 59},
#line 90 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str419), 60},
#line 334 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str420), 38},
#line 296 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str421), 4},
#line 261 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str422), 20},
#line 160 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str423), 54},
#line 94 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str424), 38},
#line 378 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str425), 79},
      {-1,-1},
#line 106 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str427), 14},
      {-1,-1},
#line 29 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str429), 18},
      {-1,-1}, {-1,-1}, {-1,-1},
#line 28 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str433), 17},
#line 77 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str434), 59},
#line 256 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str435), 84},
      {-1,-1}, {-1,-1}, {-1,-1},
#line 122 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str439), 45},
#line 228 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str440), 65},
#line 72 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str441), 57},
#line 204 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str442), 62},
#line 198 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str443), 59},
      {-1,-1},
#line 123 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str445), 46},
#line 206 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str446), 63},
      {-1,-1},
#line 262 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str448), 19},
#line 224 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str449), 74},
#line 322 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str450), 21},
#line 290 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str451), 93},
#line 142 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str452), 53},
      {-1,-1},
#line 327 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str454), 26},
#line 78 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str455), 61},
#line 125 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str456), 48},
#line 86 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str457), 3},
#line 307 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str458), 20},
#line 314 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str459), 27},
#line 311 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str460), 24},
      {-1,-1},
#line 79 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str462), 62},
      {-1,-1},
#line 365 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str464), 34},
#line 276 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str465), 91},
#line 88 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str466), 94},
      {-1,-1},
#line 67 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str468), 15},
      {-1,-1},
#line 196 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str470), 58},
#line 312 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str471), 25},
#line 292 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str472), 2},
#line 281 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str473), 67},
#line 99 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str474), 39},
#line 316 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str475), 70},
#line 253 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str476), 75},
#line 317 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str477), 68},
      {-1,-1},
#line 335 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str479), 39},
      {-1,-1}, {-1,-1}, {-1,-1},
#line 95 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str483), 39},
#line 211 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str484), 68},
#line 306 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str485), 19},
      {-1,-1},
#line 310 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str487), 23},
      {-1,-1}, {-1,-1},
#line 116 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str490), 92},
#line 266 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str491), 90},
#line 323 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str492), 22},
#line 13 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str493), 60},
#line 329 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str494), 29},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 100 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str499), 40},
      {-1,-1},
#line 222 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str501), 72},
      {-1,-1}, {-1,-1},
#line 336 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str504), 40},
      {-1,-1},
#line 117 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str506), 29},
#line 304 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str507), 94},
#line 96 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str508), 40},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 27 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str514), 16},
#line 227 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str515), 65},
#line 315 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str516), 33},
#line 219 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str517), 28},
#line 190 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str518), 0},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1},
#line 63 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str527), 45},
      {-1,-1},
#line 286 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str529), 7},
      {-1,-1},
#line 200 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str531), 60},
#line 126 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str532), 49},
#line 275 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str533), 91},
#line 308 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str534), 21},
      {-1,-1},
#line 313 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str536), 26},
#line 179 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str537), 92},
#line 194 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str538), 57},
#line 87 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str539), 93},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1},
#line 60 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str549), 32},
      {-1,-1}, {-1,-1}, {-1,-1},
#line 124 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str553), 47},
      {-1,-1},
#line 309 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str555), 22},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 212 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str561), 70},
      {-1,-1},
#line 213 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str563), 68},
      {-1,-1},
#line 101 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str565), 38},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 81 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str572), 65},
      {-1,-1}, {-1,-1}, {-1,-1},
#line 284 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str576), 3},
#line 109 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str577), 61},
#line 252 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str578), 82},
#line 209 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str579), 70},
#line 295 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str580), 5},
#line 298 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str581), 8},
#line 248 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str582), 78},
      {-1,-1}, {-1,-1}, {-1,-1},
#line 291 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str586), 3},
      {-1,-1},
#line 283 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str588), 67},
#line 68 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str589), 14},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 192 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str594), 56},
      {-1,-1},
#line 202 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str596), 61},
#line 254 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str597), 75},
      {-1,-1}, {-1,-1},
#line 318 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str600), 66},
      {-1,-1},
#line 59 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str602), 89},
      {-1,-1},
#line 69 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str604), 68},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 215 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str609), 68},
      {-1,-1}, {-1,-1},
#line 127 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str612), 50},
#line 21 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str613), 12},
      {-1,-1},
#line 282 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str615), 67},
      {-1,-1}, {-1,-1}, {-1,-1},
#line 65 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str619), 49},
      {-1,-1}, {-1,-1},
#line 20 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str622), 12},
      {-1,-1}, {-1,-1},
#line 82 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str625), 75},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 58 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str631), 70},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1},
#line 64 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str641), 47},
      {-1,-1},
#line 264 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str643), 24},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1},
#line 217 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str657), 66},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 84 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str675), 31},
      {-1,-1},
#line 114 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str677), 44},
      {-1,-1},
#line 214 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str679), 68},
      {-1,-1},
#line 268 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str681), 88},
      {-1,-1},
#line 57 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str683), 16},
      {-1,-1}, {-1,-1},
#line 216 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str686), 66},
      {-1,-1}, {-1,-1}, {-1,-1},
#line 54 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str690), 39},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 22 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str695), 11},
      {-1,-1},
#line 255 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str697), 83},
      {-1,-1}, {-1,-1},
#line 66 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str700), 50},
      {-1,-1},
#line 111 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str702), 62},
      {-1,-1},
#line 18 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str704), 11},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 56 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str715), 40},
      {-1,-1},
#line 11 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str717), 92},
      {-1,-1}, {-1,-1}, {-1,-1},
#line 260 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str721), 25},
      {-1,-1},
#line 249 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str723), 79},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 12 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str729), 92},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 83 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str735), 30},
      {-1,-1},
#line 115 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str737), 18},
      {-1,-1}, {-1,-1},
#line 297 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str740), 9},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1},
#line 265 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str749), 23},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1},
#line 218 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str764), 66},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 247 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str793), 77},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1},
#line 257 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str815), 85},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 263 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str822), 22},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 246 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str827), 76},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 305 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str868), 26},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1},
#line 258 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str878), 86},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 277 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str897), 91},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1},
#line 250 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str911), 80},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1},
#line 55 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str1057), 38},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
      {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1},
#line 251 "charset_lookup.gperf"
      {(int)offsetof(struct stringpool_t, stringpool_str1069), 81}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register unsigned int key = hash_charset (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register int o = wordlist[key].alias_pos;
          if (o >= 0)
            {
              register const char *s = o + stringpool;

              if (*str == *s && !strcmp (str + 1, s + 1))
                return &wordlist[key];
            }
        }
    }
  return 0;
}
#line 382 "charset_lookup.gperf"

