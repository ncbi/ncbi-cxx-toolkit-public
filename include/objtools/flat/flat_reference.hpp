#ifndef OBJECTS_FLAT___FLAT_REFERENCE__HPP
#define OBJECTS_FLAT___FLAT_REFERENCE__HPP

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
*
* File Description:
*   new (early 2003) flat-file generator -- bibliographic references
*
*/

#include <objtools/flat/flat_item.hpp>

#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

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

class NCBI_FLAT_EXPORT CFlatReference : public IFlatItem
{
public:
    enum ECategory {
        eUnknown,     // none of the below
        ePublished,   // published paper
        eUnpublished, // unpublished paper
        eSubmission   // direct submission
    };

    CFlatReference(const CPubdesc& pub, const CSeq_loc* loc,
                   const CFlatContext& ctx);
    ~CFlatReference();
    // also drops duplicates and cleans up remaining items
    static void Sort(vector<CRef<CFlatReference> >& v, CFlatContext& ctx);

    void Format (IFlatFormatter& f)  const;
    bool Matches(const CPub_set& ps) const;
    bool HasMUID(int id) const { return m_MUIDs.find(id) != m_MUIDs.end(); }
    bool HasPMID(int id) const { return m_PMIDs.find(id) != m_PMIDs.end(); }

    // same form(!)
    string GetMedlineURL(int muid) const { return x_GetURL(muid); }
    string GetPubMedURL (int pmid) const { return x_GetURL(pmid); }

    string GetRange(const CFlatContext& ctx) const;

    // typically has to go through m_Pub(desc), since we don't want to store
    // format-dependent strings
    void GetTitles(string& title, string& journal, const CFlatContext& ctx)
        const;

    const CPubdesc&     GetPubdesc   (void) const { return *m_Pubdesc;   }
    const CSeq_loc*     GetLoc       (void) const { return m_Loc;        }
    const set<int>&     GetPMIDs     (void) const { return m_PMIDs;      }
    const set<int>&     GetMUIDs     (void) const { return m_MUIDs;      }
    int                 GetSerial    (void) const { return m_Serial;     }
    const list<string>& GetAuthors   (void) const { return m_Authors;    }
    const string&       GetConsortium(void) const { return m_Consortium; }
    const string&       GetRemark    (void) const { return m_Remark;     }

private: // XXX - many of these should become pointers
    CConstRef<CPubdesc>  m_Pubdesc;
    CConstRef<CPub>      m_Pub; // main entry
    CConstRef<CSeq_loc>  m_Loc; // null if from a descriptor
    set<int>             m_PMIDs;
    set<int>             m_MUIDs;
    ECategory            m_Category;
    int                  m_Serial;
    CConstRef<CDate_std> m_StdDate;
    list<string>         m_Authors; // GB-style: Last,F.M.
    string               m_Consortium;
    string               m_Title;
    string               m_Journal; // or contact info for submissions
    string               m_Volume; // normally numeric
    string               m_Issue;
    string               m_Pages;
    CConstRef<CDate>     m_Date;
    string               m_Remark;

    static string x_GetURL(int id);

    void x_Init(const CPub&           pub,  const CFlatContext& ctx);
    void x_Init(const CCit_gen&       gen,  const CFlatContext& ctx);
    void x_Init(const CCit_sub&       sub,  const CFlatContext& ctx);
    void x_Init(const CMedline_entry& mle,  const CFlatContext& ctx);
    void x_Init(const CCit_art&       art,  const CFlatContext& ctx);
    void x_Init(const CCit_jour&      jour, const CFlatContext& ctx);
    void x_Init(const CCit_book&      book, const CFlatContext& ctx);
    void x_Init(const CCit_pat&       pat,  const CFlatContext& ctx);
    void x_Init(const CCit_let&       man,  const CFlatContext& ctx);

    void x_AddAuthors(const CAuth_list& auth);
    void x_SetJournal(const CTitle& title, const CFlatContext& ctx);
    void x_AddImprint(const CImprint& imp, const CFlatContext& ctx);

private:
    /// forbidden
    CFlatReference(const CFlatReference&);
    CFlatReference& operator=(const CFlatReference&);
};


///////////////////////////////////////////////////////////////////////////
//
// INLINE METHODS


inline string CFlatReference::x_GetURL(int id)
{
    return "http://www.ncbi.nlm.nih.gov/entrez/utils/qmap.cgi?uid="
        + NStr::IntToString(id) + "&form=6&db=m&Dopt=r";
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2005/03/31 13:11:33  dicuccio
* Added export specifiers.  Implemented dtor, added hidden copy ctor/alignment
* operator to satisfy requirements of predeclaration.
*
* Revision 1.4  2003/06/02 16:01:39  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.3  2003/04/10 20:06:46  ucko
* GetLoc: return a pointer rather than a reference, as it's usually NULL.
*
* Revision 1.2  2003/03/21 18:47:47  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.1  2003/03/10 16:39:08  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_FLAT___FLAT_REFERENCE__HPP */
