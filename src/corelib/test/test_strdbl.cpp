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

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

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

    static double PreciseStringToDouble(const CTempStringEx& s);

    int m_VerboseLevel;
};


double StringToDoublePosixOld(const char* ptr, char** endptr)
{
// skip leading blanks
    for ( ; isspace(*ptr); ++ptr)
        ;
    const char* start = ptr;
    long double ret = NCBI_CONST_LONGDOUBLE(0.);
    bool sign= false, negate= false, dot= false, expn=false, anydigits=false;
    int digits = 0, dot_position = 0, exponent = 0;
    unsigned int first=0, second=0, first_exp=1;
    long double second_exp=NCBI_CONST_LONGDOUBLE(1.),
                third    = NCBI_CONST_LONGDOUBLE(0.);
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
    ret = ((long double)first * first_exp + second)* second_exp + third;
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
            long double power      = NCBI_CONST_LONGDOUBLE(1.),
                        power_mult = NCBI_CONST_LONGDOUBLE(10.);
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
    d->AddFlag("precision",
               "Run conversion precision benchmark");
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

    SetupArgDescriptions(d.release());
}


int CTestApp::Run(void)
{
    // Process command line
    const CArgs& args = GetArgs();

    m_VerboseLevel = args["verbose"].AsInteger();

    if ( args["speed"] ) {
        RunSpeedBenchmark();
    }
    
    if ( args["precision"] ) {
        RunPrecisionBenchmark();
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

    string ToString(void) const;

    CDecimal& operator=(int v);
    CDecimal& operator=(const CTempStringEx& s);

    CDecimal operator-(void) const;

    void Normalize(void);
    int Increment(size_t i, int c);
    int Decrement(size_t i, int c);

    double ToDouble(void) const;

    int m_Sign;
    int m_Exponent;
    string m_Mantissa; // digits after decimal point
};


ostream& operator<<(ostream& out, const CDecimal& d)
{
    if ( d.m_Sign == 0 )
        out << '0';
    else {
        if ( d.m_Sign < 0 ) out << '-';
        out << '.' << d.m_Mantissa;
        if ( d.m_Exponent )
            out << 'e' << d.m_Exponent;
    }
    return out;
}


string CDecimal::ToString(void) const
{
    CNcbiOstrstream out;
    out << *this;
    return CNcbiOstrstreamToString(out);
}


double CDecimal::ToDouble(void) const
{
    return NStr::StringToDouble(ToString());
}


CDecimal::CDecimal(double value, int digits)
{
    *this = NStr::DoubleToString(value, digits, NStr::fDoubleScientific|NStr::fDecimalPosix);
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
        ret.m_Mantissa.insert(0, 1, '0'+c);
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
        int c = ret.Increment(size, 1);
        if ( c ) {
            ret.m_Mantissa.insert(0, 1, '0'+c);
            ret.m_Exponent += 1;
        }
    }
    //LOG_POST("ret="<<ret);
    ret.Normalize();
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
    double best_err = (CDecimal(best_ret, 24)-d0).ToDouble();
    if ( best_err == 0 ) {
        return best_ret;
    }
    double last_v = best_ret, last_err = best_err;
    double toward_v;
    if ( (last_v > 0) == (last_err > 0) ) {
        toward_v = 0;
    }
    else {
        toward_v = last_v*2;
    }
    //LOG_POST(s0<<" err: "<<best_err);
    for ( ;; ) {
        double v = GetNextToward(last_v, toward_v);
        double err = (CDecimal(v, 24)-d0).ToDouble();
        //LOG_POST(" new err: "<<err);
        if ( fabs(err) < fabs(best_err) ) {
            if ( err == 0 ) {
                return v;
            }
            best_err = err;
            best_ret = v;
        }
        if ( (err > 0) != (last_err > 0) ) {
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
    for ( size_t i = 0; i < TESTS; ++i ) {
        double r;
        try {
            r = PreciseStringToDouble(ss[i]);
        }
        catch ( CException& /*exc*/ ) {
            //ERR_POST(exc);
            ssr[i] = ssr_min[i] = ssr_max[i] = -1;
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
            if ( errno ) v = -1;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_POST(Fatal<<v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }

        if ( 1 ) {
            errno = 0;
            v = NStr::StringToDouble(ss[t], flags);
            if ( errno ) v = -1;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_POST(Fatal<<v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }

        if ( 1 ) {
            errno = 0;
            char* errptr;
            v = NStr::StringToDoublePosix(ss[t].c_str(), &errptr);
            if ( errno || (errptr&&(*errptr||errptr==ss[t].c_str())) ) v = -1;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_POST(Fatal<<v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }

        if ( 1 ) {
            errno = 0;
            char* errptr;
            v = StringToDoublePosixOld(ss[t].c_str(), &errptr);
            if ( errno || (errptr&&(*errptr||errptr==ss[t].c_str())) ) v = -1;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_POST(Fatal<<v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }

        if ( 1 ) {
            errno = 0;
            char* errptr;
            v = strtod(ss[t].c_str(), &errptr);
            if ( errno || (errptr&&(*errptr||errptr==ss[t].c_str())) ) v = -1;
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
                if ( errno ) v = -1;
            }
            time = sw.Elapsed();
            NcbiCout << "   StringToDouble(Posix): " << time << endl;
        }
        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                errno = 0;
                v = NStr::StringToDouble(s, flags);
                if ( errno ) v = -1;
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
                if ( errno || (errptr&&(*errptr||errptr==s2)) ) v = -1;
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
                if ( errno || (errptr&&(*errptr||errptr==s2)) ) v = -1;
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
                if ( errno || (errptr&&(*errptr||errptr==s2)) ) v = -1;
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

    char str[200];
    char* errptr;
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
        double v = strtod(str, &errptr);
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
        double err_ref = fabs((d_ref-d0).ToDouble());
        double err_v = fabs((d_v-d0).ToDouble());
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


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestApp().AppMain(argc, argv);
}
