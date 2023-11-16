#ifndef _INFLUENZA_SET_HPP_
#define _INFLUENZA_SET_HPP_

/* $Id$
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
 *
 * File Description:
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <objmgr/bioseq_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class NCBI_CLEANUP_EXPORT CInfluenzaSet : public CObject {
public:
    CInfluenzaSet(const string& key);
    ~CInfluenzaSet() {}

    static string GetKey(const COrg_ref& org);
    bool OkToMakeSet() const;
    void MakeSet();

    typedef enum {
        eNotInfluenza = 0,
        eInfluenzaA,
        eInfluenzaB,
        eInfluenzaC,
        eInfluenzaD
    } EInfluenzaType;

    static EInfluenzaType GetInfluenzaType(const string& taxname);
    static size_t GetNumRequired(EInfluenzaType fluType);

    void AddBioseq(CBioseq_Handle bsh);

protected:
    typedef vector<CBioseq_Handle> TMembers;
    TMembers m_Members;
    const string m_Key;
    EInfluenzaType m_FluType;
    size_t m_Required;
};

bool NCBI_CLEANUP_EXPORT g_FindSegs(const CBioSource& src, size_t numRequired, set<size_t>& segsFound);

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _INFLUENZA_SET_HPP_
