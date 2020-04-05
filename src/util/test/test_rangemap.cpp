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
 * Author: Eugene Vasilchenko
 *
 * File Description:
 *   Test program for CRangeMap<>
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiutil.hpp>
#include <util/rangemap.hpp>
#include <util/itree.hpp>
#include <util/random_gen.hpp>
#include <stdlib.h>

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE

class CTestRangeMap : public CNcbiApplication
{
public:
    typedef CRange<int> TRange;

    void Init(void);
    int Run(void);

    void TestRangeMap(void) const;
    void TestIntervalTree(void) const;

    void Filling(const char* type) const;
    void Filled(size_t size) const;
    void Stat(pair<double, size_t> stat) const;
    void Added(TRange range) const;
    void Old(TRange range) const;
    void FromAll(TRange range) const;
    void StartFrom(TRange search) const;
    void From(TRange search, TRange range) const;
    void End(void) const;
    void PrintTotalScannedNumber(size_t number) const;

    TRange RandomRange(void) const;

    mutable CRandom m_Random;
    int m_Count;

    int m_Length;
    int m_RangeNumber;
    int m_RangeLength;

    int m_ScanCount;
    int m_ScanStep;
    int m_ScanLength;
    
    bool m_Silent;
    bool m_PrintSize;
    bool m_PrintTotalScannedNumber;
};


inline
CNcbiOstream& operator<<(CNcbiOstream& out, CTestRangeMap::TRange range)
{
    return out <<
        '('<<range.GetFrom()<<','<<range.GetTo()<<")="<<range.GetLength();
}

inline
string ToString(CTestRangeMap::TRange range)
{
    CNcbiOstrstream b;
    b << range;
    return CNcbiOstrstreamToString(b);
}

inline
void CTestRangeMap::Filling(const char* type) const
{
    if ( !m_Silent )
        NcbiCout << "Filling " << type << ":" << NcbiEndl;
}

inline
void CTestRangeMap::Filled(size_t size) const
{
    NcbiCout << "Filled: size=" << size << NcbiEndl;
}

inline
void CTestRangeMap::Stat(pair<double, size_t> stat) const
{
    NcbiCout << "Stat: mean=" << stat.first << " max=" << stat.second << NcbiEndl;
}

inline
void CTestRangeMap::Added(TRange range) const
{
    if ( !m_Silent )
        NcbiCout << "Added " << range << NcbiEndl;
}

inline
void CTestRangeMap::Old(TRange range) const
{
    if ( !m_Silent )
        NcbiCout << "Old " << range << NcbiEndl;
}

inline
void CTestRangeMap::FromAll(TRange range) const
{
    if ( !m_Silent )
        NcbiCout << "All " << range << NcbiEndl;
}

inline
void CTestRangeMap::StartFrom(TRange search) const
{
    if ( !m_Silent )
        NcbiCout << "In range " << search << ":" << NcbiEndl;
}

inline
void CTestRangeMap::From(TRange search, TRange range) const
{
    if ( !m_Silent )
        NcbiCout << "In " << search << ": " << range << NcbiEndl;
}

inline
void CTestRangeMap::End(void) const
{
    if ( !m_Silent )
        NcbiCout << "End." << NcbiEndl;
}

inline
void CTestRangeMap::PrintTotalScannedNumber(size_t number) const
{
    if ( m_PrintTotalScannedNumber )
        NcbiCout << "Total scanned intervals: " << number << NcbiEndl;
}

inline
CTestRangeMap::TRange CTestRangeMap::RandomRange(void) const
{
    int len = m_Random.GetRand(1, m_RangeLength);
    for ( int i = 0; i < 4; ++i ) {
        len *= int((double(m_Random.GetRand())/m_Random.GetMax()));
        if ( len == 0 )
            len = 1;
    }
    int from = m_Random.GetRand(0, m_Length-len);
    int to = from + len-1;
    return TRange(from, to);
}

void CTestRangeMap::TestIntervalTree(void) const
{
    Filling("CIntervalTree");

    typedef CIntervalTree TMap;
    typedef TMap::const_iterator TMapCI;

    TMap m;

    // fill
    CStopWatch sw;
    sw.Restart();
    for ( int count = 0; count < m_RangeNumber; ) {
        TRange range = RandomRange();
        m.Insert(range, CConstRef<CObject>(0));
        ++count;
        Added(range);
    }
    cout << "Add time: "<<sw.Elapsed()<<endl;
    
    if ( m_PrintSize ) {
        Filled(m.Size());
        Stat(m.Stat());
    }

    sw.Restart();
    for ( TMapCI i = m.AllIntervals(); i; ++i ) {
        FromAll(i.GetInterval());
    }
    cout << "Full scan time: "<<sw.Elapsed()<<endl;

    size_t scannedCount = 0;
    for ( int count = 0; count < m_ScanCount; ++count ) {
        sw.Restart();
        for ( int pos = 0; pos <= m_Length + 2*m_RangeLength;
              pos += m_ScanStep ) {
            TRange range(pos, pos + m_ScanLength - 1);
            
            StartFrom(range);
            
            for ( TMapCI i = m.IntervalsOverlapping(range); i; ++i ) {
                From(range, i.GetInterval());
                ++scannedCount;
            }
        }
        cout << "Lookup time: "<<sw.Elapsed()<<endl;
    }
    PrintTotalScannedNumber(scannedCount);

    End();
}

