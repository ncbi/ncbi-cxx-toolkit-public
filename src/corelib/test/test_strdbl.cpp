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
 * Author:  Eugene Vasilchenko
 *
 * File Description:
 *   Test and benchmark for String <-> double conversions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiapp.hpp>
#include <math.h>
#include <map>
#include <corelib/ncbifloat.h>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

#if 0
# undef SIZEOF_LONG_DOUBLE
# define SIZEOF_LONG_DOUBLE SIZEOF_DOUBLE
# undef NCBI_CONST_LONGDOUBLE
# define NCBI_CONST_LONGDOUBLE(v) v
#endif


static const double EPS = 2.22e-16; // 2^-52


/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

    void RunSpeedBenchmark(void);
    void RunPrecisionBenchmark(void);

    void RunD2SSpeedBenchmark(void);
    void RunD2SPrecisionBenchmark(void);
    bool CompareSerialization(double data, size_t digits);

    static double PreciseStringToDouble(const CTempStringEx& s);
    double GenerateDouble(void);

    int m_VerboseLevel;
};


double StringToDoublePosixOld(const char* ptr, char** endptr)
{
// skip leading blanks
    for ( ; isspace(*ptr); ++ptr)
        ;
    const char* start = ptr;
#if SIZEOF_LONG_DOUBLE == SIZEOF_DOUBLE
    typedef double Double;
#else
    typedef long double Double;
#endif
    Double ret = NCBI_CONST_LONGDOUBLE(0.);
    bool sign= false, negate= false, dot= false, expn=false, anydigits=false;
    int digits = 0, dot_position = 0, exponent = 0;
    unsigned int first=0, second=0, first_exp=1;
    Double second_exp = NCBI_CONST_LONGDOUBLE(1.);
    Double third = NCBI_CONST_LONGDOUBLE(0.);
    char c;
// up to exponent
    for( ; ; ++ptr) {
        c = *ptr;
        // sign: should be no digits at this point
        if (c == '-' || c == '+') {
            // if there was sign or digits, stop
            if (sign || digits) {
                break;
            }
            sign = true;
            negate = c == '-';
        }
        // digits: accumulate
        else if (c >= '0' && c <= '9') {
            anydigits = true;
            ++digits;
            if (first == 0 && c == '0') {
                --digits;
                if (dot) {
                    --dot_position;
                }
            } else if (digits <= 9) {
                first = first*10 + (c-'0');
            } else if (digits <= 18) {
                first_exp *= 10;
                second = second*10 + (c-'0');
            } else {
                second_exp *= NCBI_CONST_LONGDOUBLE(10.);
                third = third * NCBI_CONST_LONGDOUBLE(10.) + (c-'0');
            }
        }
        // dot
        else if (c == '.') {
            // if second dot, stop
            if (dot) {
                break;
            }
            dot_position = digits;
            dot = true;
        }
        // if exponent, stop
        else if (c == 'e' || c == 'E') {
            if (!anydigits) { // FIXED ERROR
                break;
            }
            expn = true;
            ++ptr;
            break;
        }
        else {
            if (!digits) {
                if (!dot && NStr::strncasecmp(ptr,"nan",3)==0) {
                    if (endptr) {
                        *endptr = (char*)(ptr+3);
                    }
                    return HUGE_VAL/HUGE_VAL; /* NCBI_FAKE_WARNING */
                }
                if (NStr::strncasecmp(ptr,"inf",3)==0) {
                    if (endptr) {
                        *endptr = (char*)(ptr+3);
                    }
                    return negate ? -HUGE_VAL : HUGE_VAL;
                }
                if (NStr::strncasecmp(ptr,"infinity",8)==0) {
                    if (endptr) {
                        *endptr = (char*)(ptr+8);
                    }
                    return negate ? -HUGE_VAL : HUGE_VAL;
                }
            }
            break;
        }
    }
    // if no digits, stop now - error
    if (!anydigits) {
        if (endptr) {
            *endptr = (char*)start;
        }
        errno = EINVAL;
        return 0.;
    }
    exponent = dot ? dot_position - digits : 0;
// read exponent
    if (expn && *ptr) {
        int expvalue = 0;
        bool expsign = false, expnegate= false;
        int expdigits= 0;
        for( ; ; ++ptr) {
            c = *ptr;
            // sign: should be no digits at this point
            if (c == '-' || c == '+') {
                // if there was sign or digits, stop
                if (expsign || expdigits) {
                    break;
                }
                expsign = true;
                expnegate = c == '-';
            }
            // digits: accumulate
            else if (c >= '0' && c <= '9') {
                ++expdigits;
                int newexpvalue = expvalue*10 + (c-'0');
                if (newexpvalue > expvalue) {
                    expvalue = newexpvalue;
                }
            }
            else {
                break;
            }
        }
        // if no digits, rollback
        if (!expdigits) {
            // rollback sign
            if (expsign) {
                --ptr;
            }
            // rollback exponent
            if (expn) {
                --ptr;
            }
        }
        else {
            exponent = expnegate ? exponent - expvalue : exponent + expvalue;
        }
    }
    ret = ((Double)first * first_exp + second)* second_exp + third;
    // calculate exponent
    if ((first || second) && exponent) {
        if (exponent > 2*DBL_MAX_10_EXP) {
            ret = HUGE_VAL;
            errno = ERANGE;
        } else if (exponent < 2*DBL_MIN_10_EXP) {
            ret = 0.;
            errno = ERANGE;
        } else {
            for (; exponent < -256; exponent += 256) {
                ret /= NCBI_CONST_LONGDOUBLE(1.e256);
            }
            Double power      = NCBI_CONST_LONGDOUBLE(1.);
            Double power_mult = NCBI_CONST_LONGDOUBLE(10.);
            unsigned int mask = 1;
            unsigned int uexp = exponent < 0 ? -exponent : exponent;
            int count = 1;
            for (; count < 32 && mask <= uexp; ++count, mask <<= 1) {
                if (mask & uexp) {
                    switch (mask) {
                    case   0: break;
                    case   1: power *= NCBI_CONST_LONGDOUBLE(10.);    break;
                    case   2: power *= NCBI_CONST_LONGDOUBLE(100.);   break;
                    case   4: power *= NCBI_CONST_LONGDOUBLE(1.e4);   break;
                    case   8: power *= NCBI_CONST_LONGDOUBLE(1.e8);   break;
                    case  16: power *= NCBI_CONST_LONGDOUBLE(1.e16);  break;
                    case  32: power *= NCBI_CONST_LONGDOUBLE(1.e32);  break;
                    case  64: power *= NCBI_CONST_LONGDOUBLE(1.e64);  break;
                    case 128: power *= NCBI_CONST_LONGDOUBLE(1.e128); break;
                    case 256: power *= NCBI_CONST_LONGDOUBLE(1.e256); break;
                    default:  power *= power_mult;                    break;
                    }
                }
                if (mask >= 256) {
                    if (mask == 256) {
                        power_mult = NCBI_CONST_LONGDOUBLE(1.e256);
                    }
                    power_mult = power_mult*power_mult;
                }
            }
            if (exponent < 0) {
                ret /= power;
                if (double(ret) == 0.) {
                    errno = ERANGE;
                }
            } else {
                ret *= power;
                if (!finite(double(ret))) {
                    errno = ERANGE;
                }
            }
        }
    }
    if (negate) {
        ret = -ret;
    }
    // done
    if (endptr) {
        *endptr = (char*)ptr;
    }
    return ret;
}


