#ifndef OBJTOOLS_FORMAT___GATHER_ITEMS__HPP
#define OBJTOOLS_FORMAT___GATHER_ITEMS__HPP

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
* Author:  Mati Shomrat
*
* File Description:
*
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/items/comment_item.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CBioseq;
class CSeq_feat;
class CFlatItemOStream;
class CFFContext;


class CFlatGatherer : public CObject
{
public:
    
    // types
    typedef CFlatFileGenerator::TFormat         TFormat;
    typedef CFlatFileGenerator::TMode           TMode;
    typedef CFlatFileGenerator::TStyle          TStyle;
    typedef CFlatFileGenerator::TFlags          TFlags;
    typedef CFlatFileGenerator::TFilter         TFilter;

    // virtual constructor
    static CFlatGatherer* New(TFormat format);

    virtual void Gather(CFFContext& ctx, CFlatItemOStream& os) const;

    virtual ~CFlatGatherer(void);

protected:
    CFlatGatherer(void) {}

    // Gather the header part of the report (locus/defline/accession ...)
    // This is sone in a sparate function for EMBL to override.
    // The body of the report is the same for all formats.
    virtual void x_GatherHeader(CFFContext& ctx, CFlatItemOStream& os) const;
        
    CFlatItemOStream& ItemOS(void) const  { return *m_ItemOS;  }
    CFFContext&     Context(void) const { return *m_Context; }

    virtual void x_GatherSeqEntry(const CSeq_entry& entry) const;
    virtual void x_GatherBioseq(const CBioseq& seq) const;
    virtual void x_DoMultipleSections(const CBioseq& seq) const;
    virtual void x_DoSingleSection(const CBioseq& seq) const = 0;

    void x_GatherReferences(void) const;
    void x_GatherSourceFeatures(void) const;
    void x_GatherFeatures  (void) const;

    // comments
    void x_GatherComments  (void) const;
    void x_AddComment(CCommentItem* comment) const;
    void x_AddGSDBComment(const CDbtag& dbtag, CFFContext& ctx) const;
    void x_FlushComments(void) const;
    void x_IdComments(CFFContext& ctx) const;
    void x_RefSeqComments(CFFContext& ctx) const;
    void x_HistoryComments(CFFContext& ctx) const;
    void x_WGSComment(CFFContext& ctx) const;
    void x_GBBSourceComment(CFFContext& ctx) const;
    void x_DescComments(CFFContext& ctx) const;
    void x_MaplocComments(CFFContext& ctx) const;
    void x_RegionComments(CFFContext& ctx) const;
    void x_HTGSComments(CFFContext& ctx) const;
    void x_FeatComments(CFFContext& ctx) const;

    // features 
    void x_GatherSequence  (void) const;
    bool x_SkipFeature(const CSeq_feat& feat, const CFFContext& ctx) const;

private:
    CFlatGatherer(const CFlatGatherer&);
    CFlatGatherer& operator=(const CFlatGatherer&);

    // data
    mutable CRef<CFlatItemOStream>  m_ItemOS;
    mutable CRef<CFFContext>      m_Context;
    mutable vector< CRef<CCommentItem> > m_Comments; // in case of GSDB comment
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2003/12/17 19:52:53  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT___GATHER_INFO__HPP */
