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
#include <objtools/format/items/feature_item.hpp>



BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CBioseq;
class CSeq_feat;
class CFlatItemOStream;
class CFFContext;


class CFlatGatherer : public CObject
{
public:
    
    // virtual constructor
    static CFlatGatherer* New(TFormat format);

    virtual void Gather(CFFContext& ctx, CFlatItemOStream& os) const;

    virtual ~CFlatGatherer(void);

protected:
    CFlatGatherer(void) {}

    CFlatItemOStream& ItemOS(void) const  { return *m_ItemOS;  }
    CFFContext&     Context(void) const { return *m_Context; }

    virtual void x_GatherSeqEntry(const CSeq_entry& entry) const;
    virtual void x_GatherBioseq(const CBioseq& seq) const;
    virtual void x_DoMultipleSections(const CBioseq& seq) const;
    virtual bool x_DisplayBioseq(const CSeq_entry& entry, const CBioseq& seq) const;
    virtual void x_DoSingleSection(const CBioseq& seq) const = 0;

    // references
    void x_GatherReferences(void) const;

    // features
    void x_GatherFeatures  (void) const;
    void x_GetFeatsOnCdsProduct(const CSeq_feat& feat, CFFContext& ctx) const;
    bool x_SkipFeature(const CSeq_feat& feat, const CFFContext& ctx) const;

    // source features
    typedef CRef<CSourceFeatureItem>    TSFItem;
    typedef deque<TSFItem>              TSourceFeatSet;
    void x_GatherSourceFeatures(void) const;
    void x_CollectBioSources(TSourceFeatSet& srcs) const;
    void x_CollectBioSourcesOnBioseq(CBioseq_Handle bh, CRange<TSeqPos> range,
        CFFContext& ctx, TSourceFeatSet& srcs) const;
    void x_CollectSourceDescriptors(CBioseq_Handle& bh, const CRange<TSeqPos>& range,
        CFFContext& ctx, TSourceFeatSet& srcs) const;
    void x_CollectSourceFeatures(CBioseq_Handle& bh, const CRange<TSeqPos>& range,
        CFFContext& ctx, TSourceFeatSet& srcs) const;
    void x_MergeEqualBioSources(TSourceFeatSet& srcs) const;
    void x_SubtractFromFocus(TSourceFeatSet& srcs) const;

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

    // sequence 
    void x_GatherSequence  (void) const;
    

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