void CTestApp::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostFlag(eDPF_All);
    
    auto_ptr<CArgDescriptions> d(new CArgDescriptions);
    d->SetUsageContext("test_strdbl",
                       "test string <-> double conversion");
    d->AddFlag("speed",
               "Run conversion speed benchmark");
    d->AddOptionalKey("precision", "precision",
                      "Run conversion precision benchmark",
                      CArgDescriptions::eString);
    d->SetConstraint("precision",
                     &(*new CArgAllow_Strings,
                       "Posix", "PosixOld", "strtod"));
    d->AddDefaultKey("count", "count",
                     "Number of test iterations to run",
                     CArgDescriptions::eInteger, "1000000");
    d->AddOptionalKey("test_strings", "test_strings",
                      "List of test strings to run",
                      CArgDescriptions::eString);
    d->AddDefaultKey("verbose", "verbose",
                     "Verbose level",
                     CArgDescriptions::eInteger, "0");
    d->AddDefaultKey("threshold", "threshold",
                     "Close match threshold",
                     CArgDescriptions::eDouble, "0.01");

    d->AddFlag("randomize",
               "Randomize test data (for precision and double-to-string tests)");
    d->AddOptionalKey("seed", "Randomization",
                             "Randomization seed value",
                             CArgDescriptions::eInt8);
    d->AddDefaultKey("digits", "significantDigits",
                     "The number of significant digits in double-to-string conversion",
                     CArgDescriptions::eInteger, NStr::NumericToString(DBL_DIG));
    d->SetConstraint("digits", new CArgAllow_Integers(1, DBL_DIG));

    d->AddDefaultKey("mode", "testmode",
                     "Test string-to-doube, or double-to-string",
                     CArgDescriptions::eString, "both");
    d->SetConstraint( "mode", &(*new CArgAllow_Strings,
            "str2dbl", "dbl2str", "both"));

    SetupArgDescriptions(d.release());
}


