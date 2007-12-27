#ifndef ALGO_GNOMON___GNOMON__HPP
#define ALGO_GNOMON___GNOMON__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbiobj.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <util/range.hpp>

#include <algo/gnomon/gnomon_model.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

class CEResidueVec;

class NCBI_XALGOGNOMON_EXPORT CGnomonEngine {
public:
    static void ReadOrgHMMParameters(CNcbiIstream& from);
    CGnomonEngine(const CResidueVec& sequence, TSignedSeqRange range);
    ~CGnomonEngine();

    void ResetRange(TSignedSeqRange range);
    void ResetRange(TSignedSeqPos from, TSignedSeqPos to) { ResetRange(TSignedSeqRange(from,to)); }

    const CResidueVec& GetSeq() const;
    int GetGCcontent() const;
    int GetMinIntronLen() const;
    double GetChanceOfIntronLongerThan(int l) const;

    // calculate gnomon score for a gene model
    void GetScore(CGeneModel& model) const;
    double SelectBestReadingFrame(const CGeneModel& model, const CEResidueVec& mrna, const CFrameShiftedSeqMap& mrnamap,
                                  TIVec starts[3],  TIVec stops[3], int& frame, int& best_start, int& best_stop) const;

    // run gnomon. return score
    double Run(bool repeats = true, bool leftwall = true, bool rightwall = true, double mpp = 10); // pure ab initio

    double Run(const TAlignList& chains,
	       bool repeats, bool leftwall, bool rightwall, bool leftanchor, bool rightanchor, double mpp, double consensuspenalty = BadScore());

    CRef<objects::CSeq_annot> GetAnnot(const objects::CSeq_id& id);

    list<CGeneModel> GetGenes() const;

    TSignedSeqPos PartialModelStepBack(list<CGeneModel>& genes) const;
    void PrintInfo() const;

private:
    // Prohibit copy constructor and assignment operator
    CGnomonEngine(const CGnomonEngine& value);
    CGnomonEngine& operator= (const CGnomonEngine& value);

    // check range within sequence. throws exceptions. 
    void CheckRange();

    struct SGnomonEngineImplData;
    auto_ptr<SGnomonEngineImplData> m_data;
};

// NCBI_XALGOGNOMON_EXPORT
// void PrintGenes(const list<CGene>& genes, CUniqNumber& unumber, CNcbiOstream& to = cout, CNcbiOstream& toprot = cout);

class NCBI_XALGOGNOMON_EXPORT CCodingPropensity {
public:

    // calculates CodingPropensity and start score
    static double GetScore(const string& modeldatafilename, const objects::CSeq_loc& cds, objects::CScope& scope, int *const gccontent, double *const startscore = 0);

};

END_SCOPE(gnomon)
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.7.2.6  2006/11/09 15:34:47  souvorov
 * Start score evaluation
 *
 * Revision 1.7.2.5  2006/11/03 20:22:24  chetvern
 * Removed unused parameter from GetScore
 *
 * Revision 1.7.2.4  2006/10/25 17:46:23  souvorov
 * FirstStart out of GnomonEngine
 *
 * Revision 1.7.2.3  2006/10/24 19:42:39  souvorov
 * Modification for open alignments
 *
 * Revision 1.7.2.2  2006/10/12 18:57:20  chetvern
 * Changed hmm parameters reading
 *
 * Revision 1.7.2.1  2006/10/06 14:17:56  chetvern
 * Major overhaul. Single format for intermediate files.
 *
 * Revision 1.10  2006/10/05 15:30:58  souvorov
 * Implementation of anchors for intergenics
 *
 * Revision 1.9  2006/06/29 19:19:22  souvorov
 * Confirmed start implementation
 *
 * Revision 1.8  2006/03/06 15:53:23  souvorov
 * Changes needed for ChanceOfIntronLongerThan(int l)
 *
 * Revision 1.7  2005/11/29 15:21:37  jcherry
 * Added export specifier
 *
 * Revision 1.6  2005/11/21 21:25:54  chetvern
 * Extracted PartialModelStepBack from PrintGenes
 *
 * Revision 1.5  2005/10/20 19:34:46  souvorov
 * Penalty for nonconsensus starts/stops/splices
 *
 * Revision 1.4  2005/10/06 14:34:25  souvorov
 * CGnomonEngine::GetSeqName() introduced
 *
 * Revision 1.3  2005/09/15 21:16:01  chetvern
 * redesigned API
 *
 * Revision 1.2  2004/03/16 15:37:43  vasilche
 * Added required include
 *
 * Revision 1.1  2003/10/24 15:06:30  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // ALGO_GNOMON___GNOMON__HPP
