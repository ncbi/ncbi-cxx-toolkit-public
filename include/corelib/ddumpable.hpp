#ifndef DDUMPABLE__HPP
#define DDUMPABLE__HPP

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
 * Author:  Andrei Gourianov
 *
 * File Description:
 *      Debug Dump functionality
 *
 */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE


//---------------------------------------------------------------------------
//  CDebugDumpFormatter defines debug dump formatter interface
 
class NCBI_XNCBI_EXPORT CDebugDumpFormatter
{
public:
    enum EValueType {
        eValue,
        eString,
        ePointer
    };
public:
    virtual ~CDebugDumpFormatter() { }

    virtual bool StartBundle(unsigned int level, const string& bundle) = 0;
    virtual void EndBundle(  unsigned int level, const string& bundle) = 0;

    virtual bool StartFrame( unsigned int level, const string& frame) = 0;
    virtual void EndFrame(   unsigned int level, const string& frame) = 0;

    virtual void PutValue(   unsigned int level, const string& name,
                             const string& value, EValueType type,
                             const string& comment) = 0;
};


//---------------------------------------------------------------------------
//  CDebugDumpContext provides client interface in the form [name=value]

class CDebugDumpable;
class NCBI_XNCBI_EXPORT CDebugDumpContext
{
public:
    CDebugDumpContext(CDebugDumpFormatter& formatter, const string& bundle);
    // This is not exactly a copy constructor -
    // this mechanism is used internally to find out
    // where are we on the Dump tree
    CDebugDumpContext(CDebugDumpContext& ddc);
    CDebugDumpContext(CDebugDumpContext& ddc, const string& bundle);
    virtual ~CDebugDumpContext(void);

public:
    // First thing in DebugDump() function - call this function
    // providing class type as the frame name
    void SetFrame(const string& frame);
    // Log data in the form [name, data, comment]
    // All data is passed to a formatter as string, still sometimes
    // it is probably worth to emphasize that the data is REALLY a string
    void Log(const string& name, const char* value, 
             CDebugDumpFormatter::EValueType type=CDebugDumpFormatter::eValue,
             const string& comment = kEmptyStr);
    void Log(const string& name, const string& value, 
             CDebugDumpFormatter::EValueType type=CDebugDumpFormatter::eValue,
             const string& comment = kEmptyStr);
    void Log(const string& name, bool value,
             const string& comment = kEmptyStr);
    void Log(const string& name, short value,
             const string& comment = kEmptyStr);
    void Log(const string& name, unsigned short value,
             const string& comment = kEmptyStr);
    void Log(const string& name, int value,
             const string& comment = kEmptyStr);
    void Log(const string& name, unsigned int value,
             const string& comment = kEmptyStr);
    void Log(const string& name, long value,
             const string& comment = kEmptyStr);
    void Log(const string& name, unsigned long value,
             const string& comment = kEmptyStr);
#if (SIZEOF_LONG < 8)
    void Log(const string& name, Int8 value,
             const string& comment = kEmptyStr);
    void Log(const string& name, Uint8 value,
             const string& comment = kEmptyStr);
#endif
    void Log(const string& name, double value,
             const string& comment = kEmptyStr);
    void Log(const string& name, const void* value,
             const string& comment = kEmptyStr);
    void Log(const string& name, const CDebugDumpable* value,
             unsigned int depth);

private:
    void x_VerifyFrameStarted(void);
    void x_VerifyFrameEnded(void);

    CDebugDumpContext&   m_Parent;
    CDebugDumpFormatter& m_Formatter;
    unsigned int         m_Level;
    bool                 m_Start_Bundle;
    string               m_Title; 
    bool                 m_Started;

};


//---------------------------------------------------------------------------
//  CDebugDumpable defines DebugDump() functionality (abstract base class)

class NCBI_XNCBI_EXPORT CDebugDumpable
{
public:
    CDebugDumpable(void) {}
    virtual ~CDebugDumpable(void);

