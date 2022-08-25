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
* Author:  Aaron Ucko, Mati Shomrat, Mike DiCuccio, Jonathan Kans, NCBI
*
* File Description:
*   flat-file generator application
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>

// this is unused code, just saved for future use

[[nodiscard]]
ICanceled* CAsn2FlatApp::x_CreateCancelBenchmarkCallback() const
{
    // This ICanceled interface always says "keep going"
    // unless SIGUSR1 is received,
    // and it keeps statistics on how often it is checked
    class CCancelBenchmarking : public ICanceled {
    public:
        CCancelBenchmarking()
            : m_TimeOfLastCheck(CTime::eCurrent)
        {
        }


        ~CCancelBenchmarking()
        {
            // On destruction, we call "IsCanceled" one more time to make
            // sure there wasn't a point where we stopped
            // checking IsCanceled.
            IsCanceled();

            // print statistics
            cerr << "##### Cancel-checking benchmarks:" << endl;
            cerr << endl;

            // maybe cancelation was never checked:
            if( m_GapSizeMap.empty() ) {
                cerr << "NO DATA" << endl;
                return;
            }

            cerr << "(all times in milliseconds)" << endl;
            cerr << endl;
            // easy stats
            cerr << "Min: " << m_GapSizeMap.begin()->first << endl;
            cerr << "Max: " << m_GapSizeMap.rbegin()->first << endl;

            // find average
            Int8   iTotalMsecs = 0;
            size_t uTotalCalls = 0;
            ITERATE( TGapSizeMap, gap_size_it, m_GapSizeMap ) {
                iTotalMsecs += gap_size_it->first * gap_size_it->second;
                uTotalCalls += gap_size_it->second;
            }
            cerr << "Avg: " << (iTotalMsecs / uTotalCalls) << endl;

            // find median
            const size_t uIdxWithMedian = (uTotalCalls / 2);
            size_t uCurrentIdx = 0;
            ITERATE( TGapSizeMap, gap_size_it, m_GapSizeMap ) {
                uCurrentIdx += gap_size_it->second;
                if( uCurrentIdx >= uIdxWithMedian ) {
                    cerr << "Median: " << gap_size_it->first << endl;
                    break;
                }
            }

            // first few
            cerr << "Chronologically first few check times: " << endl;
            copy( m_FirstFewGaps.begin(),
                m_FirstFewGaps.end(),
                ostream_iterator<Int8>(cerr, "\n") );

            // last few
            cerr << "Chronologically last few check times: " << endl;
            copy( m_LastFewGaps.begin(),
                m_LastFewGaps.end(),
                ostream_iterator<Int8>(cerr, "\n") );

            // slowest few and fastest few
            cerr << "Frequency distribution of slowest and fastest lookup times: " << endl;
            cerr << "\t" << "Time" << "\t" << "Count" << endl;
            if( m_GapSizeMap.size() <= (2 * x_kGetNumToSaveAtStartAndEnd()) ) {
                // if small enough, show the whole table at once
                ITERATE( TGapSizeMap, gap_size_it, m_GapSizeMap ) {
                    cerr << "\t" << gap_size_it ->first << "\t" << gap_size_it->second << endl;
                }
            } else {
                // table is big so only show the first few and the last few
                TGapSizeMap::const_iterator gap_size_it = m_GapSizeMap.begin();
                ITERATE_SIMPLE(x_kGetNumToSaveAtStartAndEnd()) {
                    cerr << "\t" << gap_size_it ->first << "\t" << gap_size_it->second << endl;
                    ++gap_size_it;
                }

                cout << "\t...\t..." << endl;

                TGapSizeMap::reverse_iterator gap_size_rit = m_GapSizeMap.rbegin();
                ITERATE_SIMPLE(x_kGetNumToSaveAtStartAndEnd()) {
                    cerr << "\t" << gap_size_it ->first << "\t" << gap_size_it->second << endl;
                    ++gap_size_rit;
                }
            }

            // total checks
            cerr << "Num checks: " << uTotalCalls << endl;
        }


        bool IsCanceled() const override
        {
            // getting the current time should be the first
            // command in this function.
            CTime timeOfThisCheck(CTime::eCurrent);

            const double dDiffInNsecs =
                timeOfThisCheck.DiffNanoSecond(m_TimeOfLastCheck);
            const Int8 iDiffInMsecs = static_cast<Int8>(dDiffInNsecs / 1000000.0);

            ++m_GapSizeMap[iDiffInMsecs];

            if( m_FirstFewGaps.size() < x_kGetNumToSaveAtStartAndEnd() ) {
                m_FirstFewGaps.push_back(iDiffInMsecs);
            }

            m_LastFewGaps.push_back(iDiffInMsecs);
            if( m_LastFewGaps.size() > x_kGetNumToSaveAtStartAndEnd() ) {
                m_LastFewGaps.pop_front();
            }

            const bool bIsCanceled =
                CSignal::IsSignaled(CSignal::eSignal_USR1);
            if( bIsCanceled ) {
                cerr << "Canceled by SIGUSR1" << endl;
            }

            // resetting m_TimeOfLastCheck should be the last command
            // in this function
            m_TimeOfLastCheck.SetCurrent();
            return bIsCanceled;
        }

    private:
        // local classes do not allow static fields
        size_t x_kGetNumToSaveAtStartAndEnd() const { return 10; }

        mutable CTime m_TimeOfLastCheck;
        // The key is the gap between checks in milliseconds,
        // (which is more than enough resolution for a human-level action)
        // and the value is the number of times a gap of that size occurred.
        typedef map<Int8, size_t> TGapSizeMap;
        mutable TGapSizeMap m_GapSizeMap;

        mutable vector<Int8> m_FirstFewGaps;
        mutable list<Int8>   m_LastFewGaps;
    };

    return new CCancelBenchmarking;
}
CAsn2FlatApp::TGenbankBlockCallback*
CAsn2FlatApp::x_GetGenbankCallback(const CArgs& args) const
{
    class CSimpleCallback : public TGenbankBlockCallback {
    public:
        CSimpleCallback() { }
        ~CSimpleCallback() override
        {
            cerr << endl;
            cerr << "GENBANK CALLBACK DEMO STATISTICS" << endl;
            cerr << endl;
            cerr << "Appearances of each type: " << endl;
            cerr << endl;
            x_DumpTypeToCountMap(m_TypeAppearancesMap);
            cerr << endl;
            cerr << "Total characters output for each type: " << endl;
            cerr << endl;
            x_DumpTypeToCountMap(m_TypeCharsMap);
            cerr << endl;
            cerr << "Average characters output for each type: " << endl;
            cerr << endl;
            x_PrintAverageStats();
        }

        EBioseqSkip notify_bioseq( CBioseqContext & ctx ) override
        {
            cerr << "notify_bioseq called." << endl;
            return eBioseqSkip_No;
        }

        // this macro is the lesser evil compared to the messiness that
        // you would see here otherwise. (plus it's less error-prone)
#define SIMPLE_CALLBACK_NOTIFY(TItemClass) \
        EAction notify( string & block_text, \
                        const CBioseqContext& ctx, \
                        const TItemClass & item ) override { \
        NStr::ReplaceInPlace(block_text, "MODIFY TEST", "WAS MODIFIED TEST" ); \
        cerr << #TItemClass << " {" << block_text << '}' << endl; \
        ++m_TypeAppearancesMap[#TItemClass]; \
        ++m_TypeAppearancesMap["TOTAL"]; \
        m_TypeCharsMap[#TItemClass] += block_text.length(); \
        m_TypeCharsMap["TOTAL"] += block_text.length(); \
        EAction eAction = eAction_Default; \
        if( block_text.find("SKIP TEST")  != string::npos ) { \
            eAction = eAction_Skip; \
        } \
        if ( block_text.find("HALT TEST") != string::npos ) { \
            eAction = eAction_HaltFlatfileGeneration;         \
        } \
        return eAction; }

        SIMPLE_CALLBACK_NOTIFY(CStartSectionItem);
        SIMPLE_CALLBACK_NOTIFY(CHtmlAnchorItem);
        SIMPLE_CALLBACK_NOTIFY(CLocusItem);
        SIMPLE_CALLBACK_NOTIFY(CDeflineItem);
        SIMPLE_CALLBACK_NOTIFY(CAccessionItem);
        SIMPLE_CALLBACK_NOTIFY(CVersionItem);
        SIMPLE_CALLBACK_NOTIFY(CGenomeProjectItem);
        SIMPLE_CALLBACK_NOTIFY(CDBSourceItem);
        SIMPLE_CALLBACK_NOTIFY(CKeywordsItem);
        SIMPLE_CALLBACK_NOTIFY(CSegmentItem);
        SIMPLE_CALLBACK_NOTIFY(CSourceItem);
        SIMPLE_CALLBACK_NOTIFY(CReferenceItem);
        SIMPLE_CALLBACK_NOTIFY(CCommentItem);
        SIMPLE_CALLBACK_NOTIFY(CPrimaryItem);
        SIMPLE_CALLBACK_NOTIFY(CFeatHeaderItem);
        SIMPLE_CALLBACK_NOTIFY(CSourceFeatureItem);
        SIMPLE_CALLBACK_NOTIFY(CFeatureItem);
        SIMPLE_CALLBACK_NOTIFY(CGapItem);
        SIMPLE_CALLBACK_NOTIFY(CBaseCountItem);
        SIMPLE_CALLBACK_NOTIFY(COriginItem);
        SIMPLE_CALLBACK_NOTIFY(CSequenceItem);
        SIMPLE_CALLBACK_NOTIFY(CContigItem);
        SIMPLE_CALLBACK_NOTIFY(CWGSItem);
        SIMPLE_CALLBACK_NOTIFY(CTSAItem);
        SIMPLE_CALLBACK_NOTIFY(CEndSectionItem);
#undef SIMPLE_CALLBACK_NOTIFY

    private:
        typedef map<string, size_t> TTypeToCountMap;
        // for each type, how many instances of that type did we see?
        // We use the special string "TOTAL" for a total count.
        TTypeToCountMap m_TypeAppearancesMap;
        // Like m_TypeAppearancesMap but counts total number of *characters*
        // instead of number of items.  Again, there is
        // the special value "TOTAL"
        TTypeToCountMap m_TypeCharsMap;

        void x_DumpTypeToCountMap(const TTypeToCountMap & the_map ) {
            ITERATE( TTypeToCountMap, map_iter, the_map ) {
                cerr << setw(25) << left << (map_iter->first + ':')
                     << " " << map_iter->second << endl;
            }
        }

        void x_PrintAverageStats() {
            ITERATE( TTypeToCountMap, map_iter, m_TypeAppearancesMap ) {
                const string sType = map_iter->first;
                const size_t iAppearances = map_iter->second;
                const size_t iTotalCharacters = m_TypeCharsMap[sType];
                cerr << setw(25) << left << (sType + ':')
                     << " " << (iTotalCharacters / iAppearances) << endl;
            }
        }
    };

    if( args["demo-genbank-callback"] ) {
        return new CSimpleCallback;
    } else {
        return nullptr;
    }
}
