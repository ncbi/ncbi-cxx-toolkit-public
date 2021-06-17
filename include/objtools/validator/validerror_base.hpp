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
 *`
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   Privae classes and definition for the validator
 *   .......
 *
 */

#ifndef VALIDATOR___VALIDERROR_BASE__HPP
#define VALIDATOR___VALIDERROR_BASE__HPP

#include <corelib/ncbistd.hpp>
#include <objtools/validator/cache_impl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

BEGIN_SCOPE(validator)

class CValidError_imp;


class NCBI_VALIDATOR_EXPORT CValidError_base
{
protected:
    CValidError_base(CValidError_imp& imp);
    virtual ~CValidError_base();

    void PostErr(EDiagSev sv, EErrType et, const string& msg,
        const CSerialObject& obj);
    //void PostErr(EDiagSev sv, EErrType et, const string& msg, TDesc ds);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CSeq_feat& ft);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CBioseq& sq);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CSeq_entry& ctx,
        const CSeqdesc&  ds);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CBioseq_set& set);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CSeq_annot& annot);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CSeq_graph& graph);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CBioseq& sq,
        const CSeq_graph& graph);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CSeq_align& align);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CSeq_entry& entry);

    CCacheImpl & GetCache(void);

    static CSeq_entry_Handle GetAppropriateXrefParent(CSeq_entry_Handle seh);

    CValidError_imp& m_Imp;
    CScope* m_Scope;
};


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDERROR_BASE__HPP */
