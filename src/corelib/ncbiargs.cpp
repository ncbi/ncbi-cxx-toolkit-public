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
 * Authors:  Denis Vakatov, Anton Butanayev
 *
 * File Description:
 *   Command-line arguments' processing:
 *      descriptions  -- CArgDescriptions,  CArgDesc
 *      parsed values -- CArgs,             CArgValue
 *      exceptions    -- CArgException, ARG_THROW()
 *      constraints   -- CArgAllow;  CArgAllow_{Strings,Integers,Int8s,Doubles}
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/error_codes.hpp>
#include <algorithm>

#if defined(NCBI_OS_MSWIN)
#  include <corelib/ncbi_os_mswin.hpp>
#  include <io.h> 
#  include <fcntl.h> 
#endif


#define NCBI_USE_ERRCODE_X   Corelib_Config


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  Include the private header
//

#define NCBIARGS__CPP
#include "ncbiargs_p.hpp"



/////////////////////////////////////////////////////////////////////////////
//  Constants
//

static const char* s_AutoHelp     = "h";
static const char* s_AutoHelpFull = "help";
static const char* s_AutoHelpXml  = "xmlhelp";
static const char* s_ExtraName    = "....";



/////////////////////////////////////////////////////////////////////////////

inline
string s_ArgExptMsg(const string& name, const string& what, const string& attr)
{
    return string("Argument \"") + (name.empty() ? s_ExtraName : name) +
        "\". " + what + (attr.empty() ? attr : ":  `" + attr + "'");
}

inline
void s_WriteEscapedStr(CNcbiOstream& out, const char* s)
{
    out << NStr::XmlEncode(s);
}

void s_WriteXmlLine(CNcbiOstream& out, const string& tag, const string& data)
{
    CStringUTF8 u( CUtf8::AsUTF8(data,eEncoding_Unknown) );
    out << "<"  << tag << ">";
    s_WriteEscapedStr(out, u.c_str());
    out << "</" << tag << ">" << endl;
}

