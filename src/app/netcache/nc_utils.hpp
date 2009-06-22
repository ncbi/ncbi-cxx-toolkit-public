#ifndef NETCACHE_NC_UTILS__HPP
#define NETCACHE_NC_UTILS__HPP
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
 * Authors:  Pavel Ivanov
 *
 * File Description: 
 *   Utility classes that now are used only in NetCache but can be used
 *   anywhere else.
 */

#include <corelib/ncbidiag.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbistr.hpp>


BEGIN_NCBI_SCOPE


/// Stream-like class to accumulate all in one string without actual
/// streams-related overhead. Can be automatically converted to string or
/// CTempString.
class CQuickStrStream
{
public:
    /// Clear the accumulated value and start from the beginning
    void Clear(void);

    /// Streaming operator for all supported data types
    CQuickStrStream& operator<<(const string& str);
    CQuickStrStream& operator<<(const char*   str);
    CQuickStrStream& operator<<(CTempString   str);
    CQuickStrStream& operator<<(int           i);
    CQuickStrStream& operator<<(Uint4         ui);
    CQuickStrStream& operator<<(Int8          i64);
    CQuickStrStream& operator<<(Uint8         ui64);
    CQuickStrStream& operator<<(double        d);

    /// Automatic conversion to string
    operator string(void);
    /// Automatic conversion to CTempString
    operator CTempString(void);

private:
    /// Accumulated data
    string m_Data;
};


/// Stream-like class to help unify printing some text messages to diagnostics
/// and to any iostream. Class accumulates line of text end then flushes it
/// to diagnostics by LOG_POST or to iostream by operator<<. End of line is
/// recognized only by streaming std::endl into this class. Other text is not
/// analyzed for characters '\n' or alike. In case of printing to diagnostics
/// all text is printed under the same request number and it's assumed that
/// no other output gets to diagnostics between object creation and deletion.
/// Otherwise other ouput will interfere with output from this object. Also
/// it's assumed that object is used from the same thread where it's created
/// and deleted.
class CPrintTextProxy
{
public:
    /// Type of media to print to
    enum EPrintOutput {
        ePrintLog,     ///< Print to diagnostics
        ePrintStream   ///< Print to iostream
    };

    /// Create object and set output type and stream if necessary.
    /// If stream == NULL then output type is set to ePrintLog automatically.
    CPrintTextProxy(EPrintOutput output, CNcbiIostream* stream = NULL);

    ~CPrintTextProxy(void);

    /// Universal streaming operator
    template <class T>
    CPrintTextProxy& operator<< (T x);


    typedef CNcbiOstream& (*TEndlType)(CNcbiOstream&);

    /// Special streaming operator for std::endl
    CPrintTextProxy& operator<< (TEndlType endl_func);

private:
    CPrintTextProxy(const CPrintTextProxy&);
    CPrintTextProxy& operator= (const CPrintTextProxy&);


    /// Type of media to print to
    EPrintOutput          m_PrintMode;
    /// Stream to direct the output to
    CNcbiIostream*        m_PrintStream;
    /// Diagnostics context that was set before this object was created
    CRef<CRequestContext> m_OldContext;
    /// Accumulator of the current output line
    CQuickStrStream       m_LineStream;
};



/// Utility function to safely do division even if divisor is 0
template <class TLeft, class TRight>
TLeft g_SafeDiv(TLeft left, TRight right)
{
    return right == 0? 0: left / right;
}



inline void
CQuickStrStream::Clear(void)
{
    m_Data.clear();
}

inline CQuickStrStream&
CQuickStrStream::operator<<(const string& str)
{
    m_Data += str;
    return *this;
}

inline CQuickStrStream&
CQuickStrStream::operator<<(const char* str)
{
    m_Data += str;
    return *this;
}

inline CQuickStrStream&
CQuickStrStream::operator<<(CTempString str)
{
    m_Data.append(str.data(), str.size());
    return *this;
}

inline CQuickStrStream&
CQuickStrStream::operator<<(int i)
{
    m_Data += NStr::IntToString(i);
    return *this;
}

inline CQuickStrStream&
CQuickStrStream::operator<<(Uint4 ui)
{
    m_Data += NStr::UIntToString(ui);
    return *this;
}

inline CQuickStrStream&
CQuickStrStream::operator<<(Int8 i64)
{
    m_Data += NStr::Int8ToString(i64);
    return *this;
}

inline CQuickStrStream&
CQuickStrStream::operator<<(Uint8 ui64)
{
    m_Data += NStr::UInt8ToString(ui64);
    return *this;
}

inline CQuickStrStream&
CQuickStrStream::operator<<(double d)
{
    m_Data += NStr::DoubleToString(d);
    return *this;
}

inline
CQuickStrStream::operator string(void)
{
    return m_Data;
}

inline
CQuickStrStream::operator CTempString(void)
{
    return m_Data;
}



inline
CPrintTextProxy::CPrintTextProxy(EPrintOutput   output,
                                 CNcbiIostream* stream /* = NULL */)
    : m_PrintMode(stream? output: ePrintLog),
      m_PrintStream(stream),
      m_OldContext(NULL)
{
    if (m_PrintMode == ePrintLog) {
        m_OldContext.Reset(&CDiagContext::GetRequestContext());
        AutoPtr<CRequestContext> new_context = new CRequestContext();
        new_context->SetRequestID();
        CDiagContext::SetRequestContext(new_context.release());
    }
}

inline
CPrintTextProxy::~CPrintTextProxy(void)
{
    if (m_PrintMode == ePrintLog) {
        CDiagContext::SetRequestContext(m_OldContext);
    }
}

template <class T>
inline CPrintTextProxy&
CPrintTextProxy::operator<< (T x)
{
    m_LineStream << x;
    return *this;
}

inline CPrintTextProxy&
CPrintTextProxy::operator<< (TEndlType)
{
    switch (m_PrintMode) {
    case ePrintLog:
        LOG_POST(string(m_LineStream));
        break;
    case ePrintStream:
        (*m_PrintStream) << string(m_LineStream) << endl;
        break;
    }
    m_LineStream.Clear();

    return *this;
}

END_NCBI_SCOPE

#endif /* NETCACHE_NC_UTILS__HPP */
