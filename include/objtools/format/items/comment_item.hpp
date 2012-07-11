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
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
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

class NCBI_FORMAT_EXPORT CCommentItem : public CFlatItem
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
        eRefTrackStatus_Pipeline,
        eRefTrackStatus_Provisional,
        eRefTrackStatus_Predicted,
        eRefTrackStatus_Validated,
        eRefTrackStatus_Reviewed,
        eRefTrackStatus_Model,
        eRefTrackStatus_WGS,
        eRefTrackStatus_TSA
    };

    // typedefs
    typedef EType           TType;
    typedef ERefTrackStatus TRefTrackStatus;

    // constructors
    CCommentItem(const string& comment, CBioseqContext& ctx,
        const CSerialObject* obj = 0);
    CCommentItem(const CSeqdesc&  desc, CBioseqContext& ctx);
    CCommentItem(const CSeq_feat& feat, CBioseqContext& ctx);
    CCommentItem(const CUser_object & userObject, CBioseqContext& ctx);

    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;

    NCBI_DEPRECATED
    const string GetComment(void) const;

    const list<string>& GetCommentList(void) const;

    bool IsFirst(void) const;
    int GetCommentInternalIndent(void) const;

    bool NeedPeriod(void) const;
    void SetNeedPeriod(bool val);

    void AddPeriod(void);

    enum ECommentFormat
    {
        eFormat_Text,
        eFormat_Html
    };

    static const string& GetNsAreGapsStr(void);
    static string GetStringForTPA(const CUser_object& uo, CBioseqContext& ctx);
    static string GetStringForBankIt(const CUser_object& uo);
    static string GetStringForRefTrack(const CUser_object& uo,
        const CBioseq_Handle& seq, ECommentFormat format = eFormat_Text);
    static string GetStringForWGS(CBioseqContext& ctx);
    static string GetStringForTSA(CBioseqContext& ctx);
    static string GetStringForMolinfo(const CMolInfo& mi, CBioseqContext& ctx);
    static string GetStringForHTGS(CBioseqContext& ctx);
    static string GetStringForModelEvidance(const SModelEvidance& me,
        ECommentFormat format = eFormat_Text);
    static TRefTrackStatus GetRefTrackStatus(const CUser_object& uo,
        string* st = 0);
    static string GetStringForEncode(CBioseqContext& ctx);

    static void ResetFirst(void) { sm_FirstComment = true; }

protected:

    enum EPeriod {
        ePeriod_Add,
        ePeriod_NoAdd
    };

    CCommentItem(CBioseqContext& ctx, bool need_period = true);

    void x_GatherInfo(CBioseqContext& ctx);
    void x_GatherDescInfo(const CSeqdesc& desc);
    void x_GatherFeatInfo(const CSeq_feat& feat, CBioseqContext& ctx);
    void x_GatherUserObjInfo(const CUser_object& userObject );

    void x_SetComment(const string& comment);
    void x_SetCommentWithURLlinks(const string& prefix, const string& str,
        const string& suffix, EPeriod can_add_period = ePeriod_Add );
    list<string>& x_GetComment(void) { return m_Comment; }
    void x_SetSkip(void);

private:
    bool x_IsCommentEmpty(void) const;

    static bool sm_FirstComment; 

    list<string>  m_Comment;
    int           m_CommentInternalIndent;
    bool          m_First;
    bool          m_NeedPeriod;
};


// --- CGenomeAnnotComment

class NCBI_FORMAT_EXPORT CGenomeAnnotComment : public CCommentItem
{
public:
    CGenomeAnnotComment(CBioseqContext& ctx,
                        const string& build_num = kEmptyStr);

    static string GetGenomeBuildNumber(const CBioseq_Handle& bsh);
    static string GetGenomeBuildNumber(const CUser_object& uo);

private:
    void x_GatherInfo(CBioseqContext& ctx);

    // data
    string m_GenomeBuildNumber;
};


// --- CHistComment

class NCBI_FORMAT_EXPORT CHistComment : public CCommentItem
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

class NCBI_FORMAT_EXPORT CGsdbComment : public CCommentItem
{
public:
    CGsdbComment(const CDbtag& dbtag, CBioseqContext& ctx);

private:
    void x_GatherInfo(CBioseqContext& ctx);

    // data
    CConstRef<CDbtag> m_Dbtag;
};


// --- CLocalIdComment

class NCBI_FORMAT_EXPORT CLocalIdComment : public CCommentItem
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


NCBI_DEPRECATED
inline
const string CCommentItem::GetComment(void) const
{
    return NStr::Join( m_Comment, "\n" );
}

inline
const list<string>& CCommentItem::GetCommentList(void) const
{
    return m_Comment;
}

inline
int CCommentItem::GetCommentInternalIndent(void) const
{
    return m_CommentInternalIndent;
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

#endif  /* OBJTOOLS_FORMAT_ITEMS___COMMENT_ITEM__HPP */
