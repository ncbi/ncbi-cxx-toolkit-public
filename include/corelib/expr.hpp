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
 * Author: Sergey Sikorskiy
 *
 * File Description: 
 *      Expresiion parsing and evaluation.
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

#if defined(NCBI_OS_MSWIN)
#define INT_FORMAT "I64"
#elif SIZEOF_LONG == 8
#define INT_FORMAT "l"
#else 
#define INT_FORMAT "ll"
#endif


#define BINARY(opd) (opd >= ePOW)


////////////////////////////////////////////////////////////////////////////////
class CExprSymbol;

class NCBI_XNCBI_EXPORT CExprValue 
{ 
public:
    CExprValue(void);
    template <typename VT> CExprValue(VT value);
    CExprValue(const CExprValue& value);

public:
    enum EValue { 
        eINT, 
        eFLOAT,
        eBOOL
    };

public:
    EValue GetType(void) const
    {
        return m_Tag;
    }
    void SetType(EValue type)
    {
        m_Tag = type;
    }

    double GetDouble(void) const
    { 
        switch (m_Tag) {
            case eINT:
                return static_cast<double>(ival);
            case eBOOL:
                return bval ? 1.0 : 0.0;
            default:
                break;
        }

        return fval; 
    }

    Int8 GetInt(void) const
    { 
        switch (m_Tag) {
            case eFLOAT:
                return static_cast<Int8>(fval);
            case eBOOL:
                return bval ? 1 : 0;
            default:
                break;
        }

        return ival; 
    }

    bool GetBool(void) const
    { 
        switch (m_Tag) {
            case eINT:
                return ival != 0;
            case eFLOAT:
                return fval != 0.0;
            default:
                break;
        }

        return bval; 
    }

public:
    union { 
        Int8    ival;
        double  fval;
        bool    bval;
    };

    CExprSymbol*    m_Var;
    int             m_Pos;

private:
    EValue          m_Tag;
};

template <> NCBI_XNCBI_EXPORT CExprValue::CExprValue(Int8 value);
template <> NCBI_XNCBI_EXPORT CExprValue::CExprValue(double value);
template <> NCBI_XNCBI_EXPORT CExprValue::CExprValue(bool value);

////////////////////////////////////////////////////////////////////////////////
class NCBI_XNCBI_EXPORT CExprSymbol 
{ 
public:
    CExprSymbol(void);
    template <typename VT> CExprSymbol(const char* name, VT value);
    ~CExprSymbol(void);

#if defined(_MSC_VER) && _MSC_VER < 1400
    // We need that to prevent internal compiler error with MSVC 7.1.
    CExprSymbol(const char* name, Int8 value);
    CExprSymbol(const char* name, bool value);
    CExprSymbol(const char* name, double value);
#endif

public:
    enum ESymbol { 
        eVARIABLE, 
        eIFUNC1, 
        eIFUNC2,
        eFFUNC1, 
        eFFUNC2,
        eBFUNC1, 
        eBFUNC2 
    };

    typedef Int8    (*FIntFunc1)    (Int8);
    typedef Int8    (*FIntFunc2)    (Int8,Int8);
    typedef double  (*FFloatFunc1)  (double);
    typedef double  (*FFloatFunc2)  (double, double);
    typedef bool    (*FBoolFunc1)   (bool);
    typedef bool    (*FBoolFunc2)   (bool, bool);

public:
    ESymbol         m_Tag;
    union { 
        FIntFunc1   m_IntFunc1;
        FIntFunc2   m_IntFunc2;
        FFloatFunc1 m_FloatFunc1;
        FFloatFunc2 m_FloatFunc2;
        FBoolFunc1  m_BoolFunc1;
        FBoolFunc2  m_BoolFunc2;
    };
    CExprValue      m_Val;
    string          m_Name;
    CExprSymbol*    m_Next;
};
	       
template <> NCBI_XNCBI_EXPORT CExprSymbol::CExprSymbol(const char* name, Int8 value);
template <> NCBI_XNCBI_EXPORT CExprSymbol::CExprSymbol(const char* name, double value);
template <> NCBI_XNCBI_EXPORT CExprSymbol::CExprSymbol(const char* name, bool value);
template <> NCBI_XNCBI_EXPORT CExprSymbol::CExprSymbol(const char* name, FIntFunc1 value);
template <> NCBI_XNCBI_EXPORT CExprSymbol::CExprSymbol(const char* name, FIntFunc2 value);
template <> NCBI_XNCBI_EXPORT CExprSymbol::CExprSymbol(const char* name, FFloatFunc1 value);
template <> NCBI_XNCBI_EXPORT CExprSymbol::CExprSymbol(const char* name, FFloatFunc2 value);
template <> NCBI_XNCBI_EXPORT CExprSymbol::CExprSymbol(const char* name, FBoolFunc1 value);
template <> NCBI_XNCBI_EXPORT CExprSymbol::CExprSymbol(const char* name, FBoolFunc2 value);


