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
 * File Description:  Write gtf file
 *
 */

#ifndef OBJTOOLS_WRITERS___GTF_WRITER__HPP
#define OBJTOOLS_READERS___GTF_WRITER__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objtools/writers/gff3_write_data.hpp>
//#include <objtools/writers/gff_writer.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CGtfRecord;
class CGff3WriteRecordSet;

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CGtfWriter:
    public CGffWriter
//  ============================================================================
{
public:
    CGtfWriter(
        CNcbiOstream& );
    ~CGtfWriter();

protected:
    bool x_WriteHeader();

    bool x_AssignObject( 
        CSeq_annot_Handle,
        const CSeq_feat&,        
        CGff3WriteRecordSet& );

    bool x_AssignObjectGene( 
        CSeq_annot_Handle,
        const CSeq_feat&,        
        CGff3WriteRecordSet& );

    bool x_AssignObjectMrna( 
        CSeq_annot_Handle,
        const CSeq_feat&,        
        CGff3WriteRecordSet& );

    bool x_AssignObjectCds( 
        CSeq_annot_Handle,
        const CSeq_feat&,        
        CGff3WriteRecordSet& );

    void x_PriorityProcess(
        const string&,
        map<string, string >&,
        string& ) const;

    bool x_SplitCdsLocation(
        const CSeq_feat&,
        CRef< CSeq_loc >&,
        CRef< CSeq_loc >&,
        CRef< CSeq_loc >& ) const;

    void x_AddMultipleRecords(
        const CGtfRecord&,
        CRef< CSeq_loc >,
        CGff3WriteRecordSet& );

    static bool x_NeedsQuoting(
        const string& ) { return true; };

    typedef map< int, CRef< CSeq_interval > > TExonMap;
    typedef TExonMap::const_iterator TExonCit;
    TExonMap m_exonMap;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___GTF_WRITER__HPP
