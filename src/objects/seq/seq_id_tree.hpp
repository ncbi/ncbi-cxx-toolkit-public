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
#include <unordered_map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CSeq_id;
class CSeq_id_Handle;
class CSeq_id_Info;
class CSeq_id_Mapper;
class CSeq_id_Which_Tree;

struct PHashNocase {
    static char get_hash(char c)
        {
            // In ids only ASCII characters are allowed, and in ASCII
            // upper and lower cases differ only by one bit.
            // So for efficiency it's enough to reset that bit
            // instead of using more complex tolower().
            return c&~32;
            //return tolower(c);
        }
    size_t operator()(const string& s) const
        {
            size_t h = s.size();
            for ( auto c : s )
                h = h*17 + get_hash(c);
            return h;
        }
};
struct PEqualNocase {
    bool operator()(const string& s1, const string& s2) const
        {
            // in most cases letter cases match,
            // so it's faster first to check that
            // with more efficient direct string comparison
            if ( s1 == s2 ) {
                return true;
            }
            // otherwise we first check if lengths are the same
            size_t len = s1.size();
            if ( s2.size() != len ) {
                return false;
            }
            for ( size_t i = 0; i < len; ++i ) {
                char c1 = s1[i];
                char c2 = s2[i];
                if ( tolower((unsigned char)c1) != tolower((unsigned char)c2) ) {
                    return false;
                }
            }
            return true;
            // commented out old less efficient comparison
            //return NStr::EqualNocase(s1, s2);
        }
};

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
    static const CSeq_id* GetSeqId(const CSeq_id_Info* info)
        {
            return info->m_Seq_id.GetPointerOrNull();
        }
    virtual void x_Unindex(const CSeq_id_Info* info) = 0;

    typedef CFastMutex TTreeLock;
    typedef TTreeLock::TReadLockGuard TReadLockGuard;
    typedef TTreeLock::TWriteLockGuard TWriteLockGuard;

    mutable TTreeLock m_TreeLock;
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
    CSeq_id_Gi_Info(CSeq_id_Mapper* mapper);
    
    virtual CConstRef<CSeq_id> GetPackedSeqId(TPacked packed, TVariant /*variant*/) const;
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

    virtual void FindMatchStr(const string& sid,
                              TSeq_id_MatchList& id_list) const;

    virtual size_t Dump(CNcbiOstream& out,
                        CSeq_id::E_Choice type,
                        int details) const;

protected:
    virtual void x_Unindex(const CSeq_id_Info* info);
    bool x_Check(const CSeq_id& id) const;
    TGi x_Get(const CSeq_id& id) const;

    CSeq_id_Info* m_ZeroInfo;
    CSeq_id_Info* m_SharedInfo;
};


////////////////////////////////////////////////////////////////////
// Base class for e_Genbank, e_Embl, e_Pir, e_Swissprot, e_Other,
// e_Ddbj, e_Prf, e_Tpg, e_Tpe, e_Tpd trees


class CSeq_id_Textseq_PlainInfo : public CSeq_id_Info
{
public:
    CSeq_id_Textseq_PlainInfo(const CConstRef<CSeq_id>& seq_id, CSeq_id_Mapper* mapper);
    
    virtual CConstRef<CSeq_id> GetPackedSeqId(TPacked packed, TVariant variant) const;
    