int CTestApp::Run(void)
{
    // Process command line
    const CArgs& args = GetArgs();

    m_VerboseLevel = args["verbose"].AsInteger();

    if (args["randomize"]) {
        unsigned int seed = GetArgs()["seed"]
            ? (unsigned int) GetArgs()["seed"].AsInt8()
            : (unsigned int) time(NULL);
        LOG_POST("Randomization seed value: " << seed);
        srand(seed);
    }
    if (args["mode"].AsString() == "str2dbl" || args["mode"].AsString() == "both") {

        if ( args["speed"] ) {
            RunSpeedBenchmark();
        }
    
        if ( args["precision"] ) {
            RunPrecisionBenchmark();
        }
    }
    if (args["mode"].AsString() == "dbl2str" || args["mode"].AsString() == "both") {

        if ( args["speed"] ) {
            RunD2SSpeedBenchmark();
        }
    
//        if ( args["precision"] )
        {
            RunD2SPrecisionBenchmark();
        }
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
// CDecimal helper class
/////////////////////////////////////////////////////////////////////////////

class CDecimal
{
public:
    CDecimal(int value = 0)
        {
            *this = value;
        }
    CDecimal(const CTempStringEx& value)
        {
            *this = value;
        }
    CDecimal(double value, int digits);

    ostream& Print(ostream& out, int exp_shift = 0) const;

    string ToString(int exp_shift = 0) const;

    CDecimal& operator=(int v);
    CDecimal& operator=(const CTempStringEx& s);

    CDecimal operator-(void) const;

    void Normalize(void);
    int Increment(size_t i, int c);
    int Decrement(size_t i, int c);

    double ToDouble(int exp_shift = 0) const;

    int m_Sign;
    int m_Exponent;
    string m_Mantissa; // digits after decimal point
};


ostream& CDecimal::Print(ostream& out, int exp_shift) const
{
    if ( m_Sign == 0 )
        out << '0';
    else {
        if ( m_Sign < 0 ) out << '-';
        out << '.' << m_Mantissa;
        int exp = m_Exponent+exp_shift;
        if ( exp )
            out << 'e' << exp;
    }
    return out;
}


inline ostream& operator<<(ostream& out, const CDecimal& d)
{
    return d.Print(out);
}


string CDecimal::ToString(int exp_shift) const
{
    CNcbiOstrstream out;
    Print(out, exp_shift);
    return CNcbiOstrstreamToString(out);
}


double CDecimal::ToDouble(int exp_shift) const
{
    return NStr::StringToDouble(ToString(exp_shift));
}


CDecimal::CDecimal(double value, int digits)
{
    *this = NStr::DoubleToString(value, digits, NStr::fDoubleScientific|NStr::fDoublePosix);
}


CDecimal& CDecimal::operator=(int v)
{
    if ( v == 0 ) {
        m_Sign = 0;
        m_Mantissa.erase();
        m_Exponent = 0;
    }
    else {
        if ( v < 0 ) {
            m_Sign = -1;
            v = -v;
        }
        else {
            m_Sign = 1;
        }
        m_Mantissa = NStr::UIntToString(unsigned(v));
        m_Exponent = m_Mantissa.size();
    }
    Normalize();
    return *this;
}


CDecimal& CDecimal::operator=(const CTempStringEx& s)
{
    int ptr = 0;
    char c = s[ptr++];
 // skip leading blanks
    while ( isspace((unsigned char)c) ) {
        c = s[ptr++];
    }
    
    m_Sign = 0;
    if ( c == '-' ) {
        m_Sign = -1;
        c = s[ptr++];
    }
    else if ( c == '+' ) {
        m_Sign = +1;
        c = s[ptr++];
    }

    bool dot = false, expn = false, anydigits = false;
    int dot_position = 0;
    m_Mantissa.erase();

// up to exponent
    for ( ; ; c = s[ptr++] ) {
        if (c >= '0' && c <= '9') {
            // digits: accumulate
            anydigits = true;
            if ( m_Mantissa.empty() ) {
                if ( c != '0' ) {
                    m_Mantissa += c;
                }
                else {
                    if ( dot ) {
                        --dot_position;
                    }
                }
            }
            else {
                m_Mantissa += c;
            }
        }
        else if (c == '.') {
            // dot
            // if second dot, stop
            if (dot) {
                --ptr;
                break;
            }
            dot_position = m_Mantissa.size();
            dot = true;
        }
        else if (c == 'e' || c == 'E') {
            // if exponent, stop
            if (!anydigits) {
                --ptr;
                break;
            }
            expn = true;
            break;
        }
        else if (!c) {
            --ptr;
            break;
        }
        else {
            --ptr;
            NCBI_THROW2(CStringException, eConvert,
                        "Cannot convert '"+string(s)+"'", ptr);
        }
    }
    // if no digits, stop now - error
    if (!anydigits) {
        NCBI_THROW2(CStringException, eConvert,
                    "Cannot convert '"+string(s)+"'", ptr);
    }
    int exponent = dot ? dot_position - m_Mantissa.size() : 0;
// read exponent
    if (expn && s[ptr]) {
        int expvalue = 0;
        bool expsign = false, expnegate= false;
        int expdigits= 0;
        for( ; ; ++ptr) {
            c = s[ptr];
            // sign: should be no digits at this point
            if (c == '-' || c == '+') {
                // if there was sign or digits, stop
                if (expsign || expdigits) {
                    break;
                }
                expsign = true;
                expnegate = c == '-';
            }
            // digits: accumulate
            else if (c >= '0' && c <= '9') {
                ++expdigits;
                int newexpvalue = expvalue*10 + (c-'0');
                if (newexpvalue > expvalue) {
                    expvalue = newexpvalue;
                }
            }
            else {
                break;
            }
        }
        // if no digits, rollback
        if (!expdigits) {
            // rollback sign
            if (expsign) {
                --ptr;
            }
            // rollback exponent
            if (expn) {
                --ptr;
            }
        }
        else {
            exponent = expnegate ? exponent - expvalue : exponent + expvalue;
        }
    }
    m_Exponent = exponent+m_Mantissa.size();

    if ( !m_Sign && !m_Mantissa.empty() ) {
        m_Sign = 1;
    }

    Normalize();
    return *this;
}


void CDecimal::Normalize(void)
{
    size_t size = m_Mantissa.size();
    while ( size && m_Mantissa[size-1] == '0' ) {
        m_Mantissa.resize(--size);
    }
    while ( size && m_Mantissa[0] == '0' ) {
        m_Mantissa.erase(0, 1);
        --size;
        --m_Exponent;
    }
    if ( m_Mantissa.empty() ) {
        m_Sign = 0;
        m_Exponent = 0;
    }
    else if ( m_Sign == 0 ) {
        m_Sign = 1;
    }
}


CDecimal CDecimal::operator-(void) const
{
    CDecimal ret(*this);
    ret.m_Sign = -ret.m_Sign;
    return ret;
}


CDecimal operator+(const CDecimal& d1, const CDecimal& d2);
CDecimal operator-(const CDecimal& d1, const CDecimal& d2);

CDecimal operator+(const CDecimal& d1, const CDecimal& d2)
{
    if ( d2.m_Sign == 0 ) {
        return d1;
    }
    if ( d1.m_Sign == 0 ) {
        return d2;
    }
    if ( d1.m_Sign != d2.m_Sign ) {
        return d1 - -d2;
    }
    if ( d1.m_Sign < 0 ) {
        return -(-d1 + -d2);
    }
    if ( d1.m_Exponent < d2.m_Exponent ) {
        return d2+d1;
    }
    _ASSERT(d1.m_Sign == 1 && d2.m_Sign == 1);
    _ASSERT(!d1.m_Mantissa.empty() && !d2.m_Mantissa.empty());
    _ASSERT(d1.m_Exponent >= d2.m_Exponent);
    CDecimal ret(d1);
    size_t pos = ret.m_Exponent-d2.m_Exponent;
    if ( pos + d2.m_Mantissa.size() > ret.m_Mantissa.size() ) {
        ret.m_Mantissa.resize(pos + d2.m_Mantissa.size(), '0');
    }
    int c = 0;
    for ( size_t i = d2.m_Mantissa.size(); i--; ) {
        int v = (ret.m_Mantissa[pos+i]-'0') + (d2.m_Mantissa[i]-'0') + c;
        if ( v >= 10 ) {
            c = 1;
            v -= 10;
        }
        else {
            c = 0;
        }
        ret.m_Mantissa[pos+i] = '0'+v;
    }
    if ( c ) {
        ret.m_Mantissa.insert(0u, 1u, char('0'+c));
        ret.m_Exponent += 1;
    }
    ret.Normalize();
    return ret;
}


int CDecimal::Increment(size_t i, int c)
{
    if ( !c ) return 0;
    while ( i-- ) {
        if ( ++m_Mantissa[i] <= '9' )
            return 0;
        m_Mantissa[i] -= 10;
    }
    return 1;
}


int CDecimal::Decrement(size_t i, int c)
{
    if ( !c ) return 0;
    while ( i-- ) {
        if ( --m_Mantissa[i] >= '0' )
            return 0;
        m_Mantissa[i] += 10;
    }
    return 1;
}


CDecimal operator-(const CDecimal& d1, const CDecimal& d2)
{
    if ( d2.m_Sign == 0 ) {
        return d1;
    }
    if ( d1.m_Sign == 0 ) {
        return -d2;
    }
    if ( d1.m_Sign != d2.m_Sign ) {
        return d1 + -d2;
    }
    if ( d1.m_Sign < 0 ) {
        return -(-d1 - -d2);
    }
    if ( d1.m_Exponent < d2.m_Exponent ) {
        return -(d2-d1);
    }
    _ASSERT(d1.m_Sign == 1 && d2.m_Sign == 1);
    _ASSERT(!d1.m_Mantissa.empty() && !d2.m_Mantissa.empty());
    _ASSERT(d1.m_Exponent >= d2.m_Exponent);
    //LOG_POST(" d1="<<d1);
    //LOG_POST(" d2="<<d2);
    CDecimal ret(d1);
    size_t pos = ret.m_Exponent-d2.m_Exponent;
    if ( pos + d2.m_Mantissa.size() > ret.m_Mantissa.size() ) {
        ret.m_Mantissa.resize(pos + d2.m_Mantissa.size(), '0');
    }
    int c = 0;
    for ( size_t i = d2.m_Mantissa.size(); i--; ) {
        int v = (ret.m_Mantissa[pos+i]) - (d2.m_Mantissa[i]) - c;
        if ( v < 0 ) {
            c = 1;
            v += 10;
        }
        else {
            c = 0;
        }
        ret.m_Mantissa[pos+i] = '0'+v;
    }
    c = ret.Decrement(pos, c);
    if ( c ) {
        ret.m_Sign = -1;
        size_t size = ret.m_Mantissa.size();
        for ( size_t i = 0; i < size; ++i ) {
            ret.m_Mantissa[i] = '0'+(9-(ret.m_Mantissa[i]-'0'));
        }
        c = ret.Increment(size, 1);
        if ( c ) {
            ret.m_Mantissa.insert(0u, 1u, char('0'+c));
            ret.m_Exponent += 1;
        }
    }
    //LOG_POST("ret="<<ret);
    ret.Normalize();
    return ret;
}


CDecimal abs(const CDecimal& d)
{
    CDecimal ret = d;
    if ( ret.m_Sign == -1 )
        ret.m_Sign = 1;
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// End of CDecimal
/////////////////////////////////////////////////////////////////////////////


inline double GetNextInc(double t, int inc)
{
    union {
        double d;
        Int8 i8;
    };
    d = t;
    i8 += inc;
    return d;
}

double GetNextToward(double t, double toward)
{
    double err = fabs(t-toward);
    if ( err == 0 ) return toward;
    double t1 = GetNextInc(t, 1);
    double err1 = fabs(t1-toward);
    double t2 = GetNextInc(t, -1);
    double err2 = fabs(t2-toward);
    return err1 < err2? t1: t2;
}


double CTestApp::PreciseStringToDouble(const CTempStringEx& s0)
{
    double best_ret = NStr::StringToDouble(s0);
    CDecimal d0(s0);
    CDecimal best_err = CDecimal(best_ret, 24)-d0, first_err = best_err;
    if ( best_err.m_Sign == 0 ) {
        return best_ret;
    }
    double last_v = best_ret;
    double toward_v;
    if ( (last_v > 0) == (first_err.m_Sign > 0) ) {
        toward_v = 0;
    }
    else {
        toward_v = last_v*2;
    }
    //LOG_POST(s0<<" err: "<<best_err);
    for ( ;; ) {
        double v = GetNextToward(last_v, toward_v);
        CDecimal err = CDecimal(v, 24)-d0;
        //LOG_POST(" new err: "<<err);
        if ( (abs(err) - abs(best_err)).m_Sign < 0 ) {
            if ( err.m_Sign == 0 ) {
                return v;
            }
            best_err = err;
            best_ret = v;
        }
        if ( (err.m_Sign > 0) != (first_err.m_Sign > 0) ) {
            break;
        }
        last_v = v;
    }
    //LOG_POST(s0<<" correct bits: "<<log(fabs(best_ret/best_err))/log(2.));
    return best_ret;
}


void CTestApp::RunSpeedBenchmark(void)
{
    NcbiCout << NcbiEndl << "NStr:: String to Double speed tests..."
             << NcbiEndl;

    const CArgs& args = GetArgs();

    const int COUNT = args["count"].AsInteger();

    string test_strings;
    if ( args["test_strings"] ) {
        test_strings = args["test_strings"].AsString();
    }
    else {
        test_strings = ",0,1,12,123,123456789,1234567890,TRACE,0e9,1e9"
            ",1.234567890123456789e300,-1.234567890123456789e-300"
            ",1.234567890123456789e200,-1.234567890123456789e-200";
    }
    vector<string> ss;
    NStr::Tokenize(test_strings, ",", ss);
    const size_t TESTS = ss.size();
    vector<double> ssr(TESTS), ssr_min(TESTS), ssr_max(TESTS);
    static const int kConvertError = -555;
    for ( size_t i = 0; i < TESTS; ++i ) {
        double r;
        try {
            r = PreciseStringToDouble(ss[i]);
        }
        catch ( CException& /*exc*/ ) {
            //ERR_POST(exc);
            ssr[i] = ssr_min[i] = ssr_max[i] = kConvertError;
            continue;
        }
        ssr[i] = r;
        double min, max;
        if ( r < 0 ) {
            min = r*(1+EPS);
            max = r*(1-EPS);
        }
        else {
            min = r*(1-EPS);
            max = r*(1+EPS);
        }
        ssr_min[i] = min;
        ssr_max[i] = max;
    }

    int flags = NStr::fConvErr_NoThrow|NStr::fAllowLeadingSpaces;
    double v;
    for ( size_t t = 0; t < TESTS; ++t ) {
        if ( 1 ) {
            errno = 0;
            v = NStr::StringToDouble(ss[t], flags|NStr::fDecimalPosix);
            if ( errno ) v = kConvertError;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_POST(Fatal<<v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }

        if ( 1 ) {
            errno = 0;
            v = NStr::StringToDouble(ss[t], flags);
            if ( errno ) v = kConvertError;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_POST(Fatal<<v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }

        if ( 1 ) {
            errno = 0;
            char* errptr;
            v = NStr::StringToDoublePosix(ss[t].c_str(), &errptr);
            if ( errno || (errptr&&(*errptr||errptr==ss[t].c_str())) ) v = kConvertError;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_POST(Fatal<<v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }

        if ( 1 ) {
            errno = 0;
            char* errptr;
            v = StringToDoublePosixOld(ss[t].c_str(), &errptr);
            if ( errno || (errptr&&(*errptr||errptr==ss[t].c_str())) ) v = kConvertError;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_POST(Fatal<<v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }

        if ( 1 ) {
            errno = 0;
            char* errptr;
            v = strtod(ss[t].c_str(), &errptr);
            if ( errno || (errptr&&(*errptr||errptr==ss[t].c_str())) ) v = kConvertError;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_POST(Fatal<<v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }
    }
    for ( size_t t = 0; t < TESTS; ++t ) {
        NcbiCout << "Testing "<<ss[t]<<":" << endl;
        string s1 = ss[t];
        CTempStringEx s = ss[t];
        const char* s2 = ss[t].c_str();
        CStopWatch sw;
        double time;

        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                errno = 0;
                v = NStr::StringToDouble(s, flags|NStr::fDecimalPosix);
                if ( errno ) v = kConvertError;
            }
            time = sw.Elapsed();
            NcbiCout << "   StringToDouble(Posix): " << time << endl;
        }
        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                errno = 0;
                v = NStr::StringToDouble(s, flags);
                if ( errno ) v = kConvertError;
            }
            time = sw.Elapsed();
            NcbiCout << "        StringToDouble(): " << time << endl;
        }
        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                errno = 0;
                char* errptr;
                v = NStr::StringToDoublePosix(s2, &errptr);
                if ( errno || (errptr&&(*errptr||errptr==s2)) ) v = kConvertError;
            }
            time = sw.Elapsed();
            NcbiCout << "   StringToDoublePosix(): " << time << endl;
        }
        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                errno = 0;
                char* errptr;
                v = StringToDoublePosixOld(s2, &errptr);
                if ( errno || (errptr&&(*errptr||errptr==s2)) ) v = kConvertError;
            }
            time = sw.Elapsed();
            NcbiCout << "StringToDoublePosixOld(): " << time << endl;
        }
        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                errno = 0;
                char* errptr;
                v = strtod(s2, &errptr);
                if ( errno || (errptr&&(*errptr||errptr==s2)) ) v = kConvertError;
            }
            time = sw.Elapsed();
            NcbiCout << "                strtod(): " << time << endl;
        }
    }
}


