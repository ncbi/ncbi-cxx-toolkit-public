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

#include <objtools/format/context.hpp>
#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/items/feature_item.hpp>

#include <deque>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CBioseq_Handle;
class CSeq_feat;
class CFlatItemOStream;


class CFlatGatherer : public CObject
{
public:
    
    // virtual constructor
    static CFlatGatherer* New(CFlatFileConfig::TFormat format);

    virtual void Gather(CFlatFileContext& ctx, CFlatItemOStream& os) const;

    virtual ~CFlatGatherer(void);

protected:
    typedef CRange<TSeqPos> TRange;

    CFlatGatherer(void) {}

    CFlatItemOStream& ItemOS     (void) const { return *m_ItemOS;  }
    CBioseqContext& Context      (void) const { return *m_Current; }
    const CFlatFileConfig& Config(void) const { return m_Context->GetConfig(); }

    virtual void x_GatherSeqEntry(const CSeq_entry_Handle& entry) const;
    virtual void x_GatherBioseq(const CBioseq_Handle& seq) const;
    virtual void x_DoMultipleSections(const CBioseq_Handle& seq) const;
    virtual void x_DoSingleSection(CBioseqContext& ctx) const = 0;

    // references
    typedef CBioseqContext::TReferences TReferences;
    void x_GatherReferences(void) const;
    void x_GatherReferences(const CSeq_loc& loc, TReferences& refs) const;
    void x_GatherCDSReferences(TReferences& refs) const;

    // features
    void x_GatherFeatures  (void) const;
    void x_GetFeatsOnCdsProduct(const CSeq_feat& feat, CBioseqContext& ctx,
        CRef<CSeq_loc_Mapper>& mapper) const;
    void x_CopyCDSFromCDNA(const CSeq_feat& feat, CBioseqContext& ctx) const;
    bool x_SkipFeature(const CSeq_feat& feat, const CBioseqContext& ctx) const;
    void x_GatherFeaturesOnLocation(const CSeq_loc& loc, SAnnotSelector& sel,
        CBioseqContext& ctx) const;

    // source features
    typedef CRef<CSourceFeatureItem>    TSFItem;
    typedef deque<TSFItem>              TSourceFeatSet;
    void x_GatherSourceFeatures(void) const;
    void x_CollectBioSources(TSourceFeatSet& srcs) const;
    void x_CollectBioSourcesOnBioseq(const CBioseq_Handle& bh,
        const TRange& range, CBioseqContext& ctx, TSourceFeatSet& srcs) const;
    void x_CollectSourceDescriptors(const CBioseq_Handle& bh, CBioseqContext& ctx,
        TSourceFeatSet& srcs) const;
    void x_CollectSourceFeatures(const CBioseq_Handle& bh,
        const TRange& range, CBioseqContext& ctx,
        TSourceFeatSet& srcs) const;
    void x_MergeEqualBioSources(TSourceFeatSet& srcs) const;
    void x_SubtractFromFocus(TSourceFeatSet& srcs) const;

    // alignments
    void x_GatherAlignments(void) const;

    // comments
    void x_GatherComments  (void) const;
    void x_AddComment(CCommentItem* comment) const;
    void x_AddGSDBComment(const CDbtag& dbtag, CBioseqContext& ctx) const;
    void x_FlushComments(void) const;
    void x_IdComments(CBioseqContext& ctx) const;
    void x_RefSeqComments(CBioseqContext& ctx) const;
    void x_HistoryComments(CBioseqContext& ctx) const;
    void x_WGSComment(CBioseqContext& ctx) const;
    void x_GBBSourceComment(CBioseqContext& ctx) const;
    void x_BarcodeComment(CBioseqContext& ctx) const;
    void x_DescComments(CBioseqContext& ctx) const;
    void x_MaplocComments(CBioseqContext& ctx) const;
    void x_RegionComments(CBioseqContext& ctx) const;
    void x_HTGSComments(CBioseqContext& ctx) const;
    void x_FeatComments(CBioseqContext& ctx) const;

    // sequence 
    void x_GatherSequence  (void) const;
    

private:
    CFlatGatherer(const CFlatGatherer&);
    CFlatGatherer& operator=(const CFlatGatherer&);

    // types
    typedef vector< CRef<CCommentItem> > TCommentVec;

    // data
    mutable CRef<CFlatItemOStream>   m_ItemOS;
    mutable CRef<CFlatFileContext>   m_Context;
    mutable CRef<CBioseqContext>     m_Current;
    mutable TCommentVec              m_Comments;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.16  2005/03/28 17:12:40  shomrat
* Removed no longer used method
*
* Revision 1.15  2005/02/02 19:34:49  shomrat
* Added barcode comment
*
* Revision 1.14  2005/01/12 15:49:21  shomrat
* Use typedef TReferences
*
* Revision 1.13  2004/06/21 18:50:19  ucko
* +x_GatherAlignments
*
* Revision 1.12  2004/05/06 17:45:08  shomrat
* private methods changes
*
* Revision 1.11  2004/04/27 15:10:30  shomrat
* use TRange instead of CRange<TSeqPos>
*
* Revision 1.10  2004/04/22 15:44:29  shomrat
* Changes in context
*
* Revision 1.9  2004/03/26 21:58:06  ucko
* +<context.hpp> due to use of CRef<CBioseqContext>, which requires a full
* declaration with some compilers.
*
* Revision 1.8  2004/03/26 17:21:57  shomrat
* + typedef TCommentVec
*
* Revision 1.7  2004/03/25 20:31:28  shomrat
* Use handles
*
* Revision 1.6  2004/03/16 15:40:21  vasilche
* Added required include
*
* Revision 1.5  2004/03/12 16:54:32  shomrat
* + x_DisplayBioseq
*
* Revision 1.4  2004/02/11 22:47:14  shomrat
* using types in flag file
*
* Revision 1.3  2004/02/11 16:41:30  shomrat
* modification to feature gathering methods
*
* Revision 1.2  2004/01/14 15:54:06  shomrat
* added source indicator to x_GatherFeatures (temporary)
*
* Revision 1.1  2003/12/17 19:52:53  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT___GATHER_INFO__HPP */
