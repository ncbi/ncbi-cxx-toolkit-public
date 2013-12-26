#ifndef NETCACHE__SRV_STAT__HPP
#define NETCACHE__SRV_STAT__HPP
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
 */


BEGIN_NCBI_SCOPE


struct SMMStat;


/// Stream-like class to accumulate all in one string without actual
/// streams-related overhead. Can be automatically converted to string or
/// CTempString.
class CSrvStrStream
{
public:
    /// Clear the accumulated value and start from the beginning
    void Clear(void);

    /// Streaming operator for all supported data types
    CSrvStrStream& operator<<(const string&          str);
    CSrvStrStream& operator<<(const char*            str);
    CSrvStrStream& operator<<(CTempString            str);
    CSrvStrStream& operator<<(int                    i);
    CSrvStrStream& operator<<(unsigned int           ui);
    CSrvStrStream& operator<<(long                   l);
    CSrvStrStream& operator<<(unsigned long          ul);
#if !NCBI_INT8_IS_LONG
    CSrvStrStream& operator<<(Int8                   i64);
    CSrvStrStream& operator<<(Uint8                  ui64);
#endif
    CSrvStrStream& operator<<(double                 d);
    CSrvStrStream& operator<<(const CSrvStrStream& str);

    /// Automatic conversion to string
    operator const string& (void) const;
    /// Automatic conversion to CTempString
    operator CTempString(void) const;

private:
    /// Accumulated data
    string m_Data;
};


/// Stream-like class to help unify printing some text messages to diagnostics
/// and to any socket. Class accumulates line of text end then flushes it
/// to diagnostics by INFO or to iostream by operator<<. End of line is
/// recognized only by streaming std::endl into this class. Other text is not
/// analyzed for characters '\n' or alike. In case of printing to diagnostics
/// all text is printed under the same request number and it's assumed that
/// no other output gets to diagnostics between object creation and deletion.
/// Otherwise other output will interfere with output from this object. Also
/// it's assumed that object is used from the same thread where it's created
/// and deleted.
class CSrvPrintProxy
{
public:
    /// Type of media to print to
    enum EPrintOutput {
        ePrintLog,     ///< Print to diagnostics
        ePrintSocket   ///< Print to socket
    };

    CSrvPrintProxy(CSrvSocketTask* sock);
    CSrvPrintProxy(CRequestContext* ctx);

    ~CSrvPrintProxy(void);

    /// Universal streaming operator
    template <class T>
    CSrvPrintProxy& operator<< (T x);


    typedef CNcbiOstream& (*TEndlType)(CNcbiOstream&);

    /// Special streaming operator for std::endl
    CSrvPrintProxy& operator<< (TEndlType endl_func);

private:
    CSrvPrintProxy(const CSrvPrintProxy&);
    CSrvPrintProxy& operator= (const CSrvPrintProxy&);


    /// Type of media to print to
    EPrintOutput    m_PrintMode;
    bool            m_LineStarted;
    CSrvStrStream   m_StrStream;
    CSrvSocketTask* m_Socket;
    CSrvDiagMsg     m_DiagMsg;
    CRequestContext* m_DiagCtx;
};


/// Class representing one statistical value.
/// Object collects set of values and can return number of values in set,
/// sum of all values, maximum value and average of all values.
template <class T>
class CSrvStatTerm
{
public:
    /// Empty constructor, all initialization should be made in Initialize()
    /// because in memory manager static objects are used before constructors
    /// are called.
    CSrvStatTerm   (void);
    /// Initialize all data members
    void Initialize (void);

    /// Add next value into the set.
    void  AddValue  (T value);

    /// Get number of values in the set.
    Uint8 GetCount  (void) const;
    /// Get sum of all values in the set.
    /// The return type is Uint8 because the class is not used with doubles.
    /// If it ever will collect doubles the return type should be adjusted
    /// smartly (to be double for T==double and Uint8 for all unsigned integers).
    Uint8 GetSum    (void) const;
    /// Get maximum value in the set.
    T     GetMaximum(void) const;
    /// Get average of all values in the set.
    T     GetAverage(void) const;
    double GetDoubleAvg(void) const;

    /// Add all values from another set.
    void  AddValues (const CSrvStatTerm<T>& other);

private:
    /// Sum of all values collected.
    /// The type is Uint8 because the class is not used with doubles. If it
    /// ever will collect doubles the type of m_ValueSum should be adjusted
    /// smartly (to be double for T==double and Uint8 for all unsigned integers).
    Uint8  m_ValuesSum;
    /// Number of all values collected.
    Uint8  m_ValuesCount;
    /// Maximum value among collected.
    T      m_ValuesMax;
};


typedef CSrvStatTerm<Uint8> TSrvTimeTerm;