    // Enable/disable debug dump
    static void EnableDebugDump(bool on);

    // Dump using text formatter
    void DebugDumpText(ostream& out,
                       const string& bundle, unsigned int depth) const;
    // Dump using external dump formatter
    void DebugDumpFormat(CDebugDumpFormatter& ddf, 
                   const string& bundle, unsigned int depth) const;

    // Function that does the dump - to be overloaded
    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const = 0;

private:
    static bool sm_DumpEnabled;
};


//---------------------------------------------------------------------------
//  CDebugDumpFormatterText defines text debug dump formatter class

class NCBI_XNCBI_EXPORT CDebugDumpFormatterText : public CDebugDumpFormatter
{
public:
    CDebugDumpFormatterText(ostream& out);
    virtual ~CDebugDumpFormatterText(void);

public:
    virtual bool StartBundle(unsigned int level, const string& bundle);
    virtual void EndBundle(  unsigned int level, const string& bundle);

    virtual bool StartFrame( unsigned int level, const string& frame);
    virtual void EndFrame(   unsigned int level, const string& frame);

    virtual void PutValue(   unsigned int level, const string& name,
                             const string& value, EValueType type,
                             const string& comment);

private:
    void x_IndentLine(unsigned int level, char c = ' ', unsigned int len = 2);
    void x_InsertPageBreak(const string& title = kEmptyStr,
                           char c = '=', unsigned int len = 78);

    ostream& m_Out;
};



/****************************************************************************
        Collection of debug dump function templates
****************************************************************************/
 
//---------------------------------------------------------------------------
// Value

// Log a "simple" value
// "Simple" means that output stream understands how to dump it
// (i.e. operator 'os<<value' makes sense)
template<class T>
void DebugDumpValue( CDebugDumpContext& _this, const string& name,
    const T& value, const string& comment = kEmptyStr)
{
    ostrstream os;
    os << value << '\0';
    _this.Log(name, string(os.str()), CDebugDumpFormatter::eValue, comment);
}


//---------------------------------------------------------------------------
// non-associative STL containers

// Log range of pointers to dumpable objects
template<class T>
void DebugDumpRangePtr( CDebugDumpContext& _this, const string& name,
    T it, T it_end, unsigned int depth)
{
    if (depth == 0) {
        return;
    }
    --depth;
    // container is an object - so,
    // to start a sub-bundle we create a new CDebugDumpContext
    CDebugDumpContext ddc2(_this,name);
    for ( int n=0; it != it_end; ++it, ++n) {
        string member_name = name + "[ " + NStr::IntToString(n) + " ]";
        ddc2.Log(member_name, (*it), depth);
    }
}


// Log range of dumpable objects
template<class T>
void DebugDumpRangeObj( CDebugDumpContext& _this, const string& name,
    T it, T it_end, unsigned int depth)
{
    if (depth == 0) {
        return;
    }
    --depth;
    CDebugDumpContext ddc2(_this,name);
    for ( int n=0; it != it_end; ++it, ++n) {
        string member_name = name + "[ " + NStr::IntToString(n) + " ]";
        ddc2.Log(member_name, &(*it), depth);
    }
}


// Log range of CRefs to dumpable objects
template<class T>
void DebugDumpRangeCRef( CDebugDumpContext& _this, const string& name,
    T it, T it_end, unsigned int depth)
{
    if (depth == 0) {
        return;
    }
    --depth;
    CDebugDumpContext ddc2(_this,name);
    for ( int n=0; it != it_end; ++it, ++n) {
        string member_name = name + "[ " + NStr::IntToString(n) + " ]";
        ddc2.Log(member_name, (*it).GetPointer(), depth);
    }
}

//---------------------------------------------------------------------------
// associative STL containers