void CTestApp::RunPrecisionBenchmark(void)
{
    const CArgs& args = GetArgs();

    const int COUNT = args["count"].AsInteger();
    double threshold = args["threshold"].AsDouble();
    const int kCallPosix = 0;
    const int kCallPosixOld = 1;
    const int kCallstrtod = 2;
    int call_type = kCallPosix;
    if ( args["precision"].AsString() == "Posix" ) {
        call_type = kCallPosix;
    }
    if ( args["precision"].AsString() == "PosixOld" ) {
        call_type = kCallPosixOld;
    }
    if ( args["precision"].AsString() == "strtod" ) {
        call_type = kCallstrtod;
    }

    char str[200];
    char* errptr = 0;
    const int MAX_DIGITS = 24;

    typedef map<int, int> TErrCount;
    int err_close = 0;
    TErrCount err_count;
    
    for ( int test = 0; test < COUNT; ++test ) {
        {
            int digits = 1+rand()%MAX_DIGITS;
            int exp = rand()%600-300;
            char* ptr = str;
            if ( rand()%1 ) *ptr++ = '-';
            *ptr++ = '.';
            for ( int i = 0; i < digits; ++i ) {
                *ptr++ = '0'+rand()%10;
            }
            sprintf(ptr, "e%d", exp);
        }

        double v_ref = PreciseStringToDouble(str);
        
        errno = 0;
        double v = 0;
        switch ( call_type ) {
        case kCallPosix:
            v = NStr::StringToDoublePosix(str, &errptr);
            break;
        case kCallPosixOld:
            v = StringToDoublePosixOld(str, &errptr);
            break;
        case kCallstrtod:
            v = strtod(str, &errptr);
            break;
        }
        if ( errno||(errptr&&(*errptr||errptr==str)) ) {
            // error
            ERR_POST("Failed to convert: "<< str);
            err_count[-1] += 1;
            continue;
        }
        if ( v == v_ref ) {
            continue;
        }
        CDecimal d0(str);
        CDecimal d_ref(v_ref, 24);
        CDecimal d_v(v, 24);
        int exp_shift = 0;
        if ( d0.m_Exponent > 200 ) exp_shift = -100;
        if ( d0.m_Exponent < -200 ) exp_shift = 100;
        double err_ref = fabs((d_ref-d0).ToDouble(exp_shift));
        double err_v = fabs((d_v-d0).ToDouble(exp_shift));
        if ( err_v <= err_ref*(1+threshold) ) {
            if ( m_VerboseLevel >= 2 ) {
                LOG_POST("d_str: "<<d0);
                LOG_POST("d_ref: "<<d_ref<<" err="<<err_ref);
                LOG_POST("d_cur: "<<d_v<<" err="<<err_v);
            }
            ++err_close;
            continue;
        }
        if ( m_VerboseLevel >= 1 ) {
            LOG_POST("d_str: "<<d0);
            LOG_POST("d_ref: "<<d_ref<<" err="<<err_ref);
            LOG_POST("d_cur: "<<d_v<<" err="<<err_v);
        }

        int err = 0;
        for ( double t = v; t != v_ref; ) {
            //LOG_POST(setprecision(20)<<t<<" - "<<v_ref<<" = "<<(t-v_ref));
            ++err;
            t = GetNextToward(t, v_ref);
        }
        err_count[err] += 1;
    }
    NcbiCout << "Close errors: "<<err_close<<"/"<<COUNT
             << " = " << 1e2*err_close/COUNT<<"%"
             << NcbiEndl;
    ITERATE ( TErrCount, it, err_count ) {
        NcbiCout << "Errors["<<it->first<<"] = "<<it->second<<"/"<<COUNT
                 << " = " << 1e2*it->second/COUNT<<"%"
                 << NcbiEndl;
    }
}


