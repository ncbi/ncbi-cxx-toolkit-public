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
 * Author:  Eugene Vasilchenko
 *
 * File Description:
 *   Interface to CSeq_feat methods.
 *
 */

#ifndef OBJECTS_SEQFEAT_ISEQ_FEAT_HPP
#define OBJECTS_SEQFEAT_ISEQ_FEAT_HPP

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CDbtag;
class CGene_ref;
class CProt_ref;

class NCBI_SEQFEAT_EXPORT ISeq_feat
{
public:
    virtual ~ISeq_feat(void);

    /// get gene (if present) from Seq-feat.xref list
    virtual const CGene_ref* GetGeneXref(void) const = 0;

    /// get protein (if present) from Seq-feat.xref list
    virtual const CProt_ref* GetProtXref(void) const = 0;

    /// Return a specified DB xref.  This will find the *first* item in the
    /// given referenced database.  If no item is found, an empty CConstRef<>
    /// is returned.
    virtual CConstRef<CDbtag> GetNamedDbxref(const CTempString& db) const = 0;

    /// Return a named qualifier.  This will return the first item matching the
    /// qualifier name.  If no such qualifier is found, an empty string is
    /// returned.
    virtual const string& GetNamedQual(const CTempString& qual_name) const = 0;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJECTS_SEQFEAT_ISEQ_FEAT_HPP
