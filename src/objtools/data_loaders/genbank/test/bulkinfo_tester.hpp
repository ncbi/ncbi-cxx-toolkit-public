#ifndef BULKINFO_TESTER_HPP
#define BULKINFO_TESTER_HPP

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
* Author:
*           Andrei Gourianov, Michael Kimelman
*
* File Description:
*           Basic test of GenBank data loader
*
* ===========================================================================
*/

#include <objmgr/scope.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class IBulkTester
{
public:
    DECLARE_CLASS_STATIC_MUTEX(sm_DisplayMutex);

    typedef vector<CSeq_id_Handle> TIds;
    TIds ids;
    CScope::TGetFlags get_flags;
    bool report_all_errors;

    enum EBulkType {
        eBulk_gi,
        eBulk_acc,
        eBulk_label,
        eBulk_taxid,
        eBulk_hash,
        eBulk_length,
        eBulk_type,
        eBulk_state
    };
    static IBulkTester* CreateTester(EBulkType type);

    IBulkTester(void);
    virtual ~IBulkTester(void);

    void SetParams(const TIds& ids, CScope::TGetFlags get_flags);

    virtual const char* GetType(void) const = 0;
    virtual void LoadBulk(CScope& scope) = 0;
    virtual void LoadSingle(CScope& scope) = 0;
    virtual void LoadVerify(CScope& scope) = 0;
    virtual void LoadVerify(const vector<string>& lines) = 0;
    virtual void SaveResults(CNcbiOstream& out) const = 0;
    virtual bool Correct(size_t i) const = 0;
    virtual void DisplayData(CNcbiOstream& out, size_t i) const = 0;
    virtual void DisplayDataVerify(CNcbiOstream& out, size_t i) const = 0;

    void Display(CNcbiOstream& out, bool verify) const;
    void Display(CNcbiOstream& out, size_t i, bool verify) const;
    vector<bool> GetErrors(void) const;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif// BULKINFO_TESTER_HPP