double CTestApp::GenerateDouble(void)
{
    int exp = rand()%614-307;
    double value = (1000 + rand()%9000)/1000.; // from 1.000 to 9.999
    value += (rand()%10000)/1.e8;
    value += (rand()%10000)/1.e12;
#if 1
    if (rand()%2)
    {
        value += (rand()%10000)/1.e16;
        value += (rand()%10000)/1.e20;
    }
#endif
    if (exp > 0) {
        if (exp>=256) {value *= 1.e256; exp-=256;}
        if (exp>=128) {value *= 1.e128; exp-=128;}
        if (exp>= 64) {value *= 1.e64;  exp-= 64;}
        if (exp>= 32) {value *= 1.e32;  exp-= 32;}
        if (exp>= 16) {value *= 1.e16;  exp-= 16;}
        if (exp>=  8) {value *= 1.e8;   exp-=  8;}
        if (exp>=  4) {value *= 1.e4;   exp-=  4;}
        if (exp>=  2) {value *= 1.e2;   exp-=  2;}
        if (exp>=  1) {value *= 10.;    exp-=  1;}
    } else {
        if (exp<=-256) {value /= 1.e256; exp+=256;}
        if (exp<=-128) {value /= 1.e128; exp+=128;}
        if (exp<= -64) {value /= 1.e64;  exp+= 64;}
        if (exp<= -32) {value /= 1.e32;  exp+= 32;}
        if (exp<= -16) {value /= 1.e16;  exp+= 16;}
        if (exp<=  -8) {value /= 1.e8;   exp+=  8;}
        if (exp<=  -4) {value /= 1.e4;   exp+=  4;}
        if (exp<=  -2) {value /= 1.e2;   exp+=  2;}
        if (exp<=  -1) {value /= 10.;    exp+=  1;}
    }
    return value;
}