    TVariant ParseCaseVariant(const string& acc) const;
    TVariant ParseCaseVariant(const CTextseq_id& id) const;
};


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
        Uint1 m_PrefixLen;
        enum {
            kMaxPrefixLen = 7
        };
        char m_PrefixBuf[kMaxPrefixLen];

        DECLARE_OPERATOR_BOOL(m_Hash != 0);

        bool operator==(const TKey& b) const {
            return m_Hash == b.m_Hash && m_Version == b.m_Version &&
                NStr::EqualNocase(GetAccPrefix(), b.GetAccPrefix());
        }
        bool operator!=(const TKey& b) const {
            return !(*this == b);
        }
        bool operator<(const TKey& b) const {
            return m_Hash < b.m_Hash ||
                (m_Hash == b.m_Hash &&
                 (m_Version < b.m_Version ||
                  (m_Version == b.m_Version &&
                   NStr::CompareNocase(GetAccPrefix(), b.GetAccPrefix()) < 0)));
        }

        bool SameHash(const TKey& b) const {
            return m_Hash == b.m_Hash;
        }
        bool SameHashNoVer(const TKey& b) const {
            return ((m_Hash ^ b.m_Hash) & ~1) == 0;
        }
        bool EqualAcc(const TKey& b) const {
            return SameHashNoVer(b) &&
                NStr::EqualNocase(GetAccPrefix(), b.GetAccPrefix());
        }

        bool IsSetVersion(void) const {
            return (m_Hash & 1) != 0;
        }
        const TVersion& GetVersion(void) const {
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
        int GetAccDigits(void) const {
            return (m_Hash & 0xff) >> 1;
        }
        TVariant ParseCaseVariant(const string& acc) const;
        
        size_t GetPrefixLen() const {
            return m_PrefixLen;
        }
        CTempString GetAccPrefix(void) const {
            return CTempString(m_PrefixBuf, m_PrefixLen);
        }
    };
    CSeq_id_Textseq_Info(CSeq_id::E_Choice type,
                         CSeq_id_Mapper* mapper,
                         const TKey& key);
    ~CSeq_id_Textseq_Info(void);
    
    const TKey& GetKey(void) const {
        return m_Key;
    }
    CTempString GetAccPrefix(void) const {
        return m_Key.GetAccPrefix();
    }
    bool GoodPrefix(const CTempString& acc) const {
        return NStr::StartsWith(acc, GetAccPrefix(), NStr::eNocase);
    }
    int GetAccDigits(void) const {
        return m_Key.GetAccDigits();
    }
    bool IsSetVersion(void) const {
        return m_Key.IsSetVersion();
    }
    const TVersion& GetVersion(void) const {
        return m_Key.GetVersion();
    }
    void RestoreAccession(string& acc, TPacked param, TVariant variant) const;
    void Restore(CTextseq_id& id, TPacked param, TVariant variant) const;

    static TKey ParseAcc(const string& acc, const TVersion* ver);
    static TKey ParseAcc(const string& acc, const CTextseq_id& tid) {
        TVersion ver;
        const TVersion *ver_ptr = 0;
        if ( tid.IsSetVersion() ) {
            ver = tid.GetVersion();
            ver_ptr = &ver;
        }
        return ParseAcc(acc, ver_ptr);
    }
    static TPacked Pack(const TKey& key, const string& acc);
    static TPacked Pack(const TKey& key, const CTextseq_id& id);
    static TVariant ParseCaseVariant(const CSeq_id_Info* info, const string& acc);
    
    virtual CConstRef<CSeq_id> GetPackedSeqId(TPacked packed, TVariant variant) const;
    
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
    CSeq_id_Textseq_PlainInfo* x_FindStrInfo(CSeq_id::E_Choice type,
                                             const CTextseq_id& tid) const;
    bool x_GetVersion(TVersion& version, const CSeq_id_Handle& id) const;

