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

#ifndef VALIDATOR___VALIDATORP__HPP
#define VALIDATOR___VALIDATORP__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>
#include <objects/seqfeat/PCRReactionSet.hpp>
#include <objtools/validator/validator.hpp>
#include <objtools/validator/validerror_imp.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


BEGIN_SCOPE(validator)


// ===========================  for handling PCR primer subtypes on BioSource ==

class CPCRSet
{
public:
    CPCRSet(size_t pos);
    virtual ~CPCRSet(void);

    string GetFwdName(void)            const { return m_FwdName; }
    string GetFwdSeq(void)             const { return m_FwdSeq; }
    string GetRevName(void)            const { return m_RevName; }
    string GetRevSeq(void)             const { return m_RevSeq; }
    size_t GetOrigPos(void)            const { return m_OrigPos; }

    void SetFwdName(string fwd_name) { m_FwdName = fwd_name; }
    void SetFwdSeq(string fwd_seq)   { m_FwdSeq = fwd_seq; }
    void SetRevName(string rev_name) { m_RevName = rev_name; }
    void SetRevSeq(string rev_seq)   { m_RevSeq = rev_seq; }

private:
    string m_FwdName;
    string m_FwdSeq;
    string m_RevName;
    string m_RevSeq;
    size_t m_OrigPos;
};

class CPCRSetList
{
public:
    CPCRSetList(void);
    ~CPCRSetList(void);

    void AddFwdName (string name);
    void AddRevName (string name);
    void AddFwdSeq (string name);
    void AddRevSeq (string name);

    bool AreSetsUnique(void);
    static bool AreSetsUnique(const CPCRReactionSet& primers);

private:
    vector <CPCRSet *> m_SetList;
};



END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDATORP__HPP */
