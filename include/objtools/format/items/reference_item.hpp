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
#include <objects/biblio/Cit_book.hpp>
#include <objects/seq/Pubdesc.hpp>

#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSubmit_block;
class CSeq_loc;
class CSeq_feat;
class CAuth_list;
class CCit_art;
class CCit_gen;
class CCit_jour;
class CCit_let;
class CCit_pat;
class CCit_proc;
class CCit_sub;
class CMedline_entry;
class CPub;
class CPub_set;
class CDate;
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

    enum EPubType
    {
        ePub_not_set,
        ePub_sub,
        ePub_gen,
        ePub_jour,
        ePub_book,
        ePub_book_art,
        ePub_thesis,
        ePub_pat
    };

    typedef ECategory                       TCategory;
    typedef EPubType                        TPubType;
    typedef vector< CRef<CReferenceItem> >  TReferences;
    typedef list<string>                    TStrList;
    typedef int                             TSeqid;
    typedef CPubdesc::TReftype              TReftype;

    /// constructors
    CReferenceItem(const CSeqdesc&  desc, CBioseqContext& ctx);
    CReferenceItem(const CSeq_feat& feat, CBioseqContext& ctx,
        const CSeq_loc* loc = NULL);
    CReferenceItem(const CSubmit_block&  sub, CBioseqContext& ctx);
    
    /// format
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;
    
    TPubType          GetPubType   (void) const { return m_PubType;    }
    TCategory         GetCategory  (void) const { return m_Category;   }
    const CPubdesc&   GetPubdesc   (void) const { return *m_Pubdesc;   }
    TReftype          GetReftype   (void) const;
    const CSeq_loc&   GetLoc       (void) const { return *m_Loc;       }
    int               GetSerial    (void) const { return m_Serial;     }

    // Date
    bool              IsSetDate    (void) const { return m_Date.NotEmpty();    }
    const CDate&      GetDate      (void) const { return *m_Date;              }
    //Authors
    bool              IsSetAuthors (void) const { return m_Authors.NotEmpty(); }
    const CAuth_list& GetAuthors   (void) const { return *m_Authors;           }

    bool              IsSetBook    (void) const { return m_Book.NotEmpty();    }
    const CCit_book&  GetBook      (void) const { return *m_Book;              }

    bool              IsSetPatent  (void) const { return m_Patent.NotEmpty();  }
    const CCit_pat&   GetPatent    (void) const { return *m_Patent;            }
    TSeqid            GetPatSeqid  (void) const { return m_PatentId;           }

    bool              IsSetGen     (void) const { return m_Gen.NotEmpty();     }
    const CCit_gen&   GetGen       (void) const { return *m_Gen;               }

    bool              IsSetSub     (void) const { return m_Sub.NotEmpty();     }
    const CCit_sub&   GetSub       (void) const { return *m_Sub;               }

    bool              IsSetJournal (void) const { return m_Journal.NotEmpty(); }
    const CCit_jour&  GetJournal   (void) const { return *m_Journal;           }

    int               GetPMID      (void) const { return m_PMID;               }
    int               GetMUID      (void) const { return m_MUID;               }

    const string&     GetTitle     (void) const { return m_Title;              }
    
    const string&     GetUniqueStr (void) const;

    bool              IsJustUids   (void) const { return m_JustUids;   }
    bool              IsElectronic (void) const { return m_Elect;      }
    const string&     GetConsortium(void) const { return m_Consortium; }
    const string&     GetRemark    (void) const { return m_Remark;     }

    void SetLoc(const CConstRef<CSeq_loc>& loc);

    // test if matches a publication in the set
    bool Matches(const CPub_set& ps) const;

    // sort, merge duplicates and cleans up remaining items
    static void Rearrange(TReferences& refs, CBioseqContext& ctx);
    // get names from authors list
    static void GetAuthNames(const CAuth_list& alp, TStrList& authors);
    // format author list
    static void FormatAuthors(const CAuth_list& alp, string& auth);
    // format affiliation
    static void FormatAffil(const CAffil& affil, string& result, bool gen_sub = false);

private:
    
    void x_GatherInfo(CBioseqContext& ctx);
    void x_Init(const CPub&           pub,  CBioseqContext& ctx);
    void x_Init(const CCit_gen&       gen,  CBioseqContext& ctx);
    void x_Init(const CCit_sub&       sub,  CBioseqContext& ctx);
    void x_Init(const CMedline_entry& mle,  CBioseqContext& ctx);
    void x_Init(const CCit_art&       art,  CBioseqContext& ctx);
    void x_Init(const CCit_jour&      jour, CBioseqContext& ctx);
    void x_Init(const CCit_book&      book, CBioseqContext& ctx);
    void x_Init(const CCit_pat&       pat,  CBioseqContext& ctx);
    void x_Init(const CCit_let&       man,  CBioseqContext& ctx);
    void x_Init(const CCit_proc&      proc, CBioseqContext& ctx);
    void x_InitProc(const CCit_book&      book, CBioseqContext& ctx);

    void x_AddAuthors(const CAuth_list& auth);
    void x_AddImprint(const CImprint& imp, CBioseqContext& ctx);
    
    // Genbank format specific
    void x_GatherRemark(CBioseqContext& ctx);

    void x_CreateUniqueStr(void) const;
    void x_CleanData(void);
    bool x_Matches(const CPub& pub) const;

    // data
    TPubType              m_PubType;
    TCategory             m_Category;
    CConstRef<CPubdesc>   m_Pubdesc;
    CConstRef<CAuth_list> m_Authors;
    CConstRef<CCit_book>  m_Book;
    CConstRef<CCit_pat>   m_Patent;
    TSeqid                m_PatentId;
    CConstRef<CCit_gen>   m_Gen;
    CConstRef<CCit_sub>   m_Sub;
    CConstRef<CCit_jour>  m_Journal;
    CConstRef<CSeq_loc>   m_Loc;
    CConstRef<CDate>      m_Date;
    int                   m_PMID;
    int                   m_MUID;
    int                   m_Serial;
    mutable string        m_UniqueStr;
    bool                  m_JustUids;
    string                m_Title;
    bool                  m_Elect;
    string                m_Consortium;
    string                m_Remark;      // genbank specific
};


///////////////////////////////////////////////////////////////////////////
//
// INLINE METHODS

inline
const string& CReferenceItem::GetUniqueStr(void) const
{
    // supress creation if other identifiers exist.
    if (m_MUID == 0  &&  m_PMID == 0) {
        x_CreateUniqueStr();
    }
    return m_UniqueStr;
}


inline
CReferenceItem::TReftype CReferenceItem::GetReftype(void) const
{
    return (m_Pubdesc.NotEmpty()) ? m_Pubdesc->GetReftype() : CPubdesc::eReftype_seq;
}

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
* Revision 1.17  2005/04/27 17:08:15  shomrat
* Suppress unique string creation if other identifiers exist
*
* Revision 1.16  2005/02/07 14:57:33  shomrat
* Added support for submissions
*
* Revision 1.15  2005/01/12 18:43:30  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.14  2005/01/12 16:42:07  shomrat
* Code refactoring
*
* Revision 1.13  2004/11/15 20:04:10  shomrat
* Added Pubstatus
*
* Revision 1.12  2004/10/18 18:45:25  shomrat
* Indicate if electronic publication
*
* Revision 1.11  2004/10/05 15:30:41  shomrat
* + UseData()
*
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
