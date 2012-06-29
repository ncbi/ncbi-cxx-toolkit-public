#ifndef ALGO_ALIGN_PROSPLIGN_COMPART__HPP
#define ALGO_ALIGN_PROSPLIGN_COMPART__HPP

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
 * Author:  Boris Kiryutin, Vyacheslav Chetvernin
 * File Description: Get protein compartments from BLAST hits
 *
 */

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiargs.hpp>
#include <algo/align/splign/splign.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <algo/align/util/compartment_finder.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

BEGIN_SCOPE(prosplign)

class NCBI_XALGOALIGN_EXPORT CCompartOptions {
public:
    static void SetupArgDescriptions(CArgDescriptions* argdescr);

    CCompartOptions(const CArgs& args);

    double m_CompartmentPenalty;
    double m_MinCompartmentIdty;
    double m_MinSingleCompartmentIdty;
    int    m_MaxExtent;
    bool   m_ByCoverage;
    int    m_MaxIntron;
    enum EMaximizing {
        eCoverage = 0,
        eIdentity,
        eScore
    };
    EMaximizing m_Maximizing;

    static const double default_CompartmentPenalty;
    static const double default_MinCompartmentIdty;
    static const double default_MinSingleCompartmentIdty;
    static const int    default_MaxExtent = 500;
    static const bool   default_ByCoverage = true;
    static const int    default_MaxIntron;
    static const EMaximizing default_Maximizing = default_ByCoverage?eCoverage:eIdentity;
    static const char* s_scoreNames[];
};

struct SCompartment {
    SCompartment(int from_, int to_, bool strand_, int covered_aa_, double score_) :
        from(from_), to(to_), strand(strand_), covered_aa(covered_aa_), score(score_) {}

    int from;
    int to;
    bool strand;
    int covered_aa;
    double score;

    bool operator< (const SCompartment& comp) const
    {
        if (strand == comp.strand) {
            return from < comp.from;
        } else {
            return strand < comp.strand;
        }
    }
};

typedef CSplign::THit     THit;
typedef CSplign::THitRef  THitRef;
typedef CSplign::THitRefs THitRefs;

typedef list<CRef<CSeq_annot> > TCompartments;
typedef vector<SCompartment> TCompartmentStructs;

/// Selects compartments. Hits should be for a single query-subject pair.

NCBI_XALGOALIGN_EXPORT
auto_ptr<CCompartmentAccessor<THit> > CreateCompartmentAccessor(const THitRefs& orig_hitrefs,
                                                                CCompartOptions compart_options);

NCBI_XALGOALIGN_EXPORT
TCompartments FormatAsAsn(CCompartmentAccessor<THit>* comparts_ptr, CCompartOptions compart_options);


NCBI_XALGOALIGN_EXPORT
TCompartmentStructs MakeCompartments(const TCompartments& asn_representation,
                                     CCompartOptions compart_options);


/// Composition of first two functions
NCBI_XALGOALIGN_EXPORT
TCompartments SelectCompartmentsHits(const THitRefs& hitrefs,
                                     CCompartOptions compart_options);

/// Composition of all three functions
NCBI_XALGOALIGN_EXPORT
TCompartmentStructs MakeCompartments(const THitRefs& hitrefs,
                                     CCompartOptions compart_options);

END_SCOPE(prosplign)
END_NCBI_SCOPE
#endif