// Log range of pairs of pointers (map of ptr to ptr)
// "second" is a pointer to a dumpable objects
template<class T>
void DebugDumpPairsPtrPtr( CDebugDumpContext& _this, const string& name,
    T it, T it_end, unsigned int depth)
{
    if (depth == 0) {
        return;
    }
    --depth;
    CDebugDumpContext ddc2(_this,name);
    for ( int n=0; it != it_end; ++it, ++n) {
        string member_name = name + "[ " + NStr::PtrToString(
            dynamic_cast<const CDebugDumpable*>(it->first)) + " ]";
        ddc2.Log(member_name, it->second, depth);
    }
}


// Log range of pairs of pointers (map of ptr to CRef)
// "second" is a pointer to a dumpable objects
template<class T>
void DebugDumpPairsPtrCRef( CDebugDumpContext& _this, const string& name,
    T it, T it_end, unsigned int depth)
{
    if (depth == 0) {
        return;
    }
    --depth;
    CDebugDumpContext ddc2(_this,name);
    for ( int n=0; it != it_end; ++it, ++n) {
        string member_name = name + "[ " + NStr::PtrToString(
            dynamic_cast<const CDebugDumpable*>(it->first)) + " ]";
        ddc2.Log(member_name,(it->second).GetPointer(), depth);
    }
}


// Log range of pairs of pointers (map of CRef to CRef)
// "second" is a CRef to a dumpable objects
template<class T>
void DebugDumpPairsCRefCRef( CDebugDumpContext& _this, const string& name,
    T it, T it_end, unsigned int depth)
{
    if (depth == 0) {
        return;
    }
    --depth;
    CDebugDumpContext ddc2(_this,name);
    for ( int n=0; it != it_end; ++it, ++n) {
        string member_name = name + "[ " + NStr::PtrToString(
            dynamic_cast<const CDebugDumpable*>((it->first).GetPointer()))
            + " ]";
        ddc2.Log(member_name,(it->second).GetPointer(), depth);
    }
}

// Log range of pairs of pointers
// "first" can be serialized into ostream
// "second" is a pointer to a dumpable objects
template<class T>
void DebugDumpPairsValuePtr( CDebugDumpContext& _this, const string& name,
    T it, T it_end, unsigned int depth)
{
    if (depth == 0) {
        return;
    }
    --depth;
    CDebugDumpContext ddc2(_this,name);
    for ( int n=0; it != it_end; ++it, ++n) {
        ostrstream os;
        os << (it->first) << '\0';
        string member_name = name + "[ " + os.str() + " ]";
        ddc2.Log(member_name,it->second, depth);
    }
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2005/05/09 18:45:08  ucko
 * Ensure that widely-included classes with virtual methods have virtual dtors.
 *
 * Revision 1.9  2003/12/09 21:13:27  ucko
 * Re-tweak logging of 64-bit numbers, as the previous revision broke
 * when __int64 and long long were synonymous.  (Now uses Int8/Uint8
 * everywhere, but only when sizeof(long) < 8.)
 *
 * Revision 1.8  2003/12/09 17:25:51  gouriano
 * Corrected previously defined Log() functions to avoid conflicts
 * on 64 bit platforms
 *
 * Revision 1.7  2003/12/09 16:55:12  gouriano
 * Added Log() methods for Int8 and Uint8
 *
 * Revision 1.6  2003/11/12 20:15:23  gouriano
 * Added more Log() methods: for short, int, and char* types
 *
 * Revision 1.5  2003/05/19 21:05:39  vakatov
 * x_InsertPageBreak() --  title: "" -> kEmptyStr,  len:  int -> unsigned int.
 * Fixed code indentation and simplified some code.
 *
 * Revision 1.4  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.3  2002/05/29 21:15:08  gouriano
 * changed formatter interface: added value type
 *
 * Revision 1.2  2002/05/28 17:57:11  gouriano
 * all DebugDump-related classes were put in one header
 * templates adjusted and debugged on multiple platforms
 *
 * Revision 1.1  2002/05/17 14:25:39  gouriano
 * added DebugDump base class and function to CObject
 *
 *
 *
 * ===========================================================================
 */

#endif // DDUMPABLE__HPP
