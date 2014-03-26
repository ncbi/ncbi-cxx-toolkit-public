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
#include <algo/gnomon/chainer.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CGnomon_params;
END_SCOPE(objects)
BEGIN_SCOPE(gnomon)

class CEResidueVec;
class CInputModel;

/// HMM model parameters
/// just create it and pass to a Gnomon engine
class NCBI_XALGOGNOMON_EXPORT CHMMParameters : public CObject {
public:
    CHMMParameters(const objects::CGnomon_params& hmm_params_asn);
    CHMMParameters(CNcbiIstream& hmm_params_istr, ESerialDataFormat format=eSerial_AsnText);
    ~CHMMParameters();
    const CInputModel& GetParameter(const string& type, int cgcontent) const;
private:
    class SDetails;
    CRef<SDetails> m_details;

    // Prohibit copy constructor and assignment operator
    CHMMParameters(const CHMMParameters& value);
    CHMMParameters& operator= (const CHMMParameters& value);
};


class NCBI_XALGOGNOMON_EXPORT CGnomonEngine {
public:
    CGnomonEngine(CConstRef<CHMMParameters> hmm_params, const CResidueVec& sequence, TSignedSeqRange range = TSignedSeqRange::GetWhole());
    ~CGnomonEngine();

    void ResetRange(TSignedSeqRange range);
    void ResetRange(TSignedSeqPos from, TSignedSeqPos to) { ResetRange(TSignedSeqRange(from,to)); }

    const CResidueVec& GetSeq() const;
    int GetGCcontent() const;
    int GetMinIntronLen() const;
    int GetMaxIntronLen() const;
    int GetMinIntergenicLen() const;
    double GetChanceOfIntronLongerThan(int l) const;

    // calculate gnomon score for a gene model
    void GetScore(CGeneModel& model, bool extend5p = false, bool obeystart = false) const;
    double SelectBestReadingFrame(const CGeneModel& model, const CEResidueVec& mrna, const CAlignMap& mrnamap,
                                  TIVec starts[3],  TIVec stops[3], int& frame, int& best_start, int& best_stop, bool extend5p = true) const;

    // run gnomon. return score
    double Run(bool repeats = true, bool leftwall = true, bool rightwall = true, double mpp = 10); // pure ab initio

    double Run(const TGeneModelList& chains,
               bool repeats, bool leftwall, bool rightwall, bool leftanchor, bool rightanchor, double mpp, double consensuspenalty = BadScore(), 
               const CGnomonAnnotator_Base::TGgapInfo& ggapinfo = CGnomonAnnotator_Base::TGgapInfo());

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

class NCBI_XALGOGNOMON_EXPORT CCodingPropensity {
public:

    // calculates CodingPropensity and start score
    static double GetScore(CConstRef<CHMMParameters> hmm_params, const objects::CSeq_loc& cds, objects::CScope& scope, int *const gccontent, double *const startscore = 0);

};

END_SCOPE(gnomon)
END_NCBI_SCOPE

#endif  // ALGO_GNOMON___GNOMON__HPP
