#ifndef OBJECTS_OBJMGR___SEQ_ID_TREE__HPP
#define OBJECTS_OBJMGR___SEQ_ID_TREE__HPP

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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Seq-id mapper for Object Manager
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbi_limits.hpp>

#include <objects/general/Date.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>

#include <objects/biblio/Id_pat.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqloc/Giimport_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objects/seq/seq_id_handle.hpp>

#include <vector>
#include <set>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CSeq_id;
class CSeq_id_Handle;
class CSeq_id_Info;
class CSeq_id_Mapper;
class CSeq_id_Which_Tree;

////////////////////////////////////////////////////////////////////
//
//  CSeq_id_***_Tree::
//
//    Seq-id sub-type specific trees
//


// Base class for seq-id type-specific trees
class CSeq_id_Which_Tree : public CObject
{
public:
    // 'ctors
    CSeq_id_Which_Tree(CSeq_id_Mapper* mapper);
    virtual ~CSeq_id_Which_Tree(void);

    static void Initialize(CSeq_id_Mapper* mapper,
                           vector<CRef<CSeq_id_Which_Tree> >& v);

    virtual bool Empty(void) const = 0;

    // Find exaclty the same seq-id
    virtual CSeq_id_Handle FindInfo(const CSeq_id& id) const = 0;
    virtual CSeq_id_Handle FindOrCreate(const CSeq_id& id) = 0;
    virtual CSeq_id_Handle GetGiHandle(int gi);

    virtual void DropInfo(const CSeq_id_Info* info);

    typedef set<CSeq_id_Handle> TSeq_id_MatchList;

    // Get the list of matching seq-id.
    virtual bool HaveMatch(const CSeq_id_Handle& id) const;
    virtual void FindMatch(const CSeq_id_Handle& id,
                           TSeq_id_MatchList& id_list) const;
    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const = 0;

    // returns true if FindMatch(h1, id_list) will put h2 in id_list.
    virtual bool Match(const CSeq_id_Handle& h1,
                       const CSeq_id_Handle& h2) const;

    virtual bool IsBetterVersion(const CSeq_id_Handle& h1,
                                 const CSeq_id_Handle& h2) const;

    // Reverse matching
    virtual bool HaveReverseMatch(const CSeq_id_Handle& id) const;
    virtual void FindReverseMatch(const CSeq_id_Handle& id,
                                  TSeq_id_MatchList& id_list);
protected:
    friend class CSeq_id_Mapper;

    CSeq_id_Info* CreateInfo(CSeq_id::E_Choice type);
    CSeq_id_Info* CreateInfo(const CSeq_id& id);

    virtual void x_Unindex(const CSeq_id_Info* info) = 0;

/*
    typedef CRWLockPosix TRWLock;
    typedef TRWLock::TReadLockGuard TReadLockGuard;
    typedef TRWLock::TWriteLockGuard TWriteLockGuard;
*/
    typedef CMutex TRWLock;
    typedef CMutexGuard TReadLockGuard;
    typedef CMutexGuard TWriteLockGuard;

    mutable TRWLock m_TreeLock;
    CSeq_id_Mapper* m_Mapper;

private:
    CSeq_id_Which_Tree(const CSeq_id_Which_Tree& tree);
    const CSeq_id_Which_Tree& operator=(const CSeq_id_Which_Tree& tree);
};



////////////////////////////////////////////////////////////////////
// not-set tree (maximum 1 entry allowed)


class CSeq_id_not_set_Tree : public CSeq_id_Which_Tree
{
public:
    CSeq_id_not_set_Tree(CSeq_id_Mapper* mapper);
    ~CSeq_id_not_set_Tree(void);

    virtual bool Empty(void) const;

    virtual CSeq_id_Handle FindInfo(const CSeq_id& id) const;
    virtual CSeq_id_Handle FindOrCreate(const CSeq_id& id);

    virtual void DropInfo(const CSeq_id_Info* info);

    virtual void FindMatch(const CSeq_id_Handle& id,
                           TSeq_id_MatchList& id_list) const;
    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const;
    virtual void FindReverseMatch(const CSeq_id_Handle& id,
                                  TSeq_id_MatchList& id_list);

protected:
    virtual void x_Unindex(const CSeq_id_Info* info);
    bool x_Check(const CSeq_id& id) const;
};


////////////////////////////////////////////////////////////////////
// Base class for Gi, Gibbsq & Gibbmt trees


