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

#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


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
class CFFContext;
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
    typedef ECategory TCategory;

    CReferenceItem(const CPubdesc& pub, CFFContext& ctx,
        const CSeq_loc* loc = 0);
    CReferenceItem(const CSeqdesc&  desc, CFFContext& ctx);
    CReferenceItem(const CSeq_feat& feat, CFFContext& ctx);
        
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;

    // sort, drops duplicates and cleans up remaining items
    static void Rearrange(vector<CRef<CReferenceItem> >& refs, CFFContext& ctx);

    //bool Matches(const CPub_set& ps) const;
    //bool HasMUID(int id) const { return m_MUIDs.find(id) != m_MUIDs.end(); }
    //bool HasPMID(int id) const { return m_PMIDs.find(id) != m_PMIDs.end(); }

    // same form(!)
    //string GetMedlineURL(int muid) const { return x_GetURL(muid); }
    //string GetPubMedURL (int pmid) const { return x_GetURL(pmid); }

    //string GetRange(CFFContext& ctx) const;
    TSeqPos GetFrom(void) const;
    TSeqPos GetTo(void) const;

    // typically has to go through m_Pub(desc), since we don't want to store
    // format-dependent strings
    //void GetTitles(string& title, string& journal, const CFFContext& ctx)
    //    const;
        
    const CPubdesc&     GetPubdesc   (void) const { return *m_Pubdesc;   }
    CPubdesc::TReftype  GetReftype   (void) const { return GetPubdesc().GetReftype(); }
    TCategory           GetCategory  (void) const { return m_Category; }
    const CDate*        GetDate      (void) const { return m_Date; }

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

    bool                JustUids     (void) const { return m_JustUids;   }

    static string GetAuthString(const CAuth_list* alp);

private:
    
    void x_GatherInfo(CFFContext& ctx);

    void x_Init(const CPub&           pub,  CFFContext& ctx);
    void x_Init(const CCit_gen&       gen,  CFFContext& ctx);
    void x_Init(const CCit_sub&       sub,  CFFContext& ctx);
    void x_Init(const CMedline_entry& mle,  CFFContext& ctx);
    void x_Init(const CCit_art&       art,  CFFContext& ctx);
    void x_Init(const CCit_jour&      jour, CFFContext& ctx);
    void x_Init(const CCit_book&      book, CFFContext& ctx);
    void x_Init(const CCit_pat&       pat,  CFFContext& ctx);
    void x_Init(const CCit_let&       man,  CFFContext& ctx);

    void x_AddAuthors(const CAuth_list& auth);
    void x_SetJournal(const CTitle& title, CFFContext& ctx);
    void x_SetJournal(const CCit_gen& gen, CFFContext& ctx);
    void x_AddImprint(const CImprint& imp, CFFContext& ctx);

    void x_CleanData(void);

    // XXX - many of these should become pointers
    CConstRef<CPubdesc>  m_Pubdesc;
    CConstRef<CPub>      m_Pub; // main entry
    CConstRef<CSeq_loc>  m_Loc; // null if from a descriptor
    int                  m_PMID;
    int                  m_MUID;
    TCategory            m_Category;
    int                  m_Serial;
    //CConstRef<CDate_std> m_StdDate;
    //list<string>         m_Authors; // GB-style: Last,F.M.
    CConstRef<CAuth_list> m_Authors;
    string               m_Consortium;
    string               m_Title;
    string               m_Journal; // or contact info for submissions
    string               m_Volume; // normally numeric
    string               m_Issue;
    string               m_Pages;
    CConstRef<CDate>     m_Date;
    string               m_Remark;
    bool                 m_JustUids;

    //static string x_GetURL(int id);
};


class LessEqual
{
public:
    LessEqual(bool serial_first, bool is_refseq);
    bool operator()(const CRef<CReferenceItem>& ref1, const CRef<CReferenceItem>& ref2);
private:
    bool m_SerialFirst;
    bool m_IsRefSeq;
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
* Revision 1.1  2003/12/17 19:49:28  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT_ITEMS___REFERENCE_ITEM__HPP */