// Allow autodetection among decimal and hex, but NOT octal, in case
// anyone's been relying on leading zeros being meaningless.
inline
Int8 s_StringToInt8(const string& value)
{
    try {
        return NStr::StringToInt8(value);
    } catch (CStringException&) {
        if (NStr::StartsWith(value, "0x", NStr::eNocase)) {
            return NStr::StringToInt8(value, 0, 16);
        } else {
            throw;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//  CArg_***::   classes representing various types of argument value
//
//    CArgValue
//       CArg_NoValue        : CArgValue
//       CArg_String         : CArgValue
//          CArg_Int8        : CArg_String
//             CArg_Integer  : CArg_Int8
//          CArg_Double      : CArg_String
//          CArg_Boolean     : CArg_String
//          CArg_InputFile   : CArg_String
//          CArg_OutputFile  : CArg_String
//          CArg_IOFile      : CArg_String
//


///////////////////////////////////////////////////////
//  CArgValue::

CArgValue::CArgValue(const string& name)
    : m_Name(name), m_Ordinal(0)
{
    if ( !CArgDescriptions::VerifyName(m_Name, true) ) {
        NCBI_THROW(CArgException,eInvalidArg,
            "Invalid argument name: " + m_Name);
    }
}


CArgValue::~CArgValue(void)
{
    return;
}


const CArgValue::TStringArray& CArgValue::GetStringList() const
{
    NCBI_THROW(CArgException, eInvalidArg,
        "Value lists not implemented for this argument: " + m_Name);
}


CArgValue::TStringArray& CArgValue::SetStringList()
{
    NCBI_THROW(CArgException, eInvalidArg,
        "Value lists not implemented for this argument: " + m_Name);
}


//  Overload the comparison operator -- to handle "CRef<CArgValue>" elements
//  in "CArgs::m_Args" stored as "set< CRef<CArgValue> >"
//
inline bool operator< (const CRef<CArgValue>& x, const CRef<CArgValue>& y)
{
    return x->GetName() < y->GetName();
}



///////////////////////////////////////////////////////
//  CArg_NoValue::

inline CArg_NoValue::CArg_NoValue(const string& name)
    : CArgValue(name)
{
    return;
}


bool CArg_NoValue::HasValue(void) const
{
    return false;
}


#define THROW_CArg_NoValue \
    NCBI_THROW(CArgException,eNoValue, s_ArgExptMsg(GetName(), \
        "The argument has no value", ""));

const string& CArg_NoValue::AsString    (void) const { THROW_CArg_NoValue; }
Int8          CArg_NoValue::AsInt8      (void) const { THROW_CArg_NoValue; }
int           CArg_NoValue::AsInteger   (void) const { THROW_CArg_NoValue; }
double        CArg_NoValue::AsDouble    (void) const { THROW_CArg_NoValue; }
bool          CArg_NoValue::AsBoolean   (void) const { THROW_CArg_NoValue; }
const CDir&   CArg_NoValue::AsDirectory (void) const { THROW_CArg_NoValue; }
CNcbiIstream& CArg_NoValue::AsInputFile (CArgValue::TFileFlags) const { THROW_CArg_NoValue; }
CNcbiOstream& CArg_NoValue::AsOutputFile(CArgValue::TFileFlags) const { THROW_CArg_NoValue; }
CNcbiIostream& CArg_NoValue::AsIOFile(CArgValue::TFileFlags) const { THROW_CArg_NoValue; }
void          CArg_NoValue::CloseFile   (void) const { THROW_CArg_NoValue; }


///////////////////////////////////////////////////////
//  CArg_ExcludedValue::

inline CArg_ExcludedValue::CArg_ExcludedValue(const string& name)
    : CArgValue(name)
{
    return;
}


bool CArg_ExcludedValue::HasValue(void) const
{
    return false;
}


#define THROW_CArg_ExcludedValue \
    NCBI_THROW(CArgException,eExcludedValue, s_ArgExptMsg(GetName(), \
        "The value is excluded by other arguments.", ""));

const string& CArg_ExcludedValue::AsString    (void) const { THROW_CArg_ExcludedValue; }
Int8          CArg_ExcludedValue::AsInt8      (void) const { THROW_CArg_ExcludedValue; }
int           CArg_ExcludedValue::AsInteger   (void) const { THROW_CArg_ExcludedValue; }
double        CArg_ExcludedValue::AsDouble    (void) const { THROW_CArg_ExcludedValue; }
bool          CArg_ExcludedValue::AsBoolean   (void) const { THROW_CArg_ExcludedValue; }
const CDir&   CArg_ExcludedValue::AsDirectory (void) const { THROW_CArg_ExcludedValue; }
CNcbiIstream& CArg_ExcludedValue::AsInputFile (CArgValue::TFileFlags) const { THROW_CArg_ExcludedValue; }
CNcbiOstream& CArg_ExcludedValue::AsOutputFile(CArgValue::TFileFlags) const { THROW_CArg_ExcludedValue; }
CNcbiIostream& CArg_ExcludedValue::AsIOFile(CArgValue::TFileFlags) const { THROW_CArg_ExcludedValue; }
void          CArg_ExcludedValue::CloseFile   (void) const { THROW_CArg_ExcludedValue; }


///////////////////////////////////////////////////////
//  CArg_String::

inline CArg_String::CArg_String(const string& name, const string& value)
    : CArgValue(name)
{
    m_StringList.push_back(value);
}


bool CArg_String::HasValue(void) const
{
    return !m_StringList.empty();
}


const string& CArg_String::AsString(void) const
{
    if (m_StringList.empty()) {
        return kEmptyStr;
    }
    return m_StringList[0];
}


const CArgValue::TStringArray& CArg_String::GetStringList() const
{
    return m_StringList;
}


CArgValue::TStringArray& CArg_String::SetStringList()
{
    return m_StringList;
}


Int8 CArg_String::AsInt8(void) const
{ NCBI_THROW(CArgException,eWrongCast,s_ArgExptMsg(GetName(),
    "Attempt to cast to a wrong (Int8) type", AsString()));}

int CArg_String::AsInteger(void) const
{ NCBI_THROW(CArgException,eWrongCast,s_ArgExptMsg(GetName(),
    "Attempt to cast to a wrong (Integer) type", AsString()));}

double CArg_String::AsDouble(void) const
{ NCBI_THROW(CArgException,eWrongCast,s_ArgExptMsg(GetName(),
    "Attempt to cast to a wrong (Double) type", AsString()));}

bool CArg_String::AsBoolean(void) const
{ NCBI_THROW(CArgException,eWrongCast,s_ArgExptMsg(GetName(),
    "Attempt to cast to a wrong (Boolean) type", AsString()));}

const CDir& CArg_String::AsDirectory (void) const
{ NCBI_THROW(CArgException,eWrongCast,s_ArgExptMsg(GetName(),
    "Attempt to cast to a wrong (CDir) type", AsString()));}

CNcbiIstream& CArg_String::AsInputFile(CArgValue::TFileFlags) const
{ NCBI_THROW(CArgException,eWrongCast,s_ArgExptMsg(GetName(),
    "Attempt to cast to a wrong (InputFile) type", AsString()));}

CNcbiOstream& CArg_String::AsOutputFile(CArgValue::TFileFlags) const
{ NCBI_THROW(CArgException,eWrongCast,s_ArgExptMsg(GetName(),
    "Attempt to cast to a wrong (OutputFile) type", AsString()));}

CNcbiIostream& CArg_String::AsIOFile(CArgValue::TFileFlags) const
{ NCBI_THROW(CArgException,eWrongCast,s_ArgExptMsg(GetName(),
    "Attempt to cast to a wrong (IOFile) type", AsString()));}

void CArg_String::CloseFile(void) const
{ NCBI_THROW(CArgException,eWrongCast,s_ArgExptMsg(GetName(),
    "Attempt to close an argument of non-file type", AsString()));}



///////////////////////////////////////////////////////
//  CArg_Int8::

inline CArg_Int8::CArg_Int8(const string& name, const string& value)
    : CArg_String(name, value)
{
    try {
        m_Integer = s_StringToInt8(value);
    } catch (CException& e) {
        NCBI_RETHROW(e, CArgException, eConvert, s_ArgExptMsg(GetName(),
            "Argument cannot be converted", value));
    }
}


Int8 CArg_Int8::AsInt8(void) const
{
    return m_Integer;
}



///////////////////////////////////////////////////////
//  CArg_Integer::

inline CArg_Integer::CArg_Integer(const string& name, const string& value)
    : CArg_Int8(name, value)
{
    if (m_Integer < kMin_Int  ||  m_Integer > kMax_Int) {
        NCBI_THROW(CArgException, eConvert, s_ArgExptMsg(GetName(),
            "Integer value is out of range", value));
    }
}


int CArg_Integer::AsInteger(void) const
{
    return static_cast<int> (m_Integer);
}



///////////////////////////////////////////////////////
//  CArg_Double::

inline CArg_Double::CArg_Double(const string& name, const string& value)
    : CArg_String(name, value)
{
    try {
        m_Double = NStr::StringToDouble(value, NStr::fDecimalPosixOrLocal);
    } catch (CException& e) {
        NCBI_RETHROW(e,CArgException,eConvert,
            s_ArgExptMsg(GetName(),"Argument cannot be converted",value));
    }
}


double CArg_Double::AsDouble(void) const
{
    return m_Double;
}



///////////////////////////////////////////////////////
//  CArg_Boolean::

inline CArg_Boolean::CArg_Boolean(const string& name, bool value)
    : CArg_String(name, NStr::BoolToString(value))
{
    m_Boolean = value;
}


inline CArg_Boolean::CArg_Boolean(const string& name, const string& value)
    : CArg_String(name, value)
{
    try {
        m_Boolean = NStr::StringToBool(value);
    } catch (CException& e) {
        NCBI_RETHROW(e,CArgException,eConvert, s_ArgExptMsg(GetName(),
            "Argument cannot be converted",value));
    }
}

bool CArg_Boolean::AsBoolean(void) const
{
    return m_Boolean;
}


///////////////////////////////////////////////////////
//  CArg_Flag

CArg_Flag::CArg_Flag(const string& name, bool value)
    : CArg_Boolean(name, value)
{
}
bool CArg_Flag::HasValue(void) const
{
    return AsBoolean();
}


///////////////////////////////////////////////////////
//  CArg_Dir::

CArg_Dir::CArg_Dir(const string& name, const string& value,
                CArgDescriptions::TFlags flags)
    : CArg_String(name, value), m_Dir(value), m_DescriptionFlags(flags)
{
}

CArg_Dir::~CArg_Dir(void)
{
}

const CDir&  CArg_Dir::AsDirectory() const
{
    if (m_DescriptionFlags & CArgDescriptions::fCreatePath) {
        m_Dir.CreatePath();
    }
    return m_Dir;
}



///////////////////////////////////////////////////////
//  CArg_Ios::

IOS_BASE::openmode CArg_Ios::IosMode(CArgValue::TFileFlags flags)
{
    IOS_BASE::openmode openmode = (IOS_BASE::openmode) 0;
    if (flags & CArgValue::fBinary) {
        openmode |= IOS_BASE::binary;
    }
    if (flags & CArgValue::fAppend) {
        openmode |= IOS_BASE::app;
    }
    if (flags & CArgValue::fTruncate) {
        openmode |= IOS_BASE::trunc;
    }
    return openmode;
}

CArg_Ios::CArg_Ios(
        const string& name, const string& value,
        CArgDescriptions::TFlags flags)
    : CArg_String(name, value),
      m_DescriptionFlags(0),
      m_CurrentFlags(0),
      m_Ios(NULL),
      m_DeleteFlag(true)
{
    m_DescriptionFlags = (CArgValue::TFileFlags)(
        (flags & CArgDescriptions::fFileFlags) & ~CArgDescriptions::fPreOpen);
}


CArg_Ios::~CArg_Ios(void)
{
    if (m_Ios  &&  m_DeleteFlag)
        delete m_Ios;
}

void CArg_Ios::x_Open(CArgValue::TFileFlags /*flags*/) const
{
    if ( !m_Ios ) {
        NCBI_THROW(CArgException,eNoFile, s_ArgExptMsg(GetName(),
            "File is not accessible",AsString()));
    }
}

bool CArg_Ios::x_CreatePath(TFileFlags flags) const
{
    CDirEntry entry( AsString() );
    if (flags & CArgDescriptions::fCreatePath) {
        CDir( entry.GetDir() ).CreatePath();;
    }
    return ((flags & CArgDescriptions::fNoCreate)==0 || entry.Exists());
}

CNcbiIstream&  CArg_Ios::AsInputFile(  TFileFlags flags) const
{
    CFastMutexGuard LOCK(m_AccessMutex);
    x_Open(flags);
    CNcbiIstream *str = dynamic_cast<CNcbiIstream*>(m_Ios);
    if (str) {
        return *str;
    }
    return CArg_String::AsInputFile(flags);
}

CNcbiOstream&  CArg_Ios::AsOutputFile( TFileFlags flags) const
{
    CFastMutexGuard LOCK(m_AccessMutex);
    x_Open(flags);
    CNcbiOstream *str = dynamic_cast<CNcbiOstream*>(m_Ios);
    if (str) {
        return *str;
    }
    return CArg_String::AsOutputFile(flags);
}

CNcbiIostream& CArg_Ios::AsIOFile(     TFileFlags flags) const
{
    CFastMutexGuard LOCK(m_AccessMutex);
    x_Open(flags);
    CNcbiIostream *str = dynamic_cast<CNcbiIostream*>(m_Ios);
    if (str) {
        return *str;
    }
    return CArg_String::AsIOFile(flags);
}


void CArg_Ios::CloseFile(void) const
{
    CFastMutexGuard LOCK(m_AccessMutex);
    if ( !m_Ios ) {
        ERR_POST_X(21, Warning << s_ArgExptMsg( GetName(),
            "CArg_Ios::CloseFile: File was not opened", AsString()));
        return;
    }

    if ( m_DeleteFlag ) {
        delete m_Ios;
        m_Ios = NULL;
    }
}



///////////////////////////////////////////////////////
//  CArg_InputFile::

CArg_InputFile::CArg_InputFile(const string& name, const string& value,
                               CArgDescriptions::TFlags flags)
    : CArg_Ios(name, value, flags)
{
    if ( flags & CArgDescriptions::fPreOpen ) {
        x_Open(m_DescriptionFlags);
    }
}

void CArg_InputFile::x_Open(CArgValue::TFileFlags flags) const
{
    CNcbiIfstream *fstrm = NULL;
    if ( m_Ios ) {
        if (flags != m_CurrentFlags && flags != 0) {
            if (m_DeleteFlag) {
                fstrm = dynamic_cast<CNcbiIfstream*>(m_Ios);
                _ASSERT(fstrm);
                fstrm->close();
            } else {
                m_Ios = NULL;
            }
        }
    }
    if ( m_Ios != NULL && fstrm == NULL) {
        return;
    }

    m_CurrentFlags = flags ? flags : m_DescriptionFlags;
    IOS_BASE::openmode mode = CArg_Ios::IosMode( m_CurrentFlags);
    m_DeleteFlag = false;

    if (AsString() == "-") {
#if defined(NCBI_OS_MSWIN)
        setmode(fileno(stdin), (mode & IOS_BASE::binary) ? O_BINARY : O_TEXT);
#endif
        m_Ios  = &cin;
    } else if ( !AsString().empty() ) {
        if (!fstrm) {
            fstrm = new CNcbiIfstream;
        }
        if (fstrm) {
            fstrm->open(AsString().c_str(),IOS_BASE::in | mode);
            if ( !fstrm->is_open() ) {
                delete fstrm;
                fstrm = NULL;
            } else {
                m_DeleteFlag = true;
            }
        }
        m_Ios = fstrm;
    }
    CArg_Ios::x_Open(flags);
}


///////////////////////////////////////////////////////
//  CArg_OutputFile::

CArg_OutputFile::CArg_OutputFile(
        const string& name, const string& value,
        CArgDescriptions::TFlags flags)
    : CArg_Ios(name, value, flags)
{
    if ( flags & CArgDescriptions::fPreOpen ) {
        x_Open(m_DescriptionFlags);
    }
}

void CArg_OutputFile::x_Open(CArgValue::TFileFlags flags) const
{
    CNcbiOfstream *fstrm = NULL;
    if ( m_Ios ) {
        if ((flags != m_CurrentFlags && flags != 0) ||
            (flags & fTruncate)) {
            if (m_DeleteFlag) {
                fstrm = dynamic_cast<CNcbiOfstream*>(m_Ios);
                _ASSERT(fstrm);
                fstrm->close();
            } else {
                m_Ios = NULL;
            }
        }
    }
    if ( m_Ios != NULL && fstrm == NULL) {
        return;
    }

    m_CurrentFlags = flags ? flags : m_DescriptionFlags;
    IOS_BASE::openmode mode = CArg_Ios::IosMode( m_CurrentFlags);
    m_DeleteFlag = false;

    if (AsString() == "-") {
#if defined(NCBI_OS_MSWIN)
        setmode(fileno(stdout), (mode & IOS_BASE::binary) ? O_BINARY : O_TEXT);
#endif
        m_Ios = &cout;
    } else if ( !AsString().empty() ) {
        if (!fstrm) {
            fstrm = new CNcbiOfstream;
        }
        if (fstrm) {
            if (x_CreatePath(m_CurrentFlags)) {
                fstrm->open(AsString().c_str(),IOS_BASE::out | mode);
            }
            if ( !fstrm->is_open() ) {
                delete fstrm;
                fstrm = NULL;
            } else {
                m_DeleteFlag = true;
            }
        }
        m_Ios = fstrm;
    }
    CArg_Ios::x_Open(flags);
}



///////////////////////////////////////////////////////
//  CArg_IOFile::

CArg_IOFile::CArg_IOFile(
        const string& name, const string& value,
        CArgDescriptions::TFlags flags)
    : CArg_Ios(name, value, flags)
{
    if ( flags & CArgDescriptions::fPreOpen ) {
        x_Open(m_DescriptionFlags);
    }
}

void CArg_IOFile::x_Open(CArgValue::TFileFlags flags) const
{
    CNcbiFstream *fstrm = NULL;
    if ( m_Ios ) {
        if ((flags != m_CurrentFlags && flags != 0) ||
            (flags & fTruncate)) {
            if (m_DeleteFlag) {
                fstrm = dynamic_cast<CNcbiFstream*>(m_Ios);
                _ASSERT(fstrm);
                fstrm->close();
            } else {
                m_Ios = NULL;
            }
        }
    }
    if ( m_Ios != NULL && fstrm == NULL) {
        return;
    }

    m_CurrentFlags = flags ? flags : m_DescriptionFlags;
    IOS_BASE::openmode mode = CArg_Ios::IosMode( m_CurrentFlags);
    m_DeleteFlag = false;

    if ( !AsString().empty() ) {
        if (!fstrm) {
            fstrm = new CNcbiFstream;
        }
        if (fstrm) {
            if (x_CreatePath(m_CurrentFlags)) {
                fstrm->open(AsString().c_str(),IOS_BASE::in | IOS_BASE::out | mode);
            }
            if ( !fstrm->is_open() ) {
                delete fstrm;
                fstrm = NULL;
            } else {
                m_DeleteFlag = true;
            }
        }
        m_Ios = fstrm;
    }
    CArg_Ios::x_Open(flags);
}


/////////////////////////////////////////////////////////////////////////////
//  Aux.functions to figure out various arg. features
//
//    s_IsPositional(arg)
//    s_IsOptional(arg)
//    s_IsFlag(arg)
//

class CArgDesc;

inline bool s_IsKey(const CArgDesc& arg)
{
    return (dynamic_cast<const CArgDescSynopsis*> (&arg) != 0);
}


inline bool s_IsPositional(const CArgDesc& arg)
{
    return dynamic_cast<const CArgDesc_Pos*> (&arg) &&  !s_IsKey(arg);
}


inline bool s_IsOpening(const CArgDesc& arg)
{
    return dynamic_cast<const CArgDesc_Opening*> (&arg) != NULL;
}


inline bool s_IsOptional(const CArgDesc& arg)
{
    return (dynamic_cast<const CArgDescOptional*> (&arg) != 0);
}


inline bool s_IsFlag(const CArgDesc& arg)
{
    return (dynamic_cast<const CArgDesc_Flag*> (&arg) != 0);
}


inline bool s_IsAlias(const CArgDesc& arg)
{
    return (dynamic_cast<const CArgDesc_Alias*> (&arg) != 0);
}


/////////////////////////////////////////////////////////////////////////////
//  CArgDesc***::   abstract base classes for argument descriptions
//
//    CArgDesc
//
//    CArgDescMandatory  : CArgDesc
//    CArgDescOptional   : virtual CArgDescMandatory
//    CArgDescDefault    : virtual CArgDescOptional
//
//    CArgDescSynopsis
//


///////////////////////////////////////////////////////
//  CArgDesc::

CArgDesc::CArgDesc(const string& name, const string& comment)
    : m_Name(name), m_Comment(comment)
{
    if ( !CArgDescriptions::VerifyName(m_Name) ) {
        NCBI_THROW(CArgException,eInvalidArg,
            "Invalid argument name: " + m_Name);
    }
}


CArgDesc::~CArgDesc(void)
{
    return;
}


void CArgDesc::VerifyDefault(void) const
{
    return;
}


void CArgDesc::SetConstraint(CArgAllow*   constraint,
                             CArgDescriptions::EConstraintNegate)
{
    NCBI_THROW(CArgException, eConstraint, s_ArgExptMsg(GetName(),
        "No-value arguments may not be constrained",
        constraint ? constraint->GetUsage() : kEmptyStr));
}


const CArgAllow* CArgDesc::GetConstraint(void) const
{
    return 0;
}


string CArgDesc::GetUsageConstraint(void) const
{
    const CArgAllow* constraint = GetConstraint();
    if (!constraint)
        return kEmptyStr;
    string usage;
    if (IsConstraintInverted()) {
        usage = " NOT ";
    }
    usage += constraint->GetUsage();
    return usage;
}


//  Overload the comparison operator -- to handle "AutoPtr<CArgDesc>" elements
//  in "CArgs::m_Args" stored as "set< AutoPtr<CArgDesc> >"
//
inline bool operator< (const AutoPtr<CArgDesc>& x, const AutoPtr<CArgDesc>& y)
{
    return x->GetName() < y->GetName();
}

string CArgDesc::PrintXml(CNcbiOstream& out) const
// note: I open 'role' tag, but do not close it here
{
    string role;
    if (s_IsKey(*this)) {
        role = "key";
    } else if (s_IsOpening(*this)) {
        role = "opening";
    } else if (s_IsPositional(*this)) {
        role = GetName().empty() ? "extra" : "positional";
    } else if (s_IsFlag(*this)) {
        role = "flag";
    } else {
        role = "UNKNOWN";
    }

// name
    out << "<" << role << " name=\"";
    string name = CUtf8::AsUTF8(GetName(),eEncoding_Unknown);
    s_WriteEscapedStr(out,name.c_str());
    out  << "\"";
// type
    const CArgDescMandatory* am =
        dynamic_cast<const CArgDescMandatory*>(this);
    if (am) {
        out << " type=\"" << CArgDescriptions::GetTypeName(am->GetType()) << "\"";
    }
// use (Flags are always optional)
    if (s_IsOptional(*this) || s_IsFlag(*this)) {
        out << " optional=\"true\"";
    }
    out << ">" << endl;

    s_WriteXmlLine(out, "description", GetComment());
    size_t group = GetGroup();
    if (group != 0) {
        s_WriteXmlLine(out, "group", NStr::SizetToString(group));
    }
    const CArgDescSynopsis* syn = 
        dynamic_cast<const CArgDescSynopsis*>(this);
    if (syn && !syn->GetSynopsis().empty()) {
        s_WriteXmlLine(out, "synopsis", syn->GetSynopsis());
    }

// constraint
    string constraint = CUtf8::AsUTF8(GetUsageConstraint(),eEncoding_Unknown);
    if (!constraint.empty()) {
        out << "<" << "constraint";
        if (IsConstraintInverted()) {
            out << " inverted=\"true\"";
        }
        out << ">" << endl;
        s_WriteXmlLine( out, "description", constraint.c_str());
        GetConstraint()->PrintUsageXml(out);
        out << "</" << "constraint" << ">" << endl;
    }

// flags
    CArgDescriptions::TFlags flags = GetFlags();
    if (flags != 0) {
        out << "<" << "flags" << ">";
        if (flags & CArgDescriptions::fPreOpen) {
            out << "<" << "preOpen" << "/>";
        }
        if (flags & CArgDescriptions::fBinary) {
            out << "<" << "binary" << "/>";
        }
        if (flags & CArgDescriptions::fAppend) {
            out << "<" << "append" << "/>";
        }
        if (flags & CArgDescriptions::fTruncate) {
            out << "<" << "truncate" << "/>";
        }
        if (flags & CArgDescriptions::fNoCreate) {
            out << "<" << "noCreate" << "/>";
        }
        if (flags & CArgDescriptions::fAllowMultiple) {
            out << "<" << "allowMultiple" << "/>";
        }
        if (flags & CArgDescriptions::fIgnoreInvalidValue) {
            out << "<" << "ignoreInvalidValue" << "/>";
        }
        if (flags & CArgDescriptions::fWarnOnInvalidValue) {
            out << "<" << "warnOnInvalidValue" << "/>";
        }
        if (flags & CArgDescriptions::fOptionalSeparator) {
            out << "<" << "optionalSeparator" << "/>";
        }
        if (flags & CArgDescriptions::fMandatorySeparator) {
            out << "<" << "mandatorySeparator" << "/>";
        }
        if (flags & CArgDescriptions::fCreatePath) {
            out << "<" << "createPath" << "/>";
        }
        if (flags & CArgDescriptions::fOptionalSeparatorAllowConflict) {
            out << "<" << "optionalSeparatorAllowConflict" << "/>";
        }
        out << "</" << "flags" << ">" << endl;
    }
    const CArgDescDefault* def = dynamic_cast<const CArgDescDefault*>(this);
    if (def) {
        s_WriteXmlLine(     out, "default", def->GetDefaultValue());
    } else if (s_IsFlag(*this)) {
        const CArgDesc_Flag* fl = dynamic_cast<const CArgDesc_Flag*>(this);
        if (fl && !fl->GetSetValue()) {
            s_WriteXmlLine( out, "setvalue", "false");
        }
    }
    return role;
}


///////////////////////////////////////////////////////
//  CArgDescMandatory::

CArgDescMandatory::CArgDescMandatory(const string&            name,
                                     const string&            comment,
                                     CArgDescriptions::EType  type,
                                     CArgDescriptions::TFlags flags)
    : CArgDesc(name, comment),
      m_Type(type), m_Flags(flags),
      m_NegateConstraint(CArgDescriptions::eConstraint)
{
    // verify if "flags" "type" are matching
    switch ( type ) {
    case CArgDescriptions::eBoolean:
    case CArgDescriptions::eOutputFile:
    case CArgDescriptions::eIOFile:
        return;
    case CArgDescriptions::eInputFile:
        if((flags &
            (CArgDescriptions::fAllowMultiple | CArgDescriptions::fAppend | CArgDescriptions::fTruncate)) == 0)
            return;
        break;
    case CArgDescriptions::k_EType_Size:
        _TROUBLE;
        NCBI_THROW(CArgException, eArgType, s_ArgExptMsg(GetName(),
            "Invalid argument type", "k_EType_Size"));
        /*NOTREACHED*/
        break;
    case CArgDescriptions::eDirectory:
        if ( (flags & ~CArgDescriptions::fCreatePath) == 0 )
            return;
        break;
    default:
        if ( (flags & CArgDescriptions::fFileFlags) == 0 )
            return;
    }

    NCBI_THROW(CArgException, eArgType,
               s_ArgExptMsg(GetName(),
                            "Argument type/flags mismatch",
                            string("(type=") +
                            CArgDescriptions::GetTypeName(type) +
                            ", flags=" + NStr::UIntToString(flags) + ")"));
}


CArgDescMandatory::~CArgDescMandatory(void)
{
    return;
}


string CArgDescMandatory::GetUsageCommentAttr(void) const
{
    // Print type name
    string str = CArgDescriptions::GetTypeName(GetType());

    // Print constraint info, if any
    string constr = GetUsageConstraint();
    if ( !constr.empty() ) {
        str += ", ";
        str += constr;
    }
    return str;
}


CArgValue* CArgDescMandatory::ProcessArgument(const string& value) const
{
    // Process according to the argument type
    CRef<CArgValue> arg_value;
    switch ( GetType() ) {
    case CArgDescriptions::eString:
        arg_value = new CArg_String(GetName(), value);
        break;
    case CArgDescriptions::eBoolean:
        arg_value = new CArg_Boolean(GetName(), value);
        break;
    case CArgDescriptions::eInt8:
        arg_value = new CArg_Int8(GetName(), value);
        break;
    case CArgDescriptions::eInteger:
        arg_value = new CArg_Integer(GetName(), value);
        break;
    case CArgDescriptions::eDouble:
        arg_value = new CArg_Double(GetName(), value);
        break;
    case CArgDescriptions::eInputFile: {
        arg_value = new CArg_InputFile(GetName(), value, GetFlags());
        break;
    }
    case CArgDescriptions::eOutputFile: {
        arg_value = new CArg_OutputFile(GetName(), value, GetFlags());
        break;
    }
    case CArgDescriptions::eIOFile: {
        arg_value = new CArg_IOFile(GetName(), value, GetFlags());
        break;
    }
    case CArgDescriptions::eDirectory: {
        arg_value = new CArg_Dir(GetName(), value, GetFlags());
        break;
    }
    case CArgDescriptions::k_EType_Size: {
        _TROUBLE;
        NCBI_THROW(CArgException, eArgType, s_ArgExptMsg(GetName(),
            "Unknown argument type", NStr::IntToString((int)GetType())));
    }
    } /* switch GetType() */


    // Check against additional (user-defined) constraints, if any imposed
    if ( m_Constraint ) {
        bool err = false;
        try {
            bool check = m_Constraint->Verify(value);
            if (m_NegateConstraint == CArgDescriptions::eConstraintInvert) {
                err = check;
            } else {
                err = !check;
            }

        } catch (...) {
            err = true;
        }

        if (err) {
            string err_msg;
            if (m_NegateConstraint == CArgDescriptions::eConstraintInvert) {
                err_msg = "Illegal value, unexpected ";
            } else {
                err_msg = "Illegal value, expected ";
            }
            NCBI_THROW(CArgException, eConstraint, s_ArgExptMsg(GetName(),
                       err_msg + m_Constraint->GetUsage(),value));
        }
    }

    return arg_value.Release();
}


CArgValue* CArgDescMandatory::ProcessDefault(void) const
{
    NCBI_THROW(CArgException, eNoArg,
               s_ArgExptMsg(GetName(), "Mandatory value is missing",
                            GetUsageCommentAttr()));
}


void CArgDescMandatory::SetConstraint
(CArgAllow*                          constraint,
 CArgDescriptions::EConstraintNegate negate)
{
    m_Constraint       = constraint;
    m_NegateConstraint = negate;
}


const CArgAllow* CArgDescMandatory::GetConstraint(void) const
{
    return m_Constraint;
}


bool CArgDescMandatory::IsConstraintInverted() const
{
    return m_NegateConstraint == CArgDescriptions::eConstraintInvert;
}


///////////////////////////////////////////////////////
//  CArgDescOptional::


CArgDescOptional::CArgDescOptional(const string&            name,
                                   const string&            comment,
                                   CArgDescriptions::EType  type,
                                   CArgDescriptions::TFlags flags)
    : CArgDescMandatory(name, comment, type, flags),
      m_Group(0)
{
    return;
}


CArgDescOptional::~CArgDescOptional(void)
{
    return;
}


CArgValue* CArgDescOptional::ProcessDefault(void) const
{
    return new CArg_NoValue(GetName());
}




///////////////////////////////////////////////////////
//  CArgDescDefault::


CArgDescDefault::CArgDescDefault(const string&            name,
                                 const string&            comment,
                                 CArgDescriptions::EType  type,
                                 CArgDescriptions::TFlags flags,
                                 const string&            default_value,
                                 const string&            env_var)
    : CArgDescMandatory(name, comment, type, flags),
      CArgDescOptional(name, comment, type, flags),
      m_DefaultValue(default_value), m_EnvVar(env_var)
{
    return;
}


CArgDescDefault::~CArgDescDefault(void)
{
    return;
}

const string& CArgDescDefault::GetDefaultValue(void) const
{
    if (!m_EnvVar.empty() && CNcbiApplication::Instance()) {
        const string& value =
            CNcbiApplication::Instance()->GetEnvironment().Get(m_EnvVar);
        if (!value.empty()) {
            return value;
        }
    }
    return m_DefaultValue;
}

CArgValue* CArgDescDefault::ProcessDefault(void) const
{
    return ProcessArgument(GetDefaultValue());
}


void CArgDescDefault::VerifyDefault(void) const
{
    if (GetType() == CArgDescriptions::eInputFile  ||
        GetType() == CArgDescriptions::eOutputFile ||
        GetType() == CArgDescriptions::eIOFile ||
        GetType() == CArgDescriptions::eDirectory) {
        return;
    }

    // Process, then immediately delete
    CRef<CArgValue> arg_value(ProcessArgument(GetDefaultValue()));
}


///////////////////////////////////////////////////////
//  CArgDescSynopsis::


CArgDescSynopsis::CArgDescSynopsis(const string& synopsis)
    : m_Synopsis(synopsis)
{
    for (string::const_iterator it = m_Synopsis.begin();
         it != m_Synopsis.end();  ++it) {
        if (*it != '_'  &&  !isalnum((unsigned char)(*it))) {
            NCBI_THROW(CArgException,eSynopsis,
                "Argument synopsis must be alphanumeric: "+ m_Synopsis);
        }
    }
}



/////////////////////////////////////////////////////////////////////////////
//  CArgDesc_***::   classes for argument descriptions
//    CArgDesc_Flag    : CArgDesc
//
//    CArgDesc_Key     : virtual CArgDescMandatory
//    CArgDesc_KeyOpt  : CArgDesc_Key, virtual CArgDescOptional
//    CArgDesc_KeyDef  : CArgDesc_Key, CArgDescDefault
//
//    CArgDesc_Pos     : virtual CArgDescMandatory
//    CArgDesc_PosOpt  : CArgDesc_Pos, virtual CArgDescOptional
//    CArgDesc_PosDef  : CArgDesc_Pos, CArgDescDefault
//


///////////////////////////////////////////////////////
//  CArgDesc_Flag::


CArgDesc_Flag::CArgDesc_Flag(const string& name,
                             const string& comment,
                             bool  set_value)
    : CArgDesc(name, comment),
      m_Group(0),
      m_SetValue(set_value)
{
    return;
}


CArgDesc_Flag::~CArgDesc_Flag(void)
{
    return;
}


string CArgDesc_Flag::GetUsageSynopsis(bool /*name_only*/) const
{
    return "-" + GetName();
}


string CArgDesc_Flag::GetUsageCommentAttr(void) const
{
    return kEmptyStr;
}


CArgValue* CArgDesc_Flag::ProcessArgument(const string& /*value*/) const
{
    return new CArg_Flag(GetName(), m_SetValue);
}


CArgValue* CArgDesc_Flag::ProcessDefault(void) const
{
    return new CArg_Flag(GetName(), !m_SetValue);
}



///////////////////////////////////////////////////////
//  CArgDesc_Pos::


CArgDesc_Pos::CArgDesc_Pos(const string&            name,
                           const string&            comment,
                           CArgDescriptions::EType  type,
                           CArgDescriptions::TFlags flags)
    : CArgDescMandatory(name, comment, type, flags)
{
    return;
}


CArgDesc_Pos::~CArgDesc_Pos(void)
{
    return;
}


string CArgDesc_Pos::GetUsageSynopsis(bool /*name_only*/) const
{
    return GetName().empty() ? s_ExtraName : GetName();
}


///////////////////////////////////////////////////////
//  CArgDesc_Opening::


CArgDesc_Opening::CArgDesc_Opening(const string&            name,
                           const string&            comment,
                           CArgDescriptions::EType  type,
                           CArgDescriptions::TFlags flags)
    : CArgDescMandatory(name, comment, type, flags)
{
    return;
}


CArgDesc_Opening::~CArgDesc_Opening(void)
{
    return;
}


string CArgDesc_Opening::GetUsageSynopsis(bool /*name_only*/) const
{
    return GetName().empty() ? s_ExtraName : GetName();
}



///////////////////////////////////////////////////////
//  CArgDesc_PosOpt::


CArgDesc_PosOpt::CArgDesc_PosOpt(const string&            name,
                                 const string&            comment,
                                 CArgDescriptions::EType  type,
                                 CArgDescriptions::TFlags flags)
    : CArgDescMandatory (name, comment, type, flags),
      CArgDescOptional  (name, comment, type, flags),
      CArgDesc_Pos      (name, comment, type, flags)
{
    return;
}


CArgDesc_PosOpt::~CArgDesc_PosOpt(void)
{
    return;
}



///////////////////////////////////////////////////////
//  CArgDesc_PosDef::


CArgDesc_PosDef::CArgDesc_PosDef(const string&            name,
                                 const string&            comment,
                                 CArgDescriptions::EType  type,
                                 CArgDescriptions::TFlags flags,
                                 const string&            default_value,
                                 const string&            env_var)
    : CArgDescMandatory (name, comment, type, flags),
      CArgDescOptional  (name, comment, type, flags),
      CArgDescDefault   (name, comment, type, flags, default_value, env_var),
      CArgDesc_PosOpt   (name, comment, type, flags)
{
    return;
}


CArgDesc_PosDef::~CArgDesc_PosDef(void)
{
    return;
}



///////////////////////////////////////////////////////
//  CArgDesc_Key::


CArgDesc_Key::CArgDesc_Key(const string&            name,
                           const string&            comment,
                           CArgDescriptions::EType  type,
                           CArgDescriptions::TFlags flags,
                           const string&            synopsis)
    : CArgDescMandatory(name, comment, type, flags),
      CArgDesc_Pos     (name, comment, type, flags),
      CArgDescSynopsis(synopsis)
{
    return;
}


CArgDesc_Key::~CArgDesc_Key(void)
{
    return;
}


inline string s_KeyUsageSynopsis(const string& name, const string& synopsis,
                                 bool name_only,
                                 CArgDescriptions::TFlags flags)
{
    if ( name_only ) {
        return '-' + name;
    } else {
        char separator =
            (flags & CArgDescriptions::fMandatorySeparator) ? '=' : ' ';
        return '-' + name + separator + synopsis;
    }
}


string CArgDesc_Key::GetUsageSynopsis(bool name_only) const
{
    return s_KeyUsageSynopsis(GetName(), GetSynopsis(), name_only, GetFlags());
}



///////////////////////////////////////////////////////
//  CArgDesc_KeyOpt::


CArgDesc_KeyOpt::CArgDesc_KeyOpt(const string&            name,
                                 const string&            comment,
                                 CArgDescriptions::EType  type,
                                 CArgDescriptions::TFlags flags,
                                 const string&            synopsis)
    : CArgDescMandatory(name, comment, type, flags),
      CArgDescOptional (name, comment, type, flags),
      CArgDesc_PosOpt  (name, comment, type, flags),
      CArgDescSynopsis(synopsis)
{
    return;
}


CArgDesc_KeyOpt::~CArgDesc_KeyOpt(void)
{
    return;
}


string CArgDesc_KeyOpt::GetUsageSynopsis(bool name_only) const
{
    return s_KeyUsageSynopsis(GetName(), GetSynopsis(), name_only, GetFlags());
}



///////////////////////////////////////////////////////
//  CArgDesc_KeyDef::


CArgDesc_KeyDef::CArgDesc_KeyDef(const string&            name,
                                 const string&            comment,
                                 CArgDescriptions::EType  type,
                                 CArgDescriptions::TFlags flags,
                                 const string&            synopsis,
                                 const string&            default_value,
                                 const string&            env_var)
    : CArgDescMandatory(name, comment, type, flags),
      CArgDescOptional (name, comment, type, flags),
      CArgDesc_PosDef  (name, comment, type, flags, default_value, env_var),
      CArgDescSynopsis(synopsis)
{
    return;
}


CArgDesc_KeyDef::~CArgDesc_KeyDef(void)
{
    return;
}


string CArgDesc_KeyDef::GetUsageSynopsis(bool name_only) const
{
    return s_KeyUsageSynopsis(GetName(), GetSynopsis(), name_only, GetFlags());
}


///////////////////////////////////////////////////////
//  CArgDesc_Alias::

CArgDesc_Alias::CArgDesc_Alias(const string& alias,
                               const string& arg_name,
                               const string& comment)
    : CArgDesc(alias, comment),
      m_ArgName(arg_name),
      m_NegativeFlag(false)
{
}


CArgDesc_Alias::~CArgDesc_Alias(void)
{
}


const string& CArgDesc_Alias::GetAliasedName(void) const
{
    return m_ArgName;
}


string CArgDesc_Alias::GetUsageSynopsis(bool /*name_only*/) const
{
    return kEmptyStr;
}


string CArgDesc_Alias::GetUsageCommentAttr(void) const
{
    return kEmptyStr;
}

    
CArgValue* CArgDesc_Alias::ProcessArgument(const string& /*value*/) const
{
    return new CArg_NoValue(GetName());
}


CArgValue* CArgDesc_Alias::ProcessDefault(void) const
{
    return new CArg_NoValue(GetName());
}


/////////////////////////////////////////////////////////////////////////////
//  CArgs::
//


CArgs::CArgs(void)
{
    m_nExtra = 0;
}


CArgs::~CArgs(void)
{
    return;
}


static string s_ComposeNameExtra(size_t idx)
{
    return '#' + NStr::UInt8ToString(idx);
}


inline bool s_IsArgNameChar(char c)
{
    return isalnum(c)  ||  c == '_'  ||  c == '-';
}


CArgs::TArgsCI CArgs::x_Find(const string& name) const
{
    CArgs::TArgsCI arg = m_Args.find(CRef<CArgValue> (new CArg_NoValue(name)));
    if (arg != m_Args.end() || name.empty() || name[0] == '-'  ||
        !s_IsArgNameChar(name[0])) {
        return arg;
    }
    return m_Args.find(CRef<CArgValue> (new CArg_NoValue("-" + name)));
}

CArgs::TArgsI CArgs::x_Find(const string& name)
{
    CArgs::TArgsI arg = m_Args.find(CRef<CArgValue> (new CArg_NoValue(name)));
    if (arg != m_Args.end() || name.empty() || name[0] == '-'  ||
        !s_IsArgNameChar(name[0])) {
        return arg;
    }
    return m_Args.find(CRef<CArgValue> (new CArg_NoValue("-" + name)));
}


bool CArgs::Exist(const string& name) const
{
    return (x_Find(name) != m_Args.end());
}


const CArgValue& CArgs::operator[] (const string& name) const
{
    TArgsCI arg = x_Find(name);
    if (arg == m_Args.end()) {
        // Special diagnostics for "extra" args
        if (!name.empty()  &&  name[0] == '#') {
            size_t idx;
            try {
                idx = NStr::StringToUInt(name.c_str() + 1);
            } catch (...) {
                idx = kMax_UInt;
            }
            if (idx == kMax_UInt) {
                NCBI_THROW(CArgException, eInvalidArg,
                           "Asked for an argument with invalid name: \"" +
                           name + "\"");
            }
            if (m_nExtra == 0) {
                NCBI_THROW(CArgException, eInvalidArg,
                           "No \"extra\" (unnamed positional) arguments "
                           "provided, cannot Get: " + s_ComposeNameExtra(idx));
            }
            if (idx == 0  ||  idx >= m_nExtra) {
                NCBI_THROW(CArgException, eInvalidArg,
                           "\"Extra\" (unnamed positional) arg is "
                           "out-of-range (#1.." + s_ComposeNameExtra(m_nExtra)
                           + "): " + s_ComposeNameExtra(idx));
            }
        }

        // Diagnostics for all other argument classes
        NCBI_THROW(CArgException, eInvalidArg,
                   "Unknown argument requested: \"" + name + "\"");
    }

    // Found arg with name "name"
    return **arg;
}


const CArgValue& CArgs::operator[] (size_t idx) const
{
    return (*this)[s_ComposeNameExtra(idx)];
}

vector< CRef<CArgValue> > CArgs::GetAll(void) const
{
    vector< CRef<CArgValue> > res;
    ITERATE( TArgs, a, m_Args) {
        if ((**a).HasValue()) {
            res.push_back( *a );
        }
    }
    return res;
}


string& CArgs::Print(string& str) const
{
    for (TArgsCI arg = m_Args.begin();  arg != m_Args.end();  ++arg) {
        // Arg. name
        const string& arg_name = (*arg)->GetName();
        str += arg_name;

        // Arg. value, if any
        const CArgValue& arg_value = (*this)[arg_name];
        if ( arg_value ) {
            str += " = `";
            string tmp;
            try {
                tmp = NStr::Join( arg_value.GetStringList(), " "); 
            } catch (...) {
                tmp = arg_value.AsString();
            }
            str += tmp;
            str += "'\n";
        } else {
            str += ":  <not assigned>\n";
        }
    }
    return str;
}


void CArgs::Remove(const string& name)
{
    CArgs::TArgsI it =  m_Args.find(CRef<CArgValue> (new CArg_NoValue(name)));
    m_Args.erase(it);
}


void CArgs::Reset(void)
{
    m_nExtra = 0;
    m_Args.clear();
}


void CArgs::Add(CArgValue* arg, bool update, bool add_value)
{
    // special case:  add an "extra" arg (generate virtual name for it)
    bool is_extra = false;
    if ( arg->GetName().empty() ) {
        arg->m_Name = s_ComposeNameExtra(m_nExtra + 1);
        is_extra = true;
    }

    // check-up
    _ASSERT(CArgDescriptions::VerifyName(arg->GetName(), true));
    CArgs::TArgsI arg_it = x_Find(arg->GetName());
    if ( arg_it !=  m_Args.end()) {
        if (update) {
            Remove(arg->GetName());
        } else {
            if (add_value) {
                const string& v = arg->AsString();
                CRef<CArgValue> av = *arg_it;
                av->SetStringList().push_back(v);
            } else {
                NCBI_THROW(CArgException,eSynopsis,
                   "Argument with this name is defined already: " 
                   + arg->GetName());
            }
        }
    }

    // add
    arg->SetOrdinalPosition(m_Args.size()+1);
    m_Args.insert(CRef<CArgValue>(arg));

    if ( is_extra ) {
        m_nExtra++;
    }
}


bool CArgs::IsEmpty(void) const
{
    return m_Args.empty();
}


///////////////////////////////////////////////////////
//  CArgErrorHandler::

CArgValue* CArgErrorHandler::HandleError(const CArgDesc& arg_desc,
                                         const string& value) const
{
    if ((arg_desc.GetFlags() & CArgDescriptions::fIgnoreInvalidValue) == 0) {
        // Re-process invalid value to throw the same exception
        arg_desc.ProcessArgument(value);
        // Should never get past ProcessArgument()
    }
    if ((arg_desc.GetFlags() & CArgDescriptions::fWarnOnInvalidValue) == 0) {
        ERR_POST_X(22, Warning << "Invalid value " << value <<
            " for argument " << arg_desc.GetName() <<
            " - argument will be ignored.");
    }
    // return 0 to ignore the argument
    return 0;
}


///////////////////////////////////////////////////////
//  CArgDescriptions::
//


CArgDescriptions::CArgDescriptions(bool              auto_help,
                                   CArgErrorHandler* err_handler)
    : m_ArgsType(eRegularArgs),
      m_nExtra(0),
      m_nExtraOpt(0),
      m_CurrentGroup(0),
      m_PositionalMode(ePositionalMode_Strict),
      m_MiscFlags(fMisc_Default),
      m_AutoHelp(auto_help),
      m_ErrorHandler(err_handler)
{
    if ( !m_ErrorHandler ) {
        // Use default error handler
        m_ErrorHandler.Reset(new CArgErrorHandler);
    }

    SetUsageContext("NCBI_PROGRAM", kEmptyStr);
    m_ArgGroups.push_back(kEmptyStr); // create default group #0
    if ( m_AutoHelp ) {
        AddFlag(s_AutoHelp,
                "Print USAGE and DESCRIPTION;  ignore all other parameters");
    }
    AddFlag(s_AutoHelpFull,
            "Print USAGE, DESCRIPTION and ARGUMENTS;"
            " ignore all other parameters");
    AddFlag(s_AutoHelpXml,
            "Print USAGE, DESCRIPTION and ARGUMENTS in XML format;"
            " ignore all other parameters");
}


CArgDescriptions::~CArgDescriptions(void)
{
    return;
}


void CArgDescriptions::SetArgsType(EArgSetType args_type)
{
    m_ArgsType = args_type;

    // Run args check for a CGI application
    if (m_ArgsType == eCgiArgs) {
        // Must have no named positional arguments
        if ( !m_PosArgs.empty() ) {
            NCBI_THROW(CArgException, eInvalidArg,
                       "CGI application cannot have positional arguments, "
                       "name of the offending argument: '"
                       + *m_PosArgs.begin() + "'.");
        }

        // Must have no flag arguments
        ITERATE (TArgs, it, m_Args) {
            const CArgDesc& arg = **it;
            if ( s_IsFlag(arg) ) {
                const string& name = arg.GetName();

                if ((m_AutoHelp && name == s_AutoHelp) ||
                    name == s_AutoHelpFull ||
                    name == s_AutoHelpXml)  // help
                    continue;

                NCBI_THROW(CArgException, eInvalidArg,
                           "CGI application cannot have flag arguments, "
                           "name of the offending flag: '" + name + "'.");
            }
        }

        // Must have no unnamed positional arguments
        if (m_nExtra  ||  m_nExtraOpt) {
            NCBI_THROW(CArgException, eInvalidArg,
                       "CGI application cannot have unnamed positional "
                       "arguments.");
        }
    }
}


const char* CArgDescriptions::GetTypeName(EType type)
{
    static const char* s_TypeName[k_EType_Size] = {
        "String",
        "Boolean",
        "Int8",
        "Integer",
        "Real",
        "File_In",
        "File_Out",
        "File_IO",
        "Directory"
    };

    if (type == k_EType_Size) {
        _TROUBLE;
        NCBI_THROW(CArgException, eArgType,
                   "Invalid argument type: k_EType_Size");
    }
    return s_TypeName[(int) type];
}


void CArgDescriptions::AddKey
(const string& name,
 const string& synopsis,
 const string& comment,
 EType         type,
 TFlags        flags)
{
    auto_ptr<CArgDesc_Key> arg(new CArgDesc_Key(name,
        comment, type, flags, synopsis));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddOptionalKey
(const string& name,
 const string& synopsis,
 const string& comment,
 EType         type,
 TFlags        flags)
{
    auto_ptr<CArgDesc_KeyOpt> arg(new CArgDesc_KeyOpt(name,
        comment, type, flags, synopsis));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddDefaultKey
(const string& name,
 const string& synopsis,
 const string& comment,
 EType         type,
 const string& default_value,
 TFlags        flags,
 const string& env_var)
{
    auto_ptr<CArgDesc_KeyDef> arg(new CArgDesc_KeyDef(name,
        comment, type, flags, synopsis, default_value, env_var));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddFlag
(const string& name,
 const string& comment,
 bool          set_value)
{
    auto_ptr<CArgDesc_Flag> arg(new CArgDesc_Flag(name, comment, set_value));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddPositional
(const string& name,
 const string& comment,
 EType         type,
 TFlags        flags)
{
    auto_ptr<CArgDesc_Pos> arg(new CArgDesc_Pos(name, comment, type, flags));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddOpening
(const string& name,
 const string& comment,
 EType         type,
 TFlags        flags)
{
    auto_ptr<CArgDesc_Opening> arg(new CArgDesc_Opening(name, comment, type, flags));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddOptionalPositional
(const string& name,
 const string& comment,
 EType         type,
 TFlags        flags)
{
    auto_ptr<CArgDesc_PosOpt> arg
        (new CArgDesc_PosOpt(name, comment, type, flags));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddDefaultPositional
(const string& name,
 const string& comment,
 EType         type,
 const string& default_value,
 TFlags        flags,
 const string& env_var)
{
    auto_ptr<CArgDesc_PosDef> arg(new CArgDesc_PosDef(name,
        comment, type, flags, default_value, env_var));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddExtra
(unsigned      n_mandatory,
 unsigned      n_optional,
 const string& comment,
 EType         type,
 TFlags        flags)
{
    if (!n_mandatory  &&  !n_optional) {
        NCBI_THROW(CArgException,eSynopsis,
            "Number of extra arguments cannot be zero");
    }
    if (n_mandatory > 4096) {
        NCBI_THROW(CArgException,eSynopsis,
            "Number of mandatory extra arguments is too big");
    }

    m_nExtra    = n_mandatory;
    m_nExtraOpt = n_optional;

    auto_ptr<CArgDesc_Pos> arg
        (m_nExtra ?
         new CArgDesc_Pos   (kEmptyStr, comment, type, flags) :
         new CArgDesc_PosOpt(kEmptyStr, comment, type, flags));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddAlias(const string& alias,
                                const string& arg_name)
{
    auto_ptr<CArgDesc_Alias> arg(
        new CArgDesc_Alias(alias, arg_name, kEmptyStr));

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::AddNegatedFlagAlias(const string& alias,
                                           const string& arg_name,
                                           const string& comment)
{
    // Make sure arg_name describes a flag
    TArgsCI orig = x_Find(arg_name);
    if (orig == m_Args.end()  ||  !s_IsFlag(**orig)) {
        NCBI_THROW(CArgException, eArgType,
            "Attempt to negate a non-flag argument: "+ arg_name);
    }

    auto_ptr<CArgDesc_Alias> arg(new CArgDesc_Alias(alias, arg_name, comment));
    arg->SetNegativeFlag(true);

    x_AddDesc(*arg);
    arg.release();
}


void CArgDescriptions::SetConstraint(const string&      name, 
                                     CArgAllow*         constraint,
                                     EConstraintNegate  negate)
{
    CRef<CArgAllow> safe_delete(constraint);

    TArgsI it = x_Find(name);
    if (it == m_Args.end()) {
        NCBI_THROW(CArgException, eConstraint,
            "Attempt to set constraint for undescribed argument: "+ name);
    }
    (*it)->SetConstraint(constraint, negate);
}
void CArgDescriptions::SetConstraint(const string&      name,
                                     const CArgAllow&   constraint,
                                     EConstraintNegate  negate)
{
    CArgAllow* onheap = constraint.Clone();
    if (!onheap) {
        NCBI_THROW(CArgException, eConstraint,
            "Clone method not implemented for: "+ name);
    }
    SetConstraint(name, onheap, negate);
}


void CArgDescriptions::SetDependency(const string& arg1,
                                     EDependency   dep,
                                     const string& arg2)
{
    m_Dependencies.insert(TDependencies::value_type(arg1,
        SArgDependency(arg2, dep)));
    if (dep == eExcludes) {
        // Exclusions must work in both directions
        m_Dependencies.insert(TDependencies::value_type(arg2,
            SArgDependency(arg1, dep)));
    }
}


void CArgDescriptions::SetCurrentGroup(const string& group)
{
    m_CurrentGroup = x_GetGroupIndex(group);
    if (m_CurrentGroup >= m_ArgGroups.size()) {
        m_ArgGroups.push_back(group);
        m_CurrentGroup = m_ArgGroups.size() - 1;
    }
}


void CArgDescriptions::SetErrorHandler(const string&      name,
                                       CArgErrorHandler*  err_handler)
{
    TArgsI it = x_Find(name);
    if (it == m_Args.end()) {
        NCBI_THROW(CArgException, eInvalidArg,
            "Attempt to set error handler for undescribed argument: "+ name);
    }
    (*it)->SetErrorHandler(err_handler);
}


bool CArgDescriptions::Exist(const string& name) const
{
    return (x_Find(name) != m_Args.end());
}


void CArgDescriptions::Delete(const string& name)
{
    {{ // ...from the list of all args
        TArgsI it = x_Find(name);
        if (it == m_Args.end()) {
            NCBI_THROW(CArgException,eSynopsis,
                "Argument description is not found");
        }
        m_Args.erase(it);
        if (name == s_AutoHelp) {
            m_AutoHelp = false;
        }

        // take special care of the extra args
        if ( name.empty() ) {
            m_nExtra = 0;
            m_nExtraOpt = 0;
            return;
        }
    }}

    {{ // ...from the list of key/flag args
        TKeyFlagArgs::iterator it =
            find(m_KeyFlagArgs.begin(), m_KeyFlagArgs.end(), name);
        if (it != m_KeyFlagArgs.end()) {
            m_KeyFlagArgs.erase(it);
            _ASSERT(find(m_KeyFlagArgs.begin(), m_KeyFlagArgs.end(), name) ==
                         m_KeyFlagArgs.end());
            _ASSERT(find(m_PosArgs.begin(), m_PosArgs.end(), name) ==
                         m_PosArgs.end());
            return;
        }
    }}

    {{ // ...from the list of positional args' positions
        TPosArgs::iterator it =
            find(m_PosArgs.begin(), m_PosArgs.end(), name);
        _ASSERT (it != m_PosArgs.end());
        m_PosArgs.erase(it);
        _ASSERT(find(m_PosArgs.begin(), m_PosArgs.end(), name) ==
                     m_PosArgs.end());
    }}
}


// Fake class to hold only a name -- to find in "m_Args"
class CArgDesc_NameOnly : public CArgDesc
{
public:
    CArgDesc_NameOnly(const string& name) :
        CArgDesc(name, kEmptyStr) {}
private:
    virtual string GetUsageSynopsis(bool/*name_only*/) const{return kEmptyStr;}
    virtual string GetUsageCommentAttr(void) const {return kEmptyStr;}
    virtual CArgValue* ProcessArgument(const string&) const {return 0;}
    virtual CArgValue* ProcessDefault(void) const {return 0;}
};

CArgDescriptions::TArgsCI CArgDescriptions::x_Find(const string& name,
                                                   bool* negative) const
{
    CArgDescriptions::TArgsCI arg =
        m_Args.find(AutoPtr<CArgDesc> (new CArgDesc_NameOnly(name)));
    if ( arg != m_Args.end() ) {
        const CArgDesc_Alias* al =
            dynamic_cast<const CArgDesc_Alias*>(arg->get());
        if ( al ) {
            if ( negative ) {
                *negative = al->GetNegativeFlag();
            }
            return x_Find(al->GetAliasedName(), negative);
        }
    }
    return arg;
}

CArgDescriptions::TArgsI CArgDescriptions::x_Find(const string& name,
                                                   bool* negative)
{
    CArgDescriptions::TArgsI arg =
        m_Args.find(AutoPtr<CArgDesc> (new CArgDesc_NameOnly(name)));
    if ( arg != m_Args.end() ) {
        const CArgDesc_Alias* al =
            dynamic_cast<const CArgDesc_Alias*>(arg->get());
        if ( al ) {
            if ( negative ) {
                *negative = al->GetNegativeFlag();
            }
            return x_Find(al->GetAliasedName(), negative);
        }
    }
    return arg;
}


size_t CArgDescriptions::x_GetGroupIndex(const string& group) const
{
    if ( group.empty() ) {
        return 0;
    }
    for (size_t i = 1; i < m_ArgGroups.size(); ++i) {
        if ( NStr::EqualNocase(m_ArgGroups[i], group) ) {
            return i;
        }
    }
    return m_ArgGroups.size();
}


void CArgDescriptions::x_PreCheck(void) const
{
    // Check for the consistency of positional args
    if ( m_nExtra ) {
        for (TPosArgs::const_iterator name = m_PosArgs.begin();
             name != m_PosArgs.end();  ++name) {
            TArgsCI arg_it = x_Find(*name);
            _ASSERT(arg_it != m_Args.end());
            CArgDesc& arg = **arg_it;

            if (dynamic_cast<const CArgDesc_PosOpt*> (&arg)) {
                NCBI_THROW(CArgException, eSynopsis,
                    "Having both optional named and required unnamed "
                    "positional arguments is prohibited");
            }
        }
    }

    // Check for the validity of default values.
    // Also check for conflict between no-separator and regular names
    for (TArgsCI it = m_Args.begin();  it != m_Args.end();  ++it) {
        CArgDesc& arg = **it;

        const string& name = arg.GetName();
        if (name.length() > 1  &&  m_NoSeparator.find(name[0]) != NPOS) {
            // find the argument with optional separator and check its flags
            for (TArgsCI i = m_Args.begin();  i != m_Args.end();  ++i) {
                CArgDesc& a = **i;
                const string& n = a.GetName();
                if (n.length() == 1 && n[0] == name[0] &&
                    (a.GetFlags() & CArgDescriptions::fOptionalSeparator)) {
                    if ((a.GetFlags() & CArgDescriptions::fOptionalSeparatorAllowConflict) == 0) {
                        NCBI_THROW(CArgException, eInvalidArg,
                            string("'") + name[0] +
                            "' argument allowed to contain no separator conflicts with '" +
                            name + "' argument. To allow such conflicts, add" +
                            " CArgDescriptions::fOptionalSeparatorAllowConflict flag into" +
                            " description of '" + name[0] + "'.");
                    }
                    break;
                }
            }
        }

        if (dynamic_cast<CArgDescDefault*> (&arg) == 0) {
            continue;
        }

        try {
            arg.VerifyDefault();
            continue;
        } catch (CException& e) {
            NCBI_RETHROW(e,CArgException,eConstraint,
                "Invalid default argument value");
        } catch (exception& e) {
            NCBI_THROW(CArgException, eConstraint,
                string("Invalid default value: ") + e.what());
        }
    }
}


CArgs* CArgDescriptions::CreateArgs(const CNcbiArguments& args) const
{
    const_cast<CArgDescriptions&>(*this).SetCurrentGroup(kEmptyStr);
    return CreateArgs(args.Size(), args);
}


void CArgDescriptions::x_CheckAutoHelp(const string& arg) const
{
    if (arg.compare(string("-") + s_AutoHelp) == 0) {
        if (m_AutoHelp) {
            NCBI_THROW(CArgHelpException,eHelp,kEmptyStr);
        }
    } else if (arg.compare(string("-") + s_AutoHelpFull) == 0) {
        NCBI_THROW(CArgHelpException,eHelpFull,kEmptyStr);
    } else if (arg.compare(string("-") + s_AutoHelpXml) == 0) {
        NCBI_THROW(CArgHelpException,eHelpXml,kEmptyStr);
    }
}


// (return TRUE if "arg2" was used)
bool CArgDescriptions::x_CreateArg(const string& arg1,
                                   bool have_arg2, const string& arg2,
                                   unsigned* n_plain, CArgs& args) const
{
    // Argument name
    string name;

    // Check if to start processing the args as positional
    if (*n_plain == kMax_UInt) {
        // Check for the "--" delimiter
        if (arg1.compare("--") == 0) {
            *n_plain = 0;  // pos.args started
            return false;
        }
        size_t  argssofar = args.GetAll().size();
        // Check if argument has not a key/flag syntax
        if ((arg1.length() > 1)  &&  arg1[0] == '-') {
            name = arg1.substr(1);
            TArgsCI it = m_Args.end();
            try {
                it = x_Find(name);
            } catch (CArgException&) {
            }
            if (it == m_Args.end()) {
                if (m_OpeningArgs.size() > argssofar) {
                    return x_CreateArg(arg1, m_OpeningArgs[argssofar], have_arg2, arg2, *n_plain, args);
                }
            }
            // Check for '=' in the arg1
            size_t eq = name.find('=');
            if (eq != NPOS) {
                name = name.substr(0, eq);
            }
            if (m_PositionalMode == ePositionalMode_Loose) {
                // If not a valid key/flag, treat it as a positional value
                if (!VerifyName(name)  ||  x_Find(name) == m_Args.end()) {
                    *n_plain = 0;  // pos.args started
                }
            }
        } else {
            if (m_OpeningArgs.size() > argssofar) {
                return x_CreateArg(arg1, m_OpeningArgs[argssofar], have_arg2, arg2, *n_plain, args);
            }
            *n_plain = 0;  // pos.args started
        }
    }

    // Whether the value of "arg2" is used
    bool arg2_used = false;

    // Extract name of positional argument
    if (*n_plain != kMax_UInt) {
        if (*n_plain < m_PosArgs.size()) {
            name = m_PosArgs[*n_plain];  // named positional argument
        } else {
            name = kEmptyStr;  // unnamed (extra) positional argument
        }
        (*n_plain)++;

        // Check for too many positional arguments
        if (kMax_UInt - m_nExtraOpt > m_nExtra + m_PosArgs.size()  &&
            *n_plain > m_PosArgs.size() + m_nExtra + m_nExtraOpt) {
            NCBI_THROW(CArgException,eSynopsis,
                "Too many positional arguments (" +
                NStr::UIntToString(*n_plain) +
                "), the offending value: "+ arg1);
        }
    }

    arg2_used = x_CreateArg(arg1, name, have_arg2, arg2, *n_plain, args);

    // Success (also indicate whether one or two "raw" args have been used)
    return arg2_used;
}


bool CArgDescriptions::x_CreateArg(const string& arg1,
                                   const string& name, 
                                   bool          have_arg2,
                                   const string& arg2,
                                   unsigned      n_plain,
                                   CArgs&        args,
                                   bool          update,
                                   CArgValue**   new_value) const
{
    if (new_value)
        *new_value = 0;

    bool arg2_used = false;
    bool no_separator = false;
    bool eq_separator = false;
    bool negative = false;

    // Get arg. description
    TArgsCI it;
    try {
        it = x_Find(name, &negative);
    } catch (CArgException&) {
        // Suppress overzealous "invalid argument name" exceptions
        // in the no-separator case.
        if (m_NoSeparator.find(name[0]) != NPOS) {
            it = m_Args.end(); // avoid duplicating the logic below
        } else {
            throw;
        }
    }
    if (it == m_Args.end()  &&  m_NoSeparator.find(name[0]) != NPOS) {
        it = x_Find(name.substr(0, 1), &negative);
        _ASSERT(it != m_Args.end());
        no_separator = true;
    }
    if (it == m_Args.end()) {
        if ( name.empty() ) {
            NCBI_THROW(CArgException,eInvalidArg,
                    "Unexpected extra argument, at position # " +
                    NStr::UIntToString(n_plain));
        } else {
            NCBI_THROW(CArgException,eInvalidArg,
                    "Unknown argument: \"" + name + "\"");
        }
    }
    _ASSERT(*it);

    const CArgDesc& arg = **it;

    if ( s_IsFlag(arg) ) {
        x_CheckAutoHelp(arg1);
    }

    // Check value separated by '='
    string arg_val;
    if ( s_IsKey(arg) ) {
        eq_separator = arg1.length() > name.length()  &&
            (arg1[name.length() + 1] == '=');
        if ( !eq_separator ) {
            if ((arg.GetFlags() & fMandatorySeparator) != 0) {
                NCBI_THROW(CArgException,eInvalidArg,
                    "Invalid argument: " + arg1);
            }
            no_separator |= (arg.GetFlags() & fOptionalSeparator) != 0  &&
                name.length() == 1  &&  arg1.length() > 2;
        }
    }

    // Get argument value
    const string* value = 0;
    if ( !eq_separator  &&  !no_separator ) {
        if ( s_IsKey(arg) ) {
            // <key> <value> arg  -- advance from the arg.name to the arg.value
            if ( !have_arg2  &&  !value ) {

                // if update specified we try to add default value
                //  (mandatory throws an exception out of the ProcessDefault())
                if (update) {
                    CRef<CArgValue> arg_value(arg.ProcessDefault());
                    // Add the value to "args"
                    args.Add(arg_value, update);
                    return arg2_used;
                }

                NCBI_THROW(CArgException,eNoArg,s_ArgExptMsg(arg1,
                    "Value is missing", kEmptyStr));
            }
            value = &arg2;
            arg2_used = true;
        } else {
            value = &arg1;
        }
    }
    else {
        _ASSERT(s_IsKey(arg));
        if ( no_separator ) {
            arg_val = arg1.substr(2);
        }
        else {
            arg_val = arg1.substr(name.length() + 2);
        }
        value = &arg_val;
    }

    CArgValue* av = 0;
    try {
        // Process the "raw" argument value into "CArgValue"
        if ( negative  &&  s_IsFlag(arg) ) {
            // Negative flag - use default value rather than
            // normal one.
            av = arg.ProcessDefault();
        }
        else {
            av = arg.ProcessArgument(*value);
        }
    }
    catch (CArgException) {
        const CArgErrorHandler* err_handler = arg.GetErrorHandler();
        if ( !err_handler ) {
            err_handler = m_ErrorHandler.GetPointerOrNull();
        }
        _ASSERT(err_handler);
        av = err_handler->HandleError(arg, *value);
    }

    if ( !av ) {
        return arg2_used;
    }
    CRef<CArgValue> arg_value(av);

    if (new_value) {
        *new_value = av;
    }

    bool allow_multiple = false;
    const CArgDescMandatory* adm = 
        dynamic_cast<const CArgDescMandatory*>(&arg);

    if (adm) {
        allow_multiple = 
            (adm->GetFlags() & CArgDescriptions::fAllowMultiple) != 0;
    }

    // Add the argument value to "args"
    args.Add(arg_value, update, allow_multiple);

    return arg2_used;
}


bool CArgDescriptions::x_IsMultiArg(const string& name) const
{
    TArgsCI it = x_Find(name);
    if (it == m_Args.end()) {
        return false;
    }
    const CArgDesc& arg = **it;
    const CArgDescMandatory* adm = 
        dynamic_cast<const CArgDescMandatory*>(&arg);

    if (!adm) {
        return false;
    }
    return (adm->GetFlags() & CArgDescriptions::fAllowMultiple) != 0;
}


void CArgDescriptions::x_PostCheck(CArgs&           args,
                                   unsigned int     n_plain,
                                   EPostCheckCaller caller)
    const
{
    // If explicitly specified, printout usage and exit in case there
    // was no args passed to the application
    if (IsSetMiscFlag(fUsageIfNoArgs)  &&  args.IsEmpty()) {
        NCBI_THROW(CArgHelpException, eHelp, kEmptyStr);
    }

    // Check dependencies, create set of exclusions
    set<string> exclude;
    map<string,string> require;
    ITERATE(TDependencies, dep, m_Dependencies) {
        // Skip missing and empty arguments
        if (!args.Exist(dep->first)  ||  !args[dep->first]) {
            continue;
        }
        switch ( dep->second.m_Dep ) {
        case eRequires:
            require.insert(make_pair(dep->second.m_Arg,dep->first));
            break;
        case eExcludes:
            // Excluded exists and is not empty?
            if (args.Exist(dep->second.m_Arg)  &&  args[dep->second.m_Arg]) {
                NCBI_THROW(CArgException, eConstraint,
                    s_ArgExptMsg(dep->second.m_Arg,
                    "Incompatible with argument", dep->first));
            }
            exclude.insert(dep->second.m_Arg);
            break;
        }
    }

    // Check that all opening args are provided
    ITERATE (TPosArgs, it, m_OpeningArgs) {
        if (!args.Exist(*it)) {
            NCBI_THROW(CArgException,eNoArg, "Opening argument not provided: " + *it);
        }
    }

    // Check if all mandatory unnamed positional arguments are provided
    if (m_PosArgs.size() <= n_plain  &&
        n_plain < m_PosArgs.size() + m_nExtra){
        NCBI_THROW(CArgException,eNoArg,
            "Too few (" + NStr::UIntToString(n_plain) +
            ") unnamed positional arguments. Must define at least " +
            NStr::UIntToString(m_nExtra));
    }

    // Compose an ordered list of args
    list<const CArgDesc*> def_args;
    ITERATE (TKeyFlagArgs, it, m_KeyFlagArgs) {
        const CArgDesc& arg = **x_Find(*it);
        def_args.push_back(&arg);
    }
    ITERATE (TPosArgs, it, m_PosArgs) {
        const CArgDesc& arg = **x_Find(*it);
        def_args.push_back(&arg);
    }

    // Set default values (if available) for the arguments not defined
    // in the command line.
    ITERATE (list<const CArgDesc*>, it, def_args) {
        const CArgDesc& arg = **it;

        // Nothing to do if defined in the command-line
        if ( args.Exist(arg.GetName()) ) {
            continue;
        }

        if (require.find(arg.GetName()) != require.end() ) {
            string requester(require.find(arg.GetName())->second);
            // Required argument must be present
            NCBI_THROW(CArgException, eConstraint,
                s_ArgExptMsg(arg.GetName(),
                "Must be specified, as it is required by argument", requester));
        }

        if (exclude.find(arg.GetName()) != exclude.end()) {
            CRef<CArg_ExcludedValue> arg_exvalue(
                new CArg_ExcludedValue(arg.GetName()));
            // Add the excluded-value argument to "args"
            args.Add(arg_exvalue);
            continue;
        }
        // Use default argument value
        try {
            CRef<CArgValue> arg_value(arg.ProcessDefault());
            // Add the value to "args"
            args.Add(arg_value);
        } 
        catch (CArgException&) {
            // mandatory argument, for CGI can be taken not only from the
            // command line but also from the HTTP request
            if (GetArgsType() != eCgiArgs  ||  caller == eConvertKeys) {
                throw;
            }
        }
    }
}


void CArgDescriptions::SetUsageContext
(const string& usage_name,
 const string& usage_description,
 bool          usage_sort_args,
 SIZE_TYPE     usage_width)
{
    m_UsageName        = usage_name;
    m_UsageDescription = usage_description;
    usage_sort_args ? SetMiscFlags(fUsageSortArgs) : ResetMiscFlags(fUsageSortArgs);

    const SIZE_TYPE kMinUsageWidth = 30;
    if (usage_width < kMinUsageWidth) {
        usage_width = kMinUsageWidth;
        ERR_POST_X(23, Warning <<
                       "CArgDescriptions::SetUsageContext() -- usage_width=" <<
                       usage_width << " adjusted to " << kMinUsageWidth);
    }
    m_UsageWidth = usage_width;
}

void CArgDescriptions::SetDetailedDescription( const string& usage_description)
{
    m_DetailedDescription = usage_description;
}

bool CArgDescriptions::VerifyName(const string& name, bool extended)
{
    if ( name.empty() )
        return true;

    string::const_iterator it = name.begin();
    if (extended  &&  *it == '#') {
        for (++it;  it != name.end();  ++it) {
            if ( !isdigit((unsigned char)(*it)) ) {
                return false;
            }
        }
    } else {
        if (name[0] == '-') {
            // Prohibit names like '-' or '--foo'.
            // The second char must be present and may not be '-'.
            if (name.length() == 1  ||  name[1] == '-') {
                return false;
            }
        }
        for ( ;  it != name.end();  ++it) {
            if ( !s_IsArgNameChar((unsigned char)(*it)) )
                return false;
        }
    }

    return true;
}


void CArgDescriptions::x_AddDesc(CArgDesc& arg)
{
    const string& name = arg.GetName();

    if ( Exist(name) ) {
        NCBI_THROW(CArgException,eSynopsis,
            "Argument with this name is already defined: " + name);
    }

    arg.SetGroup(m_CurrentGroup);

    if (s_IsKey(arg)  ||  s_IsFlag(arg)) {
        _ASSERT(find(m_KeyFlagArgs.begin(), m_KeyFlagArgs.end(), name)
                == m_KeyFlagArgs.end());
        m_KeyFlagArgs.push_back(name);
    } else if ( !s_IsAlias(arg)  &&  !name.empty() ) {
        TPosArgs& container = s_IsOpening(arg) ? m_OpeningArgs : m_PosArgs;
        _ASSERT(find(container.begin(), container.end(), name)
                == container.end());
        if ( s_IsOptional(arg) ) {
            container.push_back(name);
        } else {
            TPosArgs::iterator it;
            for (it = container.begin();  it != container.end();  ++it) {
                if ( s_IsOptional(**x_Find(*it)) )
                    break;
            }
            container.insert(it, name);
        }
    }
    
    if ((arg.GetFlags() & fOptionalSeparator) != 0  &&
        name.length() == 1  &&
        s_IsKey(arg)) {
        m_NoSeparator += arg.GetName();
    }

    arg.SetErrorHandler(m_ErrorHandler.GetPointerOrNull());
    m_Args.insert(&arg);
}


void CArgDescriptions::PrintUsageIfNoArgs(bool do_print)
{
    do_print ? SetMiscFlags(fUsageIfNoArgs) : ResetMiscFlags(fUsageIfNoArgs);
}



///////////////////////////////////////////////////////
//  CArgDescriptions::PrintUsage()


static void s_PrintCommentBody(list<string>& arr, const string& s,
                               SIZE_TYPE width)
{
    NStr::Wrap(s, width, arr, NStr::fWrap_Hyphenate, "   ");
}


void CArgDescriptions::x_PrintComment(list<string>&   arr,
                                      const CArgDesc& arg,
                                      SIZE_TYPE       width) const
{
    string intro = ' ' + arg.GetUsageSynopsis(true/*name_only*/);

    // Print type (and value constraint, if any)
    string attr = arg.GetUsageCommentAttr();
    if ( !attr.empty() ) {
        char separator =
            (arg.GetFlags() & CArgDescriptions::fMandatorySeparator) ? '=' : ' ';
        string t;
        t += separator;
        t += '<' + attr + '>';
        attr = t;
    }

    // Add aliases for non-positional arguments
    list<string> negatives;
    if ( !s_IsPositional(arg) ) {
        ITERATE(CArgDescriptions::TArgs, it, m_Args) {
            const CArgDesc_Alias* alias =
                dynamic_cast<const CArgDesc_Alias*>(it->get());
            if (!alias  ||  alias->GetAliasedName() != arg.GetName()) {
                continue;
            }
            if ( alias->GetNegativeFlag() ) {
                negatives.push_back(alias->GetName());
            }
            else {
                intro += ", -" + alias->GetName();
            }
        }
    }

    intro += attr;

    // Wrap intro if necessary...
    {{
        SIZE_TYPE indent = intro.find(", ");
        if (indent == NPOS  ||  indent > width / 2) {
            indent = intro.find(" <");
            if (indent == NPOS  ||  indent > width / 2) {
                indent = 0;
            }
        }
        NStr::Wrap(intro, width, arr, NStr::fWrap_Hyphenate,
                   string(indent + 2, ' '), kEmptyStr);
    }}

    // Print description
    s_PrintCommentBody(arr, arg.GetComment(), width);

    // Print default value, if any
    const CArgDescDefault* dflt = dynamic_cast<const CArgDescDefault*> (&arg);
    if ( dflt ) {
        s_PrintCommentBody
            (arr, "Default = `" + dflt->GetDefaultValue() + '\'', width);
    }

    // Print required/excluded args
    string require;
    string exclude;
    pair<TDependency_CI, TDependency_CI> dep_rg =
        m_Dependencies.equal_range(arg.GetName());
    for (TDependency_CI dep = dep_rg.first; dep != dep_rg.second; ++dep) {
        switch ( dep->second.m_Dep ) {
        case eRequires:
            if ( !require.empty() ) {
                require += ", ";
            }
            require += dep->second.m_Arg;
            break;
        case eExcludes:
            if ( !exclude.empty() ) {
                exclude += ", ";
            }
            exclude += dep->second.m_Arg;
            break;
        }
    }
    if ( !require.empty() ) {
        s_PrintCommentBody(arr, " * Requires:  " + require, width);
    }
    if ( !exclude.empty() ) {
        s_PrintCommentBody(arr, " * Incompatible with:  " + exclude, width);
    }
    if ( !negatives.empty() ) {
        string neg_info;
        ITERATE(list<string>, neg, negatives) {
            if ( !neg_info.empty() ) {
                neg_info += ", ";
            }
            neg_info += *neg;
        }
        SIZE_TYPE indent = neg_info.find(", ");
        if (indent == NPOS  ||  indent > width / 2) {
            indent = 0;
        }
        neg_info = " -" + neg_info;
        NStr::Wrap(neg_info, width, arr, NStr::fWrap_Hyphenate,
                string(indent + 2, ' '), kEmptyStr);

        // Print description
        string neg_comment = arg.GetComment();
        if ( neg_comment.empty() ) {
            neg_comment = "Negative for " + arg.GetName();
        }
        s_PrintCommentBody(arr, neg_comment, width);
    }
    if (s_IsFlag(arg)) {
        const CArgDesc_Flag* fl = dynamic_cast<const CArgDesc_Flag*>(&arg);
        if (fl && !fl->GetSetValue()) {
            s_PrintCommentBody(arr, "When the flag is present, its value is FALSE", width);
        }
    }
}

CArgDescriptions::CPrintUsage::CPrintUsage(const CArgDescriptions& desc)
    : m_desc(desc)
{
    typedef list<const CArgDesc*> TList;
    typedef TList::iterator       TListI;

    m_args.push_front(0);
    TListI it_pos = m_args.begin();

    // Opening
    for (TPosArgs::const_iterator name = desc.m_OpeningArgs.begin();
         name != desc.m_OpeningArgs.end();  ++name) {
        TArgsCI it = desc.x_Find(*name);
        _ASSERT(it != desc.m_Args.end());
        if (it->get()->GetFlags() & CArgDescriptions::fHidden)
        {
            continue;
        }
        m_args.insert(it_pos, it->get());
    }

    // Keys and Flags
    if ( desc.IsSetMiscFlag(fUsageSortArgs) ) {
        // Alphabetically ordered,
        // mandatory keys to go first, then flags, then optional keys
        TListI& it_opt_keys = it_pos;
        TListI it_keys  = m_args.insert(it_pos,nullptr);
        TListI it_flags = m_args.insert(it_pos,nullptr);

        for (TArgsCI it = desc.m_Args.begin();  it != desc.m_Args.end();  ++it) {
            const CArgDesc* arg = it->get();
            if (it->get()->GetFlags() & CArgDescriptions::fHidden)
            {
                continue;
            }

            if (dynamic_cast<const CArgDesc_KeyOpt*> (arg)  ||
                dynamic_cast<const CArgDesc_KeyDef*> (arg)) {
                m_args.insert(it_opt_keys, arg);
            } else if (dynamic_cast<const CArgDesc_Key*> (arg)) {
                m_args.insert(it_keys, arg);
            } else if (dynamic_cast<const CArgDesc_Flag*> (arg)) {
                if ((desc.m_AutoHelp &&
                    strcmp(s_AutoHelp,     (arg->GetName()).c_str()) == 0) ||
                    strcmp(s_AutoHelpFull, (arg->GetName()).c_str()) == 0)
                    m_args.push_front(arg);
                else
                    m_args.insert(it_flags, arg);
            }
        }
        m_args.erase(it_keys);
        m_args.erase(it_flags);
    } else {
        // Unsorted, just the order they were described by user
        for (TKeyFlagArgs::const_iterator name = desc.m_KeyFlagArgs.begin();
             name != desc.m_KeyFlagArgs.end();  ++name) {
            TArgsCI it = desc.x_Find(*name);
            _ASSERT(it != desc.m_Args.end());
            if (it->get()->GetFlags() & CArgDescriptions::fHidden)
            {
                continue;
            }

            m_args.insert(it_pos, it->get());
        }
    }

    // Positional
    for (TPosArgs::const_iterator name = desc.m_PosArgs.begin();
         name != desc.m_PosArgs.end();  ++name) {
        TArgsCI it = desc.x_Find(*name);
        _ASSERT(it != desc.m_Args.end());
        if (it->get()->GetFlags() & CArgDescriptions::fHidden)
        {
            continue;
        }
        const CArgDesc* arg = it->get();

        // Mandatory args to go first, then go optional ones
        if (dynamic_cast<const CArgDesc_PosOpt*> (arg)) {
            m_args.push_back(arg);
        } else if (dynamic_cast<const CArgDesc_Pos*> (arg)) {
            m_args.insert(it_pos, arg);
        }
    }
    m_args.erase(it_pos);

    // Extra
    {{
        TArgsCI it = desc.x_Find(kEmptyStr);
        if (it != desc.m_Args.end()) {
            if ((it->get()->GetFlags() & CArgDescriptions::fHidden) == 0)
            {
                m_args.push_back(it->get());
            }
        }
    }}
}

CArgDescriptions::CPrintUsage::~CPrintUsage()
{
}

void CArgDescriptions::CPrintUsage::AddSynopsis(list<string>& arr,
    const string& intro, const string& prefix) const
{
    list<const CArgDesc*>::const_iterator it;
    list<string> syn;
    if (m_desc.GetArgsType() == eCgiArgs) {
        for (it = m_args.begin();  it != m_args.end();  ++it) {
            const CArgDescSynopsis* as = 
                dynamic_cast<const CArgDescSynopsis*>(&**it);

            if (as) {
                const string& name  = (*it)->GetName();
                const string& synopsis  = as->GetSynopsis();
                syn.push_back(name+"="+synopsis);
            }
        } // for
        NStr::WrapList(
            syn, m_desc.m_UsageWidth, "&", arr, 0, "?", "  "+m_desc.m_UsageName+"?");

    } else { // regular application
        if (!intro.empty()) {
            syn.push_back(intro);
        }
        for (it = m_args.begin();  it != m_args.end();  ++it) {
            if ( s_IsOptional(**it) || s_IsFlag(**it) ) {
                syn.push_back('[' + (*it)->GetUsageSynopsis() + ']');
            } else if ( s_IsPositional(**it) || s_IsOpening(**it) ) {
                syn.push_back('<' + (*it)->GetUsageSynopsis() + '>');
            } else {
                syn.push_back((*it)->GetUsageSynopsis());
            }
        } // for
        NStr::WrapList(syn, m_desc.m_UsageWidth, " ", arr, 0, prefix, "  ");
    }
}

void CArgDescriptions::CPrintUsage::AddDescription(list<string>& arr, bool detailed) const
{
    if ( m_desc.m_UsageDescription.empty() ) {
        arr.push_back("DESCRIPTION    -- none");
    } else {
        arr.push_back("DESCRIPTION");
        s_PrintCommentBody(arr,
            (detailed  && !m_desc.m_DetailedDescription.empty()) ?
                m_desc.m_DetailedDescription : m_desc.m_UsageDescription,
            m_desc.m_UsageWidth);
    }
}

void CArgDescriptions::CPrintUsage::AddCommandDescription(list<string>& arr,
    const string& cmd, const map<string,string>* aliases,
    size_t max_cmd_len, bool detailed) const
{
    if (detailed) {
        arr.push_back(kEmptyStr);
    }
    string cmd_full(cmd);
    if (aliases) {
        map<string,string>::const_iterator a = aliases->find(cmd);
        if (a != aliases->end()) {
            cmd_full += " (" + a->second + ")";
        }
    }
    cmd_full += string( max_cmd_len - cmd_full.size(), ' ');
    cmd_full += "- ";
    cmd_full += m_desc.m_UsageDescription;
    arr.push_back(string("  ")+ cmd_full);
    if (detailed) {
        AddSynopsis(arr,string(max_cmd_len+3,' '),string(max_cmd_len+6,' '));
    }
}

void CArgDescriptions::CPrintUsage::AddDetails(list<string>& arr) const
{
    list<const CArgDesc*>::const_iterator it;
    list<string> req;
    list<string> opt;
    // Collect mandatory args
    for (it = m_args.begin();  it != m_args.end();  ++it) {
        if (s_IsOptional(**it)  ||  s_IsFlag(**it)) {
            continue;
        }
        m_desc.x_PrintComment(req, **it, m_desc.m_UsageWidth);
    }
    // Collect optional args
    for (size_t grp = 0;  grp < m_desc.m_ArgGroups.size();  ++grp) {
        list<string> grp_opt;
        bool group_not_empty = false;
        if ( !m_desc.m_ArgGroups[grp].empty() ) {
            NStr::Wrap(m_desc.m_ArgGroups[grp], m_desc.m_UsageWidth, grp_opt,
                NStr::fWrap_Hyphenate, " *** ");
        }
        for (it = m_args.begin();  it != m_args.end();  ++it) {
            if (!s_IsOptional(**it)  &&  !s_IsFlag(**it)) {
                continue;
            }
            if ((*it)->GetGroup() == grp) {
                m_desc.x_PrintComment(grp_opt, **it, m_desc.m_UsageWidth);
                group_not_empty = true;
            }
        }
        if ( group_not_empty ) {
            opt.insert(opt.end(), grp_opt.begin(), grp_opt.end());
            opt.push_back(kEmptyStr);
        }
    }
    if ( !req.empty() ) {
        arr.push_back(kEmptyStr);
        arr.push_back("REQUIRED ARGUMENTS");
        arr.splice(arr.end(), req);
    }
    if ( !m_desc.m_nExtra  &&  !opt.empty() ) {
        arr.push_back(kEmptyStr);
        arr.push_back("OPTIONAL ARGUMENTS");
        arr.splice(arr.end(), opt);
    }

    // # of extra arguments
    if (m_desc.m_nExtra  ||  (m_desc.m_nExtraOpt != 0  && m_desc.m_nExtraOpt != kMax_UInt)) {
        string str_extra = "NOTE:  Specify ";
        if ( m_desc.m_nExtra ) {
            str_extra += "at least ";
            str_extra += NStr::UIntToString(m_desc.m_nExtra);
            if (m_desc.m_nExtraOpt != kMax_UInt) {
                str_extra += ", and ";
            }
        }
        if (m_desc.m_nExtraOpt != kMax_UInt) {
            str_extra += "no more than ";
            str_extra += NStr::UIntToString(m_desc.m_nExtra + m_desc.m_nExtraOpt);
        }
        str_extra +=
            " argument" + string(&"s"[m_desc.m_nExtra + m_desc.m_nExtraOpt == 1]) +
            " in \"....\"";
        s_PrintCommentBody(arr, str_extra, m_desc.m_UsageWidth);
    }
    if ( m_desc.m_nExtra  &&  !opt.empty() ) {
        arr.push_back(kEmptyStr);
        arr.push_back("OPTIONAL ARGUMENTS");
        arr.splice(arr.end(), opt);
    }
}

string& CArgDescriptions::PrintUsage(string& str, bool detailed) const
{
    CPrintUsage x(*this);
    list<string> arr;

    // SYNOPSIS
    arr.push_back("USAGE");
    x.AddSynopsis(arr, m_UsageName,"    ");

    // DESCRIPTION
    arr.push_back(kEmptyStr);
    x.AddDescription(arr, detailed);

    // details
    if (detailed) {
        x.AddDetails(arr);
    } else {
        arr.push_back(kEmptyStr);
        arr.push_back("Use '-help' to print detailed descriptions of command line arguments");
    }

    str += NStr::Join(arr, "\n");
    str += "\n";
    return str;
}

CArgDescriptions::CPrintUsageXml::CPrintUsageXml(const CArgDescriptions& desc, CNcbiOstream& out)
    : m_out(out)
{
    m_out << "<?xml version=\"1.0\"?>" << endl;
    m_out << "<" << "ncbi_application xmlns=\"ncbi:application\"" << endl
        << " xmlns:xs=\"http://www.w3.org/2001/XMLSchema-instance\"" << endl
        << " xs:schemaLocation=\"ncbi:application ncbi_application.xsd\"" << endl
        << ">" << endl;
    m_out << "<" << "program" << " type=\"";
    if (desc.GetArgsType() == eRegularArgs) {
        m_out << "regular";
    } else if (desc.GetArgsType() == eCgiArgs) {
        m_out << "cgi";
    } else {
        m_out << "UNKNOWN";
    }
    m_out << "\"" << ">" << endl;    
    s_WriteXmlLine(m_out, "name", desc.m_UsageName);
    s_WriteXmlLine(m_out, "version", 
        CNcbiApplication::Instance()->GetVersion().Print());
    s_WriteXmlLine(m_out, "description", desc.m_UsageDescription);
    m_out << "</" << "program" << ">" << endl;
}
CArgDescriptions::CPrintUsageXml::~CPrintUsageXml()
{
    m_out << "</" << "ncbi_application" << ">" << endl;
}

void CArgDescriptions::CPrintUsageXml::PrintArguments(const CArgDescriptions& desc) const
{
    m_out << "<" << "arguments";
    if (desc.GetPositionalMode() == ePositionalMode_Loose) {
        m_out << " positional_mode=\"loose\"";
    }
    m_out << ">" << endl;

    string tag;

// opening
    ITERATE(TPosArgs, p, desc.m_OpeningArgs) {
        ITERATE (TArgs, a, desc.m_Args) {
            if ((**a).GetName() == *p) {
                tag = (*a)->PrintXml(m_out);
                m_out << "</" << tag << ">" << endl;
            }
        }
    }
// positional
    ITERATE(TPosArgs, p, desc.m_PosArgs) {
        ITERATE (TArgs, a, desc.m_Args) {
            if ((**a).GetName() == *p) {
                tag = (*a)->PrintXml(m_out);
                desc.x_PrintAliasesAsXml(m_out, (*a)->GetName());
                m_out << "</" << tag << ">" << endl;
            }
        }
    }
// keys
    ITERATE (TArgs, a, desc.m_Args) {
        if (s_IsKey(**a)) {
            tag = (*a)->PrintXml(m_out);
            desc.x_PrintAliasesAsXml(m_out, (*a)->GetName());
            m_out << "</" << tag << ">" << endl;
        }
    }
// flags
    ITERATE (TArgs, a, desc.m_Args) {
        if (s_IsFlag(**a)) {
            tag = (*a)->PrintXml(m_out);
            desc.x_PrintAliasesAsXml(m_out, (*a)->GetName());
            desc.x_PrintAliasesAsXml(m_out, (*a)->GetName(), true);
            m_out << "</" << tag << ">" << endl;
        }
    }
// extra positional
    ITERATE (TArgs, a, desc.m_Args) {
        if (s_IsPositional(**a) && (**a).GetName().empty()) {
            tag = (*a)->PrintXml(m_out);
            s_WriteXmlLine(m_out, "min_occurs", NStr::UIntToString(desc.m_nExtra));
            s_WriteXmlLine(m_out, "max_occurs", NStr::UIntToString(desc.m_nExtraOpt));
            m_out << "</" << tag << ">" << endl;
        }
    }
    if (!desc.m_Dependencies.empty()) {
        m_out << "<" << "dependencies" << ">" << endl;
        ITERATE(TDependencies, dep, desc.m_Dependencies) {
            if (dep->second.m_Dep == eRequires) {
                m_out << "<" << "first_requires_second" << ">" << endl;
                s_WriteXmlLine(m_out, "arg1", dep->first);
                s_WriteXmlLine(m_out, "arg2", dep->second.m_Arg);
                m_out << "</" << "first_requires_second" << ">" << endl;
            }
        }
        ITERATE(TDependencies, dep, desc.m_Dependencies) {
            if (dep->second.m_Dep == eExcludes) {
                m_out << "<" << "first_excludes_second" << ">" << endl;
                s_WriteXmlLine(m_out, "arg1", dep->first);
                s_WriteXmlLine(m_out, "arg2", dep->second.m_Arg);
                m_out << "</" << "first_excludes_second" << ">" << endl;
            }
        }
        m_out << "</" << "dependencies" << ">" << endl;
    }
    m_out << "</" << "arguments" << ">" << endl;
}

void CArgDescriptions::PrintUsageXml(CNcbiOstream& out) const
{
    CPrintUsageXml x(*this,out);
    x.PrintArguments(*this);
}

void CArgDescriptions::x_PrintAliasesAsXml( CNcbiOstream& out,
    const string& name, bool negated /* =false*/) const
{
    ITERATE (TArgs, a, m_Args) {
        if (s_IsAlias(**a)) {
            const CArgDesc_Alias& alias =
                dynamic_cast<const CArgDesc_Alias&>(**a);
            if (negated == alias.GetNegativeFlag()) {
                string tag = negated ? "negated_alias" : "alias";
                if (alias.GetAliasedName() == name) {
                    s_WriteXmlLine(out, tag, alias.GetName());
                }
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// CCommandArgDescriptions

CCommandArgDescriptions::CCommandArgDescriptions(
    bool auto_help, CArgErrorHandler* err_handler, TCommandArgFlags cmd_flags)
    : CArgDescriptions(auto_help,err_handler), m_Cmd_req(cmd_flags), m_CurrentGroup(0)
{
}

CCommandArgDescriptions::~CCommandArgDescriptions(void)
{
}

void CCommandArgDescriptions::SetCurrentCommandGroup(const string& group)
{
    m_CurrentGroup = x_GetCommandGroupIndex(group);
    if (m_CurrentGroup == 0) {
        m_CmdGroups.push_back(group);
        m_CurrentGroup = m_CmdGroups.size();
    }
}

bool CCommandArgDescriptions::x_IsCommandMandatory(void) const
{
    return (m_Cmd_req & eCommandOptional) == 0;
}

size_t CCommandArgDescriptions::x_GetCommandGroupIndex(const string& group) const
{
    size_t i = 1;
    ITERATE( list<string>, g, m_CmdGroups) {
        if ( NStr::EqualNocase(*g, group) ) {
            return i;
        }
        ++i;
    }
    return 0;
}

void CCommandArgDescriptions::AddCommand(
    const string& cmd, CArgDescriptions* description,
    const string& alias)
{

    string command( NStr::TruncateSpaces(cmd));
    if (command.empty()) {
        NCBI_THROW(CArgException,eSynopsis,
            "Command cannot be empty: "+ cmd);
    }
    if (description) {
        if ( m_AutoHelp ) {
            if (description->Exist(s_AutoHelp)) {
                description->Delete(s_AutoHelp);
            }
        }
        if (description->Exist(s_AutoHelpFull)) {
            description->Delete(s_AutoHelpFull);
        }
        if (description->Exist(s_AutoHelpXml)) {
            description->Delete(s_AutoHelpXml);
        }

        if (m_CurrentGroup == 0) {
            SetCurrentCommandGroup(kEmptyStr);
        }
        m_Commands.remove(command);
        m_Commands.push_back(command);
        m_Description[command] = description;
        m_Groups[command] = m_CurrentGroup;
        if (!alias.empty()) {
            m_Aliases[command] = alias;
        } else {
            m_Aliases.erase(command);
        }
    } else {
        m_Commands.remove(command);
        m_Description.erase(command);
        m_Groups.erase(command);
        m_Aliases.erase(command);
    }
}

string CCommandArgDescriptions::x_IdentifyCommand(const string& command) const
{
    if (m_Description.find(command) != m_Description.end()) {
        return command;
    }
    map<string,string>::const_iterator a = m_Aliases.begin();
    for ( ; a != m_Aliases.end(); ++a) {
        if (a->second == command) {
            return a->first;
        }
    }
    string cmd(command);
    if (cmd != "-") {
        vector<string> candidates;
        TDescriptions::const_iterator d;
        for (d = m_Description.begin(); d != m_Description.end(); ++d) {
            if (NStr::StartsWith(d->first,cmd)) {
                candidates.push_back(d->first);
            }
        }
        if (candidates.size() == 1) {
            return candidates.front();
        }
    }
    return kEmptyStr;
}

CArgs* CCommandArgDescriptions::CreateArgs(const CNcbiArguments& argv) const
{
    if (argv.Size() > 1) {
        if (x_IsCommandMandatory()) {
            if (argv[1].empty()) {
                NCBI_THROW(CArgException,eInvalidArg, "Nonempty command is required");
            }
            x_CheckAutoHelp(argv[1]);
        }
        string command( x_IdentifyCommand(argv[1]));
        TDescriptions::const_iterator d;
        d = m_Description.find(command);
        if (d != m_Description.end()) {
            CNcbiArguments argv2(argv);
            argv2.Shift();
            m_Command = command;
            return d->second->CreateArgs(argv2)->SetCommand(command);
        }
        m_Command.clear();
        if (x_IsCommandMandatory() && !m_Description.empty()) {
            NCBI_THROW(CArgException,eInvalidArg, "Command not recognized: " + argv[1]);
        }
    }
    if (x_IsCommandMandatory() && !m_Description.empty()) {
        NCBI_THROW(CArgException,eInvalidArg, "Command is required");
    }
    return CArgDescriptions::CreateArgs(argv)->SetCommand(kEmptyStr);
}

string& CCommandArgDescriptions::PrintUsage(string& str, bool detailed) const
{
    const CArgDescriptions* argdesc = NULL;
    string cmd(m_Command);
    if (cmd.empty())
    {
        const CNcbiArguments& cmdargs = CNcbiApplication::Instance()->GetArguments();
        size_t cmdsize = cmdargs.Size();
        if (cmdsize > 2) {
            cmd = cmdargs[ 2];
            if (cmd.empty()) {
                if (!x_IsCommandMandatory()) {
                    argdesc = this;
                }
            } else {
                cmd = x_IdentifyCommand(cmd);
            }
        }
    }
    TDescriptions::const_iterator d;
    if (!m_Description.empty()) {
        d = m_Description.find(cmd);
        if (d != m_Description.end()) {
            argdesc = d->second.get();
        }
    } else {
        argdesc = this;
    }

    if (argdesc) {
        CPrintUsage x(*argdesc);
        list<string> arr;

/*
        if (!cmd.empty()) {
            arr.push_back("COMMAND");
            arr.push_back(string("    ")+cmd);
            arr.push_back(kEmptyStr);
        }
*/

        // SYNOPSIS
        arr.push_back("USAGE");
        x.AddSynopsis(arr, m_UsageName + " " + cmd,"    ");

        // DESCRIPTION
        arr.push_back(kEmptyStr);
        x.AddDescription(arr, detailed);

        // details
        if (detailed) {
            x.AddDetails(arr);
        } else {
            arr.push_back(kEmptyStr);
            arr.push_back("Use '-help " + cmd + "' to print detailed descriptions of command line arguments");
        }

        str += NStr::Join(arr, "\n");
        str += "\n";
        return str;
    }

    CPrintUsage x(*this);
    list<string> arr;

    arr.push_back("USAGE");
    arr.push_back(string("  ")+ m_UsageName +" <command> [options]");
    if (!x_IsCommandMandatory()) {
        arr.push_back("or");
        x.AddSynopsis(arr, m_UsageName,"    ");
    }

    arr.push_back(kEmptyStr);
    x.AddDescription(arr, detailed);

// max command name length
    size_t max_cmd_len = 0;
    for (d = m_Description.begin(); d != m_Description.end(); ++d) {
        size_t alias_size=0;
        map<string,string>::const_iterator a = m_Aliases.find(d->first);
        if (a != m_Aliases.end()) {
            alias_size = a->second.size() + 3;
        }
        max_cmd_len = max(max_cmd_len, d->first.size() + alias_size);
    }
    max_cmd_len += 2;

    list<string> cmds = m_Commands;
    if ((m_Cmd_req & eNoSortCommands)==0) {
        cmds.sort();
    }
    if (m_CmdGroups.size() > 1) {
        list<string> cmdgroups = m_CmdGroups;
        if ((m_Cmd_req & eNoSortGroups)==0) {
            cmdgroups.sort();
        }
        ITERATE( list<string>, g, cmdgroups) {
            string grouptitle;
            bool titleprinted = false;
            if (g->empty()) {
                grouptitle = "Commands";
            } else {
                grouptitle = *g;
            }
            size_t group = x_GetCommandGroupIndex(*g);
            ITERATE( list<string>, d, cmds) {
                map<string, size_t >::const_iterator j = m_Groups.find(*d);
                if (j != m_Groups.end() && j->second == group) {
                    if (!titleprinted) {
                        arr.push_back(kEmptyStr);
                        arr.push_back(grouptitle + ":");
                        titleprinted = true;
                    }
                    CPrintUsage y(*(m_Description.lower_bound(*d)->second));
                    y.AddCommandDescription(arr, *d, &m_Aliases, max_cmd_len, detailed);
                }
            }
            ++group;
        }
    } else {
        arr.push_back(kEmptyStr);
        arr.push_back("AVAILABLE COMMANDS:");
        ITERATE( list<string>, d, cmds) {
            CPrintUsage y(*(m_Description.find(*d)->second));
            y.AddCommandDescription(arr, *d, &m_Aliases, max_cmd_len, detailed);
        }
    }

    if (!x_IsCommandMandatory() && detailed) {
        arr.push_back(kEmptyStr);
        arr.push_back("Missing command:");
        x.AddDetails(arr);
    }

    arr.push_back(kEmptyStr);
    if (m_AutoHelp) {
        arr.push_back("Use '-h command' to print help on a specific command");
    }
    arr.push_back("Use '-help command' to print detailed descriptions of command line arguments");

    str += NStr::Join(arr, "\n");
    str += "\n";
    return str;
}

void CCommandArgDescriptions::PrintUsageXml(CNcbiOstream& out) const
{
    CPrintUsageXml x(*this,out);
    if (!x_IsCommandMandatory()) {
        x.PrintArguments(*this);
    }
    TDescriptions::const_iterator d;
    for (d = m_Description.begin(); d != m_Description.end(); ++d) {
        out << "<command>" << endl;
        out << "<name>" << d->first << "</name>" << endl;
        x.PrintArguments(*(d->second));
        out << "</command>" << endl;
    }
}
///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
// CArgAllow::
//   CArgAllow_Symbols::
//   CArgAllow_String::
//   CArgAllow_Strings::
//   CArgAllow_Int8s::
//   CArgAllow_Integers::
//   CArgAllow_Doubles::
//


///////////////////////////////////////////////////////
//  CArgAllow::
//

CArgAllow::~CArgAllow(void)
{
}

void CArgAllow::PrintUsageXml(CNcbiOstream& ) const
{
}

///////////////////////////////////////////////////////
//  s_IsSymbol() -- check if the symbol belongs to one of standard character
//                  classes from <ctype.h>, or to user-defined symbol set
//

inline bool s_IsAllowedSymbol(unsigned char                   ch,
                              CArgAllow_Symbols::ESymbolClass symbol_class,
                              const string&                   symbol_set)
{
    switch ( symbol_class ) {
    case CArgAllow_Symbols::eAlnum:   return isalnum(ch) != 0;
    case CArgAllow_Symbols::eAlpha:   return isalpha(ch) != 0;
    case CArgAllow_Symbols::eCntrl:   return iscntrl(ch) != 0;
    case CArgAllow_Symbols::eDigit:   return isdigit(ch) != 0;
    case CArgAllow_Symbols::eGraph:   return isgraph(ch) != 0;
    case CArgAllow_Symbols::eLower:   return islower(ch) != 0;
    case CArgAllow_Symbols::ePrint:   return isprint(ch) != 0;
    case CArgAllow_Symbols::ePunct:   return ispunct(ch) != 0;
    case CArgAllow_Symbols::eSpace:   return isspace(ch) != 0;
    case CArgAllow_Symbols::eUpper:   return isupper(ch) != 0;
    case CArgAllow_Symbols::eXdigit:  return isxdigit(ch) != 0;
    case CArgAllow_Symbols::eUser:
        return symbol_set.find_first_of(ch) != NPOS;
    }
    _TROUBLE;  return false;
}


static string s_GetUsageSymbol(CArgAllow_Symbols::ESymbolClass symbol_class,
                               const string&                   symbol_set)
{
    switch ( symbol_class ) {
    case CArgAllow_Symbols::eAlnum:   return "alphanumeric";
    case CArgAllow_Symbols::eAlpha:   return "alphabetic";
    case CArgAllow_Symbols::eCntrl:   return "control symbol";
    case CArgAllow_Symbols::eDigit:   return "decimal";
    case CArgAllow_Symbols::eGraph:   return "graphical symbol";
    case CArgAllow_Symbols::eLower:   return "lower case";
    case CArgAllow_Symbols::ePrint:   return "printable";
    case CArgAllow_Symbols::ePunct:   return "punctuation";
    case CArgAllow_Symbols::eSpace:   return "space";
    case CArgAllow_Symbols::eUpper:   return "upper case";
    case CArgAllow_Symbols::eXdigit:  return "hexadecimal";
    case CArgAllow_Symbols::eUser:
        return "'" + NStr::PrintableString(symbol_set) + "'";
    }
    _TROUBLE;  return kEmptyStr;
}

static string s_GetSymbolClass(CArgAllow_Symbols::ESymbolClass symbol_class)
{
    switch ( symbol_class ) {
    case CArgAllow_Symbols::eAlnum:   return "Alnum";
    case CArgAllow_Symbols::eAlpha:   return "Alpha";
    case CArgAllow_Symbols::eCntrl:   return "Cntrl";
    case CArgAllow_Symbols::eDigit:   return "Digit";
    case CArgAllow_Symbols::eGraph:   return "Graph";
    case CArgAllow_Symbols::eLower:   return "Lower";
    case CArgAllow_Symbols::ePrint:   return "Print";
    case CArgAllow_Symbols::ePunct:   return "Punct";
    case CArgAllow_Symbols::eSpace:   return "Space";
    case CArgAllow_Symbols::eUpper:   return "Upper";
    case CArgAllow_Symbols::eXdigit:  return "Xdigit";
    case CArgAllow_Symbols::eUser:    return "User";
    }
    _TROUBLE;  return kEmptyStr;
}



///////////////////////////////////////////////////////
//  CArgAllow_Symbols::
//

CArgAllow_Symbols::CArgAllow_Symbols(ESymbolClass symbol_class)
    : CArgAllow()
{
    Allow( symbol_class);
    return;
}


CArgAllow_Symbols::CArgAllow_Symbols(const string& symbol_set)
    : CArgAllow()
{
    Allow( symbol_set);
    return;
}


bool CArgAllow_Symbols::Verify(const string& value) const
{
    if (value.length() != 1)
        return false;

    ITERATE( set< TSymClass >, pi,  m_SymClass) {
        if (s_IsAllowedSymbol(value[0], pi->first, pi->second)) {
            return true;
        }
    }
    return false;
}


string CArgAllow_Symbols::GetUsage(void) const
{
    string usage;
    ITERATE( set< TSymClass >, pi,  m_SymClass) {
        if (!usage.empty()) {
            usage += ", or ";
        }
        usage += s_GetUsageSymbol(pi->first, pi->second);
    }

    return "one symbol: " + usage;
}

void CArgAllow_Symbols::PrintUsageXml(CNcbiOstream& out) const
{
    out << "<" << "Symbols" << ">" << endl;
    ITERATE( set< TSymClass >, pi,  m_SymClass) {
        if (pi->first != eUser) {
            s_WriteXmlLine( out, "type", s_GetSymbolClass(pi->first).c_str());
        } else {
            ITERATE( string, p, pi->second) {
                string c;
                s_WriteXmlLine( out, "value", c.append(1,*p).c_str());
            }
        }
    }
    out << "</" << "Symbols" << ">" << endl;
}

CArgAllow_Symbols&
CArgAllow_Symbols::Allow(CArgAllow_Symbols::ESymbolClass symbol_class)
{
    m_SymClass.insert( make_pair(symbol_class, kEmptyStr) );
    return *this;
}

CArgAllow_Symbols& CArgAllow_Symbols::Allow(const string& symbol_set)
{
    m_SymClass.insert( make_pair(eUser, symbol_set ));
    return *this;
}

CArgAllow* CArgAllow_Symbols::Clone(void) const
{
    CArgAllow_Symbols* clone = new CArgAllow_Symbols;
    clone->m_SymClass = m_SymClass;
    return clone;
}



///////////////////////////////////////////////////////
//  CArgAllow_String::
//

CArgAllow_String::CArgAllow_String(ESymbolClass symbol_class)
    : CArgAllow_Symbols(symbol_class)
{
    return;
}


CArgAllow_String::CArgAllow_String(const string& symbol_set)
    : CArgAllow_Symbols(symbol_set)
{
    return;
}


bool CArgAllow_String::Verify(const string& value) const
{
    ITERATE( set< TSymClass >, pi,  m_SymClass) {
        string::const_iterator it;
        for (it = value.begin();  it != value.end(); ++it) {
            if ( !s_IsAllowedSymbol(*it, pi->first, pi->second) )
                break;;
        }
        if (it == value.end()) {
            return true;
        }
    }
    return false;
}


string CArgAllow_String::GetUsage(void) const
{
    string usage;
    ITERATE( set< TSymClass >, pi,  m_SymClass) {
        if (!usage.empty()) {
            usage += ", or ";
        }
        usage += s_GetUsageSymbol(pi->first, pi->second);
    }

    return "to contain only symbols: " + usage;
}


void CArgAllow_String::PrintUsageXml(CNcbiOstream& out) const
{
    out << "<" << "String" << ">" << endl;
    ITERATE( set< TSymClass >, pi,  m_SymClass) {
        if (pi->first != eUser) {
            s_WriteXmlLine( out, "type", s_GetSymbolClass(pi->first).c_str());
        } else {
            s_WriteXmlLine( out, "charset", pi->second.c_str());
        }
    }
    out << "</" << "String" << ">" << endl;
}

CArgAllow* CArgAllow_String::Clone(void) const
{
    CArgAllow_String* clone = new CArgAllow_String;
    clone->m_SymClass = m_SymClass;
    return clone;
}



///////////////////////////////////////////////////////
//  CArgAllow_Strings::
//

CArgAllow_Strings::CArgAllow_Strings(NStr::ECase use_case)
    : CArgAllow(),
      m_Strings(PNocase_Conditional(use_case))
{
    return;
}


CArgAllow_Strings* CArgAllow_Strings::Allow(const string& value)
{
    m_Strings.insert(value);
    return this;
}


bool CArgAllow_Strings::Verify(const string& value) const
{
    TStrings::const_iterator it = m_Strings.find(value);
    return it != m_Strings.end();
}


string 
CArgAllow_Strings::GetUsage(void) const
{
    if ( m_Strings.empty() ) {
        return "ERROR:  Constraint with no values allowed(?!)";
    }

    string str;
    TStrings::const_iterator it = m_Strings.begin();
    for (;;) {
        str += "`";
        str += *it;

        ++it;
        if (it == m_Strings.end()) {
            str += "'";
            if ( m_Strings.key_comp()("a", "A") ) {
                str += "  {case insensitive}";
            }
            break;
        }
        str += "', ";
    }
    return str;
}


void CArgAllow_Strings::PrintUsageXml(CNcbiOstream& out) const
{
    out << "<" << "Strings";
    out << " case_sensitive=\"";
    if ( m_Strings.key_comp()("a", "A") ) {
        out << "false";
    } else {
        out << "true";
    }
    out << "\">" << endl;
    ITERATE( TStrings, p, m_Strings) {
        s_WriteXmlLine( out, "value", (*p).c_str());
    }
    out << "</" << "Strings" << ">" << endl;
}


CArgAllow_Strings& CArgAllow_Strings::AllowValue(const string& value)
{
    return *Allow(value);
}

CArgAllow* CArgAllow_Strings::Clone(void) const
{
    CArgAllow_Strings* clone = new CArgAllow_Strings(m_Strings.key_comp().GetCase());
    clone->m_Strings = m_Strings;
    return clone;
}


///////////////////////////////////////////////////////
//  CArgAllow_Int8s::
//

CArgAllow_Int8s::CArgAllow_Int8s(Int8 x_)
    : CArgAllow()
{
    Allow( x_);
}

CArgAllow_Int8s::CArgAllow_Int8s(Int8 x_min, Int8 x_max)
    : CArgAllow()
{
    AllowRange(x_min, x_max);
}


bool CArgAllow_Int8s::Verify(const string& value) const
{
    Int8 val = s_StringToInt8(value);
    ITERATE( set< TInterval >, pi, m_MinMax) {
        if (pi->first <= val && val<= pi->second) {
            return true;
        }
    }
    return false;
}


string CArgAllow_Int8s::GetUsage(void) const
{
    if (m_MinMax.size() == 1) {
        Int8 x_min = m_MinMax.begin()->first;
        Int8 x_max = m_MinMax.begin()->second;
        if (x_min == x_max) {
            return NStr::Int8ToString(x_min);
        } else if (x_min == kMin_I8 && x_max != kMax_I8) {
            return string("less or equal to ") + NStr::Int8ToString(x_max);
        } else if (x_min != kMin_I8 && x_max == kMax_I8) {
            return string("greater or equal to ") + NStr::Int8ToString(x_min);
        } else if (x_min == kMin_I8 && x_max == kMax_I8) {
            return kEmptyStr;
        }
    }
    string usage;
    ITERATE( set< TInterval >, pi, m_MinMax) {
        if (!usage.empty()) {
            usage += ", ";
        }
        if (pi->first == pi->second) {
            usage += NStr::Int8ToString(pi->first);
        } else {
            usage += NStr::Int8ToString(pi->first) + ".." + NStr::Int8ToString(pi->second);
        }

    }
    return usage;
}


void CArgAllow_Int8s::PrintUsageXml(CNcbiOstream& out) const
{
    string tag("Int8s");
    if (dynamic_cast<const CArgAllow_Integers*>(this) != 0) {
        tag = "Integers";
    }
    out << "<" << tag << ">" << endl;
    ITERATE( set< TInterval >, pi, m_MinMax) {
        s_WriteXmlLine( out, "min", NStr::Int8ToString(pi->first).c_str());
        s_WriteXmlLine( out, "max", NStr::Int8ToString(pi->second).c_str());
    }
    out << "</" << tag << ">" << endl;
}

CArgAllow_Int8s& CArgAllow_Int8s::AllowRange(Int8 from, Int8 to)
{
    m_MinMax.insert( make_pair(from,to) );
    return *this;
}

CArgAllow_Int8s& CArgAllow_Int8s::Allow(Int8 value)
{
    m_MinMax.insert( make_pair(value,value) );
    return *this;
}

CArgAllow* CArgAllow_Int8s::Clone(void) const
{
    CArgAllow_Int8s* clone = new CArgAllow_Int8s;
    clone->m_MinMax = m_MinMax;
    return clone;
}



///////////////////////////////////////////////////////
//  CArgAllow_Integers::
//

CArgAllow_Integers::CArgAllow_Integers(int x_)
    : CArgAllow_Int8s(x_)
{
}

CArgAllow_Integers::CArgAllow_Integers(int x_min, int x_max)
    : CArgAllow_Int8s(x_min, x_max)
{
}

string CArgAllow_Integers::GetUsage(void) const
{
    if (m_MinMax.size() == 1) {
        Int8 x_min = m_MinMax.begin()->first;
        Int8 x_max = m_MinMax.begin()->second;
        if (x_min == x_max) {
            return NStr::Int8ToString(x_min);
        } else if (x_min == kMin_Int && x_max != kMax_Int) {
            return string("less or equal to ") + NStr::Int8ToString(x_max);
        } else if (x_min != kMin_Int && x_max == kMax_Int) {
            return string("greater or equal to ") + NStr::Int8ToString(x_min);
        } else if (x_min == kMin_Int && x_max == kMax_Int) {
            return kEmptyStr;
        }
    }
    return CArgAllow_Int8s::GetUsage();;
}

CArgAllow* CArgAllow_Integers::Clone(void) const
{
    CArgAllow_Integers* clone = new CArgAllow_Integers;
    clone->m_MinMax = m_MinMax;
    return clone;
}


///////////////////////////////////////////////////////
//  CArgAllow_Doubles::
//

CArgAllow_Doubles::CArgAllow_Doubles(double x_value)
    : CArgAllow()
{
    Allow(x_value);
}

CArgAllow_Doubles::CArgAllow_Doubles(double x_min, double x_max)
    : CArgAllow()
{
    AllowRange( x_min, x_max );
}


bool CArgAllow_Doubles::Verify(const string& value) const
{
    double val = NStr::StringToDouble(value, NStr::fDecimalPosixOrLocal);
    ITERATE( set< TInterval >, pi, m_MinMax) {
        if (pi->first <= val && val<= pi->second) {
            return true;
        }
    }
    return false;
}


string CArgAllow_Doubles::GetUsage(void) const
{
    if (m_MinMax.size() == 1) {
        double x_min = m_MinMax.begin()->first;
        double x_max = m_MinMax.begin()->second;
        if (x_min == x_max) {
            return NStr::DoubleToString(x_min);
        } else if (x_min == kMin_Double && x_max != kMax_Double) {
            return string("less or equal to ") + NStr::DoubleToString(x_max);
        } else if (x_min != kMin_Double && x_max == kMax_Double) {
            return string("greater or equal to ") + NStr::DoubleToString(x_min);
        } else if (x_min == kMin_Double && x_max == kMax_Double) {
            return kEmptyStr;
        }
    }
    string usage;
    ITERATE( set< TInterval >, pi, m_MinMax) {
        if (!usage.empty()) {
            usage += ", ";
        }
        if (pi->first == pi->second) {
            usage += NStr::DoubleToString(pi->first);
        } else {
            usage += NStr::DoubleToString(pi->first) + ".." + NStr::DoubleToString(pi->second);
        }

    }
    return usage;
}


void CArgAllow_Doubles::PrintUsageXml(CNcbiOstream& out) const
{
    out << "<" << "Doubles" << ">" << endl;
    ITERATE( set< TInterval >, pi, m_MinMax) {
        s_WriteXmlLine( out, "min", NStr::DoubleToString(pi->first).c_str());
        s_WriteXmlLine( out, "max", NStr::DoubleToString(pi->second).c_str());
    }
    out << "</" << "Doubles" << ">" << endl;
}

CArgAllow_Doubles& CArgAllow_Doubles::AllowRange(double from, double to)
{
    m_MinMax.insert( make_pair(from,to) );
    return *this;
}

CArgAllow_Doubles& CArgAllow_Doubles::Allow(double value)
{
    m_MinMax.insert( make_pair(value,value) );
    return *this;
}

CArgAllow* CArgAllow_Doubles::Clone(void) const
{
    CArgAllow_Doubles* clone = new CArgAllow_Doubles;
    clone->m_MinMax = m_MinMax;
    return clone;
}


///////////////////////////////////////////////////////
// CArgException

const char* CArgException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eInvalidArg: return "eInvalidArg";
    case eNoValue:    return "eNoValue";
    case eExcludedValue: return "eExcludedValue";
    case eWrongCast:  return "eWrongCast";
    case eConvert:    return "eConvert";
    case eNoFile:     return "eNoFile";
    case eConstraint: return "eConstraint";
    case eArgType:    return "eArgType";
    case eNoArg:      return "eNoArg";
    case eSynopsis:   return "eSynopsis";
    default:    return CException::GetErrCodeString();
    }
}

const char* CArgHelpException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eHelp:     return "eHelp";
    case eHelpFull: return "eHelpFull";
    case eHelpXml:  return "eHelpXml";
    default:    return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