class CSeq_id_int_Tree : public CSeq_id_Which_Tree
{
public:
    CSeq_id_int_Tree(CSeq_id_Mapper* mapper);
    ~CSeq_id_int_Tree(void);

    virtual bool Empty(void) const;

    virtual CSeq_id_Handle FindInfo(const CSeq_id& id) const;
    virtual CSeq_id_Handle FindOrCreate(const CSeq_id& id);

    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const;

protected:
    virtual void x_Unindex(const CSeq_id_Info* info);
    virtual bool x_Check(const CSeq_id& id) const = 0;
    virtual int  x_Get(const CSeq_id& id) const = 0;

private:
    typedef map<int, CSeq_id_Info*> TIntMap;
    TIntMap m_IntMap;
};


////////////////////////////////////////////////////////////////////
// Gibbsq tree


class CSeq_id_Gibbsq_Tree : public CSeq_id_int_Tree
{
public:
    CSeq_id_Gibbsq_Tree(CSeq_id_Mapper* mapper);
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual int  x_Get(const CSeq_id& id) const;
};


////////////////////////////////////////////////////////////////////
// Gibbmt tree


class CSeq_id_Gibbmt_Tree : public CSeq_id_int_Tree
{
public:
    CSeq_id_Gibbmt_Tree(CSeq_id_Mapper* mapper);
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual int  x_Get(const CSeq_id& id) const;
};


////////////////////////////////////////////////////////////////////
// Gi tree


class CSeq_id_Gi_Tree : public CSeq_id_Which_Tree
{
public:
    CSeq_id_Gi_Tree(CSeq_id_Mapper* mapper);
    ~CSeq_id_Gi_Tree(void);

    virtual bool Empty(void) const;

    virtual CSeq_id_Handle FindInfo(const CSeq_id& id) const;
    virtual CSeq_id_Handle FindOrCreate(const CSeq_id& id);
    virtual CSeq_id_Handle GetGiHandle(int gi);

    virtual void DropInfo(const CSeq_id_Info* info);

    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const;

protected:
    virtual void x_Unindex(const CSeq_id_Info* info);
    bool x_Check(const CSeq_id& id) const;
    int  x_Get(const CSeq_id& id) const;

    CConstRef<CSeq_id_Info> m_ZeroInfo;
    CConstRef<CSeq_id_Info> m_SharedInfo;
};


////////////////////////////////////////////////////////////////////
// Base class for e_Genbank, e_Embl, e_Pir, e_Swissprot, e_Other,
// e_Ddbj, e_Prf, e_Tpg, e_Tpe, e_Tpd trees


class CSeq_id_Textseq_Tree : public CSeq_id_Which_Tree
{
public:
    CSeq_id_Textseq_Tree(CSeq_id_Mapper* mapper);
    ~CSeq_id_Textseq_Tree(void);

    virtual bool Empty(void) const;

    virtual CSeq_id_Handle FindInfo(const CSeq_id& id) const;
    virtual CSeq_id_Handle FindOrCreate(const CSeq_id& id);

    virtual bool HaveMatch(const CSeq_id_Handle& id) const;
    virtual void FindMatch(const CSeq_id_Handle& id,
                           TSeq_id_MatchList& id_list) const;
    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const;

    virtual bool Match(const CSeq_id_Handle& h1,
                       const CSeq_id_Handle& h2) const;
    virtual bool IsBetterVersion(const CSeq_id_Handle& h1,
                                 const CSeq_id_Handle& h2) const;

    virtual bool HaveReverseMatch(const CSeq_id_Handle& id) const;
    virtual void FindReverseMatch(const CSeq_id_Handle& id,
                                  TSeq_id_MatchList& id_list);
protected:
    virtual void x_Unindex(const CSeq_id_Info* info);
    virtual bool x_Check(const CSeq_id& id) const = 0;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const = 0;
    CSeq_id_Info* x_FindInfo(CSeq_id::E_Choice type,
                             const CTextseq_id& tid) const;

private:
    typedef vector<CSeq_id_Info*>   TVersions;
    typedef map<string, TVersions, PNocase> TStringMap;

    static bool x_Equals(const CTextseq_id& id1, const CTextseq_id& id2);
    CSeq_id_Info* x_FindVersionEqual(const TVersions& ver_list,
                                     CSeq_id::E_Choice type,
                                     const CTextseq_id& tid) const;

