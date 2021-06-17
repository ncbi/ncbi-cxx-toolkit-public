/* $Id$
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
 * Author:  Chris Lanczycki
 *
 * File Description:
 *
 *       Utility class to define and manage the ranking of the typical database sources found
 *       in NCBI Bioseqs and Seq_ids.  Use functions in cuSequence to query with a particular sequence.
 *
 * ===========================================================================
 */

#ifndef CU_DBPRIORITY_HPP
#define CU_DBPRIORITY_HPP

#include <string>
#include <map>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

class CCdDbPriority {

public:

    //  Within a priority tier, database sources are equivalent.  If each database source is equivalent,
    //  need a tier for each EDbSource.
    enum {
        eTopTier = 0,
        eTier1 = 1,
        eTier2 = 2,
        eTier3 = 3,
        eTier4 = 4,
        eBottomTier
    };

    //  This enum can have arbitrary order; the priority of each source is
    //  maintained in a static map from this enum to an integral priority.
    //  Anything not supported should be put after 'eDPUnsupported' and
    //  before 'eDPUnknown'.  In general, these values map to CSeq_id
    //  choice options, but there are exceptions particularly when a CSeq_id
    //  type can provide a free-form database source string (e_General) or
    //  when subtypes we may be interested in aren't distinguished (e_Other == refseq).
    enum EDbSource {
        eDPGi = 0,
        eDPPdb = 1,
        eDPSwissprot = 2,
        eDPGenbank = 3,
        eDPEmbl = 4,
        eDPDdbj = 5,
        eDPPrf = 6,
        eDPPir = 7,
        eDPGeneral = 8,    //  not sure what text this will be...

        //  refseq accession format: http://www.ncbi.nlm.nih.gov/RefSeq/key.html#accession or  http://www.ncbi.nlm.nih.gov/Sequin/acc.html
        eDPRefSeqAny = 10,   //  a refseq independent of subclassification; any refseq between this value and eDPLocal

        eDPRefSeqCurated = 11,  //  any refseq between this value and eDPRefSeqAutomated
        eDPRefSeqNP = 12,       
        eDPRefSeqAP = 13,   

        eDPRefSeqAutomated = 15,  //  any refseq between this value and eDPLocal
        eDPRefSeqXP = 16,
        eDPRefSeqYP = 17,
        eDPRefSeqWP = 18,    
        eDPRefSeqZP = 19,    //  these became obsolete in May 2013, replaced by WP
        //  end refseqs

        eDPLocal = 20,

        eDPUnsupported = 99,   //  in case want to use a catch-all for unsupported types to distinguish from simply unknown
        eDPGibbsq = 100,         //  the rest of these are not expected, but provide a name anyway
        eDPGibbmt = 101,
        eDPGiim = 102,
        eDPPatent = 103,
        eDPTPGenbank = 104,
        eDPTPEmbl = 105,
        eDPTPDdbj = 106,
        eDPGpipe = 107,
        //  end of the unsupported/unexpected types

        eDPUnknown = 999
    };

    //  Priority typedefs and mapping between db sources above and a priority
    typedef unsigned int TDbPriority;
    typedef map<EDbSource, TDbPriority> TSourcePriorityMap;
    typedef TSourcePriorityMap::iterator TSourcePriorityIt;
    typedef TSourcePriorityMap::value_type TSourcePriorityVT;


    //  Multimap to handle cases like the 'general' seq-id where the db source isn't predefined.
    typedef multimap<EDbSource, string> TSourceNameMap;
    typedef TSourceNameMap::value_type TSourceNameVT;
    typedef map<string, EDbSource> TNameSourceMap;

    CCdDbPriority() { Initialize();};
    ~CCdDbPriority() {};

    //  Standard 'compare' semantics:  returns 1 if source1 ">" source2, 0 if source1 "=" source2, -1 if source1 "<" source2
    //  based on their established priorities in the enum above. 
    //  An unrecognized source has lower priority than any known source (i.e., has priority eDPUnknown).
    static int CompareSources(const string& source1, const string& source2);

    static string GetSourceName(EDbSource priority);
    static unsigned int GetSourceNames(EDbSource priority, vector<string>& names);

    static bool IsKnownDbSource(string dbSource);
    static TDbPriority GetPriority(string dbSource);

    static bool IsSupported(string dbSource) {
        return (IsKnownDbSource(dbSource) && GetSourceCode(dbSource) < eDPUnsupported);
    }

    //  Use the CSeq_id::EChoice enum, cast to an unsigned int to avoid the various include files/scope,
    //  to predict a source or priority.
    //  Not all source values can be distinguished from the type alone, particularly for refseq and general types.
    //  For refseqs, use accession, when provided, to give a more detailed source code when possible.
    static string SeqIdTypeToSource(unsigned int seqIdType, string accession = kEmptyStr);

    static TDbPriority SeqIdTypeToPriority(unsigned int seqIdType, string accession = kEmptyStr);

private:

    static TSourcePriorityMap m_sourcePriorityMap;
    static TSourceNameMap  m_sourceNameMap;
    static TNameSourceMap  m_nameSourceMap;

    static void Initialize();
    static EDbSource GetSourceCode(string dbSource);
    static EDbSource SeqIdTypeToSourceCode(unsigned int seqIdType, string accession = kEmptyStr);

};


END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif // CU_DBPRIORITY_HPP
