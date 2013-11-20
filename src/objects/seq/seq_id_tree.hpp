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
#include <corelib/hash_map.hpp>

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
    virtual CSeq_id_Handle GetGiHandle(TGi gi);

    virtual void DropInfo(const CSeq_id_Info* info);

    typedef set<CSeq_id_Handle> TSeq_id_MatchList;

    // Get the list of matching seq-id.
    virtual bool HaveMatch(const CSeq_id_Handle& id) const;
    virtual void FindMatch(const CSeq_id_Handle& id,
                           TSeq_id_MatchList& id_list) const;
    virtual void FindMatchStr(const string& sid,
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

    virtual size_t Dump(CNcbiOstream& out,
                        CSeq_id::E_Choice type,
                        int details) const = 0;

protected:
    friend class CSeq_id_Mapper;

    typedef CSeq_id_Info::TPacked TPacked;

    CSeq_id_Info* CreateInfo(CSeq_id::E_Choice type);
    CSeq_id_Info* CreateInfo(const CSeq_id& id);

    static const CSeq_id_Info* GetInfo(const CSeq_id_Handle& id)
        {
            return id.m_Info;
        }
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
    virtual void FindMatchStr(const string& sid,
                              TSeq_id_MatchList& id_list) const;
    virtual void FindReverseMatch(const CSeq_id_Handle& id,
                                  TSeq_id_MatchList& id_list);

    virtual size_t Dump(CNcbiOstream& out,
                        CSeq_id::E_Choice type,
                        int details) const;

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

    virtual void FindMatchStr(const string& sid,
                              TSeq_id_MatchList& id_list) const;

    virtual size_t Dump(CNcbiOstream& out,
                        CSeq_id::E_Choice type,
                        int details) const;

protected:
    virtual void x_Unindex(const CSeq_id_Info* info);
    virtual bool x_Check(const CSeq_id& id) const = 0;
    virtual TPacked x_Get(const CSeq_id& id) const = 0;

private:
    typedef map<TPacked, CSeq_id_Info*> TIntMap;
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
    virtual TPacked x_Get(const CSeq_id& id) const;
};


////////////////////////////////////////////////////////////////////
// Gibbmt tree


class CSeq_id_Gibbmt_Tree : public CSeq_id_int_Tree
{
public:
    CSeq_id_Gibbmt_Tree(CSeq_id_Mapper* mapper);
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual TPacked x_Get(const CSeq_id& id) const;
};


////////////////////////////////////////////////////////////////////
// Gi tree


class CSeq_id_Gi_Info : public CSeq_id_Info
{
public:
    CSeq_id_Gi_Info(CSeq_id_Mapper* mapper)
        : CSeq_id_Info(CSeq_id::e_Gi, mapper)
        {
        }
    
    virtual CConstRef<CSeq_id> GetPackedSeqId(TPacked packed) const;
};


class CSeq_id_Gi_Tree : public CSeq_id_Which_Tree
{
public:
    CSeq_id_Gi_Tree(CSeq_id_Mapper* mapper);
    ~CSeq_id_Gi_Tree(void);

    virtual bool Empty(void) const;

    virtual CSeq_id_Handle FindInfo(const CSeq_id& id) const;
    virtual CSeq_id_Handle FindOrCreate(const CSeq_id& id);
    virtual CSeq_id_Handle GetGiHandle(TGi gi);

    virtual void DropInfo(const CSeq_id_Info* info);

    virtual void FindMatchStr(const string& sid,
                              TSeq_id_MatchList& id_list) const;

    virtual size_t Dump(CNcbiOstream& out,
                        CSeq_id::E_Choice type,
                        int details) const;

protected:
    virtual void x_Unindex(const CSeq_id_Info* info);
    bool x_Check(const CSeq_id& id) const;
    TGi x_Get(const CSeq_id& id) const;

    CConstRef<CSeq_id_Info> m_ZeroInfo;
    CConstRef<CSeq_id_Info> m_SharedInfo;
};


////////////////////////////////////////////////////////////////////
// Base class for e_Genbank, e_Embl, e_Pir, e_Swissprot, e_Other,
// e_Ddbj, e_Prf, e_Tpg, e_Tpe, e_Tpd trees


class CSeq_id_Textseq_Info : public CSeq_id_Info {
public:
    typedef CTextseq_id::TVersion TVersion;