////////////////////////////////////////////////////////////////////////////////
class NCBI_XNCBI_EXPORT CExprParserException : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    enum EErrCode {
        eParseError
    };


    CExprParserException(
        const CDiagCompileInfo& info,
        const CException* prev_exception, EErrCode err_code,
        const string& message, int pos, 
        EDiagSev severity = eDiag_Error)
    : CException(info, prev_exception, CException::eInvalid, message)
    , m_Pos(pos)
    NCBI_EXCEPTION_DEFAULT_IMPLEMENTATION(CExprParserException, CException);

    virtual const char* GetErrCodeString(void) const;

    virtual void ReportExtra(ostream& out) const;

public:
    int GetPos(void) const
    {
        return m_Pos;
    }

protected:
    virtual void x_Assign(const CException& src);

private:
    int    m_Pos;
}; 

////////////////////////////////////////////////////////////////////////////////
class NCBI_XNCBI_EXPORT CExprParser
{
public:
    /// eAllowAutoVar - means "create variables without previous declaration".
    /// eDenyAutoVar - means "call AddSymbol() to register a variable".
    enum EAutoVar {eAllowAutoVar, eDenyAutoVar};

    CExprParser(EAutoVar auto_var = eAllowAutoVar);
    ~CExprParser(void);

public:
    void Parse(const char* str);

    const CExprValue& GetResult(void) const
    {
        if (m_v_sp != 1) {
            ReportError("Result is not available");
        }

        return m_VStack[0];
    }
    
    template <typename VT> 
    CExprSymbol* AddSymbol(const char* name, VT value);

private:
    enum EOperator { 
        eBEGIN, eOPERAND, eERROR, eEND, 
        eLPAR, eRPAR, eFUNC, ePOSTINC, ePOSTDEC,
        ePREINC, ePREDEC, ePLUS, eMINUS, eNOT, eCOM,
        ePOW,
        eMUL, eDIV, eMOD,
        eADD, eSUB, 
        eASL, eASR, eLSR, 
        eGT, eGE, eLT, eLE,     
        eEQ, eNE, 
        eAND,
        eXOR,
        eOR,
        eSET, eSETADD, eSETSUB, eSETMUL, eSETDIV, eSETMOD, eSETASL, eSETASR, eSETLSR, 
        eSETAND, eSETXOR, eSETOR, eSETPOW,
        eCOMMA,
        eTERMINALS
    };

private:
    EOperator Scan(bool operand);
    bool Assign(void);
    CExprSymbol* GetSymbol(const char* name) const;

    static void ReportError(int pos, const string& msg) 
    {
        NCBI_THROW2(CExprParserException, eParseError, msg, pos);
    }
    void ReportError(const string& msg) const { ReportError(m_Pos - 1, msg); }

    EOperator IfChar(
            char c, EOperator val, 
            EOperator val_def);
    EOperator IfElseChar(
            char c1, EOperator val1, 
            char c2, EOperator val2, 
            EOperator val_def);
    EOperator IfLongest2ElseChar(
            char c1, char c2, 
            EOperator val_true_longest, 
            EOperator val_true, 
            EOperator val_false, 
            EOperator val_def);

    EAutoVar AutoCreateVariable(void) const
    {
        return m_AutoCreateVariable;
    }

private:
    enum {hash_table_size = 1013};
    CExprSymbol* hash_table[hash_table_size];

    enum {max_stack_size = 256};
    enum {max_expression_length = 1024};

    static int sm_lpr[eTERMINALS];
    static int sm_rpr[eTERMINALS];

    CExprValue  m_VStack[max_stack_size];
    int         m_v_sp;
    EOperator   m_OStack[max_stack_size];
    int         m_o_sp;
    const char* m_Buf;
    int         m_Pos;
    int         m_TmpVarCount;
    EAutoVar    m_AutoCreateVariable;
};


////////////////////////////////////////////////////////////////////////////////
// Inline methods.
//

NCBI_XNCBI_EXPORT
unsigned string_hash_function(const char* p);

template <typename VT> 
inline
CExprSymbol* CExprParser::AddSymbol(const char* name, VT value)
{
    CExprSymbol* sp = GetSymbol(name);

    if (!sp) {
        // Add ...
        sp = new CExprSymbol(name, value);

        unsigned h = string_hash_function(name) % hash_table_size;
        sp->m_Next = hash_table[h];
        hash_table[h] = sp;
    }

    return sp;
}

inline
CExprParser::EOperator 
CExprParser::IfChar(
        char c, EOperator val, 
        EOperator val_def)
{
    if (m_Buf[m_Pos] == c) { 
        m_Pos += 1;
        return val;
    }

    return val_def;
}

inline
CExprParser::EOperator 
CExprParser::IfElseChar(
        char c1, EOperator val1, 
        char c2, EOperator val2, 
        EOperator val_def)
{
    if (m_Buf[m_Pos] == c1) { 
        m_Pos += 1;
        return val1;
    } else if (m_Buf[m_Pos] == c2) { 
        m_Pos += 1;
        return val2;
    }

    return val_def;
}

inline
CExprParser::EOperator 
CExprParser::IfLongest2ElseChar(
        char c1, char c2, 
        EOperator val_true_longest, 
        EOperator val_true, 
        EOperator val_false, 
        EOperator val_def)
{
    if (m_Buf[m_Pos] == c1) { 
        m_Pos += 1;
        return IfChar(c2, val_true_longest, val_true);
    } else if (m_Buf[m_Pos] == c2) { 
        m_Pos += 1;
        return val_false;
    }

    return val_def;
}

END_NCBI_SCOPE


