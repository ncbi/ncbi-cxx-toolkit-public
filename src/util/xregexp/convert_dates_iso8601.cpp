/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Alex Kotliarov
 *
 * File Description:
 *
 * File Description:
 *    Convert dates from an arbitrary format to corresponding ISO 8601.
 *    Match a date string against set of regular expressions 
 *    to determine whether the string contains date that can be transformed 
 *    into ISO date.
 *
 * Copied and adapted from gpipe/common/read_date_iso8601.[ch]pp
 *
 */

#include <ncbi_pch.hpp>
#include <sstream>
#include <memory>
#include <map>
#include <util/xregexp/regexp.hpp>
#include <util/xregexp/convert_dates_iso8601.hpp>

BEGIN_NCBI_SCOPE


class TParse_rule;

typedef string (* TFun_transform)(string const&);
typedef pair<string, string> (* TFun_transform_other)(string const&);

static pair<string, string> extract_date_iso8601(string const& value, 
                                                 vector<TParse_rule> const& rules,
                                                 vector<TFun_transform_other> const& range_rules,
                                                 TFun_transform_other ambig_rule);

static vector<TParse_rule> const& get_date_rule_collection();
static vector<TFun_transform_other> const& get_date_range_rule_collection();
static vector<TParse_rule> make_parse_rules();
static vector<TFun_transform_other> make_parse_range_rules();

static string const& GetMonth_code_by_name(string const& month_name);
static map<string, string> s_GetMonthLookup_table();
static TFun_transform_other get_transform_for_ambiguous_date();

// Collection of functions that transform to ISO 8601
static string transform_identity(string const& value);
static string transform_missing(string const& value);
static string transform_YYYY_mm_DD(string const& value);
static string transform_mm_DD_YYYY(string const& value);
static string transform_DD_mm_YYYY(string const& value);
static string transform_DD_month_YYYY(string const& value);
static string transform_DD_month_comma_YYYY(string const& value);
static string transform_month_DD_YYYY(string const& value);
static string transform_month_YYYY(string const& value);
static string transform_YYYY_month(string const& value);
static string transform_MM_YYYY(string const& value);
static string transform_YYYY_MM(string const& value);
static string transform_range_decade(string const& value);
static string transform_range_before(string const& value);

static pair<string, string> transform_ambiguous_date(string const& value);
static pair<string, string> transform_range(string const& value);

static const char* kTransform_code_iso8601 = "ISO-8601";
static const char* transfrom_code_range_iso8601 = "RANGE|ISO-8601";
static const char* kTransform_code_cast_na = "CAST|NA";
static const char* kTransform_code_cast_iso8601 = "CAST|ISO-8601";
static const char* kTransform_code_range_cast_iso8601 = "RANGE|CAST|ISO-8601";
static const char* kTransform_code_no_date = "NODATE";
static const char* kTransform_code_cast_ambig = "CAST|YYYY"; // cast ambig date to YYYY


//////////////////////////////////////////////////////////////////////////////
//
//  API
//      

string ConvertDateTo_iso8601(string const& value)
{
    pair<string, string> result = 
        extract_date_iso8601(value,
                             get_date_rule_collection(),
                             get_date_range_rule_collection(),
                             get_transform_for_ambiguous_date());
    return result.second;
}

pair<string, string> ConvertDateTo_iso8601_and_annotate(string const& value)
{
    return extract_date_iso8601(value,
                                get_date_rule_collection(),
                                get_date_range_rule_collection(),
                                get_transform_for_ambiguous_date());
}


//////////////////////////////////////////////////////////////////////////////
//
//  Implementation
//

class TParse_rule 
{
public:
    TParse_rule(string const& tag,
                string const& regex,
                TFun_transform transform)
    : m_Tag(tag),
      m_Transform(transform),
      m_Regexp_s(regex),
      m_Regexp( new CRegexp(regex) )
    {        
    }

    TParse_rule(TParse_rule const & rhs)
    : m_Tag(rhs.m_Tag), 
      m_Transform(rhs.m_Transform),
      m_Regexp_s(rhs.m_Regexp_s),
      m_Regexp( new CRegexp(rhs.m_Regexp_s) )
    {
    }

