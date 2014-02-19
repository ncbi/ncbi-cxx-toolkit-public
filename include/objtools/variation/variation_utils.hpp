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

#include <objects/variation/Variation.hpp>
#include <objects/variation/VariantPlacement.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);
using namespace std;


#define SEQVEC_CACHE

class CVariationUtilities
{
public:
    static void CorrectRefAllele(CRef<CVariation>& v, CScope& scope);      
    static void CorrectRefAllele(CRef<CVariation_ref>& var, CScope& scope);    
    static void CorrectRefAllele(CRef<CSeq_annot>& v, CScope& scope);
    static void CorrectRefAllele(CVariation_ref& var, CScope& scope, const string& new_ref); 
    static bool IsReferenceCorrect(const CSeq_feat& feat, string& wrong_ref, string& correct_ref, CScope& scope);
private:
    static string GetRefAlleleFromVP(CRef<CVariantPlacement> vp, CScope& scope, TSeqPos length);
    static string GetAlleleFromLoc(const CSeq_loc& loc, CScope& scope);
    static void FixAlleles(CRef<CVariation> v, string old_ref, string new_ref);
    static void FixAlleles(CVariation_ref& vr, string old_ref, string new_ref) ;
    static const unsigned int MAX_LEN=1000;
};




class CVariationNormalization_base_cache
{
protected:
    static int x_GetSeqSize(string accession);
    static void x_PrefetchSequence(CScope &scope,  CRef<CSeq_id> seq_id, string accession, ENa_strand strand = eNa_strand_unknown);
    static string x_GetSeq(int pos, int length, string accession);
    static void x_rotate_left(string &v);
    static void x_rotate_right(string &v);
    static string x_CompactifySeq(string a);
private:    
#ifdef SEQVEC_CACHE
    static CCache<string,CSeqVector> m_cache;
#else
    static CCache<string, string> m_cache;
#endif
};

class CVariationNormalization_base : public CVariationNormalization_base_cache
{
protected:
    void x_Shift(CRef<CVariation>& var, CScope &scope);
    void x_Shift(CRef<CSeq_annot>& var, CScope &scope);
    void x_Shift(CRef<CVariation_ref>& var, CScope &scope);
    void x_ProcessInstance(CVariation_inst &inst, CSeq_loc &loc, bool &is_deletion,  CSeq_literal *&refref, string &ref, int &pos_left, int &pos_right, int &new_pos_left, int &new_pos_right);    
    virtual bool x_ProcessShift(string &a, int &pos_left, int &pos_right) = 0;
    virtual void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right) = 0;
    bool x_IsShiftable(CSeq_loc &loc, string &ref, CScope &scope, int type);
    void x_ExpandLocation(CSeq_loc &loc, int pos_left, int pos_right);
    int x_GetSeqSize() {return CVariationNormalization_base_cache:: x_GetSeqSize(m_accession);}
    void x_PrefetchSequence(CScope &scope, CRef<CSeq_id> seq_id, ENa_strand strand = eNa_strand_unknown) 
        {
            seq_id->GetLabel(&m_accession);
            CVariationNormalization_base_cache::x_PrefetchSequence(scope, seq_id, m_accession, strand);
        }
    string x_GetSeq(int pos, int length) {return CVariationNormalization_base_cache::x_GetSeq(pos,length,m_accession);}
    int m_Type;         // Current limitation - the same type for the whole placement/location.

private:
    string m_accession;
};

class CVariationNormalizationLeft : public virtual CVariationNormalization_base
{
public:
    virtual bool x_ProcessShift(string &a, int &pos_left, int &pos_right);
    virtual void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right);
};

class CVariationNormalizationRight : public virtual CVariationNormalization_base
{
public:
    virtual bool x_ProcessShift(string &a, int &pos_left, int &pos_right);
    virtual void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right);
};

class CVariationNormalizationInt : public CVariationNormalizationLeft, public CVariationNormalizationRight
{
public:
    virtual bool x_ProcessShift(string &a, int &pos_left, int &pos_right);
    virtual void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right);
};


class CVariationNormalizationLeftInt : public CVariationNormalizationLeft, public CVariationNormalizationRight
{
public:
    virtual bool x_ProcessShift(string &a, int &pos_left, int &pos_right);
    virtual void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right);
};

class CVariationNormalizationVCFandHGVS :   public CVariationNormalizationLeft, public CVariationNormalizationRight
{
public:
    //HGVS
    //*Assume* Seq-equiv's are present and second one is 'IOA'
    //  (i.e. that SetContextToDbSnp has already been executed on the object)
    //Produce one var/var-ref *per* allele, each with single SeqLoc, 
    //representing right-most position
    //For an insert, this is a SeqLoc-Point
    //For a deletion, this is either a point or interval, depending on the size of the deletion.
    // and precisely why we are breaking up alleles into separate HGVS expressions
    void AlterToHGVSVar(CRef<CVariation>& var, CScope& scope);
    void AlterToHGVSVar(CRef<CSeq_annot>& var, CScope& scope);
 
    //VCF business logic
    void AlterToVCFVar(CRef<CVariation>& var, CScope& scope);
    void AlterToVCFVar(CRef<CSeq_annot>& var, CScope& scope);

    virtual bool x_ProcessShift(string &a, int &pos_left, int &pos_right) { return true;}
    virtual void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right) {}
  
};


class CVariationNormalizationIntAndLeftInt :   public CVariationNormalizationInt, public CVariationNormalizationLeftInt
{
public: 
//Initially, the following will be the public methods, each encapsulating a *step* in the normalization process.
//We can add the above wrappers and enum as a final touch. 
 
    //These methods search out ambiguous ins/del, and alter SeqLoc
    //to a Seq-equiv: the first is the original SeqLoc
    //the second is an interval representing the IOA
    void NormalizeAmbiguousVars(CRef<CVariation>& var, CScope &scope);
    void NormalizeAmbiguousVars(CRef<CSeq_annot>& var, CScope &scope);
 
 
    //Future thoughts:
    // Combine/collapse objects with same SeqLoc/Type
    // Simplify objects with common prefix in ref and all alts.
    // Identify mixed type objects, and split.

    void AlterToVarLoc(CRef<CVariation>& var, CScope& scope);
    void AlterToVarLoc(CRef<CSeq_annot>& var, CScope& scope);

    bool IsShiftable(CSeq_loc &loc, string &ref, CScope &scope, int type); 

    virtual bool x_ProcessShift(string &a, int &pos_left, int &pos_right) { return true;}
    virtual void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right) {}
};

class CVariationNormalization :   public CVariationNormalizationVCFandHGVS,  public CVariationNormalizationIntAndLeftInt
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
    void NormalizeVariation(CRef<CVariation>& var, ETargetContext target_ctxt, CScope& scope);     
    void  NormalizeVariation(CRef<CSeq_annot>& var, ETargetContext target_ctxt, CScope& scope);

    virtual bool x_ProcessShift(string &a, int &pos_left, int &pos_right) {return true;}
    virtual void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right) {}

};
