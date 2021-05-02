#ifndef CLEANUP___CLEANUP_PUB__HPP
#define CLEANUP___CLEANUP_PUB__HPP

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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   Basic Cleanup for publications.
 *   .......
 *
 */
#include <objmgr/scope.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/pub/Pub.hpp>

#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Cit_pat.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/medline/Medline_entry.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_submit;
class CSubmit_block;
class CName_std;



class CPubCleaner : public CObject
{
public:

    virtual ~CPubCleaner() {};
    virtual bool Clean(bool fix_initials, bool strip_serial) = 0;
    virtual bool IsEmpty() = 0;

protected:
    enum EImprintBC {
        eImprintBC_AllowStatusChange = 2,
        eImprintBC_ForbidStatusChange
    };

    static bool CleanImprint(CImprint& imprint, EImprintBC is_status_change_allowed);
};


class CPubEquivCleaner : public CPubCleaner
{
public:
    CPubEquivCleaner(CPub_equiv& equiv) : m_Equiv(equiv) {};
    virtual ~CPubEquivCleaner() {};

    virtual bool Clean(bool fix_initials, bool strip_serial);
    virtual bool IsEmpty();

    static bool ShouldWeFixInitials(const CPub_equiv& equiv);

protected:
    CPub_equiv& m_Equiv;

    static bool s_Flatten(CPub_equiv& equiv);
};


class CCitGenCleaner : public CPubCleaner
{
public:
    CCitGenCleaner(CCit_gen& gen) : m_Gen(gen) { }
    virtual ~CCitGenCleaner() {};

    virtual bool Clean(bool fix_initials, bool strip_serial);
    virtual bool IsEmpty();

protected:
    CCit_gen& m_Gen;
};


class CCitSubCleaner : public CPubCleaner
{
public:
    CCitSubCleaner(CCit_sub& sub) : m_Sub(sub) { }
    virtual ~CCitSubCleaner() {};

    virtual bool Clean(bool fix_initials, bool strip_serial);
    virtual bool IsEmpty();

protected:
    CCit_sub& m_Sub;
};


class CCitArtCleaner : public CPubCleaner
{
public:
    CCitArtCleaner(CCit_art& art) : m_Art(art) { }
    virtual ~CCitArtCleaner() {};

    virtual bool Clean(bool fix_initials, bool strip_serial);
    virtual bool IsEmpty() { return false; }

protected:
    CCit_art& m_Art;
};


class CCitBookCleaner : public CPubCleaner
{
public:
    CCitBookCleaner(CCit_book& book) : m_Book(book) { }
    virtual ~CCitBookCleaner() {};

    virtual bool Clean(bool fix_initials, bool strip_serial);
    virtual bool IsEmpty() { return false; }

protected:
    CCit_book& m_Book;
};


class CCitJourCleaner : public CPubCleaner
{
public:
    CCitJourCleaner(CCit_jour& jour) : m_Jour(jour) { }
    virtual ~CCitJourCleaner() {};

    virtual bool Clean(bool fix_initials, bool strip_serial);
    virtual bool IsEmpty() { return false; }

protected:
    CCit_jour& m_Jour;
};


class CCitProcCleaner : public CPubCleaner
{
public:
    CCitProcCleaner(CCit_proc& proc) : m_Proc(proc) { }
    virtual ~CCitProcCleaner() {};

    virtual bool Clean(bool fix_initials, bool strip_serial);
    virtual bool IsEmpty() { return false; }

protected:
    CCit_proc& m_Proc;
};


class CCitPatCleaner : public CPubCleaner
{
public:
    CCitPatCleaner(CCit_pat& pat) : m_Pat(pat) { }
    virtual ~CCitPatCleaner() {};

    virtual bool Clean(bool fix_initials, bool strip_serial);
    virtual bool IsEmpty() { return false; }

protected:
    CCit_pat& m_Pat;
};


class CCitLetCleaner : public CPubCleaner
{
public:
    CCitLetCleaner(CCit_let& let) : m_Let(let) { }
    virtual ~CCitLetCleaner() {};

    virtual bool Clean(bool fix_initials, bool strip_serial);
    virtual bool IsEmpty() { return false; }

protected:
    CCit_let& m_Let;
};


class CMedlineEntryCleaner : public CPubCleaner
{
public:
    CMedlineEntryCleaner(CMedline_entry& men) : m_Men(men) { }
    virtual ~CMedlineEntryCleaner() {};

    virtual bool Clean(bool fix_initials, bool strip_serial);
    virtual bool IsEmpty() { return false; }

protected:
    CMedline_entry& m_Men;
};


CRef<CPubCleaner> PubCleanerFactory(CPub& pub);


class NCBI_CLEANUP_EXPORT CCleanupPub : public CObject
{
public:
    static bool CleanPubdesc(CPubdesc& pubdesc, bool strip_serial);


private:
    static bool x_CleanPubdescComment(string& str);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* CLEANUP___CLEANUP__HPP */