void CTestApp::RunD2SSpeedBenchmark(void)
{
    const CArgs& args = GetArgs();
    // generate test data
    const Int8 COUNT = args["count"].AsInt8();
    const unsigned int precision = args["digits"].AsInteger();
    vector<double> dbls;
    NcbiCout << "--------------------------------" << NcbiEndl
             << "DoubleToString speed: converting " << COUNT << " numbers" << NcbiEndl;
    double dbl2str = 0.;
    double sprn = 0.;
    Int8 count=0;
    const int count_per_iteration = 10000000;

    do {
        dbls.clear();
        for (int i=0; i< count_per_iteration && count < COUNT; ++i, ++count) {
            dbls.push_back( GenerateDouble());
        }
        // test double-to-string
        char buffer[64];

        CStopWatch sw(CStopWatch::eStart);
        for (vector<double>::const_iterator d=dbls.begin(); d != dbls.end(); ++d) {
            NStr::DoubleToStringPosix(*d, precision, buffer, sizeof(buffer));
        }
        dbl2str += sw.Restart();

        for (vector<double>::const_iterator d=dbls.begin(); d != dbls.end(); ++d) {
            sprintf(buffer, "%.*g", precision, *d);
        }
        sprn += sw.Elapsed();
    } while (count < COUNT);

    NcbiCout << "NStr::DoubleToStringPosix takes " << dbl2str << " sec" << NcbiEndl;
    NcbiCout << "sprintf takes " << sprn << " sec" << NcbiEndl;
    if (dbl2str != 0.) {
        NcbiCout << "speedup: " << sprn/dbl2str << NcbiEndl;
    }
    NcbiCout << NcbiEndl;
}