    struct TKey {
        TKey(void)
            : m_Hash(0), m_Version(0)
            {
            }

        unsigned m_Hash;
        TVersion m_Version;
        string m_Prefix;

        DECLARE_OPERATOR_BOOL(m_Hash != 0);

        bool operator==(const TKey& b) const {
            return m_Hash == b.m_Hash && m_Version == b.m_Version &&
                NStr::EqualNocase(m_Prefix, b.m_Prefix);
        }
        bool operator!=(const TKey& b) const {
            return !(*this == b);
        }
        bool operator<(const TKey& b) const {
            return m_Hash < b.m_Hash ||
                (m_Hash == b.m_Hash &&
                 (m_Version < b.m_Version ||
                  (m_Version == b.m_Version &&
                   NStr::CompareNocase(m_Prefix, b.m_Prefix) < 0)));
        }

        bool SameHash(const TKey& b) const {
            return m_Hash == b.m_Hash;
        }
        bool SameHashNoVer(const TKey& b) const {
            return ((m_Hash ^ b.m_Hash) & ~1) == 0;
        }
        bool EqualAcc(const TKey& b) const {
            return SameHashNoVer(b) &&
                NStr::EqualNocase(m_Prefix, b.m_Prefix);
        }

        bool IsSetVersion(void) const {
            return (m_Hash & 1) != 0;
        }
        TVersion GetVersion(void) const {
            _ASSERT(IsSetVersion());
            return m_Version;
        }
        void ResetVersion(void) {
            m_Hash &= ~1;
            m_Version = 0;
        }
        void SetVersion(TVersion version) {
            m_Hash |= 1;
            m_Version = version;
        }
    };
    CSeq_id_Textseq_Info(CSeq_id::E_Choice type,
                         CSeq_id_Mapper* mapper,
                         const TKey& key);
    ~CSeq_id_Textseq_Info(void);
    
    const TKey& GetKey(void) const {
        return m_Key;
    }
    const string& GetAccPrefix(void) const {
        return m_Key.m_Prefix;
    }
    bool GoodPrefix(const string& acc) const {
        return NStr::StartsWith(acc, GetAccPrefix(), NStr::eNocase);
    }
    int GetAccDigits(void) const {
        return (m_Key.m_Hash & 0xff) >> 1;
    }
    bool IsSetVersion(void) const {
        return m_Key.IsSetVersion();
    }
    TVersion GetVersion(void) const {
        return m_Key.GetVersion();
    }
    void Restore(CTextseq_id& id, TPacked param) const;

    static TKey ParseAcc(const string& acc, const TVersion* ver);
    static TKey ParseAcc(const string& acc, const CTextseq_id* tid) {
        TVersion ver, *ver_ptr = 0;
        if ( tid && tid->IsSetVersion() ) {
            ver = tid->GetVersion();
            ver_ptr = &ver;
        }
        return ParseAcc(acc, ver_ptr);
    }
    TPacked ParseAccNumber(const string& acc) const;
    TPacked Pack(const CTextseq_id& id) const;
    
    virtual CConstRef<CSeq_id> GetPackedSeqId(TPacked packed) const;
    
private:
    TKey m_Key;
};


class CSeq_id_Textseq_Tree : public CSeq_id_Which_Tree
{
public:
    typedef CTextseq_id::TVersion TVersion;

    CSeq_id_Textseq_Tree(CSeq_id_Mapper* mapper, CSeq_id::E_Choice type);
    ~CSeq_id_Textseq_Tree(void);
    
    virtual bool Empty(void) const;

    virtual CSeq_id_Handle FindInfo(const CSeq_id& id) const;
    virtual CSeq_id_Handle FindOrCreate(const CSeq_id& id);

    virtual bool HaveMatch(const CSeq_id_Handle& id) const;
    virtual void FindMatch(const CSeq_id_Handle& id,
                           TSeq_id_MatchList& id_list) const;
    virtual void FindMatchStr(const string& sid,
                              TSeq_id_MatchList& id_list) const;

    virtual bool Match(const CSeq_id_Handle& h1,
                       const CSeq_id_Handle& h2) const;
    virtual bool IsBetterVersion(const CSeq_id_Handle& h1,
                                 const CSeq_id_Handle& h2) const;
    