private:
    typedef multimap<string, CSeq_id_Textseq_PlainInfo*, PNocase> TStringMap;
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

    CSeq_id_Textseq_PlainInfo* x_FindStrInfo(const TStringMap& str_map,
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

    void x_FindRevMatchByAccPacked(TSeq_id_MatchList& id_list,
                                   const string& acc,
                                   const TVersion* ver = 0) const;
    void x_FindRevMatchByAccNonPacked(TSeq_id_MatchList& id_list,
                                      const string& acc,
                                      const TVersion* ver = 0) const;
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


class CSeq_id_Local_Info : public CSeq_id_Info {
public:
    CSeq_id_Local_Info(const CObject_id& oid, CSeq_id_Mapper* mapper);
    ~CSeq_id_Local_Info(void);

    bool IsId() const {
        return m_IsId;
    }
    bool HasMatchingId() const {
        return m_HasMatchingId;
    }
    CObject_id::TId GetMatchingId() const {
        return m_MatchingId;
    }
    
    TVariant ParseCaseVariant(const string& str) const;
    TVariant ParseCaseVariant(const CObject_id& oid) const;
    
    virtual CConstRef<CSeq_id> GetPackedSeqId(TPacked packed, TVariant variant) const;
    
private:
    bool m_IsId;
    bool m_HasMatchingId;
    CObject_id::TId m_MatchingId;
};


class CSeq_id_Local_Tree : public CSeq_id_Which_Tree
{
public:
    CSeq_id_Local_Tree(CSeq_id_Mapper* mapper);
    ~CSeq_id_Local_Tree(void);

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
    CSeq_id_Local_Info* x_FindInfo(const CObject_id& oid) const;
    CSeq_id_Local_Info* x_FindStrInfo(const string& str) const;
    CSeq_id_Local_Info* x_FindIdInfo(CObject_id::TId id) const;
    
    typedef unordered_map<string, CSeq_id_Local_Info*, PHashNocase, PEqualNocase> TByStr;
    typedef map<CObject_id::TId, CSeq_id_Local_Info*>         TById;

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
    void Restore(CDbtag& id, TPacked param, TVariant variant) const;

    static TPacked Pack(const TKey& key, const CDbtag& id);
    
    virtual CConstRef<CSeq_id> GetPackedSeqId(TPacked packed, TVariant variant) const;
    
private:
    TKey m_Key;
};


class CSeq_id_General_Str_Info : public CSeq_id_Info {
public:
    struct TKey {
        TPacked m_Key;
        string m_Db;
        string m_StrPrefix;
        string m_StrSuffix;
        bool operator==(const TKey& b) const {
            return m_Key == b.m_Key &&
                PEqualNocase()(m_StrSuffix, b.m_StrSuffix) &&
                PEqualNocase()(m_StrPrefix, b.m_StrPrefix) &&
                PEqualNocase()(m_Db, b.m_Db);
        }
        bool operator!=(const TKey& b) const {
            return !(*this == b);
        }
        size_t GetStrDigits(void) const {
            return m_Key & 0xff;
        }
        TVariant ParseCaseVariant(const CDbtag& dbtag) const;
    };
    struct PKeyLess {
        bool operator()(const TKey& a, const TKey& b) const {
            if ( a.m_Key != b.m_Key ) {
                return a.m_Key < b.m_Key;
            }
            int diff = NStr::CompareNocase(a.m_StrSuffix, b.m_StrSuffix);
            if ( diff == 0 ) {
                diff = NStr::CompareNocase(a.m_StrPrefix, b.m_StrPrefix);
                if ( diff == 0 ) {
                    diff = NStr::CompareNocase(a.m_Db, b.m_Db);
                }
            }
            return diff < 0;
        }
    };
    struct PHash {
        TPacked operator()(const TKey& a) const {
            return a.m_Key;
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
    size_t GetStrDigits(void) const {
        return m_Key.GetStrDigits();
    }
    void Restore(CDbtag& id, TPacked param, TVariant variant) const;

    static TKey Parse(const CDbtag& id);
    static TPacked Pack(const TKey& key, const CDbtag& id);
    
    virtual CConstRef<CSeq_id> GetPackedSeqId(TPacked packed, TVariant variant) const;
    
private:
    TKey m_Key;
};


class CSeq_id_General_PlainInfo : public CSeq_id_Info {
public:
    CSeq_id_General_PlainInfo(const CDbtag& dbid, CSeq_id_Mapper* mapper);
    
    virtual CConstRef<CSeq_id> GetPackedSeqId(TPacked packed, TVariant variant) const;
    
    TVariant ParseCaseVariant(const CDbtag& dbtag) const;
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
    CSeq_id_General_PlainInfo* x_FindInfo(const CDbtag& dbid) const;

    struct STagMap {
    public:
        typedef unordered_map<string, CSeq_id_General_PlainInfo*, PHashNocase, PEqualNocase> TByStr;
        typedef map<TPacked, CSeq_id_General_PlainInfo*>         TById;
        TByStr m_ByStr;
        TById  m_ById;
    };
    typedef map<string, STagMap, PNocase> TDbMap;
    typedef CSeq_id_General_Id_Info::TKey TPackedIdKey;
    typedef map<TPackedIdKey, CConstRef<CSeq_id_General_Id_Info>,
                CSeq_id_General_Id_Info::PKeyLess> TPackedIdMap;
    typedef CSeq_id_General_Str_Info::TKey TPackedStrKey;
    typedef unordered_map<TPackedStrKey, CConstRef<CSeq_id_General_Str_Info>,
                          CSeq_id_General_Str_Info::PHash> TPackedStrMap;

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

    string x_IdToStrKey(const CPDB_seq_id& id) const;

    // Index by mol+chain, no date - too complicated
    typedef vector<CSeq_id_Info*>  TSubMolList;
    typedef map<string, TSubMolList, PCase> TMolMap;

    TMolMap m_MolMap;
};


// Seq-id mapper exception
class NCBI_SEQ_EXPORT CSeq_id_MapperException : public CException
{
public:
    enum EErrCode {
        eTypeError,
        eSymbolError,
        eEmptyError,
        eOtherError
    };
    const char* GetErrCodeString(void) const override;
    NCBI_EXCEPTION_DEFAULT(CSeq_id_MapperException,CException);
};


/////////////////////////////////////////////////////////////////////////////
//
// Inline methods
//
/////////////////////////////////////////////////////////////////////////////

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJECTS_OBJMGR___SEQ_ID_TREE__HPP */
