#ifndef EXTERNAL_ANNOTS_HPP
#define EXTERNAL_ANNOTS_HPP

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
 * Authors:  Justin Foley
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp> // For CRef

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_annot;
class CSeq_annot_Handle;
struct SAnnotSelector;
class CSeq_entry;
class CScope;
class CBioseq;

BEGIN_SCOPE(edit)

class CAnnotGetter
{
public:
    static void AddAnnotations(const SAnnotSelector& sel,
                               CScope& scope,
                               CRef<CSeq_entry> seq_entry);

private:
    static void x_AddAnnotations(const SAnnotSelector& sel,
                                 CScope& scope,
                                 CBioseq& bioseq);

    static CRef<CSeq_annot> x_GetCompleteSeqAnnot(const CSeq_annot_Handle& annot_handle);
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE


#endif // EXTERNAL_ANNOTS_HPP
