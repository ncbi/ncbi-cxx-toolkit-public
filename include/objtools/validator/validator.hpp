#ifndef VALIDATOR___VALIDATOR__HPP
#define VALIDATOR___VALIDATOR__HPP

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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   Validates CSeq_entries and CSeq_submits
 *   .......
 *
 */
#include <corelib/ncbistd.hpp>
#include <corelib/ncbidiag.hpp>
#include <serial/objectinfo.hpp>
#include <serial/serialbase.hpp>
#include <objects/valerr/ValidErrItem.hpp>
#include <objects/valerr/ValidError.hpp>
#include <objmgr/scope.hpp>

#include <map>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_entry_Handle;
class CSeq_submit;
class CSeq_annot;
class CSeq_annot_Handle;
class CSeq_feat;
class CBioSource;
class CPubdesc;
class CBioseq;
class CSeqdesc;
class CObjectManager;
class CScope;

BEGIN_SCOPE(validator)


class NCBI_VALIDATOR_EXPORT CValidator : public CObject 
{
public:

    enum EValidOptions {
        eVal_non_ascii               = 0x1,
        eVal_no_context              = 0x2,
        eVal_val_align               = 0x4,
        eVal_val_exons               = 0x8,
        eVal_ovl_pep_err             = 0x10,
        eVal_need_taxid              = 0x20,
        eVal_need_isojta             = 0x40,
        eVal_validate_id_set         = 0x80,
        eVal_remote_fetch            = 0x100,
        eVal_far_fetch_mrna_products = 0x200,
        eVal_far_fetch_cds_products  = 0x400,
        eVal_locus_tag_general_match = 0x800,
        eVal_do_rubisco_test         = 0x1000,
        eVal_indexer_version         = 0x2000,
		eVal_use_entrez              = 0x4000,
        eVal_inference_accns         = 0x8000,
        eVal_ignore_exceptions       = 0x10000,
        eVal_report_splice_as_error  = 0x20000,
        eVal_latlon_check_state      = 0x40000,
        eVal_latlon_ignore_water     = 0x80000,
        eVal_genome_submission       = 0x100000,
    };

    // Construtor / Destructor
    CValidator(CObjectManager& objmgr);
    ~CValidator(void);

    // Validation methods:
    // It is possible to validate objects of types CSeq_entry, CSeq_submit 
    // or CSeq_annot. In addition to the object to validate the user must 
    // provide the scope which contain that object, and validation options
    // that are created by OR'ing EValidOptions (as specified above)

    // Validate Seq-entry. 
    // If provding a scope the Seq-entry must be a 
    // top-level Seq-entry in that scope.
    CConstRef<CValidError> Validate(const CSeq_entry& se, CScope* scope = 0,
        Uint4 options = 0);
    CConstRef<CValidError> Validate(const CSeq_entry_Handle& se,
        Uint4 options = 0);
    // Validate Seq-submit.
    // Validates each of the Seq-entry contained in the submission.
    CConstRef<CValidError> Validate(const CSeq_submit& ss, CScope* scope = 0,
        Uint4 options = 0);
    // Validate Seq-annot
    // Validates stand alone Seq-annot objects. This will supress any
    // check on the context of the annotaions.
    CConstRef<CValidError> Validate(const CSeq_annot_Handle& sa, 
        Uint4 options = 0);

	// Validate Seq-feat
    CConstRef<CValidError> Validate(const CSeq_feat& feat, 
        CScope *scope = 0,
        Uint4 options = 0);
    // old call
    NCBI_DEPRECATED
    CConstRef<CValidError> Validate(const CSeq_feat& feat, 
        Uint4 options = 0);

	// Validate BioSource
    CConstRef<CValidError> Validate(const CBioSource& src, 
        CScope *scope = 0,
        Uint4 options = 0);
    // old call
    NCBI_DEPRECATED
    CConstRef<CValidError> Validate(const CBioSource& src, 
        Uint4 options = 0);

	// Validate Pubdesc
    CConstRef<CValidError> Validate(const CPubdesc& pubdesc, 
        CScope *scope = 0,
        Uint4 options = 0);
    // old call
    NCBI_DEPRECATED
    CConstRef<CValidError> Validate(const CPubdesc& pubdesc, 
        Uint4 options = 0);

    // externally callable tests
    CConstRef<CValidError> GetTSANStretchErrors(const CSeq_entry_Handle& se); 
    CConstRef<CValidError> GetTSACDSOnMinusStrandErrors (const CSeq_entry_Handle& se);
    CConstRef<CValidError> GetTSAConflictingBiomolTechErrors (const CSeq_entry_Handle& se);
    CConstRef<CValidError> GetTSANStretchErrors(const CBioseq& seq); 
    CConstRef<CValidError> GetTSACDSOnMinusStrandErrors (const CSeq_feat& f, const CBioseq& seq);
    CConstRef<CValidError> GetTSAConflictingBiomolTechErrors (const CBioseq& seq);

    // progress reporting
    class CProgressInfo
    {
    public:
        enum EState {
            eState_not_set,
            eState_Initializing,
            eState_Align,
            eState_Annot,
            eState_Bioseq,
            eState_Bioseq_set,
            eState_Desc,
            eState_Descr,
            eState_Feat,
            eState_Graph
        };

        CProgressInfo(void): m_State(eState_not_set), 
            m_Total(0), m_TotalDone(0), 
            m_Current(0), m_CurrentDone(0),
            m_UserData(0)
        {}
        EState GetState(void)       const { return m_State;       }
        size_t GetTotal(void)       const { return m_Total;       }
        size_t GetTotalDone(void)   const { return m_TotalDone;   }
        size_t GetCurrent(void)     const { return m_Current;     }
        size_t GetCurrentDone(void) const { return m_CurrentDone; }
        void*  GetUserData(void)    const { return m_UserData;    }

    private:
        friend class CValidError_imp;

        EState m_State;
        size_t m_Total;
        size_t m_TotalDone;
        size_t m_Current;
        size_t m_CurrentDone; 
        void*  m_UserData;
    };

    typedef bool (*TProgressCallback)(CProgressInfo*);
    void SetProgressCallback(TProgressCallback callback, void* user_data = 0);

private:
    // Prohibit copy constructor & assignment operator
    CValidator(const CValidator&);
    CValidator& operator= (const CValidator&);

    CRef<CObjectManager>    m_ObjMgr;
    TProgressCallback       m_PrgCallback;
    void*                   m_UserData;
};


// Inline Functions:


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDATOR__HPP */
