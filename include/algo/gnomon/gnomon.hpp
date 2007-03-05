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

USING_SCOPE(objects);

class NCBI_XALGOGNOMON_EXPORT CUniqNumber
{
    public:
        CUniqNumber(int b = 1, int s = 0) : m_base(b), m_shift(s), m_local(0) {};
        CUniqNumber& operator++() { ++m_local; return *this; }
        operator int() const { return m_base*m_local+m_shift; } 
    private:
        int m_base, m_shift, m_local;
};

struct SGnomonEngineImplData;

class NCBI_XALGOGNOMON_EXPORT CGnomonEngine {
public:
    CGnomonEngine(const string& modeldatafilename, const string& seq_name, const CResidueVec& sequence, TSignedSeqRange range);

    ~CGnomonEngine();

    void ResetRange(TSignedSeqRange range);
    void ResetRange(TSignedSeqPos from, TSignedSeqPos to) { ResetRange(TSignedSeqRange(from,to)); }

    int GetGCcontent() const;
    string GetSeqName() const;
    static int GetMinIntronLen();
    static double GetChanceOfIntronLongerThan(int l);

    // calculate gnomon score for a gene model
    void GetScore(CAlignVec& model, bool uselims = false, bool allowopen = true) const;

    // run gnomon. return score
    double Run(bool repeats = true, bool leftwall = true, bool rightwall = true, double mpp = 10); // pure ab initio

    double Run(const TAlignList& cls,
               bool repeats, bool leftwall, bool rightwall, bool leftanchor, bool rightanchor, double mpp, double consensuspenalty = BadScore());

    CRef<objects::CSeq_annot> GetAnnot(void);

    list<CGene> GetGenes() const;

    TSignedSeqPos PartialModelStepBack(list<CGene>& genes) const;
    void PrintInfo() const;

private:
    // Prohibit copy constructor and assignment operator
    CGnomonEngine(const CGnomonEngine& value);
    CGnomonEngine& operator= (const CGnomonEngine& value);

    // delete all heap allocated members
    void cleanup();

    // check range within sequence. throws exceptions. 
    void CheckRange();

    void GenerateSeqAnnot();
    
    SGnomonEngineImplData* m_data;

    // our resulting annotation
    CRef<objects::CSeq_annot> m_Annot;
};

NCBI_XALGOGNOMON_EXPORT
void PrintGenes(const list<CGene>& genes, CUniqNumber& unumber, CNcbiOstream& to = cout, CNcbiOstream& toprot = cout);

class NCBI_XALGOGNOMON_EXPORT CCodingPropensity {
public:

    // calculates CodingPropensity score
    static double GetScore(const string& modeldatafilename, const CSeq_loc& cds, CScope& scope, int *const gccontent, double *const startscore = 0);

};


END_SCOPE(gnomon)
END_NCBI_SCOPE

#endif  // ALGO_GNOMON___GNOMON__HPP
