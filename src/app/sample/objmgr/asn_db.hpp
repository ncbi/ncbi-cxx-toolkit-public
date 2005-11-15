#ifndef __ASN_DB__HPP
#define __ASN_DB__HPP

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
* Author: Maxim Didenko
*
* File Description:
*
*/

#include <corelib/ncbiobj.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CBioseq;
class CSeq_id;

class CAsnDB
{
public:
    CAsnDB( const string& db_path );
    ~CAsnDB();

    void CreateBlob(const string& blobid);
    bool HasBlob(const string& blobid) const;

    bool Save(const string& blobid, const CBioseq&);
    CRef<CBioseq> Restore(const string& blobid, const CSeq_id& id) const;

    bool SaveTSE(const string& blobid, const CSeq_entry&);
    CRef<CSeq_entry> RestoreTSE(const string& blobid) const;

private:
    string m_DBPath;
};

END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/11/15 19:25:21  didenko
 * Added bioseq_edit_sample sample
 *
 * ===========================================================================
 */

#endif // __PROT_ANNOT_DB__HPP
