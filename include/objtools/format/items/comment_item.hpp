#ifndef OBJTOOLS_FORMAT_ITEMS___COMMENT_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___COMMENT_ITEM__HPP

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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
*
* File Description:
*   Comment item for flat-file generator
*
*/
#include <corelib/ncbistd.hpp>

#include <objects/general/User_object.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CBioseq;
class CSeqdesc;
class CSeq_feat;
class CBioseqContext;
class IFormatter;
class CMolInfo;
class CBioseq_Handle;
struct SModelEvidance;


///////////////////////////////////////////////////////////////////////////
//
// Comment

class CCommentItem : public CFlatItem
{
public:
    enum EType {
        eGenomeAnnotation,
        eModel,
        eUser
    };

    enum ERefTrackStatus {
        eRefTrackStatus_Unknown,
        eRefTrackStatus_Inferred,
        eRefTrackStatus_Provisional,
        eRefTrackStatus_Predicted,
        eRefTrackStatus_Validated,
        eRefTrackStatus_Reviewd,
        eRefTrackStatus_Model,
        eRefTrackStatus_WGS
    };

    // typedefs
    typedef EType           TType;
    typedef ERefTrackStatus TRefTrackStatus;

    // constructors
    CCommentItem(const string& comment, CBioseqContext& ctx,
        const CSerialObject* obj = 0);
    CCommentItem(const CSeqdesc&  desc, CBioseqContext& ctx);
    CCommentItem(const CSeq_feat& feat, CBioseqContext& ctx);

    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;

    bool IsFirst(void) const;
    const string& GetComment(void) const;

    bool NeedPeriod(void) const;
    void SetNeedPeriod(bool val);

    void AddPeriod(void);

    static const string& GetNsAreGapsStr(void);
    static string GetStringForTPA(const CUser_object& uo, CBioseqContext& ctx);
    static string GetStringForBankIt(const CUser_object& uo);
    static string GetStringForRefTrack(const CUser_object& uo);
    static string GetStringForWGS(CBioseqContext& ctx);
    static string GetStringForMolinfo(const CMolInfo& mi, CBioseqContext& ctx);
    static string GetStringForHTGS(CBioseqContext& ctx);
    static string GetStringForModelEvidance(const SModelEvidance& me);
    static string GetStringForBarcode(CBioseqContext& ctx);
    static TRefTrackStatus GetRefTrackStatus(const CUser_object& uo,
        string* st = 0);

    static void ResetFirst(void) { sm_FirstComment = true; }

protected:
    CCommentItem(CBioseqContext& ctx, bool need_period = true);

    void x_GatherInfo(CBioseqContext& ctx);
    void x_GatherDescInfo(const CSeqdesc& desc);
    void x_GatherFeatInfo(const CSeq_feat& feat, CBioseqContext& ctx);

    void x_SetComment(const string& comment);
    void x_SetCommentWithURLlinks(const string& prefix, const string& str,
        const string& suffix);
    string& x_GetComment(void) { return m_Comment; }
    void x_SetSkip(void);

private:
    static bool sm_FirstComment; 

    string  m_Comment;
    bool    m_First;
    bool    m_NeedPeriod;
};


// --- CGenomeAnnotComment

class CGenomeAnnotComment : public CCommentItem
{
public:
    CGenomeAnnotComment(CBioseqContext& ctx, const string& build_num = kEmptyStr);

private:
    void x_GatherInfo(CBioseqContext& ctx);

    // data
    string m_GenomeBuildNumber;
};


// --- CHistComment

class CHistComment : public CCommentItem
{
public:
    enum EType {
        eReplaces,
        eReplaced_by
    };

    CHistComment(EType type, const CSeq_hist& hist, CBioseqContext& ctx);

private:
    void x_GatherInfo(CBioseqContext& ctx);

    // data
    EType                   m_Type;
    CConstRef<CSeq_hist>    m_Hist;
};


// --- CGsdbComment

class CGsdbComment : public CCommentItem
{
public:
    CGsdbComment(const CDbtag& dbtag, CBioseqContext& ctx);

private:
    void x_GatherInfo(CBioseqContext& ctx);

    // data
    CConstRef<CDbtag> m_Dbtag;
};


// --- CLocalIdComment

class CLocalIdComment : public CCommentItem
{
public:
    CLocalIdComment(const CObject_id& oid, CBioseqContext& ctx);

private:
    void x_GatherInfo(CBioseqContext& ctx);

    // data
    CConstRef<CObject_id> m_Oid;
};


/////////////////////////////////////////////////////////////////////////////
//  inline methods

inline
bool CCommentItem::IsFirst(void) const
{
    return m_First;
}


inline
const string& CCommentItem::GetComment(void) const
{
    return m_Comment;
}


inline
bool CCommentItem::NeedPeriod(void) const
{
    return m_NeedPeriod;
}


inline
void CCommentItem::SetNeedPeriod(bool val)
{
    m_NeedPeriod = val;
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.11  2005/02/02 19:35:07  shomrat
* Added barcode comment
*
* Revision 1.10  2005/01/12 16:41:00  shomrat
* Removed NsAreGaps
*
* Revision 1.9  2004/10/05 15:26:13  shomrat
* Changed x_SetComment
*
* Revision 1.8  2004/08/30 13:31:43  shomrat
* made constructor protected
*
* Revision 1.7  2004/08/19 16:19:16  shomrat
* Added constructor from CSeq_feat
*
* Revision 1.6  2004/05/06 17:40:31  shomrat
* + CLocalIdComment class
*
* Revision 1.5  2004/04/22 15:34:25  shomrat
* Changes in context
*
* Revision 1.4  2004/03/26 17:21:06  shomrat
* + AddPeriod()
*
* Revision 1.3  2004/03/18 15:26:19  shomrat
* Added missing forward declerations
*
* Revision 1.2  2004/03/05 18:48:13  shomrat
* fixed RefTrack comments
*
* Revision 1.1  2003/12/17 19:45:14  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT_ITEMS___COMMENT_ITEM__HPP */