void CTestRangeMap::TestRangeMap(void) const
{
    Filling("CRangeMap");

    typedef CRangeMultimap<CConstRef<CObject> > TMap;
    typedef TMap::const_iterator TMapCI;

    TMap m;

    CStopWatch sw;
    // fill
    sw.Restart();
    for ( int count = 0; count < m_RangeNumber; ) {
        TRange range = RandomRange();
        m.insert(TMap::value_type(range, CConstRef<CObject>(0)));
        ++count;
        Added(range);
    }
    cout << "Add time: "<<sw.Elapsed()<<endl;
    
    if ( m_PrintSize ) {
        Filled(m.size());
        // Stat(m.stat());
    }

    sw.Restart();
    for ( TMapCI i = m.begin(); i; ++i ) {
        FromAll(i.GetInterval());
    }
    cout << "Full scan time: "<<sw.Elapsed()<<endl;

    size_t scannedCount = 0;
    for ( int count = 0; count < m_ScanCount; ++count ) {
        sw.Restart();
        for ( int pos = 0; pos <= m_Length + 2*m_RangeLength;
              pos += m_ScanStep ) {
            TRange range;
            range.Set(pos, pos + m_ScanLength - 1);
            
            StartFrom(range);
            
            for ( TMapCI i = m.begin(range); i; ++i ) {
                From(range, i.GetInterval());
                ++scannedCount;
            }
        }
        cout << "Lookup time: "<<sw.Elapsed()<<endl;
    }
    PrintTotalScannedNumber(scannedCount);

    End();
}

void CTestRangeMap::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);

    unique_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("testrangemap",
                       "test different interval search classes");

    d->AddDefaultKey("t", "type",
                     "type of container to use",
                     CArgDescriptions::eString, "CIntervalTree");
    d->SetConstraint("t", (new CArgAllow_Strings)->
                     Allow("CIntervalTree")->Allow("i")->
                     Allow("CRangeMap")->Allow("r"));

    d->AddDefaultKey("c", "count",
                     "how may times to run whole test",
                     CArgDescriptions::eInteger, "1");

    d->AddFlag("s",
               "silent operation - do not print log");
    d->AddFlag("ps",
               "print total number of intervals");
    d->AddFlag("psn",
               "print total number of scanned intervals");

    d->AddDefaultKey("n", "rangeNumber",
                     "number of intervals to use",
                     CArgDescriptions::eInteger, "10000");
    d->AddDefaultKey("l", "length",
                     "length of whole region",
                     CArgDescriptions::eInteger, "1000");
    d->AddDefaultKey("il", "intervalLength",
                     "max length of intervals to use",
                     CArgDescriptions::eInteger, "20")
;
    d->AddDefaultKey("ss", "scanStep",
                     "step of scan intervals",
                     CArgDescriptions::eInteger, "10");
    d->AddDefaultKey("sl", "scanLength",
                     "length of scan intervals",
                     CArgDescriptions::eInteger, "20");
    d->AddDefaultKey("sc", "scanCount",
                     "how may times to run scan test",
                     CArgDescriptions::eInteger, "1");

    SetupArgDescriptions(d.release());
}

int CTestRangeMap::Run(void)
{
    const CArgs& args = GetArgs();

    m_Count = args["c"].AsInteger();
    m_Silent = args["s"];
    m_PrintSize = args["ps"];
    m_PrintTotalScannedNumber = args["psn"];
    m_RangeNumber = args["n"].AsInteger();
    m_Length = args["l"].AsInteger();
    m_RangeLength = args["il"].AsInteger();
    m_ScanCount = args["sc"].AsInteger();
    m_ScanStep = args["ss"].AsInteger();
    m_ScanLength = args["sl"].AsInteger();
    
    bool intervalTree =
        args["t"].AsString() == "CIntervalTree" ||
        args["t"].AsString() == "i";

    for ( int count = 0; count < m_Count; ++count ) {
        if ( intervalTree )
            TestIntervalTree();
        else
            TestRangeMap();
    }

    return 0;
}

END_NCBI_SCOPE

int main(int argc, const char* argv[])
{
    return NCBI_NS_NCBI::CTestRangeMap().AppMain(argc, argv);
}
