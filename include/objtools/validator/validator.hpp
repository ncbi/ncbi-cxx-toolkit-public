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

#include <map>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_submit;
class CSeq_annot;
class CObjectManager;
class CScope;

BEGIN_SCOPE(validator)

class CValidError;

class NCBI_VALIDATOR_EXPORT CValidErrItem : public CObject 
{
public:
    

    // destructor
    ~CValidErrItem(void);

    // Severity of error
    EDiagSev                GetSeverity (void) const;
    // Error code
    const string&           GetErrCode  (void) const;
    // Error group (SEQ_FEAT, SEQ_INST etc.)
    const string&           GetErrGroup (void) const;
    // Error message
    const string&           GetMsg      (void) const;
    // Verbose message
    const string&           GetVerbose  (void) const;
    // Offending ovject description
    const string&           GetObjDesc  (void) const;
    // Offending object
    const CSerialObject&    GetObject   (void) const;

    // Convert Severity from enum to a string representation
    static const string& ConvertSeverity(EDiagSev sev);

private:
    friend class CValidError;

    typedef CConstRef<CSerialObject>    TObject;

    // constructor
    CValidErrItem(EDiagSev             sev,       // severity
                  unsigned int         ec,        // error code
                  const string&        msg,       // message
                  const string&        obj_desc,  // object description
                  const CSerialObject& obj);      // offending object
    
    CValidErrItem(void);
    CValidErrItem(const CValidErrItem&);

    // member data values
    EDiagSev      m_Severity;   // severity level
    unsigned int  m_ErrIndex;   // error code index
    string        m_Message;    // specific error message
    string        m_Desc;       // offending object's description
    TObject       m_Object;     // offending object

    static const string sm_Terse[];
    static const string sm_Verbose[];
};


class NCBI_VALIDATOR_EXPORT CValidError : public CObject
{
public:
    // constructors
    CValidError(void);
    
    void AddValidErrItem(EDiagSev             sev,   // severity
                         unsigned int         ec,    // error code
                         const string&        msg,   // specific error message
                         const string&        desc,  // offending object's description
                         const CSerialObject& obj);  // offending object

    // Statistics
    SIZE_TYPE TotalSize(void)    const;
    SIZE_TYPE Size(EDiagSev sev) const;

    SIZE_TYPE InfoSize    (void) const;
    SIZE_TYPE WarningSize (void) const;
    SIZE_TYPE ErrorSize   (void) const;
    SIZE_TYPE CriticalSize(void) const;
    SIZE_TYPE FatalSize   (void) const;

    // destructor
    ~CValidError(void);

private:
    friend class CValidError_CI;

    typedef vector < CConstRef < CValidErrItem > > TErrs;
    typedef map<EDiagSev, SIZE_TYPE>               TSevStats;

    // Prohibit copy constructor & assignment operator
    CValidError(const CValidError&);
    CValidError& operator= (const CValidError&);

    // data
    TErrs       m_ErrItems;  // Error list
    TSevStats   m_Stats;     // severity statistics
};


class NCBI_VALIDATOR_EXPORT CValidError_CI
{
public:
    CValidError_CI(void);
    CValidError_CI(const CValidError& ve,
                   const string& errcode = kEmptyStr,
                   EDiagSev minsev  = eDiagSevMin,
                   EDiagSev maxsev  = eDiagSevMax);
    CValidError_CI(const CValidError_CI& iter);
    virtual ~CValidError_CI(void);

    CValidError_CI& operator=(const CValidError_CI& iter);
    CValidError_CI& operator++(void);

    operator bool(void) const;

    const CValidErrItem& operator* (void) const;
    const CValidErrItem* operator->(void) const;

private:
    bool Filter(const CValidErrItem& item) const;
    bool AtEnd(void) const;
    void Next(void);

    CConstRef<CValidError>               m_Validator;
    CValidError::TErrs::const_iterator   m_Current;

    // filters:
    string                               m_ErrCodeFilter;
    EDiagSev                             m_MinSeverity;
    EDiagSev                             m_MaxSeverity;
};


