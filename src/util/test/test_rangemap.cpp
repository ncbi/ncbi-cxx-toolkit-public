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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2001/01/05 15:49:52  vasilche
* Fixed warning.
*
* Revision 1.4  2001/01/05 13:59:17  vasilche
* Reduced CRangeMap* templates size.
* Added CRangeMultimap template.
*
* Revision 1.3  2001/01/03 16:39:29  vasilche
* Added CAbstractObjectManager - stub for object manager.
* CRange extracted to separate file.
*
* Revision 1.2  2000/12/26 17:27:47  vasilche
* Implemented CRangeMap<> template for sorting Seq-loc objects.
*
* Revision 1.1  2000/12/21 21:52:53  vasilche
* Added CRangeMap<> template for sorting integral ranges (Seq-loc).
*
* Revision 1.1  2000/12/19 20:52:19  vasilche
* Test program of C++ object manager.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiutil.hpp>
#include <objects/objmgr/rangemap.hpp>
#include <stdlib.h>

BEGIN_NCBI_SCOPE

class CTestRangeMap : public CNcbiApplication
{
public:
    int Run(void);
};

inline
CNcbiOstream& operator<<(CNcbiOstream& out, CRange<int> range)
{
    return out <<
        '('<<range.GetFrom()<<','<<range.GetTo()<<")="<<range.GetLength();
}

inline
string ToString(CRange<int> range)
{
    CNcbiOstrstream b;
    b << range;
    return CNcbiOstrstreamToString(b);
}

int CTestRangeMap::Run(void)
{
    typedef CRangeMultimap<string> TRangeMap;
    typedef TRangeMap::range_type range_type;

    TRangeMap rm;

    // fill
    NcbiCout << "Filling:" << NcbiEndl;
    while ( rm.size() < 100 ) {
        range_type range;
        string s;
        range.SetFrom(rand() % 20).SetLength(rand() % 10 + 1);
        s = ToString(range);
        rm.insert(TRangeMap::value_type(range, s));
        NcbiCout << range << " = \"" << s << "\"" << NcbiEndl;
    }
    NcbiCout << "Map: count=" << rm.size() << NcbiEndl;
    iterate ( TRangeMap, i, rm ) {
        NcbiCout << i->first << " = \"" << i->second << "\"" << NcbiEndl;
    }
    for ( int pos = 0; pos <= 100; pos += 10 ) {
        range_type range;
        range.SetFrom(pos).SetLength(10);
        NcbiCout << "In range " << range << ":" << NcbiEndl;
        for ( TRangeMap::const_iterator i = rm.begin(range), end = rm.end();
              i != end; ++i ) {
            NcbiCout << i->first << " = \"" << i->second << "\"" << NcbiEndl;
        }
    }
    return 0;
}

END_NCBI_SCOPE

int main(int argc, const char* argv[])
{
    return NCBI_NS_NCBI::CTestRangeMap().AppMain(argc, argv);
}
