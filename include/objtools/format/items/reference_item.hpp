#ifndef OBJTOOLS_FORMAT_ITEMS___REFERENCE_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___REFERENCE_ITEM__HPP

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
*   Reference item for flat-file generator
*
*/
#include <corelib/ncbistd.hpp>

#include <objects/general/Date_std.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/biblio/Imprint.hpp>

#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CSeq_loc;
class CSeqdesc;
class CSeq_feat;
class CAuth_list;
class CCit_art;
class CCit_book;
class CCit_gen;
class CCit_jour;
class CCit_let;
class CCit_pat;
class CCit_proc;
class CCit_sub;
class CImprint;
class CMedline_entry;
class CPub;
class CPub_equiv;
class CPub_set;
class CTitle;
class CAffil;
class CBioseqContext;
class IFormatter;


class CReferenceItem : public CFlatItem
{
public:
    enum ECategory {
        eUnknown,     // none of the below
        ePublished,   // published paper
        eUnpublished, // unpublished paper
        eSubmission   // direct submission
    };
    typedef ECategory                       TCategory;
    typedef vector<CRef<CReferenceItem> >   TReferences;

    CReferenceItem(const CPubdesc& pub, CBioseqContext& ctx,
        const CSeq_loc* loc = 0);
    CReferenceItem(const CSeqdesc&  desc, CBioseqContext& ctx);
    CReferenceItem(const CSeq_feat& feat, CBioseqContext& ctx);
        
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;

    // sort, merge duplicates and cleans up remaining items
    static void Rearrange(TReferences& refs, CBioseqContext& ctx);

    bool Matches(const CPub_set& ps) const;
       
    // typically has to go through m_Pub(desc), since we don't want to store
    // format-dependent strings
    //void GetTitles(string& title, string& journal, const CBioseqContext& ctx)
    //    const;
        
    const CPubdesc&     GetPubdesc   (void) const { return *m_Pubdesc;   }
    CPubdesc::TReftype  GetReftype   (void) const { return GetPubdesc().GetReftype(); }
    TCategory           GetCategory  (void) const { return m_Category;   }
    const CDate*        GetDate      (void) const { return m_Date;       }
    CImprint::TPrepub   GetPrepub    (void) const { return m_Prepub;     }
    int                 GetSerial    (void) const { return m_Serial;     }
    const CSeq_loc*     GetLoc       (void) const { return m_Loc;        }
    const CAuth_list*   GetAuthors   (void) const { return m_Authors;    }
    const string&       GetConsortium(void) const { return m_Consortium; }
    const string&       GetTitle     (void) const { return m_Title;      }
    const string&       GetJournal   (void) const { return m_Journal;    }
    const int           GetPMID      (void) const { return m_PMID;       }
    const int           GetMUID      (void) const { return m_MUID;       }
    const string&       GetRemark    (void) const { return m_Remark;     }
    const string&       GetVolume    (void) const { return m_Volume;     }
    const string&       GetPages     (void) const { return m_Pages;      }
    const string&       GetIssue     (void) const { return m_Issue;      }
    const CCit_book*    GetBook      (void) const { return m_Book;       }
    bool                JustUids     (void) const { return m_JustUids;   }
    const string&       GetUniqueStr (void) const { return m_UniqueStr;  }

    static string GetAuthString(const CAuth_list* alp);
    static void GetAuthNames(list<string>& authors, const CAuth_list* alp);
    static void FormatAffil(const CAffil& affil, string& result);

    void SetLoc(const CConstRef<CSeq_loc>& loc);
private:
    
    void x_GatherInfo(CBioseqContext& ctx);
    void x_Init(const CPub&           pub,  CBioseqContext& ctx);
    void x_Init(const CCit_gen&       gen,  CBioseqContext& ctx);
    void x_Init(const CCit_sub&       sub,  CBioseqContext& ctx);
    void x_Init(const CMedline_entry& mle,  CBioseqContext& ctx);
    void x_Init(const CCit_art&       art,  CBioseqContext& ctx);
    void x_Init(const CCit_jour&      jour, CBioseqContext& ctx);
    void x_Init(const CCit_book&      book, CBioseqContext& ctx, 
        bool for_art = false);
    void x_Init(const CCit_pat&       pat,  CBioseqContext& ctx);
    void x_Init(const CCit_let&       man,  CBioseqContext& ctx);

    void x_AddAuthors(const CAuth_list& auth);
    void x_SetJournal(const CTitle& title, CBioseqContext& ctx);
    void x_SetJournal(const CCit_gen& gen, CBioseqContext& ctx);
    void x_AddImprint(const CImprint& imp, CBioseqContext& ctx);
    
    // Genbank format specific
    void x_GatherRemark(CBioseqContext& ctx);

    void x_CleanData(void);
    bool x_Matches(const CPub& pub) const;

    CConstRef<CPubdesc>   m_Pubdesc;
    CConstRef<CPub>       m_Pub;         // main entry
    CConstRef<CSeq_loc>   m_Loc;         // null if from a descriptor
    int                   m_PMID;
    int                   m_MUID;
    TCategory             m_Category;    // (un)published / submitted
    int                   m_Serial;
    CConstRef<CAuth_list> m_Authors;
    string                m_Consortium;
    string                m_Title;
    string                m_Journal;     // or contact info for submissions
    string                m_Volume;      // normally numeric
    string                m_Issue;
    string                m_Pages;
    CConstRef<CDate>      m_Date;
    string                m_Remark;      // genbank specific
    bool                  m_JustUids;
    CConstRef<CCit_book>  m_Book;
    CImprint::TPrepub     m_Prepub;
    string                m_UniqueStr;
};


///////////////////////////////////////////////////////////////////////////
//
// INLINE METHODS

/*
inline string CReferenceItem::x_GetURL(int id)
{
    return "http://www.ncbi.nlm.nih.gov/entrez/utils/qmap.cgi?uid="
        + NStr::IntToString(id) + "&form=6&db=m&Dopt=r";
}
*/
END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.10  2004/08/30 13:33:18  shomrat
* + GetUniqueStr()
*
* Revision 1.9  2004/05/20 13:47:08  shomrat
* Added m_UniqueStr to be used for citation matching
*
* Revision 1.8  2004/05/06 17:43:44  shomrat
* moved non-public class to src file
*
* Revision 1.7  2004/04/27 15:08:46  shomrat
* + GetPrepub
*
* Revision 1.6  2004/04/22 15:39:18  shomrat
* Changes in context
*
* Revision 1.5  2004/04/13 16:42:53  shomrat
* + GetAuthNames()
*
* Revision 1.4  2004/03/18 15:28:01  shomrat
* + GetBook
*
* Revision 1.3  2004/03/10 16:23:49  shomrat
* + x_GatherRemark
*
* Revision 1.2  2004/02/11 16:35:50  shomrat
* using typdef TReferences
*
* Revision 1.1  2003/12/17 19:49:28  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT_ITEMS___REFERENCE_ITEM__HPP */