    TParse_rule& operator= (TParse_rule const& other)
    {
        TParse_rule temp(other);
        Swap(*this, temp);
        return *this;
    }

    string const& GetTag() const { return m_Tag; }
    string MakeTransform(string const& value) const { return m_Transform(value); }
    CRegexp& GetRegexp() const { return *m_Regexp; }

private:
    TParse_rule();
    
    void Swap(TParse_rule& lhs, TParse_rule& rhs)
    {
        using std::swap;
        swap(lhs.m_Tag, rhs.m_Tag);
        swap(lhs.m_Transform, rhs.m_Transform);
        swap(lhs.m_Regexp_s, rhs.m_Regexp_s);

        shared_ptr<CRegexp> temp(lhs.m_Regexp);
        lhs.m_Regexp = rhs.m_Regexp;
        rhs.m_Regexp = temp;
    }

    string m_Tag;
    TFun_transform m_Transform;    
    string m_Regexp_s;
    shared_ptr<CRegexp> m_Regexp;
};

class CAmbiguousDateException : public CException
{
public:
    enum EErrCode {
        eAmbigDate
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch( GetErrCode() ) {
            case eAmbigDate:
                return "eAmbiguousDate";
            default:
                return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CAmbiguousDateException, CException);
};

pair<string, string> extract_date_iso8601(string const& value, 
                                        vector<TParse_rule> const & rules,
                                        vector<TFun_transform_other> const& range_rules,
                                        TFun_transform_other transform_ambiguous_date_fun
                                   )
{
    try {
        for ( vector<TParse_rule>::const_iterator rule = rules.begin();
              rule != rules.end(); 
              ++rule ) 
        {
            CRegexp& re = rule->GetRegexp();
            if ( re.IsMatch(value) ) {
                re.GetMatch(value, 0, 0, CRegexp::fMatch_default, true);
                string match = re.GetSub(value, 1);
                return make_pair(rule->GetTag(), rule->MakeTransform(match));            
            }
        }

        // Try to match 'range' expressions
        for ( vector<TFun_transform_other>::const_iterator transform = range_rules.begin();
              transform != range_rules.end();
              ++transform ) 
        {
            pair<string, string> result = (* transform)(value);
            if ( !result.second.empty() ) {
                return result;
            }
        }
    }
    catch ( CAmbiguousDateException& ) {
        // Try to salvage a year
        return transform_ambiguous_date_fun(value);
    }
    
    // Unable to extract ISO date; record a miss.
    return make_pair(kTransform_code_no_date, "");
}

vector<TParse_rule> const& get_date_rule_collection()
{
    static vector<TParse_rule> parse_rules = make_parse_rules();
    return parse_rules;
}

vector<TParse_rule> make_parse_rules()
{
    struct TRules {
        char const* annot_tag;
        char const* regexp;
        TFun_transform transform;
    }
    rules_table[] = 
    {
        {kTransform_code_iso8601, 
            "^((?:19\\d{2}|2\\d{3}))$", 
            transform_identity}, // YYYY

        {kTransform_code_cast_na, 
            "(?i)^([a-z]+(?:\\s[a-z]+)*)$", 
            transform_missing}, // not determined | unknown | .. 

        {kTransform_code_cast_na, 
            "(?i)^((?:na|n[.]a[.]|n/a))$", 
            transform_missing},

        {kTransform_code_iso8601, 
            "^([123]\\d{3}\\-(?:[0][1-9]|[1][012])\\-(?:[0][1-9]|[12][0-9]|[3][01]))(?:[T ](?:[01][0-9]|2[0123])(?:[:][0-5][0-9]){1,2})?$", 
            transform_identity },

        {kTransform_code_iso8601, 
            "^([123]\\d{3}\\-(?:[0][1-9]|[1][012]))$",
            transform_identity }, 

        {kTransform_code_cast_iso8601, 
            "^([123]\\d{3}/(?:0?[1-9]|[1][012])/(?:0?[1-9]|[12][0-9]|[3][01]))$", 
            transform_YYYY_mm_DD},

        {kTransform_code_cast_iso8601, 
            "^([123]\\d{3}\\-(?:0?[1-9]|[1][012])\\-(?:0?[1-9]|[12][0-9]|[3][01]))$", 
            transform_YYYY_mm_DD},

        // unambiguous dates: day >= 13
        {kTransform_code_cast_iso8601, 
            "(?i)^((?:[1][3-9]|[2][0-9]|[3][012])([-./])(?:0?[1-9]|[1][012])\\2(?:[123]\\d{3}|\\d{2}))(?: (?:0[1-9]|1[012])(?:[:][0-5][0-9]){1,2}(?:[ ]?[AP]M|[ ]?[AP][.]M[.]))?$",  
            transform_DD_mm_YYYY},   // DD-mm-YYYY

        // date would be ambiguous iff day != month and day <= 12
        {kTransform_code_cast_iso8601, 
            "(?i)^((?:0?[1-9]|[1][012])([-/.])(?:0?[1-9]|[12][0-9]|[3][01])\\2(?:[123]\\d{3}|\\d{2}))(?: (?:0[1-9]|1[012])(?:[:][0-5][0-9]){1,2}(?:[ ]?[AP]M|[ ]?[AP][.]M[.]))?$", 
            transform_mm_DD_YYYY},     // mm/DD/YYYY

        {kTransform_code_cast_iso8601, 
            "(?i)^((?:Jan(uary)?|Feb(ruary)?|Mar(ch)?|Apr(il)?|May|June?|July?|Aug(ust)?|Sep(tember)?|Oct(ober)?|Nov(ember)?|Dec(ember)?)\\s(?:0?[1-9]|[12][0-9]|[3][01]),?[ ](?:[123]\\d{3}|\\d{2}))$", 
            transform_month_DD_YYYY}, // DD-Month-YYYY

        {kTransform_code_cast_iso8601, 
            "(?i)^((?:0?[1-9]|[12][0-9]|[3][01])([- ])(?:Jan(uary)?|Feb(ruary)?|Mar(ch)?|Apr(il)?|May|June?|July?|Aug(ust)?|Sep(tember)?|Oct(ober)?|Nov(ember)?|Dec(ember)?)\\2(?:[123]\\d{3}|\\d{2}))$", 
            transform_DD_month_YYYY}, // DD-Month-YYYY

        {kTransform_code_cast_iso8601, 
            "(?i)^((?:0?[1-9]|[12][0-9]|[3][01])[ ](?:Jan(uary)?|Feb(ruary)?|Mar(ch)?|Apr(il)?|May|June?|July?|Aug(ust)?|Sep(tember)?|Oct(ober)?|Nov(ember)?|Dec(ember)?),[ ](?:[123]\\d{3}|\\d{2}))$", 
            transform_DD_month_comma_YYYY}, // DD-Month-YYYY

        {kTransform_code_cast_iso8601, 
            "(?i)^((?:Jan(uary)?|Feb(ruary)?|Mar(ch)?|Apr(il)?|May|June?|July?|Aug(ust)?|Sep(tember)?|Oct(ober)?|Nov(ember)?|Dec(ember)?)[-./ ](?:[123]\\d{3}|\\d{2}))$", 
            transform_month_YYYY}, // Month-YY(YY)?

        {kTransform_code_cast_iso8601, 
            "(?i)^((?:[12]\\d{3}|\\d{2})[-./ ](?:Jan(uary)?|Feb(ruary)?|Mar(ch)?|Apr(il)?|May|June?|July?|Aug(ust)?|Sep(tember)?|Oct(ober)?|Nov(ember)?|Dec(ember)?))$", 
            transform_YYYY_month}, // YY(YY)?-Month

        {kTransform_code_cast_iso8601,
            "^((?:19\\d{2}|2\\d{3})[-/. ](?:0?[1-9]|1[012]))$",
            transform_YYYY_MM
        },

        {kTransform_code_cast_iso8601,
            "^((?:0?[1-9]|1[012])[-/. ](?:19\\d{2}|2\\d{3}))$",
            transform_MM_YYYY
        },

        {transfrom_code_range_iso8601, // YYYY-MM-DD/YYYY-MM-DD
            "^((?:19\\d{2}|2\\d{3})\\-(?:0[1-9]|1[012])\\-(?:[0][1-9]|[12][0-9]|[3][01])\\/(?:19\\d{2}|2\\d{3})\\-(?:0[1-9]|1[012])\\-(?:[0][1-9]|[12][0-9]|[3][01]))$", 
            transform_identity},

        {transfrom_code_range_iso8601, // YYYY-MM/YYYY-MM
            "^((?:19\\d{2}|2\\d{3})\\-(?:0[1-9]|1[012])\\/(?:19\\d{2}|2\\d{3})\\-(?:0[1-9]|1[012]))$", 
            transform_identity},

        {transfrom_code_range_iso8601, // YYYY/YYYY
            "^((?:19\\d{2}|2\\d{3})\\/(?:19\\d{2}|2\\d{3}))$", 
            transform_identity},

        {kTransform_code_range_cast_iso8601, 
            "^((?:19[0-9]0|2\\d{2}0))s$", 
            transform_range_decade},

        {kTransform_code_range_cast_iso8601, 
            "^.*?(?<=before[ ])((?:19\\d{2}|2\\d{3}))$", 
            transform_range_before},

        {kTransform_code_range_cast_iso8601, 
            "^.*?(?<=pre[-])((?:19\\d{2}|2\\d{3}))$", 
            transform_range_before},

        {0, 0, 0}
    };

    vector<TParse_rule> rules_coll;
    for (struct TRules* entry = &rules_table[0]; entry->annot_tag != 0; ++entry ) {
        rules_coll.push_back( TParse_rule(entry->annot_tag, entry->regexp, entry->transform) );
    }

    return rules_coll;
}

vector<TFun_transform_other> const& get_date_range_rule_collection()
{
    static vector<TFun_transform_other> range_rules = make_parse_range_rules();

    return range_rules;
}

vector<TFun_transform_other> make_parse_range_rules()
{
    TFun_transform_other table[] = {
        transform_range,
        0
    };

    vector<TFun_transform_other> rules;
    for ( TFun_transform_other *entry = &table[0]; *entry != 0; ++entry ) {
        rules.push_back(*entry);
    }

    return rules;
}

TFun_transform_other get_transform_for_ambiguous_date()
{
    return transform_ambiguous_date;
}

pair<string, string> transform_ambiguous_date(string const& value)
{
    // Ambiguous date come in following formats:
    // MM/DD/YYYY and DD/MM/YYYY
    // 
    // A date is ambiguous if DD < 13 and DD != MM
    // In this case we extract just YYYY 
    static CRegexp re("^(?:0?[1-9]|1[012])([-.\\/])(?:0?[1-9]|[12][0-9]|3[01])\\1((?:19\\d{2}|2\\d{3}|\\d{2}))$");

    if ( re.IsMatch(value) ) {
        string match= re.GetSub(value, 2);
        int year = NStr::StringToNumeric<int>(match);
        if ( year < 100 ) {
            year = 1900 + ((year > 70) ? year : year + 100);
        }
        return make_pair(string(kTransform_code_cast_ambig), NStr::NumericToString<int>(year));
    }
    else {
        return make_pair(kTransform_code_no_date, "");
    }
}

pair<string, string> transform_range(string const& value)
{
    CRegexp re("(?i)(?:between(.+?)and(.+?)|^(.+?)\\/(.+?))$");

    if ( re.IsMatch(value) ) {
        string lhs; 
        string rhs;
    
        if ( !re.GetSub(value, 1).empty() ) {
            lhs = re.GetSub(value, 1);
            rhs = re.GetSub(value, 2);
        }
        else {
            lhs = re.GetSub(value, 3);
            rhs = re.GetSub(value, 4);
        }
    
        lhs = NStr::TruncateSpaces(lhs);
        rhs = NStr::TruncateSpaces(rhs);
    
        vector<TParse_rule> const & rules = get_date_rule_collection();

        for ( vector<TParse_rule>::const_iterator rule = rules.begin(); rule != rules.end(); ++rule ) {
            // skip rules that extract ranges
            if ( rule->GetTag().find("RANGE") == 0 ) {
                continue;
            }
            
            CRegexp& re_rule = rule->GetRegexp();
            if ( re_rule.IsMatch(lhs) ) {
                re_rule.GetMatch(lhs, 0, 0, CRegexp::fMatch_default, true);
                string match_lhs = re_rule.GetSub(lhs, 1);
                if ( re_rule.IsMatch(rhs) ) {
                    string match_rhs = re_rule.GetSub(rhs, 1);
                    string result_lhs = rule->MakeTransform(match_lhs);
                    string result_rhs = rule->MakeTransform(match_rhs);

                    string prefix = "RANGE|";
                    if ( rule->GetTag().find("CAST") == string::npos ) {
                        prefix += "CAST|";
                    }
                    string range = result_lhs + "/" + result_rhs;
                    return make_pair(prefix + rule->GetTag(), range);

                }
            }
        }
    }
    return make_pair(kTransform_code_no_date, "");
}

string transform_identity(string const& value) {
    return value;
}

string transform_missing(string const& /*value*/) {
    return "missing";
}

string transform_YYYY_mm_DD(string const& value) {
    vector<string> tokens;
    NStr::Split(value, "-/", tokens);
    ostringstream oss;
    oss << tokens[0] 
        << "-"
        << setfill('0') << setw(2) 
        << NStr::StringToNumeric<int>(tokens[1]) 
        << "-"
        << setw(2)
        << NStr::StringToNumeric<int>(tokens[2]); 
 
    return oss.str();
}

string transform_mm_DD_YYYY(string const& value) {
    vector<string> tokens;
    NStr::Split(value, "-/.", tokens);

    int month = NStr::StringToNumeric<int>(tokens[0]);
    int day = NStr::StringToNumeric<int>(tokens[1]);
    int year = NStr::StringToNumeric<int>(tokens[2]);

    if ( day < 13 && day != month ) {
        NCBI_THROW(CAmbiguousDateException, eAmbigDate, "Date is ambiguous");
    }

    if ( year < 100 ) {
        year = 1900 + ( ( year > 70 ) ? year : 100 + year );
    }

    ostringstream oss;
    oss << year
        << "-"
        << setfill('0') << setw(2) 
        << month 
        << "-"
        << setw(2)
        << day;

    return oss.str();
}


string transform_DD_mm_YYYY(string const& value) {
    vector<string> tokens;
    NStr::Split(value, "-/.", tokens);

    int day = NStr::StringToNumeric<int>(tokens[0]);
    int month = NStr::StringToNumeric<int>(tokens[1]);
    int year = NStr::StringToNumeric<int>(tokens[2]);

    if ( day < 13 && day != month ) { 
        NCBI_THROW(CAmbiguousDateException, eAmbigDate, "Date is ambiguous");
    }

    if ( year < 100 ) {
        year = 1900 + ( ( year > 70 ) ? year : 100 + year );
    }

    ostringstream oss;
    oss << year
        << "-"
        << setfill('0') << setw(2) 
        << month 
        << "-"
        << setw(2)
        << day;

    return oss.str();
}

string transform_DD_month_YYYY(string const& value) {
    vector<string> tokens;
    NStr::Split(value, "- ", tokens);

    int day = NStr::StringToNumeric<int>(tokens[0]);
    int year = NStr::StringToNumeric<int>(tokens[2]);
    if ( year < 100 ) { 
        year = 1900 + ( ( year > 70 ) ? year : 100 + year );
    }

    ostringstream oss;
    oss << year
        << "-"
        << GetMonth_code_by_name(tokens[1])
        << "-"
        << setfill('0') << setw(2)
        << day;
    return oss.str();
}

string transform_DD_month_comma_YYYY(string const& value) {
    vector<string> tokens;
    NStr::Split(value, " ", tokens);

    string month = tokens[1];
    size_t pos = month.find_last_of(",");
    month.erase(pos);

    int day = NStr::StringToNumeric<int>(tokens[0]);
    int year = NStr::StringToNumeric<int>(tokens[2]);
    if ( year < 100 ) {
        year = 1900 + ( ( year > 70 ) ? year : 100 + year );
    }

    ostringstream oss;
    oss << year
        << "-"
        << GetMonth_code_by_name(month)
        << "-"
        << setfill('0') << setw(2)
        << day;
    return oss.str();
}

string transform_month_DD_YYYY(string const& value) {

    vector<string> tokens;
    NStr::Split(value, " ", tokens);
    
    // handle April 21, 1989 case
    {{
        string& day = tokens[1];
        size_t pos = day.find_last_of(",");
        if ( pos != std::string::npos ) {
            day.erase(pos);
        }
    }}
    int day = NStr::StringToNumeric<int>(tokens[1]);
    int year = NStr::StringToNumeric<int>(tokens[2]);
    if ( year < 100 ) {
        year = 1900 + ( ( year > 70 ) ? year : 100 + year );
    }

    ostringstream oss;
    oss << year
        << "-"
        << GetMonth_code_by_name(tokens[0])
        << "-"
        << setfill('0') << setw(2)
        << day;
    return oss.str();
}

string transform_month_YYYY(string const& value) {
    vector<string> tokens;
    NStr::Split(value, "-/. ", tokens);

    int year = NStr::StringToNumeric<int>(tokens[1]);
    if ( year < 100 ) {
        year = 1900 + ( ( year > 70 ) ? year : 100 + year );
    }

    ostringstream oss;
    oss << year
        << "-"
        << GetMonth_code_by_name(tokens[0]);
    return oss.str();
}

string transform_YYYY_month(string const& value) {
    vector<string> tokens;
    NStr::Split(value, "/-. ", tokens);

    int year = NStr::StringToNumeric<int>(tokens[0]);
    if ( year < 100 ) {
        year = 1900 + ( ( year > 70 ) ? year : 100 + year );
    }

    ostringstream oss;
    oss << year
        << "-"
        << GetMonth_code_by_name(tokens[1]);
    return oss.str();
}

string transform_YYYY_MM(string const& value) {

    vector<string> tokens;
    NStr::Split(value, "/-. ", tokens);
    
    int month = NStr::StringToNumeric<int>(tokens[1]);

    ostringstream oss;
    oss << tokens[0]
        << "-"
        << setfill('0') << setw(2)
        << month;
    return oss.str();
}

string transform_MM_YYYY(string const& value) {

    vector<string> tokens;
    NStr::Split(value, "/-. ", tokens);
    
    int month = NStr::StringToNumeric<int>(tokens[0]);

    ostringstream oss;
    oss << tokens[1]
        << "-"
        << setfill('0') << setw(2)
        << month;
    return oss.str();
}

string transform_range_decade(string const& value) {
    int year = NStr::StringToNumeric<int>(value);
    ostringstream oss;
    oss << year
        << "/"
        << year + 9;
    return oss.str();
}

string transform_range_before(string const& value) {
    int year = NStr::StringToNumeric<int>(value);
    year -= 1;

    ostringstream oss;
    oss << 1900
        << "/"
        << year;
    return oss.str();
}


string const& GetMonth_code_by_name(string const& month_name)
{
    static map<string, string> lookup_table = s_GetMonthLookup_table();

    string name(month_name); 
    map<string, string>::const_iterator it = lookup_table.find(NStr::ToLower(name));

    if ( it == lookup_table.end() ) {
        NCBI_THROW(CException, eUnknown, "Bad month name value.");
    }

    return it->second;
}

map<string, string> s_GetMonthLookup_table()
{
    struct TName2Code {
        char const* name;
        char const* code;
    } 
    table[] = {
        {"jan", "01"},
        {"feb", "02"},
        {"mar", "03"},
        {"apr", "04"},
        {"may", "05"},
        {"jun", "06"},
        {"jul", "07"},
        {"aug", "08"},
        {"sep", "09"},
        {"oct", "10"},
        {"nov", "11"},
        {"dec", "12"},
        {"january", "01"},
        {"february", "02"},
        {"march", "03"},
        {"april", "04"},
        {"june", "06"},
        {"july", "07"},
        {"august", "08"},
        {"september", "09"},
        {"october", "10"},
        {"november", "11"},
        {"december", "12"},
        {0, 0}
    };
    
    map<string, string> lookup_table;
    for ( struct TName2Code* entry = &table[0]; entry->name; ++entry ) {
        lookup_table[entry->name] = entry->code;
    }
    return lookup_table; 
}


END_NCBI_SCOPE