    void x_FindVersionMatch(const TVersions& ver_list,
                            const CTextseq_id& tid,
                            TSeq_id_MatchList& id_list,
                            bool by_accession) const;

    TStringMap m_ByAccession;
    TStringMap m_ByName; // Used for searching by string
};


////////////////////////////////////////////////////////////////////
// Genbank, EMBL and DDBJ joint tree


class CSeq_id_GB_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_GB_Tree(CSeq_id_Mapper* mapper);
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const;
};


////////////////////////////////////////////////////////////////////
// Pir tree


class CSeq_id_Pir_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_Pir_Tree(CSeq_id_Mapper* mapper);
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const;
};


////////////////////////////////////////////////////////////////////
// Swissprot


class CSeq_id_Swissprot_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_Swissprot_Tree(CSeq_id_Mapper* mapper);
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const;
};


////////////////////////////////////////////////////////////////////
// Prf tree


class CSeq_id_Prf_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_Prf_Tree(CSeq_id_Mapper* mapper);
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const;
};


////////////////////////////////////////////////////////////////////
// Tpg tree


class CSeq_id_Tpg_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_Tpg_Tree(CSeq_id_Mapper* mapper);
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const;
};


////////////////////////////////////////////////////////////////////
// Tpe tree


class CSeq_id_Tpe_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_Tpe_Tree(CSeq_id_Mapper* mapper);
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const;
};


////////////////////////////////////////////////////////////////////
// Tpd tree


class CSeq_id_Tpd_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_Tpd_Tree(CSeq_id_Mapper* mapper);
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const;
};


////////////////////////////////////////////////////////////////////
// Other tree


class CSeq_id_Other_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_Other_Tree(CSeq_id_Mapper* mapper);
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const;
};


////////////////////////////////////////////////////////////////////
// e_Local tree


class CSeq_id_Local_Tree : public CSeq_id_Which_Tree
{
public:
    CSeq_id_Local_Tree(CSeq_id_Mapper* mapper);
    ~CSeq_id_Local_Tree(void);

    virtual bool Empty(void) const;

    virtual CSeq_id_Handle FindInfo(const CSeq_id& id) const;
    virtual CSeq_id_Handle FindOrCreate(const CSeq_id& id);

    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const;

private:
    virtual void x_Unindex(const CSeq_id_Info* info);
    CSeq_id_Info* x_FindInfo(const CObject_id& oid) const;

    typedef map<string, CSeq_id_Info*, PNocase> TByStr;
    typedef map<int, CSeq_id_Info*>    TById;

    TByStr m_ByStr;
    TById  m_ById;
};


////////////////////////////////////////////////////////////////////
// e_General tree


class CSeq_id_General_Tree : public CSeq_id_Which_Tree
{
public:
    CSeq_id_General_Tree(CSeq_id_Mapper* mapper);
    ~CSeq_id_General_Tree(void);

    virtual bool Empty(void) const;

    virtual CSeq_id_Handle FindInfo(const CSeq_id& id) const;
    virtual CSeq_id_Handle FindOrCreate(const CSeq_id& id);

    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const;

private:
    virtual void x_Unindex(const CSeq_id_Info* info);
    CSeq_id_Info* x_FindInfo(const CDbtag& dbid) const;

    struct STagMap {
    public:
        typedef map<string, CSeq_id_Info*, PNocase> TByStr;
        typedef map<int, CSeq_id_Info*>    TById;

        TByStr m_ByStr;
        TById  m_ById;
    };
    typedef map<string, STagMap, PNocase> TDbMap;

    TDbMap m_DbMap;
};


////////////////////////////////////////////////////////////////////
// e_Giim tree


class CSeq_id_Giim_Tree : public CSeq_id_Which_Tree
{
public:
    CSeq_id_Giim_Tree(CSeq_id_Mapper* mapper);
    ~CSeq_id_Giim_Tree(void);

    virtual bool Empty(void) const;

    virtual CSeq_id_Handle FindInfo(const CSeq_id& id) const;
    virtual CSeq_id_Handle FindOrCreate(const CSeq_id& id);

    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const;

private:
    virtual void x_Unindex(const CSeq_id_Info* info);
    CSeq_id_Info* x_FindInfo(const CGiimport_id& gid) const;

    // 2-level indexing: first by Id, second by Db+Release
    typedef vector<CSeq_id_Info*> TGiimList;
    typedef map<int, TGiimList>  TIdMap;

