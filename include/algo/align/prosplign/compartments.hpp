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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)
USING_SCOPE(objects);

class NCBI_XALGOALIGN_EXPORT CCompartOptions {
public:
    static void SetupArgDescriptions(CArgDescriptions* argdescr);

    CCompartOptions(const CArgs& args);

    double m_CompartmentPenalty;
    double m_MinCompartmentIdty;
    int    m_MaxExtent;
};

struct NCBI_XALGOALIGN_EXPORT SCompartment {
    SCompartment(int from_, int to_, bool strand_, int covered_aa_, double score_) :
        from(from_), to(to_), strand(strand_), covered_aa(covered_aa_), score(score_) {}

    int from;
    int to;
    bool strand;
    int covered_aa;
    double score;

    bool operator< (const SCompartment& comp) const
    {
        return strand==comp.strand?from<comp.from:strand<comp.strand;
    }
};

typedef vector<SCompartment> TCompartmentStructs;
typedef list<CRef<CSeq_annot> > TCompartments;

/// Makes compartments. Hits should be for a single protein-genomic pair.
TCompartments SelectCompartmentsHits(const CSplign::THitRefs& hitrefs, CCompartOptions compart_options);
TCompartmentStructs MakeCompartments(const TCompartments& compartments_hits, CCompartOptions compart_options);
/// Composition of above two functions
TCompartmentStructs MakeCompartments(const CSplign::THitRefs& hitrefs, CCompartOptions compart_options);

END_SCOPE(prosplign)
END_NCBI_SCOPE
#endif
