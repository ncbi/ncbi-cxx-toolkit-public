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
#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CDbtag;
class CBioseq;
class CSeqdesc;
class CSeq_feat;
class CFFContext;
class IFormatter;
class CMolInfo;
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

    CCommentItem(CFFContext& ctx);
    CCommentItem(const string& comment, CFFContext& ctx,
        const CSerialObject* obj = 0);
    CCommentItem(const CSeqdesc&  desc, CFFContext& ctx);
    //CCommentItem(const CSeq_feat& feat, CFFContext& ctx);

    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;

    bool IsFirst(void) const { return m_First; }
    const string& GetComment(void) const { return m_Comment; }

    static const string kNsAreGaps;

    static string GetStringForTPA(const CUser_object& uo, CFFContext& ctx);
    static string GetStringForBankIt(const CUser_object& uo);
    static string GetStringForRefTrack(const CUser_object& uo);
    static bool NsAreGaps(const CBioseq& seq, CFFContext& ctx);
    static string GetStringForWGS(CFFContext& ctx);
    static string GetStringForMolinfo(const CMolInfo& mi, CFFContext& ctx);
    static string GetStringForHTGS(const CMolInfo& mi, CFFContext& ctx);
    static string GetStringForModelEvidance(const SModelEvidance& me);
    static TRefTrackStatus GetRefTrackStatus(const CUser_object& uo,
        string* st = 0);

    static void ResetFirst(void) { sm_FirstComment = true; }

protected:

    void x_GatherInfo(CFFContext& ctx);
    void x_GatherDescInfo(const CSeqdesc& desc, CFFContext& ctx);
    void x_GatherFeatInfo(const CSeq_feat& feat, CFFContext& ctx);

    void x_SetComment(const string& comment) { m_Comment = comment; }
    void x_SetCommentWithURLlinks(const string& prefix, const string& str,
        const string& suffix);
    string& x_GetComment(void) { return m_Comment; }
    void x_SetSkip(void);

private:
    static bool sm_FirstComment; 

    string  m_Comment;
    bool    m_First;
};


class CGenomeAnnotComment : public CCommentItem
{
public:
    CGenomeAnnotComment(CFFContext& ctx, const string& build_num = kEmptyStr);

private:
    void x_GatherInfo(CFFContext& ctx);

    // data
    string m_GenomeBuildNumber;
};


class CHistComment : public CCommentItem
{
public:
    enum EType {
        eReplaces,
        eReplaced_by
    };

    CHistComment(EType type, const CSeq_hist& hist, CFFContext& ctx);

private:
    void x_GatherInfo(CFFContext& ctx);

    // data
    EType                   m_Type;
    CConstRef<CSeq_hist>    m_Hist;
};


class CGsdbComment : public CCommentItem
{
public:
    CGsdbComment(const CDbtag& dbtag, CFFContext& ctx);

    void AddPeriod(void);

private:
    void x_GatherInfo(CFFContext& ctx);

    // data
    CConstRef<CDbtag> m_Dbtag;
};

END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
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
