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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write gff3 file
 *
 */

#ifndef OBJTOOLS_READERS___GVF_WRITER__HPP
#define OBJTOOLS_READERS___GVF_WRITER__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objtools/writers/gff3_writer.hpp>
#include <objtools/writers/gff3_write_data.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CGffAlignmentRecord;

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CGvfWriter
//  ============================================================================
    : public CGff3Writer
{
public:
    CGvfWriter(
        CScope&,
        CNcbiOstream&,
        unsigned int = fNormal );
    CGvfWriter(
        CNcbiOstream&,
        unsigned int = fNormal );
    virtual ~CGvfWriter();

    virtual bool WriteHeader();
    virtual bool WriteHeader(
        const CSeq_annot& );

protected:
    virtual bool xWriteFeature(
        CGffFeatureContext&,
        CMappedFeat );
    virtual bool xWriteFeatureVariationRef(
        CGffFeatureContext&,
        CMappedFeat );
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___GVF_WRITER__HPP