    virtual bool HaveReverseMatch(const CSeq_id_Handle& id) const;
    virtual void FindReverseMatch(const CSeq_id_Handle& id,
                                  TSeq_id_MatchList& id_list);

    virtual size_t Dump(CNcbiOstream& out,
                        CSeq_id::E_Choice type,
                        int details) const;

protected:
    virtual void x_Unindex(const CSeq_id_Info* info);
    virtual bool x_Check(const CSeq_id::E_Choice& type) const;
    virtual bool x_Check(const CSeq_id& id) const;
    const CTextseq_id& x_Get(const CSeq_id& id) const {
        const CTextseq_id* text_id = id.GetTextseq_Id();
        _ASSERT(text_id);
        return *text_id;
    }
    CSeq_id_Info* x_FindStrInfo(CSeq_id::E_Choice type,
                                const CTextseq_id& tid) const;
    bool x_GetVersion(TVersion& version, const CSeq_id_Handle& id) const;

private:
    typedef multimap<string, CSeq_id_Info*, PNocase> TStringMap;
    typedef TStringMap::value_type TStringMapValue;
    typedef TStringMap::const_iterator TStringMapCI;
    typedef pair<TStringMapCI, TStringMapCI> TVersions;
    typedef CSeq_id_Textseq_Info::TKey TPackedKey;
    typedef map<TPackedKey, CConstRef<CSeq_id_Textseq_Info> > TPackedMap;
    typedef TPackedMap::value_type TPackedMapValue;
    typedef TPackedMap::iterator TPackedMap_I;
    typedef TPackedMap::const_iterator TPackedMap_CI;
    
    static bool x_Equals(const CTextseq_id& id1, const CTextseq_id& id2);
    static void x_Erase(TStringMap& str_map,
                        const string& key,
                        const CSeq_id_Info* info);

    CSeq_id_Info* x_FindStrInfo(const TStringMap& str_map,
                                const string& str,
                                CSeq_id::E_Choice type,
                                const CTextseq_id& tid) const;

    void x_FindMatchByAcc(TSeq_id_MatchList& id_list,
                          const string& acc,
                          const TVersion* ver = 0) const;
    void x_FindMatchByAcc(TSeq_id_MatchList& id_list,
                          const string& acc,
                          const CTextseq_id* tid) const {
        TVersion ver, *ver_ptr = 0;
        if ( tid && tid->IsSetVersion() ) {
            ver = tid->GetVersion();
            ver_ptr = &ver;
        }
        x_FindMatchByAcc(id_list, acc, ver_ptr);
    }
    void x_FindMatchByName(TSeq_id_MatchList& id_list,
                           const string& name,
                           const CTextseq_id* tid = 0) const;

    void x_FindRevMatchByAcc(TSeq_id_MatchList& id_list,
                             const string& acc,
                             const TVersion* ver = 0) const;
    void x_FindRevMatchByAcc(TSeq_id_MatchList& id_list,
                             const string& acc,
                             const CTextseq_id* tid) const {
        TVersion ver, *ver_ptr = 0;
        if ( tid && tid->IsSetVersion() ) {
            ver = tid->GetVersion();
            ver_ptr = &ver;
        }
        x_FindRevMatchByAcc(id_list, acc, ver_ptr);
    }
    void x_FindRevMatchByName(TSeq_id_MatchList& id_list,
                              const string& name,
                              const CTextseq_id* tid = 0) const;

    CSeq_id::E_Choice m_Type;
    TStringMap m_ByAcc;
    TStringMap m_ByName; // Used for searching by string
    TPackedMap m_PackedMap;
};


////////////////////////////////////////////////////////////////////
// Genbank, EMBL and DDBJ joint tree


class CSeq_id_GB_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_GB_Tree(CSeq_id_Mapper* mapper);
protected:
    virtual bool x_Check(const CSeq_id::E_Choice& type) const;
};


////////////////////////////////////////////////////////////////////
// Pir tree


class CSeq_id_Pir_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_Pir_Tree(CSeq_id_Mapper* mapper);
};


////////////////////////////////////////////////////////////////////
// Swissprot


class CSeq_id_Swissprot_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_Swissprot_Tree(CSeq_id_Mapper* mapper);
};