class NCBI_VALIDATOR_EXPORT CValidator : public CObject 
{
public:

    enum EValidOptions {
        eVal_non_ascii       = 0x1,
        eVal_no_context      = 0x2,
        eVal_val_align       = 0x4,
        eVal_val_exons       = 0x8,
        eVal_splice_err      = 0x10,
        eVal_ovl_pep_err     = 0x20,
        eVal_need_taxid      = 0x40,
        eVal_need_isojta     = 0x80, 
        eVal_validate_id_set = 0x100,
        eVal_remote_fetch    = 0x200,

        // !!! For test purposes only {
        eVal_perf_bottlenecks  = 0x1000
        // }
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
    // Validate Seq-submit.
    // Validates each of the Seq-entry contained in the submission.
    CConstRef<CValidError> Validate(const CSeq_submit& ss, CScope* scope = 0,
        Uint4 options = 0);
    // Validate Seq-annot
    // Validates stand alone Seq-annot objects. This will supress any
    // check on the context of the annotaions.
    CConstRef<CValidError> Validate(const CSeq_annot& sa, CScope* scope = 0,
        Uint4 options = 0);

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

inline
SIZE_TYPE CValidError::TotalSize(void) const 
{
    return m_ErrItems.size();
}


inline
SIZE_TYPE CValidError::Size(EDiagSev sev) const 
{
    return const_cast<CValidError*>(this)->m_Stats[sev]; 
}


inline
SIZE_TYPE CValidError::InfoSize(void) const
{
    return Size(eDiag_Info);
}


inline
SIZE_TYPE CValidError::WarningSize(void) const
{
    return Size(eDiag_Warning);
}


inline
SIZE_TYPE CValidError::ErrorSize(void) const
{
    return Size(eDiag_Error);
}


inline
SIZE_TYPE CValidError::CriticalSize(void) const
{
    return Size(eDiag_Critical);
}


inline
SIZE_TYPE CValidError::FatalSize(void) const
{
    return Size(eDiag_Fatal);
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.15  2004/07/29 16:06:44  shomrat
* Separated error message from offending object's description
*
* Revision 1.14  2004/06/25 14:54:27  shomrat
* minor changes to CValidErrItem and CValidError APIs
*
* Revision 1.13  2003/05/15 00:23:01  ucko
* auto_ptr<> -> CConstRef<> in return type of CValidator::Validate, per
* the actual current definitions in validator.cpp...
*
* Revision 1.12  2003/04/18 19:06:52  shomrat
* redundant comma
*
* Revision 1.11  2003/04/15 14:55:02  shomrat
* Added a progress callback mechanism
*
* Revision 1.10  2003/04/04 18:29:06  shomrat
* Added remote_fetch option
*
* Revision 1.9  2003/03/21 20:57:59  shomrat
* Added validate is set agins db option
*
* Revision 1.8  2003/03/20 18:52:11  shomrat
* Addes support for standalone Seq-annot validation. Decoupling the validation class (CValidator) from the error repository (CValidError)
*
* Revision 1.7  2003/03/10 18:11:53  shomrat
* Added statistics information
*
* Revision 1.6  2003/03/06 19:31:57  shomrat
* Bug fix and code cleanup in CValidError_CI
*
* Revision 1.5  2003/02/24 20:14:59  shomrat
* Added AddValidErrItem instead of exposing the undelying container
*
* Revision 1.4  2003/02/07 21:00:27  shomrat
* Added size()
*
* Revision 1.3  2003/02/03 20:16:55  shomrat
* Added option to supress suspected performance bottlenecks (for testing)
*
* Revision 1.2  2003/01/07 19:57:03  shomrat
* Added Win32 export specifier; Memebr GetMessage() changed to GetMsg() due to conflict
*
* Revision 1.1  2002/12/19 20:54:23  shomrat
* From /objects/util/validate.hpp
*
*
* ===========================================================================
*/

#endif  /* VALIDATOR___VALIDATOR__HPP */