void CTestApp::RunD2SPrecisionBenchmark(void)
{
    const CArgs& args = GetArgs();
    // generate test data
    const Int8 COUNT = args["count"].AsInt8();
    const unsigned int precision = args["digits"].AsInteger();
    vector<double> dbls;
    NcbiCout << "-------------------------------------" << NcbiEndl
             << "DoubleToString precision: converting " << COUNT << " numbers" << NcbiEndl;

    bool first_iteration = true;
    double special[] = {
//            5.8220349999999992744e-169,
//            DBL_MIN, DBL_MAX,
                        1e-17, 1e-16, 1e-15, 1e-14, 1e-13, 1e-12, 1e-11, 1e-10,
            1e-9, 1e-8, 1e-7,  1e-6,   1e-5,  1e-4,  1e-3,  1e-2,  1e-1, 1.,
                        1e+17, 1e+16, 1e+15, 1e+14, 1e+13, 1e+12, 1e+11, 1e+10,
            1e+9, 1e+8, 1e+7,  1e+6,   1e+5,  1e+4,  1e+3,  1e+2,  1e+1,
            1.00000000000000001e+11,
            1.0000000000000001e+11,
            1.000000000000001e+11,
            1.00000000000001e+11,
            1.0000000000001e+11,
            0.99999999999999999e+11,
            0.9999999999999999e+11,
            0.999999999999999e+11,
            0.99999999999999e+11,
            0.99999999999999e+11,
            0.9999999999999e+11
            };

    Int8 count=0;
    const int count_per_iteration = 10000000;

    Int8 diff=0;
    Int8 errors=0;
    Int8 roundtrip=0;
    Int8 longer=0;
    Int8 shorter=0;

    char buffer1[64], buffer2[64], buffer0[64];
    map<size_t,Int8> diffmap;
    map<size_t,Int8> longermap;
    map<size_t,Int8> shortermap;

    do {

    int i=0;
    dbls.clear();
    if (first_iteration) {
        first_iteration = false;
        for (; i<(int)(sizeof(special)/sizeof(double)); ++i, ++count) {
            dbls.push_back( special[i]);
        }
    }
    for (; i< count_per_iteration && count < COUNT; ++i, ++count) {
        dbls.push_back( GenerateDouble());
    }

    for (vector<double>::const_iterator d=dbls.begin(); d != dbls.end(); ++d) {

        buffer1[ NStr::DoubleToStringPosix( *d, precision, buffer1, sizeof(buffer1)) ] = '\0';
        sprintf(buffer2, "%.*g", precision, *d);

        {
            char buffer_t[64];
            double d_t;
            d_t = NStr::StringToDouble(buffer2);
            buffer_t[ NStr::DoubleToStringPosix( d_t, precision, buffer_t, sizeof(buffer_t)) ] = '\0';
            if (strcmp(buffer2,buffer_t)) {
                ++roundtrip;
                LOG_POST("ERROR: roundtrip buf2: " << buffer1 << " vs " << buffer2 << " vs " << buffer_t);
            }
            if (strcmp(buffer1,buffer2)!=0) {
                d_t = NStr::StringToDouble(buffer1);
                buffer_t[ NStr::DoubleToStringPosix( d_t, precision, buffer_t, sizeof(buffer_t)) ] = '\0';
                if (strcmp(buffer1,buffer_t)) {
                    ++roundtrip;
                    LOG_POST("ERROR: roundtrip buf1: " << buffer1 << " vs " << buffer2 << " vs " << buffer_t);
                }
            }
        }
        if (strcmp(buffer1,buffer2)==0) {
            if (!CompareSerialization(*d, DBL_DIG)) {
                ++errors;
                sprintf(buffer1, "%.20g", *d);
                LOG_POST("ERROR: serialization with high precision: " << buffer1);
            }
            if (!CompareSerialization(*d, FLT_DIG)) {
//                ++errors;
                sprintf(buffer1, "%.20g", *d);
                LOG_POST("ERROR: serialization with low precision: " << buffer1);
            }
            continue;
        }
        strcpy(buffer0,buffer2);
        {
            char *e;
            double d1 = strtod(buffer1,&e);
            double d2 = strtod(buffer2,&e);
            double dr=1.;
            if (d2 != 0.) {
                dr = d1/d2;
            } else if (d1 != 0.) {
                dr = d2/d1;
            }
            if (dr < 0.) {dr=-dr;}
            if (dr>=1.1 || dr<=0.9) {
                ++errors;
                LOG_POST("ERROR: strtod: " << buffer1 << " vs " << buffer2);
            }
        }
        
        if (strlen(buffer1) > strlen(buffer2)) {
            ++longer;
            size_t st = strlen(buffer1) - strlen(buffer2);
            if (longermap.find(st) != longermap.end()) {
                longermap[st] = longermap[st]+1;
            } else {
                longermap[st] = 1;
            }
        }
        if (strlen(buffer1) < strlen(buffer2)) {
            ++shorter;
            size_t st = strlen(buffer2) - strlen(buffer1);
            if (shortermap.find(st) != shortermap.end()) {
                shortermap[st] = shortermap[st]+1;
            } else {
                shortermap[st] = 1;
            }
        }
        ++diff;

        char* b1 = strchr(buffer1, 'e');
        char* b2 = strchr(buffer2, 'e');
        if ((b1 && b2) || (!b1 && !b2)) {
            if (b1 && b2) {
                // compare exponent
                if (strcmp(b1,b2)) {
                    ++errors;
                    LOG_POST("ERROR: exponent: " << buffer1 << " vs " << buffer2);
                }
                *b1 = *b2 = '\0';
                if (strcmp(buffer1,buffer2)==0) {
                    continue;
                }
            }
            size_t s1 = min( strlen(buffer1), strlen(buffer2));
            if (s1 != 0) {
                char *sz;
                sz = strchr(buffer1,'.');
                if (sz) {
                    memmove(sz,sz+1,strlen(sz));
                }
                sz = strchr(buffer2,'.');
                if (sz) {
                    memmove(sz,sz+1,strlen(sz));
                }
                size_t sb1 = strlen(buffer1);
                size_t sb2 = strlen(buffer2);
                while (sb1 < sb2) {
                    buffer1[sb1++] = '0';
                    buffer1[sb1]='\0';
                }
                while (sb2 < sb1) {
                    buffer2[sb2++] = '0';
                    buffer2[sb2]='\0';
                }
            }

            while (s1>0) {
                if (strncmp(buffer1,buffer2,s1) == 0) {

                    Int8 l1 = NStr::StringToInt8(buffer1+s1);
                    Int8 l2 = NStr::StringToInt8(buffer2+s1);
                    int id = (int)max(l1-l2,l2-l1);
                    size_t st = id;
                    if (diffmap.find(st) != diffmap.end()) {
                        diffmap[st] = diffmap[st]+1;
                    } else {
                        diffmap[st] = 1;
                        if (st != 1) {
                            LOG_POST("WARNING: diffmap[" << st << "]: "
                                << buffer0 << " double = " << *d);
                        }
                    }
                    if (st == 0) {
                        ++errors;
                        LOG_POST("ERROR: comparison: "  << buffer0);
                    }
                    break;


                }
                --s1;
            }
            if (s1 == 0) {
                ++errors;
                LOG_POST("ERROR: comparison: "  << buffer0);
            }
        }
        else {
            ++errors;
            LOG_POST("ERROR: exponent: "  << buffer1 << " vs " << buffer2);
        }
    }
    }  while (count < COUNT);

    if (errors != 0) {
        NcbiCout << "ERRORS (need immediate attention!): " << errors << NcbiEndl;
    }
    NcbiCout << "Different result: " << diff << "  (" << (100.*diff)/COUNT << "%)" << NcbiEndl;
    for (map<size_t,Int8>::const_iterator t = diffmap.begin(); t != diffmap.end(); ++t) {
        NcbiCout << "Differ in " << t->first << " decimal digit: " << t->second << endl;
    }
    NcbiCout << "DoubleToStringPosix produces longer string in " << longer << " cases" << NcbiEndl;
    for (map<size_t,Int8>::const_iterator t = longermap.begin(); t != longermap.end(); ++t) {
        NcbiCout << "    Longer by " << t->first << " chars: " << t->second << endl;
    }
    NcbiCout << "DoubleToStringPosix produces shorter string in " << shorter << " cases" << NcbiEndl;
    for (map<size_t,Int8>::const_iterator t = shortermap.begin(); t != shortermap.end(); ++t) {
        NcbiCout << "    Shorter by " << t->first << " chars: " << t->second << endl;
    }
    NcbiCout << "Roundtrip (string-double-string) errors: " << roundtrip << NcbiEndl;
    if (errors != 0) {
        ERR_POST(Fatal<< "ERRORS (need immediate attention!): " << errors);
    }
    NcbiCout << NcbiEndl;
}