////////////////////////////////////////////////////////////////////
// Prf tree


class CSeq_id_Prf_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_Prf_Tree(CSeq_id_Mapper* mapper);
};


////////////////////////////////////////////////////////////////////
// Tpg tree


class CSeq_id_Tpg_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_Tpg_Tree(CSeq_id_Mapper* mapper);
};


////////////////////////////////////////////////////////////////////
// Tpe tree


class CSeq_id_Tpe_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_Tpe_Tree(CSeq_id_Mapper* mapper);
};


////////////////////////////////////////////////////////////////////
// Tpd tree


class CSeq_id_Tpd_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_Tpd_Tree(CSeq_id_Mapper* mapper);
};


////////////////////////////////////////////////////////////////////
// Gpipe tree


class CSeq_id_Gpipe_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_Gpipe_Tree(CSeq_id_Mapper* mapper);
};


////////////////////////////////////////////////////////////////////
// Named-annot-track tree


class CSeq_id_Named_annot_track_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_Named_annot_track_Tree(CSeq_id_Mapper* mapper);
};


////////////////////////////////////////////////////////////////////
// Other tree


class CSeq_id_Other_Tree : public CSeq_id_Textseq_Tree
{
public:
    CSeq_id_Other_Tree(CSeq_id_Mapper* mapper);
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

    virtual void FindMatchStr(const string& sid,
                              TSeq_id_MatchList& id_list) const;

    virtual size_t Dump(CNcbiOstream& out,
                        CSeq_id::E_Choice type,
                        int details) const;

private:
    virtual void x_Unindex(const CSeq_id_Info* info);
    CSeq_id_Info* x_FindInfo(const CObject_id& oid) const;

    typedef map<string, CSeq_id_Info*, PNocase> TByStr;
    typedef map<TPacked, CSeq_id_Info*>         TById;

    TByStr m_ByStr;
    TById  m_ById;
};


////////////////////////////////////////////////////////////////////
// e_General tree


class CSeq_id_General_Id_Info : public CSeq_id_Info {
public:
    typedef string TKey;
    typedef PNocase PKeyLess;

    CSeq_id_General_Id_Info(CSeq_id_Mapper* mapper, const TKey& key);
    ~CSeq_id_General_Id_Info(void);
    
    const TKey& GetKey(void) const {
        return m_Key;
    }
    const string& GetDbtag(void) const {
        return m_Key;
    }
    void Restore(CDbtag& id, TPacked param) const;

    static TKey Parse(const CDbtag& id);
    TPacked Pack(const CDbtag& id) const;
    
    virtual CConstRef<CSeq_id> GetPackedSeqId(TPacked packed) const;
    
private:
    TKey m_Key;
};


class CSeq_id_General_Str_Info : public CSeq_id_Info {
public:
    struct TKey {
        // all upper case
        TPacked m_Key;
        string m_Db;
        string m_StrPrefix;
        string m_StrSuffix;
        bool operator==(const TKey& b) const {
            return m_Key == b.m_Key &&
                NStr::EqualNocase(m_Db, b.m_Db) &&
                NStr::EqualNocase(m_StrPrefix, b.m_StrPrefix) &&
                NStr::EqualNocase(m_StrSuffix, b.m_StrSuffix);
        }
        bool operator!=(const TKey& b) const {
            return !(*this == b);
        }
    };
    struct PKeyLess {
        bool operator()(const TKey& a, const TKey& b) const {
            if ( a.m_Key != b.m_Key ) {
                return a.m_Key < b.m_Key;
            }
            int diff = NStr::CompareNocase(a.m_Db, b.m_Db);
            if ( diff == 0 ) {
                diff = NStr::CompareNocase(a.m_StrPrefix, b.m_StrPrefix);
                if ( diff == 0 ) {
                    diff = NStr::CompareNocase(a.m_StrSuffix, b.m_StrSuffix);
                }
            }
            return diff < 0;
        }
    };

    CSeq_id_General_Str_Info(CSeq_id_Mapper* mapper, const TKey& key);
    ~CSeq_id_General_Str_Info(void);
    
