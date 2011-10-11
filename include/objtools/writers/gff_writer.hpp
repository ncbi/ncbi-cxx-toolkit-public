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
 * File Description:  Write gff file
 *
 */

#ifndef OBJTOOLS_READERS___GFF_WRITER__HPP
#define OBJTOOLS_READERS___GFF_WRITER__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objtools/writers/gff2_write_data.hpp>
#include <objtools/writers/writer.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CGff2Writer:
    public CWriterBase
//  ============================================================================
{
public:
    typedef enum {
        fNormal =       0,
        fSoQuirks =     1<<15,
    } TFlags;
    
public:
    CGff2Writer(
        CScope&,
        CNcbiOstream&,
        unsigned int = fNormal );
    CGff2Writer(
        CNcbiOstream&,
        unsigned int = fNormal );
    virtual ~CGff2Writer();

    virtual bool WriteHeader();
    virtual bool WriteHeader(
        const CSeq_annot& ) { return WriteHeader(); };
    virtual bool WriteFooter();
    virtual bool WriteFooter(
        const CSeq_annot& ) { return WriteFooter(); };

    //  ------------------------------------------------------------------------
    //  Supported object types:
    //  ------------------------------------------------------------------------
    bool WriteAnnot( 
        const CSeq_annot&,
        const string& = "",
        const string& = "" );
    bool WriteAlign( 
        const CSeq_align&,
        const string& = "",
        const string& = "" );

    //  ------------------------------------------------------------------------
    //  Supported handle types:
    //  ------------------------------------------------------------------------
    virtual bool WriteSeqEntryHandle(
        CSeq_entry_Handle,
        const string& = "",
        const string& = "" );

    virtual bool WriteBioseqHandle(
        CBioseq_Handle,
        const string& = "",
        const string& = "" );

    virtual bool WriteSeqAnnotHandle(
        CSeq_annot_Handle,
        const string& = "",
        const string& = "" );

    virtual SAnnotSelector& GetAnnotSelector();

protected:
    virtual bool x_WriteAnnot( 
        const CSeq_annot& );
    virtual bool x_WriteAlign( 
        const CSeq_align&,
        bool=false );
    virtual bool x_WriteSeqEntryHandle(
        CSeq_entry_Handle );
    virtual bool x_WriteBioseqHandle(
        CBioseq_Handle );
    virtual bool x_WriteSeqAnnotHandle(
        CSeq_annot_Handle );

    virtual bool x_WriteFeature(
        feature::CFeatTree&,
        CMappedFeat );
    virtual bool x_WriteFeature(
        CGffFeatureContext&,
        CMappedFeat );
    virtual bool x_WriteAssemblyInfo(
        const string&,
        const string& );

    virtual bool x_WriteBrowserLine(
        const CRef< CUser_object > );
    virtual bool x_WriteTrackLine(
        const CRef< CUser_object > );
    virtual bool x_WriteRecord( 
        const CGffWriteRecord* );

    CRef< CUser_object > x_GetDescriptor(
        const CSeq_annot&,
        const string& ) const;

    CRef< CUser_object > x_GetDescriptor(
        const CSeq_align&,
        const string& ) const;

    static bool x_NeedsQuoting(
        const string& );

    CRef<CScope> m_pScope;
    auto_ptr<SAnnotSelector> m_Selector;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___GFF_WRITER__HPP