template <class Map, class Key>
inline TSrvTimeTerm&
g_SrvTimeTerm(Map& map, Key key)
{
    typename Map::iterator it_time = map.find(key);
    if (it_time != map.end())
        return it_time->second;
    TSrvTimeTerm& value = map[key];
    value.Initialize();
    return value;
}


struct SSrvStateStat
{
    Uint4 cnt_threads;
    Uint4 cnt_sockets;
};


class CSrvStat
{
public:
    CSrvStat(void);
    ~CSrvStat(void);

    void InitStartState(void);
    void TransferEndState(CSrvStat* src_stat);
    void CopyStartState(CSrvStat* src_stat);
    void CopyEndState(CSrvStat* src_stat);
    void SaveEndState(void);
    void AddAllStats(CSrvStat* src_stat);
    void CollectThreads(bool need_clear);
    void SaveEndStateStat(void);

    void PrintToLogs(CRequestContext* ctx, CSrvPrintProxy& proxy);
    void PrintToSocket(CSrvPrintProxy& proxy);
    void PrintState(CSrvSocketTask& sock);

public:
    void SchedJiffyStat(Uint8 jiffy_len, Uint8 max_slice,
                        Uint8 exec_time, Uint8 done_tasks, Uint8 wait_time);
    void ThreadStarted(void);
    void ThreadStopped(void);
    void SockOpenActive(void);
    void SockOpenPassive(void);
    void SockClose(int status, Uint8 open_time);
    void ErrorOnSocket(void);

    void SetMMStat(SMMStat* stat);

private:
    CSrvStat(const CSrvStat&);
    CSrvStat& operator= (const CSrvStat&);

    void x_ClearStats(void);
    void x_AddStats(CSrvStat* src_stat);
    void x_PrintUnstructured(CSrvPrintProxy& proxy);


    typedef map<int, TSrvTimeTerm>   TStatusOpenTimes;


    CMiniMutex m_StatLock;
    SSrvStateStat m_StartState;
    SSrvStateStat m_EndState;
    TSrvTimeTerm m_JiffyTime;
    Uint8 m_MaxSlice;
    Uint8 m_ExecTime;
    Uint8 m_WaitTime;
    CSrvStatTerm<Uint8> m_DoneTasks;
    Uint8 m_ThrStarted;
    Uint8 m_ThrStopped;
    Uint8 m_SockActiveOpens;
    Uint8 m_SockPassiveOpens;
    Uint8 m_SockErrors;
    TSrvTimeTerm m_SockOpenTime;
    TStatusOpenTimes m_SocksByStatus;
    CSrvStatTerm<Uint4> m_CntThreads;
    CSrvStatTerm<Uint4> m_CntSockets;
    auto_ptr<SMMStat> m_MMStat;
};


inline Uint8
g_CalcStatPct(Uint8 val, Uint8 total)
{
    return total == 0? 0: val * 100 / total;
}

inline string
g_ToSizeStr(Uint8 size)
{
    return NStr::UInt8ToString_DataSize(size,
            NStr::fDS_PutSpaceBeforeSuffix | NStr::fDS_PutBSuffixToo);
}

inline string
g_ToSmartStr(Uint8 num)
{
    return NStr::UInt8ToString(num, NStr::fWithCommas);
}

inline double
g_AsMSecStat(Uint8 time_usec)
{
    return double(time_usec) / kUSecsPerMSec;
}


/// Get integer part of the logarithm with base 2.
/// In other words function returns index of the greatest bit set when bits
/// are indexed from lowest to highest starting with 0 (e.g. for binary number
/// 10010001 it will return 7).
inline unsigned int
g_GetLogBase2(Uint8 value)
{
    unsigned int result = 0;
    if (value > 0xFFFFFFFF) {
        value >>= 32;
        result += 32;
    }
    if (value > 0xFFFF) {
        value >>= 16;
        result += 16;
    }
    if (value > 0xFF) {
        value >>= 8;
        result += 8;
    }
    if (value > 0xF) {
        value >>= 4;
        result += 4;
    }
    if (value > 0x3) {
        value >>= 2;
        result += 2;
    }
    if (value > 0x1)
        ++result;
    return result;
}



//////////////////////////////////////////////////////////////////////////
// Inline methods
//////////////////////////////////////////////////////////////////////////

inline void
CSrvStrStream::Clear(void)
{
    m_Data.clear();
}

inline CSrvStrStream&
CSrvStrStream::operator<<(const string& str)
{
    m_Data += str;
    return *this;
}

inline CSrvStrStream&
CSrvStrStream::operator<<(const char* str)
{
    m_Data += str;
    return *this;
}

inline CSrvStrStream&
CSrvStrStream::operator<<(CTempString str)
{
    m_Data.append(str.data(), str.size());
    return *this;
}