    const TKey& GetKey(void) const {
        return m_Key;
    }
    const string& GetDbtag(void) const {
        return m_Key.m_Db;
    }
    const string& GetStrPrefix(void) const {
        return m_Key.m_StrPrefix;
    }
    const string& GetStrSuffix(void) const {
        return m_Key.m_StrSuffix;
    }
    int GetStrDigits(void) const {
        return m_Key.m_Key & 0xff;
    }
    void Restore(CDbtag& id, TPacked param) const;

    static TKey Parse(const CDbtag& id);
    TPacked ParseStrNumber(const string& str) const;
    TPacked Pack(const CDbtag& id) const;
    
    virtual CConstRef<CSeq_id> GetPackedSeqId(TPacked packed) const;
    
private:
    TKey m_Key;
};


class CSeq_id_General_Tree : public CSeq_id_Which_Tree
{
public:
    CSeq_id_General_Tree(CSeq_id_Mapper* mapper);
    ~CSeq_id_General_Tree(void);

    virtual bool Empty(void) const;

    virtual CSeq_id_Handle FindInfo(const CSeq_id& id) const;
    virtual CSeq_id_Handle FindOrCreate(const CSeq_id& id);

    // Get the list of matching seq-id (int id = str id).
    virtual bool HaveMatch(const CSeq_id_Handle& id) const;
    virtual void FindMatch(const CSeq_id_Handle& id,
                           TSeq_id_MatchList& id_list) const;
    virtual void FindMatchStr(const string& sid,
                              TSeq_id_MatchList& id_list) const;

    virtual size_t Dump(CNcbiOstream& out,
                        CSeq_id::E_Choice type,
                        int details) const;

private:
    virtual void x_Unindex(const CSeq_id_Info* info);
    CSeq_id_Info* x_FindInfo(const CDbtag& dbid) const;

    struct STagMap {
    public:
        typedef map<string, CSeq_id_Info*, PNocase> TByStr;
        typedef map<TPacked, CSeq_id_Info*>         TById;
        TByStr m_ByStr;
        TById  m_ById;
    };
    typedef map<string, STagMap, PNocase> TDbMap;
    typedef CSeq_id_General_Id_Info::TKey TPackedIdKey;
    typedef map<TPackedIdKey, CConstRef<CSeq_id_General_Id_Info>,
                CSeq_id_General_Id_Info::PKeyLess> TPackedIdMap;
    typedef CSeq_id_General_Str_Info::TKey TPackedStrKey;
    typedef map<TPackedStrKey, CConstRef<CSeq_id_General_Str_Info>,
                CSeq_id_General_Str_Info::PKeyLess> TPackedStrMap;

    TDbMap m_DbMap;
    TPackedIdMap m_PackedIdMap;
    TPackedStrMap m_PackedStrMap;
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

    virtual void FindMatchStr(const string& sid,
                              TSeq_id_MatchList& id_list) const;

    virtual size_t Dump(CNcbiOstream& out,
                        CSeq_id::E_Choice type,
                        int details) const;

private:
    virtual void x_Unindex(const CSeq_id_Info* info);
    CSeq_id_Info* x_FindInfo(const CGiimport_id& gid) const;

    // 2-level indexing: first by Id, second by Db+Release
    typedef vector<CSeq_id_Info*> TGiimList;
    typedef map<TPacked, TGiimList> TIdMap;

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

    virtual void FindMatchStr(const string& sid,
                              TSeq_id_MatchList& id_list) const;

    virtual size_t Dump(CNcbiOstream& out,
                        CSeq_id::E_Choice type,
                        int details) const;

private:
    virtual void x_Unindex(const CSeq_id_Info* info);
    CSeq_id_Info* x_FindInfo(const CPatent_seq_id& pid) const;

    // 3-level indexing: country, (number|app_number), seqid.
    // Ignoring patent doc-type in indexing.
    struct SPat_idMap {
        typedef map<TPacked, CSeq_id_Info*> TBySeqid;
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
    virtual void FindMatchStr(const string& sid,
                              TSeq_id_MatchList& id_list) const;
    virtual bool HaveReverseMatch(const CSeq_id_Handle& id) const;
    virtual void FindReverseMatch(const CSeq_id_Handle& id,
                                  TSeq_id_MatchList& id_list);
    
    virtual size_t Dump(CNcbiOstream& out,
                        CSeq_id::E_Choice type,
                        int details) const;

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

#endif  /* OBJECTS_OBJMGR___SEQ_ID_TREE__HPP */
