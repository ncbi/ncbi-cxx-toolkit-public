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

USING_NCBI_SCOPE;
USING_SCOPE(objects);
using namespace std;

#define MAX_LEN 1000

class CVariationUtilities
{
public:
    static void CorrectRefAllele(CRef<CVariation>& v, CScope& scope);      
    static void CorrectRefAllele(CRef<CVariation_ref>& var, CScope& scope);    
    static void CorrectRefAllele(CRef<CSeq_annot>& v, CScope& scope);
    static void CorrectRefAllele(CVariation_ref& var, CScope& scope, const string& new_ref); 
private:
    static string GetRefAlleleFromVP(CRef<CVariantPlacement> vp, CScope& scope);
    static string GetAlleleFromLoc(const CSeq_loc& loc, CScope& scope);
    static void FixAlleles(CRef<CVariation> v, string old_ref, string new_ref);
    static void FixAlleles(CVariation_ref& vr, string old_ref, string new_ref) ;
};

class CVariationNormalization
{
public:
    enum ETargetContext {
       eDbSnp,
       eHGVS,
       eVCF
    };
    
/////////////////////////////////////////////////////////
 //For later creation, as thin wrappers on the logic below
    //ASSUME scope has everything you need, but that lookups *can* fail so handle gracefully.
    static void NormalizeVariation(CRef<CVariation>& var, ETargetContext target_ctxt, CScope& scope);     
 
    // following an overloaded model.  If anyone finds that too confusing or obfuscated, speak up.
    static void NormalizeVariation(CRef<CVariation_ref>& var, ETargetContext target_ctxt, CScope& scope); 
/////////////////////////////////////////////////////////
 
 
//Initially, the following will be the public methods, each encapsulating a *step* in the normalization process.
//We can add the above wrappers and enum as a final touch. 
 
    //These methods search out ambiguous ins/del, and alter SeqLoc
    //to a Seq-equiv: the first is the original SeqLoc
    //the second is an interval representing the IOA
    static void NormalizeAmbiguousVars(CRef<CVariation>& var);
    static void NormalizeAmbiguousVars(CRef<CVariation_ref>& var);
 
    //HGVS
    //*Assume* Seq-equiv's are present and second one is 'IOA'
    //  (i.e. that SetContextToDbSnp has already been executed on the object)
    //Produce one var/var-ref *per* allele, each with single SeqLoc, 
    //representing right-most position
    //For an insert, this is a SeqLoc-Point
    //For a deletion, this is either a point or interval, depending on the size of the deletion.
    // and precisely why we are breaking up alleles into separate HGVS expressions
    static void AlterToHGVSVar(CRef<CVariation>& var);
    static void AlterToHGVSVar(CRef<CVariation_ref>& var);
 
    //VCF business logic
    static void AlterToVCFVar(CRef<CVariation>& var);
    static void AlterToVCFVar(CRef<CVariation_ref>& var);
 
    //Future thoughts:
    // Combine/collapse objects with same SeqLoc/Type
    // Simplify objects with common prefix in ref and all alts.
    // Identify mixed type objects, and split.
 
private:
  
 
    //Shifting logic is shared between contexts
    //So shouldn't be wrapped up in the above.
    void x_ShiftLeft(CRef<CVariation>& var, CScope &scope);
    void x_ShiftLeft(CRef<CVariation_ref>& var, CScope &scope);
  
    void x_ShiftRight(CRef<CVariation>& var, CScope &scope);
    void x_ShiftRight(CRef<CVariation_ref>& var, CScope &scope);
 
    //Each Group of Business rules will get a private method
    //Ideally, operations on Var and Var-ref could share common private methods
    //that are provided common data, like a SeqLoc, Ref Allele and Alt Allele(s).

    void x_rotate_left(string &v);
    void x_rotate_right(string &v);
    void x_PrefetchSequence(CScope &scope, string accession);
    string x_GetSeq(int pos, int length);
    string m_Sequence;
    string m_Accession;
};