// this is to compare old and new WriteDouble serialization code
bool CTestApp::CompareSerialization(double data, size_t digits)
{
    bool res = true;

// CObjectOStreamAsn::WriteDouble2(double data, size_t digits)
    string asntext_old, asntext_new;
    {{
        if ( data == 0.0 ) {
            asntext_old = "{ 0, 10, 0 }";
        } else {
            char buffer[128];
            // ensure buffer is large enough to fit result
            // (additional bytes are for sign, dot and exponent)
            _ASSERT(sizeof(buffer) > digits + 16);
            int width = sprintf(buffer, "%.*e", int(digits-1), data);
            _ASSERT(int(strlen(buffer)) == width);
            char* dotPos = strchr(buffer, '.');
            if (!dotPos) {
                dotPos = strchr(buffer, ','); // non-C locale?
            }
            _ASSERT(dotPos);
            char* ePos = strchr(dotPos, 'e');
            _ASSERT(ePos);

            // now we have:
            // mantissa with dot - buffer:ePos
            // exponent - (ePos+1):

            int exp;
            // calculate exponent
            sscanf(ePos + 1, "%d", &exp);
            // remove trailing zeroes
            int fractDigits = int(ePos - dotPos - 1);
            while ( fractDigits > 0 && ePos[-1] == '0' ) {
                --ePos;
                --fractDigits;
            }
            asntext_old = "{ ";
            asntext_old += string(buffer, dotPos - buffer);
            asntext_old += string(dotPos + 1, fractDigits);
            asntext_old += string(", 10, ");
            asntext_old += NStr::NumericToString(exp - fractDigits);
            asntext_old += string(" }");

        }
    }}
    {{
        char buffer[128];
        int dec, sign;
        size_t len = NStr::DoubleToString_Ecvt(
            data, digits, buffer, sizeof(buffer), &dec, &sign);
        //todo: verify that len > 0
        asntext_new = "{ ";
        if (sign < 0) {asntext_new += "-";}
        asntext_new += string(buffer,len);
        asntext_new += string(", 10, ");
        asntext_new += NStr::NumericToString(dec - (int)(len-1));
        asntext_new += string(" }");
    }}
    if (asntext_old != asntext_new) {
        LOG_POST("ERROR: serialization ASN text: " << asntext_old << " vs " << asntext_new);
        res = false;
    }

 // CObjectOStreamAsnBinary::WriteDouble2(double data, size_t digits)
    string asnbin_old, asnbin_new;
    {{
        const size_t kMaxDoubleLength = 64;
        int shift = 0;
        int precision = int(digits - shift);
        if ( precision < 0 )
            precision = 0;
        else if ( size_t(precision) > kMaxDoubleLength ) // limit precision of data
            precision = int(kMaxDoubleLength);

        // ensure buffer is large enough to fit result
        // (additional bytes are for sign, dot and exponent)
        char buffer[kMaxDoubleLength + 16];
        int width = sprintf(buffer, "%.*g", precision, data);
        _ASSERT(strlen(buffer) == size_t(width));
        char* dot = strchr(buffer,',');
        if (dot) {
            *dot = '.'; // enforce C locale
        }
        asnbin_old = string(buffer, width);
    }}
    {{
        char buffer[64];
        size_t len = NStr::DoubleToStringPosix(data, digits, buffer, sizeof(buffer));
        asnbin_new = string(buffer,len);
    }}
    if (asnbin_old != asnbin_new) {
        LOG_POST("ERROR: serialization ASN bin: " << asnbin_old << " vs " << asnbin_new);
        res = false;
    }

// CObjectOStreamXml::WriteDouble2(double data, size_t digits)
// same as sprintf versus DoubleToStringPosix

// CObjectOStreamJson::WriteDouble(double data)
// Json uses 'fixed' format, not scientific; so, - nothing to compare
    return res;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestApp().AppMain(argc, argv);
}