inline CSrvStrStream&
CSrvStrStream::operator<<(int i)
{
    m_Data += NStr::IntToString(i);
    return *this;
}

inline CSrvStrStream&
CSrvStrStream::operator<<(unsigned int ui)
{
    m_Data += NStr::UIntToString(ui);
    return *this;
}

inline CSrvStrStream&
CSrvStrStream::operator<<(long l)
{
    m_Data += NStr::Int8ToString(l);
    return *this;
}

inline CSrvStrStream&
CSrvStrStream::operator<<(unsigned long ul)
{
    m_Data += NStr::UInt8ToString(ul);
    return *this;
}

#if !NCBI_INT8_IS_LONG

inline CSrvStrStream&
CSrvStrStream::operator<<(Int8 i64)
{
    m_Data += NStr::Int8ToString(i64);
    return *this;
}

inline CSrvStrStream&
CSrvStrStream::operator<<(Uint8 ui64)
{
    m_Data += NStr::UInt8ToString(ui64);
    return *this;
}

#endif

inline CSrvStrStream&
CSrvStrStream::operator<<(double d)
{
    m_Data += NStr::DoubleToString(d);
    return *this;
}

inline CSrvStrStream&
CSrvStrStream::operator<<(const CSrvStrStream& str)
{
    m_Data += str.m_Data;
    return *this;
}

inline
CSrvStrStream::operator const string& (void) const
{
    return m_Data;
}

inline
CSrvStrStream::operator CTempString(void) const
{
    return m_Data;
}


inline CNcbiOstream&
operator<<(CNcbiOstream& stream, const CSrvStrStream& str)
{
    stream << string(str);
    return stream;
}



inline
CSrvPrintProxy::CSrvPrintProxy(CSrvSocketTask* sock)
    : m_PrintMode(ePrintSocket),
      m_LineStarted(false),
      m_Socket(sock)
{}

inline
CSrvPrintProxy::CSrvPrintProxy(CRequestContext* ctx)
    : m_PrintMode(ePrintLog),
      m_LineStarted(false),
      m_DiagCtx(ctx)
{}

inline
CSrvPrintProxy::~CSrvPrintProxy(void)
{}

template <class T>
inline CSrvPrintProxy&
CSrvPrintProxy::operator<< (T x)
{
    switch (m_PrintMode) {
    case ePrintLog:
        if (!m_LineStarted) {
            m_DiagMsg.StartInfo(m_DiagCtx);
            m_LineStarted = true;
        }
        m_DiagMsg << x;
        break;
    case ePrintSocket:
        m_StrStream << x;
        break;
    }
    return *this;
}

inline CSrvPrintProxy&
CSrvPrintProxy::operator<< (TEndlType)
{
    switch (m_PrintMode) {
    case ePrintLog:
        if (m_LineStarted) {
            m_DiagMsg.Flush();
            m_LineStarted = false;
        }
        break;
    case ePrintSocket:
        m_Socket->WriteText(string(m_StrStream)).WriteText("\n");
        m_StrStream.Clear();
        break;
    }

    return *this;
}


template <class T>
inline
CSrvStatTerm<T>::CSrvStatTerm(void)
{}

template <class T>
inline void
CSrvStatTerm<T>::Initialize(void)
{
    m_ValuesSum   = 0;
    m_ValuesCount = 0;
    m_ValuesMax   = 0;
}

template <class T>
inline void
CSrvStatTerm<T>::AddValue(T value)
{
    m_ValuesSum += value;
    ++m_ValuesCount;
    m_ValuesMax = max(m_ValuesMax, value);
}

template <class T>
inline void
CSrvStatTerm<T>::AddValues(const CSrvStatTerm<T>& other)
{
    m_ValuesSum   += other.m_ValuesSum;
    m_ValuesCount += other.m_ValuesCount;
    m_ValuesMax    = max(other.m_ValuesMax, m_ValuesMax);
}

template <class T>
inline Uint8
CSrvStatTerm<T>::GetCount(void) const
{
    return m_ValuesCount;
}

template <class T>
inline Uint8
CSrvStatTerm<T>::GetSum(void) const
{
    return m_ValuesSum;
}

template <class T>
inline T
CSrvStatTerm<T>::GetMaximum(void) const
{
    return m_ValuesMax;
}

template <class T>
inline T
CSrvStatTerm<T>::GetAverage(void) const
{
    return m_ValuesCount == 0? 0: T(m_ValuesSum / m_ValuesCount);
}

template <class T>
inline double
CSrvStatTerm<T>::GetDoubleAvg(void) const
{
    return m_ValuesCount == 0? 0: double(m_ValuesSum) / m_ValuesCount;
}

END_NCBI_SCOPE

#endif /* NETCACHE__SRV_STAT__HPP */
