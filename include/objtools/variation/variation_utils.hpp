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
* Author:  Igor Filippov
*
* File Description:
*   Simple library to correct reference allele in Variation object
*
* ===========================================================================
*/

#include <util/ncbi_cache.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);
using namespace std;

#define MAX_LEN 1000
#define CCACHE_SIZE 64

class CVariationUtilities
{
public:
    static void CorrectRefAllele(CRef<CVariation>& v, CScope& scope);      
    static void CorrectRefAllele(CRef<CVariation_ref>& var, CScope& scope);    
    static void CorrectRefAllele(CRef<CSeq_annot>& v, CScope& scope);
    static void CorrectRefAllele(CVariation_ref& var, CScope& scope, const string& new_ref); 
private:
    static string GetRefAlleleFromVP(CRef<CVariantPlacement> vp, CScope& scope, TSeqPos length);
    static string GetAlleleFromLoc(const CSeq_loc& loc, CScope& scope);
    static void FixAlleles(CRef<CVariation> v, string old_ref, string new_ref);
    static void FixAlleles(CVariation_ref& vr, string old_ref, string new_ref) ;
};


class CVariationNormalization_base_cache
{
public:
    static int x_GetSeqSize(string accession);
    static void x_PrefetchSequence(CScope &scope, CRef<CSeq_id> seq_id, string accession,ENa_strand strand = eNa_strand_unknown);
    static string x_GetSeq(int pos, int length, string accession);

protected:
    static void x_rotate_left(string &v);
    static void x_rotate_right(string &v);
    static string x_CompactifySeq(string a);

    static CCache<string,CRef<CSeqVector> > m_cache;
};

template<class T>
class CVariationNormalization_base : public CVariationNormalization_base_cache
{
public:
    static void x_Shift(CRef<CVariation>& var, CScope &scope);
    static void x_Shift(CRef<CSeq_annot>& var, CScope &scope);
    static void x_Shift(CRef<CVariation_ref>& var, CScope &scope);
    static void x_ProcessInstance(CVariation_inst &inst, CSeq_loc &loc, bool &is_deletion,  CSeq_literal *&refref, string &ref, int &pos_left, int &pos_right, int &new_pos_left, int &new_pos_right, string accession, int &rtype);    
    static bool x_ProcessShift(string &a, int &pos_left, int &pos_right, string accession, int type) {return T::x_ProcessShift(a,pos_left,pos_right,accession,type);}
    static void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right, int type) {T::x_ModifyLocation(loc,literal,a,pos_left,pos_right,type);}
};

class CVariationNormalizationLeft : public CVariationNormalization_base<CVariationNormalizationLeft>
{
public:
    static bool x_ProcessShift(string &a, int &pos_left, int &pos_right, string accession, int type);
    static void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right, int type);
};

class CVariationNormalizationInt : public CVariationNormalization_base<CVariationNormalizationInt>
{
public:
    static bool x_ProcessShift(string &a, int &pos_left, int &pos_right, string accession, int type);
    static void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right, int type);
};

class CVariationNormalizationRight : public CVariationNormalization_base<CVariationNormalizationRight>
{
public:
    static bool x_ProcessShift(string &a, int &pos_left, int &pos_right, string accession, int type);
    static void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right, int type);
};

class CVariationNormalizationLeftInt :  public CVariationNormalization_base<CVariationNormalizationLeftInt>
{
public:
    static bool x_ProcessShift(string &a, int &pos_left, int &pos_right, string accession, int type);
    static void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right, int type);
};

class CVariationNormalization
{
public:
    enum ETargetContext {
       eDbSnp,
       eHGVS,
       eVCF,
       eVarLoc
    };
    
/////////////////////////////////////////////////////////
 //For later creation, as thin wrappers on the logic below
    //ASSUME scope has everything you need, but that lookups *can* fail so handle gracefully.
    static void NormalizeVariation(CRef<CVariation>& var, ETargetContext target_ctxt, CScope& scope);     
    static void  NormalizeVariation(CRef<CSeq_annot>& var, ETargetContext target_ctxt, CScope& scope);

/////////////////////////////////////////////////////////
 
 
//Initially, the following will be the public methods, each encapsulating a *step* in the normalization process.
//We can add the above wrappers and enum as a final touch. 
 
    //These methods search out ambiguous ins/del, and alter SeqLoc
    //to a Seq-equiv: the first is the original SeqLoc
    //the second is an interval representing the IOA
    static void NormalizeAmbiguousVars(CRef<CVariation>& var, CScope &scope);
    static void NormalizeAmbiguousVars(CRef<CSeq_annot>& var, CScope &scope);
 
    //HGVS
    //*Assume* Seq-equiv's are present and second one is 'IOA'
    //  (i.e. that SetContextToDbSnp has already been executed on the object)
    //Produce one var/var-ref *per* allele, each with single SeqLoc, 
    //representing right-most position
    //For an insert, this is a SeqLoc-Point
    //For a deletion, this is either a point or interval, depending on the size of the deletion.
    // and precisely why we are breaking up alleles into separate HGVS expressions
    static void AlterToHGVSVar(CRef<CVariation>& var, CScope& scope);
    static void AlterToHGVSVar(CRef<CSeq_annot>& var, CScope& scope);
 
    //VCF business logic
    static void AlterToVCFVar(CRef<CVariation>& var, CScope& scope);
    static void AlterToVCFVar(CRef<CSeq_annot>& var, CScope& scope);

    //Future thoughts:
    // Combine/collapse objects with same SeqLoc/Type
    // Simplify objects with common prefix in ref and all alts.
    // Identify mixed type objects, and split.

    static void AlterToVarLoc(CRef<CVariation>& var, CScope& scope);
    static void AlterToVarLoc(CRef<CSeq_annot>& var, CScope& scope);
 
};
