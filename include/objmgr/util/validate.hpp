#ifndef VALIDATE__HPP
#define VALIDATE__HPP

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
 * Author:  .Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
    Validates CSeq_entries and CSeq_submits
 *   .......
 *
 */

#include <corelib/ncbistd.hpp>
#include <serial/objectinfo.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Forward declarations
class CSeq_entry;
class CSeq_submit;
class CSeq_descr;
class CSeq_feat;
class CBioseq;
class CBioseq_set;
class CObjectManager;
class CCit_sub;

BEGIN_SCOPE(validator)

class CValidErrItem : public CObject {
public:
    // constructor
    CValidErrItem(EDiagSev sev,
                  unsigned int         ei,
                  const string&        msg,
                  const CSerialObject& obj);
    // destructor
    ~CValidErrItem(void);

    // access functions
    EDiagSev                GetSeverity (void) const;
    const string&             GetErrCode  (void) const;
    const string&             GetMessage  (void) const;
    const string&             GetVerbose  (void) const;
    const CConstObjectInfo& GetObject   (void) const;

private:
    // member data values
    EDiagSev         m_Severity;   // severity level
    unsigned int     m_ErrIndex;   // error code index
    string           m_Message;    // specific error message
    CConstObjectInfo m_Object;     // type plus offending object

};

typedef list < CRef < CValidErrItem > > TErrs;

class CValidError
{
public:

    enum EValidOptions {
        eVal_non_ascii  = 0x00000001,
        eVal_splice_err = 0x00000002,
        eVal_val_align  = 0x00000004,
        eVal_no_context = 0x00000008
    };

    // constructors
    CValidError
    (CObjectManager&   objmgr,
     const CSeq_entry& se,
     unsigned int      options = 0x00000000);

    CValidError
    (CObjectManager&    objmgr,
     const CSeq_submit& ss,
     unsigned int       options = 0x00000000);

    // destructor
    ~CValidError(void);

private:
    // Prohibit copy constructor & assignment operator
    CValidError(const CValidError&);
    CValidError& operator= (const CValidError&);

    // Error list
    TErrs m_ErrItems;

    friend class CValidError_CI;
};


class CValidError_CI
{
public:
    CValidError_CI(void);
    CValidError_CI(const CValidError& ve,
                   string errcode   = kEmptyStr,
                   EDiagSev minsev  = eDiag_Info,
                   EDiagSev maxsev  = eDiag_Critical);
    CValidError_CI(const CValidError_CI& iter);
    virtual ~CValidError_CI(void);

    CValidError_CI& operator= (const CValidError_CI& iter);
    CValidError_CI& operator++ (void);
    operator bool (void) const;

    const CValidErrItem& operator* (void) const;

private:
    const CValidError*      m_ValidError;
    TErrs::const_iterator   m_ErrIter;
    unsigned int            m_ErrCodeFilter;
    EDiagSev                m_MinSeverity;
    EDiagSev                m_MaxSeverity;
};

END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
* $Log$
* Revision 1.2  2002/10/03 18:31:03  clausen
* Removed extra whitespace
*
* Revision 1.1  2002/10/03 16:30:56  clausen
* First version
*
* ===========================================================================
*/

#endif  /* VALIDATE__HPP */