    TIdMap m_IdMap;
};


////////////////////////////////////////////////////////////////////
// e_Patent tree


class CSeq_id_Patent_Tree : public CSeq_id_Which_Tree
{
public:
    CSeq_id_Patent_Tree(CSeq_id_Mapper* mapper);
    ~CSeq_id_Patent_Tree(void);

    virtual bool Empty(void) const;

    virtual CSeq_id_Handle FindInfo(const CSeq_id& id) const;
    virtual CSeq_id_Handle FindOrCreate(const CSeq_id& id);

    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const;

private:
    virtual void x_Unindex(const CSeq_id_Info* info);
    CSeq_id_Info* x_FindInfo(const CPatent_seq_id& pid) const;

    // 3-level indexing: country, (number|app_number), seqid.
    // Ignoring patent doc-type in indexing.
    struct SPat_idMap {
        typedef map<int, CSeq_id_Info*> TBySeqid;
        typedef map<string, TBySeqid, PNocase> TByNumber; // or by App_number

        TByNumber m_ByNumber;
        TByNumber m_ByApp_number;
    };
    typedef map<string, SPat_idMap, PNocase> TByCountry;

    TByCountry m_CountryMap;
};


////////////////////////////////////////////////////////////////////
// e_PDB tree


class CSeq_id_PDB_Tree : public CSeq_id_Which_Tree
{
public:
    CSeq_id_PDB_Tree(CSeq_id_Mapper* mapper);
    ~CSeq_id_PDB_Tree(void);

    virtual bool Empty(void) const;

    virtual CSeq_id_Handle FindInfo(const CSeq_id& id) const;
    virtual CSeq_id_Handle FindOrCreate(const CSeq_id& id);

    virtual bool HaveMatch(const CSeq_id_Handle& id) const;
    virtual void FindMatch(const CSeq_id_Handle& id,
                           TSeq_id_MatchList& id_list) const;
    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const;

private:
    virtual void x_Unindex(const CSeq_id_Info* info);
    CSeq_id_Info* x_FindInfo(const CPDB_seq_id& pid) const;

    string x_IdToStrKey(const CPDB_seq_id& id) const;

    // Index by mol+chain, no date - too complicated
    typedef vector<CSeq_id_Info*>  TSubMolList;
    typedef map<string, TSubMolList, PNocase> TMolMap;

    TMolMap m_MolMap;
};


// Seq-id mapper exception
class NCBI_SEQ_EXPORT CIdMapperException : public CException
{
public:
    enum EErrCode {
        eTypeError,
        eSymbolError,
        eEmptyError,
        eOtherError
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CIdMapperException,CException);
};


/////////////////////////////////////////////////////////////////////////////
//
// Inline methods
//
/////////////////////////////////////////////////////////////////////////////

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.12  2005/03/29 16:01:24  grichenk
* Removed CSeq_id_not_set_Tree::HaveReverseMatch()
*
* Revision 1.11  2004/10/01 20:28:29  vasilche
* Accession and name are case insensitive.
*
* Revision 1.10  2004/09/30 18:43:20  vasilche
* Added CSeq_id_Handle::GetMapper() and MatchesTo().
*
* Revision 1.9  2004/07/12 18:17:31  grichenk
* Fixed export macro
*
* Revision 1.8  2004/07/12 17:23:48  grichenk
* Fixed export name
*
* Revision 1.7  2004/07/12 15:05:32  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.6  2004/06/17 18:28:38  vasilche
* Fixed null pointer exception in GI CSeq_id_Handle.
*
* Revision 1.5  2004/02/19 17:25:34  vasilche
* Use CRef<> to safely hold pointer to CSeq_id_Info.
* CSeq_id_Info holds pointer to owner CSeq_id_Which_Tree.
* Reduce number of calls to CSeq_id_Handle.GetSeqId().
*
* Revision 1.4  2004/02/10 21:15:15  grichenk
* Added reverse ID matching.
*
* Revision 1.3  2004/01/08 02:49:11  ucko
* Make m_TreeLock a recursive mutex, as CSeq_id_Textseq_Tree can no
* longer safely use a fast mutex.
*
* Revision 1.2  2003/09/30 16:22:01  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.1  2003/06/10 19:06:35  vasilche
* Simplified CSeq_id_Mapper and CSeq_id_Handle.
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_OBJMGR___SEQ_ID_TREE__HPP */
