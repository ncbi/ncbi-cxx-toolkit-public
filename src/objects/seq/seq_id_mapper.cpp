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
* Author: Aleksey Grichenko
*
* File Description:
*   Seq-id mapper for Object Manager
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2002/02/06 21:46:11  gouriano
* *** empty log message ***
*
* Revision 1.3  2002/02/05 21:46:28  gouriano
* added FindSeqid function, minor tuneup in CSeq_id_mapper
*
* Revision 1.2  2002/02/01 21:49:51  gouriano
* minor changes to make it compilable and run on Solaris Workshop
*
* Revision 1.1  2002/01/23 21:57:49  grichenk
* Splitted id_handles.hpp
*
*
* ===========================================================================
*/


#include "seq_id_mapper.hpp"
#include <corelib/ncbi_limits.hpp>
#include <serial/typeinfo.hpp>
#include <serial/serialbase.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqloc/Giimport_id.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Date.hpp>
#include <objects/biblio/Id_pat.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// to make map<string, something> case-insensitive

struct seqid_string_less
{
    bool operator()(const string& s1, const string& s2) const
    {
        return (NStr::CompareNocase(s1, s2) < 0);
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
    CSeq_id_Which_Tree(void) {}
    virtual ~CSeq_id_Which_Tree(void) {}

    typedef list<TSeq_id_Info> TSeq_id_MatchList;

    // Find exaclty the same seq-id
    virtual TSeq_id_Info FindEqual(const CSeq_id& id) const = 0;
    // Get the list of matching seq-id.
    // Not using list of handles since CSeq_id_Handle constructor
    // is private.
    virtual void FindMatch(const CSeq_id& id,
                           TSeq_id_MatchList& id_list) const = 0;
    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const = 0;

    // Map new seq-id
    virtual void AddSeq_idMapping(CSeq_id_Handle& handle) = 0;

    // Remove mapping for a given keys range
    void DropKeysRange(TSeq_id_Key first, TSeq_id_Key last);

protected:
    const CSeq_id& x_GetSeq_id(const CSeq_id_Handle& handle) const;
    TSeq_id_Key    x_GetKey(const CSeq_id_Handle& handle) const;

    void x_AddToKeyMap(const CSeq_id_Handle& handle);
    void x_RemoveFromKeyMap(TSeq_id_Key key);
    CSeq_id_Handle x_GetHandleByKey(TSeq_id_Key key) const;

    // Called by DropKeysRange() for each handle to remove it from
    // all maps. After calling x_DropHandle() the key is removed
    // from the keys map.
    virtual void x_DropHandle(const CSeq_id_Handle& handle) = 0;

    // Map for reverse lookups
    typedef map<TSeq_id_Key, CSeq_id_Handle> TKeyMap;
    TKeyMap m_KeyMap;
};


inline
void CSeq_id_Which_Tree::x_RemoveFromKeyMap(TSeq_id_Key key)
{
    m_KeyMap.erase(key);
}


void CSeq_id_Which_Tree::DropKeysRange(TSeq_id_Key first, TSeq_id_Key last)
{
    TKeyMap::iterator it_first = m_KeyMap.lower_bound(first);
    TKeyMap::iterator it_last = m_KeyMap.upper_bound(last);
    for (TKeyMap::iterator it = it_first; it != it_last; ++it) {
        x_DropHandle(it->second);
        x_RemoveFromKeyMap(it->first);
    }
}


inline
const CSeq_id&
CSeq_id_Which_Tree::x_GetSeq_id(const CSeq_id_Handle& handle) const
{
    return *handle.m_SeqId;
}

inline
TSeq_id_Key CSeq_id_Which_Tree::x_GetKey(const CSeq_id_Handle& handle) const
{
    return handle.m_Value;
}


inline
void CSeq_id_Which_Tree::x_AddToKeyMap(const CSeq_id_Handle& handle)
{
    m_KeyMap[handle.m_Value] = handle;
}


CSeq_id_Handle CSeq_id_Which_Tree::x_GetHandleByKey(TSeq_id_Key key) const
{
    TKeyMap::const_iterator it = m_KeyMap.find(key);
    if (it != m_KeyMap.end())
        return it->second;
    return CSeq_id_Handle();
}



////////////////////////////////////////////////////////////////////
// Base class for Gi, Gibbsq & Gibbmt trees


class CSeq_id_int_Tree : public CSeq_id_Which_Tree
{
public:
    virtual TSeq_id_Info FindEqual(const CSeq_id& id) const;
    virtual void FindMatch(const CSeq_id& id,
                           TSeq_id_MatchList& id_list) const;
    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const;
    virtual void AddSeq_idMapping(CSeq_id_Handle& handle);

protected:
    virtual void x_DropHandle(const CSeq_id_Handle& handle);

    virtual bool x_Check(const CSeq_id& id) const = 0;
    virtual int  x_Get(const CSeq_id& id) const = 0;

private:
    typedef map<int, CSeq_id_Handle> TIntMap;
    TIntMap m_IntMap;
};


TSeq_id_Info CSeq_id_int_Tree::FindEqual(const CSeq_id& id) const
{
    x_Check(id);
    TIntMap::const_iterator it = m_IntMap.find(x_Get(id));
    if (it != m_IntMap.end()) {
        return TSeq_id_Info(&x_GetSeq_id(it->second), x_GetKey(it->second));
    }
    return TSeq_id_Info(0, 0);
}


void CSeq_id_int_Tree::FindMatch(const CSeq_id& id,
                                TSeq_id_MatchList& id_list) const
{
    // Only one instance of each int id
    id_list.push_back(FindEqual(id));
}


void CSeq_id_int_Tree::FindMatchStr(string sid,
                                    TSeq_id_MatchList& id_list) const
{
    try {
        int value = NStr::StringToInt(sid);
        TIntMap::const_iterator it = m_IntMap.find(value);
        if (it == m_IntMap.end())
            return;
        id_list.push_back(TSeq_id_Info(
            &x_GetSeq_id(it->second), x_GetKey(it->second)));
    }
    catch (runtime_error) {
        // Not an integer value
        return;
    }
}


void CSeq_id_int_Tree::AddSeq_idMapping(CSeq_id_Handle& handle)
{
    x_Check(x_GetSeq_id(handle));
    int key = x_Get(x_GetSeq_id(handle));
    _ASSERT(m_IntMap.find(key) == m_IntMap.end());
    m_IntMap[key] = handle;
    x_AddToKeyMap(handle);
}


void CSeq_id_int_Tree::x_DropHandle(const CSeq_id_Handle& handle)
{
    const CSeq_id& id = x_GetSeq_id(handle);
    x_Check(id);
    TIntMap::iterator it = m_IntMap.find(x_Get(id));
    _ASSERT(it != m_IntMap.end());
    _ASSERT(it->second == handle);
    m_IntMap.erase(it);
}



////////////////////////////////////////////////////////////////////
// Gi tree


class CSeq_id_Gi_Tree : public CSeq_id_int_Tree
{
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual int  x_Get(const CSeq_id& id) const;
};


bool CSeq_id_Gi_Tree::x_Check(const CSeq_id& id) const
{
    _ASSERT(id.IsGi());
    return id.IsGi();
}


int CSeq_id_Gi_Tree::x_Get(const CSeq_id& id) const
{
    _ASSERT(id.IsGi());
    return id.GetGi();
}



////////////////////////////////////////////////////////////////////
// Gibbsq tree


class CSeq_id_Gibbsq_Tree : public CSeq_id_int_Tree
{
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual int  x_Get(const CSeq_id& id) const;
};


bool CSeq_id_Gibbsq_Tree::x_Check(const CSeq_id& id) const
{
    _ASSERT(id.IsGibbsq());
    return id.IsGibbsq();
}


int CSeq_id_Gibbsq_Tree::x_Get(const CSeq_id& id) const
{
    _ASSERT(id.IsGibbsq());
    return id.GetGibbsq();
}



////////////////////////////////////////////////////////////////////
// Gibbmt tree


class CSeq_id_Gibbmt_Tree : public CSeq_id_int_Tree
{
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual int  x_Get(const CSeq_id& id) const;
};


bool CSeq_id_Gibbmt_Tree::x_Check(const CSeq_id& id) const
{
    _ASSERT(id.IsGibbmt());
    return id.IsGibbmt();
}


int CSeq_id_Gibbmt_Tree::x_Get(const CSeq_id& id) const
{
    _ASSERT(id.IsGibbmt());
    return id.GetGibbmt();
}



////////////////////////////////////////////////////////////////////
// Base class for e_Genbank, e_Embl, e_Pir, e_Swissprot, e_Other,
// e_Ddbj, e_Prf, e_Tpg, e_Tpe, e_Tpd trees


class CSeq_id_Textseq_Tree : public CSeq_id_Which_Tree
{
public:
    virtual TSeq_id_Info FindEqual(const CSeq_id& id) const;
    virtual void FindMatch(const CSeq_id& id,
                           TSeq_id_MatchList& id_list) const;
    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const;
    virtual void AddSeq_idMapping(CSeq_id_Handle& handle);

protected:
    virtual void x_DropHandle(const CSeq_id_Handle& handle);

    virtual bool x_Check(const CSeq_id& id) const = 0;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const = 0;

private:
    typedef list<CSeq_id_Handle>   TVersions;
    typedef map<string, TVersions, seqid_string_less> TStringMap;
    TSeq_id_Info x_FindVersionEqual(const TVersions& ver_list,
                                    const CTextseq_id& tid) const;
    void x_FindVersionMatch(const TVersions& ver_list,
                            const CTextseq_id& tid,
                            TSeq_id_MatchList& id_list) const;

    TStringMap m_ByAccession;
    TStringMap m_ByName; // Used for searching by string
};


TSeq_id_Info
CSeq_id_Textseq_Tree::x_FindVersionEqual(const TVersions& ver_list,
                                         const CTextseq_id& tid) const
{
    iterate(TVersions, vit, ver_list) {
        const CTextseq_id& tid_it = x_Get(x_GetSeq_id(*vit));
        if (tid.IsSetVersion()  &&  tid_it.IsSetVersion()) {
            // Compare versions
            if (tid.GetVersion() == tid_it.GetVersion()) {
                return TSeq_id_Info(&x_GetSeq_id(*vit), x_GetKey(*vit));
            }
        }
        else if (tid.IsSetRelease()  &&  tid_it.IsSetRelease()) {
            // Compare releases if no version specified
            if (tid.GetRelease() == tid_it.GetRelease()) {
                return TSeq_id_Info(&x_GetSeq_id(*vit), x_GetKey(*vit));
            }
        }
        else if (!tid.IsSetVersion()  &&  !tid.IsSetRelease()  &&
            !tid_it.IsSetVersion()  &&  !tid_it.IsSetRelease()) {
            // No version/release for both seq-ids
            return TSeq_id_Info(&x_GetSeq_id(*vit), x_GetKey(*vit));
        }
    }
    return TSeq_id_Info(0, 0);
}


TSeq_id_Info CSeq_id_Textseq_Tree::FindEqual(const CSeq_id& id) const
{
    // Note: if a record is found by accession, no name is checked
    // even if it is also set.
    x_Check(id);
    const CTextseq_id& tid = x_Get(id);
    // Can not compare if no accession given
    _ASSERT( tid.IsSetAccession() );
    TStringMap::const_iterator it = m_ByAccession.find(tid.GetAccession());
    if (it != m_ByAccession.end()) {
        TSeq_id_Info info = x_FindVersionEqual(it->second, tid);
        if ( info.first )
            return info;
    }
    return TSeq_id_Info(0, 0);
}


void CSeq_id_Textseq_Tree::x_FindVersionMatch(const TVersions& ver_list,
                                              const CTextseq_id& tid,
                                              TSeq_id_MatchList& id_list) const
{
    int ver = 0;
    string rel = "";
    if ( tid.IsSetVersion() )
        ver = tid.GetVersion();
    if ( tid.IsSetRelease() )
        rel = tid.GetRelease();
    iterate(TVersions, vit, ver_list) {
        int ver_it = 0;
        string rel_it = "";
        const CTextseq_id& vit_ref = x_Get(x_GetSeq_id(*vit));
        if ( vit_ref.IsSetVersion() )
            ver_it = vit_ref.GetVersion();
        if ( vit_ref.IsSetRelease() )
            rel_it = vit_ref.GetRelease();
        if (ver == ver_it || ver == 0) {
            id_list.push_back(TSeq_id_Info(
                &x_GetSeq_id(*vit), x_GetKey(*vit)));
        }
        else if (rel == rel_it || rel.empty()) {
            id_list.push_back(TSeq_id_Info(
                &x_GetSeq_id(*vit), x_GetKey(*vit)));
        }
    }
}


void CSeq_id_Textseq_Tree::FindMatch(const CSeq_id& id,
                                     TSeq_id_MatchList& id_list) const
{
    x_Check(id);
    const CTextseq_id& tid = x_Get(id);
    _ASSERT( tid.IsSetAccession() );
    // Find by accession
    TStringMap::const_iterator it = m_ByAccession.find(tid.GetAccession());
    if (it != m_ByAccession.end()) {
        x_FindVersionMatch(it->second, tid, id_list);
    }
}


void CSeq_id_Textseq_Tree::FindMatchStr(string sid,
                                        TSeq_id_MatchList& id_list) const
{
    // ignore '.' in the search string - cut it out
    sid = sid.substr(0, sid.find('.'));
    // Find by accession
    TStringMap::const_iterator it = m_ByAccession.find(sid);
    if (it != m_ByAccession.end()) {
        iterate(TVersions, vit, it->second) {
            id_list.push_back(TSeq_id_Info(
                &x_GetSeq_id(*vit), x_GetKey(*vit)));
        }
    }
    it = m_ByName.find(sid);
    if (it != m_ByName.end()) {
        iterate(TVersions, vit, it->second) {
            id_list.push_back(TSeq_id_Info(
                &x_GetSeq_id(*vit), x_GetKey(*vit)));
        }
    }
}


void CSeq_id_Textseq_Tree::AddSeq_idMapping(CSeq_id_Handle& handle)
{
    const CSeq_id& id = x_GetSeq_id(handle);
    x_Check(id);
    const CTextseq_id& tid = x_Get(id);
    if ( tid.IsSetAccession() ) {
        TVersions& ver = m_ByAccession[tid.GetAccession()];
        iterate(TVersions, vit, ver) {
            _ASSERT(!SerialEquals<CTextseq_id>(
                x_Get(x_GetSeq_id(*vit)), tid));
        }
        ver.push_back(handle);
    }
    if ( tid.IsSetName() ) {
        TVersions& ver = m_ByName[tid.GetName()];
        iterate(TVersions, vit, ver) {
            _ASSERT(!SerialEquals<CTextseq_id>(
                x_Get(x_GetSeq_id(*vit)), tid));
        }
        ver.push_back(handle);
    }
    x_AddToKeyMap(handle);
}


void CSeq_id_Textseq_Tree::x_DropHandle(const CSeq_id_Handle& handle)
{
    const CSeq_id& id = x_GetSeq_id(handle);
    x_Check(id);
    const CTextseq_id& tid = x_Get(id);
    if ( tid.IsSetAccession() ) {
        TStringMap::iterator it = m_ByAccession.find(tid.GetAccession());
        if (it != m_ByAccession.end()) {
            non_const_iterate(TVersions, vit, it->second) {
                if (*vit == handle) {
                    it->second.erase(vit);
                    break;
                }
            }
            if (it->second.empty())
                m_ByAccession.erase(it);
        }
    }
    if ( tid.IsSetName() ) {
        TStringMap::iterator it = m_ByName.find(tid.GetName());
        if (it != m_ByName.end()) {
            non_const_iterate(TVersions, vit, it->second) {
                if (*vit == handle) {
                    it->second.erase(vit);
                    break;
                }
            }
            if (it->second.empty())
                m_ByName.erase(it);
        }
    }
}



////////////////////////////////////////////////////////////////////
// Genbank, EMBL and DDBJ joint tree


class CSeq_id_GB_Tree : public CSeq_id_Textseq_Tree
{
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const;
};


bool CSeq_id_GB_Tree::x_Check(const CSeq_id& id) const
{
    _ASSERT(id.IsGenbank()  ||  id.IsEmbl()  ||  id.IsDdbj());
    return id.IsGenbank()  ||  id.IsEmbl()  ||  id.IsDdbj();
}


const CTextseq_id& CSeq_id_GB_Tree::x_Get(const CSeq_id& id) const
{
    _ASSERT(id.IsGenbank()  ||  id.IsEmbl()  ||  id.IsDdbj());
    switch ( id.Which() ) {
    case CSeq_id::e_Genbank:
        return id.GetGenbank();
    case CSeq_id::e_Embl:
        return id.GetEmbl();
    case CSeq_id::e_Ddbj:
        return id.GetDdbj();
    default:
        throw runtime_error("Invalid seq-id type");
    }
}



////////////////////////////////////////////////////////////////////
// Pir tree


class CSeq_id_Pir_Tree : public CSeq_id_Textseq_Tree
{
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const;
};


bool CSeq_id_Pir_Tree::x_Check(const CSeq_id& id) const
{
    _ASSERT(id.IsPir());
    return id.IsPir();
}


const CTextseq_id& CSeq_id_Pir_Tree::x_Get(const CSeq_id& id) const
{
    _ASSERT(id.IsPir());
    return id.GetPir();
}



////////////////////////////////////////////////////////////////////
// Swissprot


class CSeq_id_Swissprot_Tree : public CSeq_id_Textseq_Tree
{
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const;
};


bool CSeq_id_Swissprot_Tree::x_Check(const CSeq_id& id) const
{
    _ASSERT(id.IsSwissprot());
    return id.IsSwissprot();
}


const CTextseq_id& CSeq_id_Swissprot_Tree::x_Get(const CSeq_id& id) const
{
    _ASSERT(id.IsSwissprot());
    return id.GetSwissprot();
}



////////////////////////////////////////////////////////////////////
// Prf tree


class CSeq_id_Prf_Tree : public CSeq_id_Textseq_Tree
{
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const;
};


bool CSeq_id_Prf_Tree::x_Check(const CSeq_id& id) const
{
    _ASSERT(id.IsPrf());
    return id.IsPrf();
}


const CTextseq_id& CSeq_id_Prf_Tree::x_Get(const CSeq_id& id) const
{
    _ASSERT(id.IsPrf());
    return id.GetPrf();
}



////////////////////////////////////////////////////////////////////
// Tpg tree


class CSeq_id_Tpg_Tree : public CSeq_id_Textseq_Tree
{
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const;
};


bool CSeq_id_Tpg_Tree::x_Check(const CSeq_id& id) const
{
    _ASSERT(id.IsTpg());
    return id.IsTpg();
}


const CTextseq_id& CSeq_id_Tpg_Tree::x_Get(const CSeq_id& id) const
{
    _ASSERT(id.IsTpg());
    return id.GetTpg();
}



////////////////////////////////////////////////////////////////////
// Tpe tree


class CSeq_id_Tpe_Tree : public CSeq_id_Textseq_Tree
{
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const;
};


bool CSeq_id_Tpe_Tree::x_Check(const CSeq_id& id) const
{
    _ASSERT(id.IsTpe());
    return id.IsTpe();
}


const CTextseq_id& CSeq_id_Tpe_Tree::x_Get(const CSeq_id& id) const
{
    _ASSERT(id.IsTpe());
    return id.GetTpe();
}



////////////////////////////////////////////////////////////////////
// Tpd tree


class CSeq_id_Tpd_Tree : public CSeq_id_Textseq_Tree
{
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const;
};


bool CSeq_id_Tpd_Tree::x_Check(const CSeq_id& id) const
{
    _ASSERT(id.IsTpd());
    return id.IsTpd();
}


const CTextseq_id& CSeq_id_Tpd_Tree::x_Get(const CSeq_id& id) const
{
    _ASSERT(id.IsTpd());
    return id.GetTpd();
}



////////////////////////////////////////////////////////////////////
// Other tree


class CSeq_id_Other_Tree : public CSeq_id_Textseq_Tree
{
protected:
    virtual bool x_Check(const CSeq_id& id) const;
    virtual const CTextseq_id& x_Get(const CSeq_id& id) const;
};


bool CSeq_id_Other_Tree::x_Check(const CSeq_id& id) const
{
    _ASSERT(id.IsOther());
    return id.IsOther();
}


const CTextseq_id& CSeq_id_Other_Tree::x_Get(const CSeq_id& id) const
{
    _ASSERT(id.IsOther());
    return id.GetOther();
}



////////////////////////////////////////////////////////////////////
// e_Local tree


class CSeq_id_Local_Tree : public CSeq_id_Which_Tree
{
public:
    virtual TSeq_id_Info FindEqual(const CSeq_id& id) const;
    virtual void FindMatch(const CSeq_id& id,
                           TSeq_id_MatchList& id_list) const;
    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const;
    virtual void AddSeq_idMapping(CSeq_id_Handle& handle);

protected:
    virtual void x_DropHandle(const CSeq_id_Handle& handle);

private:
    typedef map<string, CSeq_id_Handle, seqid_string_less> TByStr;
    typedef map<int, CSeq_id_Handle>    TById;

    TByStr m_ByStr;
    TById  m_ById;
};


TSeq_id_Info CSeq_id_Local_Tree::FindEqual(const CSeq_id& id) const
{
    _ASSERT( id.IsLocal() );
    const CObject_id& oid = id.GetLocal();
    if ( oid.IsStr() ) {
        TByStr::const_iterator it = m_ByStr.find(oid.GetStr());
        if (it != m_ByStr.end()) {
            return
                TSeq_id_Info(&x_GetSeq_id(it->second), x_GetKey(it->second));
        }
    }
    else if ( oid.IsId() ) {
        TById::const_iterator it = m_ById.find(oid.GetId());
        if (it != m_ById.end()) {
            return
                TSeq_id_Info(&x_GetSeq_id(it->second), x_GetKey(it->second));
        }
    }
    // Not found
    return TSeq_id_Info(0, 0);
}


void CSeq_id_Local_Tree::FindMatch(const CSeq_id& id,
                                   TSeq_id_MatchList& id_list) const
{
    // Only one entry can match each id
    id_list.push_back(FindEqual(id));
}


void CSeq_id_Local_Tree::FindMatchStr(string sid,
                                      TSeq_id_MatchList& id_list) const
{
    // In any case search in strings
    TByStr::const_iterator str_it = m_ByStr.find(sid);
    if (str_it != m_ByStr.end()) {
        id_list.push_back(TSeq_id_Info(
            &x_GetSeq_id(str_it->second), x_GetKey(str_it->second)));
    }
    try {
        int value = NStr::StringToInt(sid);
        TById::const_iterator int_it = m_ById.find(value);
        if (int_it == m_ById.end())
            return;
        id_list.push_back(TSeq_id_Info(
            &x_GetSeq_id(int_it->second), x_GetKey(int_it->second)));
    }
    catch (runtime_error) {
        // Not an integer value
        return;
    }
}


void CSeq_id_Local_Tree::AddSeq_idMapping(CSeq_id_Handle& handle)
{
    const CSeq_id& id = x_GetSeq_id(handle);
    _ASSERT( id.IsLocal() );
    if ( id.GetLocal().IsStr() ) {
        _ASSERT(m_ByStr.find(id.GetLocal().GetStr()) == m_ByStr.end());
        m_ByStr[id.GetLocal().GetStr()] = handle;
    }
    else if ( id.GetLocal().IsId() ) {
        _ASSERT(m_ById.find(id.GetLocal().GetId()) == m_ById.end());
        m_ById[id.GetLocal().GetId()] = handle;
    }
    else {
        throw runtime_error("Can not create index for an empty local seq-id");
    }
    x_AddToKeyMap(handle);
}


void CSeq_id_Local_Tree::x_DropHandle(const CSeq_id_Handle& handle)
{
    const CSeq_id& id = x_GetSeq_id(handle);
    _ASSERT( id.IsLocal() );
    if ( id.GetLocal().IsStr() ) {
        TByStr::iterator it = m_ByStr.find(id.GetLocal().GetStr());
        if (it != m_ByStr.end()) {
            _ASSERT(it->second == handle);
            m_ByStr.erase(it);
        }
    }
    else if ( id.GetLocal().IsId() ) {
        TById::iterator it = m_ById.find(id.GetLocal().GetId());
        if (it != m_ById.end()) {
            _ASSERT(it->second == handle);
            m_ById.erase(it);
        }
    }
}



////////////////////////////////////////////////////////////////////
// e_General tree


class CSeq_id_General_Tree : public CSeq_id_Which_Tree
{
public:
    virtual TSeq_id_Info FindEqual(const CSeq_id& id) const;
    virtual void FindMatch(const CSeq_id& id,
                           TSeq_id_MatchList& id_list) const;
    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const;
    virtual void AddSeq_idMapping(CSeq_id_Handle& handle);

protected:
    virtual void x_DropHandle(const CSeq_id_Handle& handle);

private:
    struct STagMap {
    public:
        typedef map<string, CSeq_id_Handle, seqid_string_less> TByStr;
        typedef map<int, CSeq_id_Handle>    TById;

        TByStr m_ByStr;
        TById  m_ById;
    };
    typedef map<string, STagMap, seqid_string_less> TDbMap;

    TDbMap m_DbMap;
};


TSeq_id_Info CSeq_id_General_Tree::FindEqual(const CSeq_id& id) const
{
    _ASSERT( id.IsGeneral() );
    const CObject_id& oid = id.GetGeneral().GetTag();
    TDbMap::const_iterator db = m_DbMap.find(id.GetGeneral().GetDb());
    if (db == m_DbMap.end())
        return TSeq_id_Info(0, 0);
    const STagMap& tm = db->second;
    if ( oid.IsStr() ) {
        STagMap::TByStr::const_iterator it = tm.m_ByStr.find(oid.GetStr());
        if (it != tm.m_ByStr.end()) {
            return TSeq_id_Info
                (&x_GetSeq_id(it->second), x_GetKey(it->second));
        }
    }
    else if ( oid.IsId() ) {
        STagMap::TById::const_iterator it = tm.m_ById.find(oid.GetId());
        if (it != tm.m_ById.end()) {
            return TSeq_id_Info
                (&x_GetSeq_id(it->second), x_GetKey(it->second));
        }
    }
    // Not found
    return TSeq_id_Info(0, 0);
}


void CSeq_id_General_Tree::FindMatch(const CSeq_id& id,
                                     TSeq_id_MatchList& id_list) const
{
    id_list.push_back(FindEqual(id));
}


void CSeq_id_General_Tree::FindMatchStr(string sid,
                                        TSeq_id_MatchList& id_list) const
{
    iterate(TDbMap, db_it, m_DbMap) {
        // In any case search in strings
        STagMap::TByStr::const_iterator str_it =
            db_it->second.m_ByStr.find(sid);
        if (str_it != db_it->second.m_ByStr.end()) {
            id_list.push_back(TSeq_id_Info(
                &x_GetSeq_id(str_it->second),
                x_GetKey(str_it->second)));
        }
        try {
            int value = NStr::StringToInt(sid);
            STagMap::TById::const_iterator int_it =
                db_it->second.m_ById.find(value);
            if (int_it == db_it->second.m_ById.end())
                return;
            id_list.push_back(TSeq_id_Info(
                &x_GetSeq_id(int_it->second),
                x_GetKey(int_it->second)));
        }
        catch (runtime_error) {
            // Not an integer value
            return;
        }
    }
}


void CSeq_id_General_Tree::AddSeq_idMapping(CSeq_id_Handle& handle)
{
    const CSeq_id& id = x_GetSeq_id(handle);
    _ASSERT( id.IsGeneral() );
    STagMap& tm = m_DbMap[id.GetGeneral().GetDb()];
    if ( id.GetGeneral().GetTag().IsStr() ) {
        _ASSERT(tm.m_ByStr.find(id.GetGeneral().GetTag().GetStr()) ==
            tm.m_ByStr.end());
        tm.m_ByStr[id.GetGeneral().GetTag().GetStr()] = handle;
    }
    else if ( id.GetGeneral().GetTag().IsId() ) {
        _ASSERT(tm.m_ById.find(id.GetGeneral().GetTag().GetId()) ==
            tm.m_ById.end());
        tm.m_ById[id.GetGeneral().GetTag().GetId()] = handle;
    }
    else {
        throw runtime_error("Can not create index for an empty db-tag");
    }
    x_AddToKeyMap(handle);
}


void CSeq_id_General_Tree::x_DropHandle(const CSeq_id_Handle& handle)
{
    const CSeq_id& id = x_GetSeq_id(handle);
    _ASSERT( id.IsGeneral() );
    TDbMap::iterator db_it = m_DbMap.find(id.GetGeneral().GetDb());
    _ASSERT(db_it != m_DbMap.end());
    STagMap& tm = db_it->second;
    if ( id.GetGeneral().GetTag().IsStr() ) {
        STagMap::TByStr::iterator it =
            tm.m_ByStr.find(id.GetGeneral().GetTag().GetStr());
        if (it != tm.m_ByStr.end()) {
            _ASSERT(it->second == handle);
            tm.m_ByStr.erase(it);
        }
    }
    else if ( id.GetGeneral().GetTag().IsId() ) {
        STagMap::TById::iterator it =
            tm.m_ById.find(id.GetGeneral().GetTag().GetId());
        if (it != tm.m_ById.end()) {
            _ASSERT(it->second == handle);
            tm.m_ById.erase(it);
        }
    }
    if (tm.m_ByStr.empty()  &&  tm.m_ById.empty())
        m_DbMap.erase(db_it);
}



////////////////////////////////////////////////////////////////////
// e_Giim tree


class CSeq_id_Giim_Tree : public CSeq_id_Which_Tree
{
public:
    virtual TSeq_id_Info FindEqual(const CSeq_id& id) const;
    virtual void FindMatch(const CSeq_id& id,
                           TSeq_id_MatchList& id_list) const;
    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const;
    virtual void AddSeq_idMapping(CSeq_id_Handle& handle);

protected:
    virtual void x_DropHandle(const CSeq_id_Handle& handle);

private:
    // 2-level indexing: first by Id, second by Db+Release
    typedef list<CSeq_id_Handle> TGiimList;
    typedef map<int, TGiimList>  TIdMap;

    TIdMap m_IdMap;
};


TSeq_id_Info CSeq_id_Giim_Tree::FindEqual(const CSeq_id& id) const
{
    _ASSERT( id.IsGiim() );
    const CGiimport_id& gid = id.GetGiim();
    TIdMap::const_iterator id_it = m_IdMap.find(gid.GetId());
    if (id_it == m_IdMap.end())
        return TSeq_id_Info(0, 0);
    iterate(TGiimList, dbr_it, id_it->second) {
        const CGiimport_id& gid2 = x_GetSeq_id(*dbr_it).GetGiim();
        // Both Db and Release must be equal
        if ( !SerialEquals<CGiimport_id>(gid, gid2) ) {
            return TSeq_id_Info
                (&x_GetSeq_id(*dbr_it), x_GetKey(*dbr_it));
        }
    }
    // Not found
    return TSeq_id_Info(0, 0);
}


void CSeq_id_Giim_Tree::FindMatch(const CSeq_id& id,
                                  TSeq_id_MatchList& id_list) const
{
    id_list.push_back(FindEqual(id));
}


void CSeq_id_Giim_Tree::FindMatchStr(string sid,
                                     TSeq_id_MatchList& id_list) const
{
    try {
        int value = NStr::StringToInt(sid);
        TIdMap::const_iterator it = m_IdMap.find(value);
        if (it == m_IdMap.end())
            return;
        iterate(TGiimList, git, it->second) {
            id_list.push_back(TSeq_id_Info(
                &x_GetSeq_id(*git), x_GetKey(*git)));
        }
    }
    catch (runtime_error) {
        // Not an integer value
        return;
    }
}


void CSeq_id_Giim_Tree::AddSeq_idMapping(CSeq_id_Handle& handle)
{
    const CSeq_id& id = x_GetSeq_id(handle);
    _ASSERT( id.IsGiim() );
    TGiimList& giims = m_IdMap[id.GetGiim().GetId()];
    iterate(TGiimList, git, giims) {
        _ASSERT(!SerialEquals<CGiimport_id>(id.GetGiim(),
            x_GetSeq_id(*git).GetGiim()));
    }
    giims.push_back(handle);
    x_AddToKeyMap(handle);
}


void CSeq_id_Giim_Tree::x_DropHandle(const CSeq_id_Handle& handle)
{
    const CSeq_id& id = x_GetSeq_id(handle);
    _ASSERT( id.IsGiim() );
    TIdMap::iterator id_it = m_IdMap.find(id.GetGiim().GetId());
    _ASSERT(id_it != m_IdMap.end());
    TGiimList& giims = id_it->second;
    non_const_iterate(TGiimList, dbr_it, giims) {
        if (x_GetKey(*dbr_it) == x_GetKey(handle)) {
            giims.erase(dbr_it);
            break;
        }
    }
    if ( giims.empty() )
        m_IdMap.erase(id_it);
}



////////////////////////////////////////////////////////////////////
// e_Patent tree


class CSeq_id_Patent_Tree : public CSeq_id_Which_Tree
{
public:
    virtual TSeq_id_Info FindEqual(const CSeq_id& id) const;
    virtual void FindMatch(const CSeq_id& id,
                           TSeq_id_MatchList& id_list) const;
    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const;
    virtual void AddSeq_idMapping(CSeq_id_Handle& handle);

protected:
    virtual void x_DropHandle(const CSeq_id_Handle& handle);

private:
    // 3-level indexing: country, (number|app_number), seqid.
    // Ignoring patent doc-type in indexing.
    struct SPat_idMap {
        typedef map<int, CSeq_id_Handle> TBySeqid;
        typedef map<string, TBySeqid, seqid_string_less>    TByNumber; // or by App_number

        TByNumber m_ByNumber;
        TByNumber m_ByApp_number;
    };
    typedef map<string, SPat_idMap, seqid_string_less> TByCountry;

    TByCountry m_CountryMap;
};


TSeq_id_Info CSeq_id_Patent_Tree::FindEqual(const CSeq_id& id) const
{
    _ASSERT( id.IsPatent() );
    const CPatent_seq_id& pid = id.GetPatent();
    TByCountry::const_iterator country_it =
        m_CountryMap.find(pid.GetCit().GetCountry());
    if (country_it == m_CountryMap.end())
        return TSeq_id_Info(0, 0);
    const SPat_idMap& pats = country_it->second;
    if ( pid.GetCit().GetId().IsNumber() ) {
        SPat_idMap::TByNumber::const_iterator num_it =
            pats.m_ByNumber.find(pid.GetCit().GetId().GetNumber());
        if (num_it == pats.m_ByNumber.end())
            return TSeq_id_Info(0, 0);
        SPat_idMap::TBySeqid::const_iterator seqid_it =
            num_it->second.find(pid.GetSeqid());
        if (seqid_it != num_it->second.end()) {
            return TSeq_id_Info(&x_GetSeq_id(seqid_it->second),
                x_GetKey(seqid_it->second));
        }
    }
    else if ( pid.GetCit().GetId().IsApp_number() ) {
        SPat_idMap::TByNumber::const_iterator app_it =
            pats.m_ByApp_number.find(pid.GetCit().GetId().GetApp_number());
        if (app_it == pats.m_ByApp_number.end())
            return TSeq_id_Info(0, 0);
        SPat_idMap::TBySeqid::const_iterator seqid_it =
            app_it->second.find(pid.GetSeqid());
        if (seqid_it != app_it->second.end()) {
            return TSeq_id_Info(&x_GetSeq_id(seqid_it->second),
                x_GetKey(seqid_it->second));
        }
    }
    // Not found
    return TSeq_id_Info(0, 0);
}

void CSeq_id_Patent_Tree::FindMatch(const CSeq_id& id,
                                    TSeq_id_MatchList& id_list) const
{
    id_list.push_back(FindEqual(id));
}


void CSeq_id_Patent_Tree::FindMatchStr(string sid,
                                       TSeq_id_MatchList& id_list) const
{
    iterate(TByCountry, cit, m_CountryMap) {
        SPat_idMap::TByNumber::const_iterator nit =
            cit->second.m_ByNumber.find(sid);
        if (nit != cit->second.m_ByNumber.end()) {
            iterate(SPat_idMap::TBySeqid, iit, nit->second) {
                id_list.push_back(TSeq_id_Info(
                    &x_GetSeq_id(iit->second), x_GetKey(iit->second)));
            }
        }
        SPat_idMap::TByNumber::const_iterator ait =
            cit->second.m_ByApp_number.find(sid);
        if (ait != cit->second.m_ByApp_number.end()) {
            iterate(SPat_idMap::TBySeqid, iit, nit->second) {
                id_list.push_back(TSeq_id_Info(
                    &x_GetSeq_id(iit->second), x_GetKey(iit->second)));
            }
        }
    }
}


void CSeq_id_Patent_Tree::AddSeq_idMapping(CSeq_id_Handle& handle)
{
    const CSeq_id& id = x_GetSeq_id(handle);
    _ASSERT( id.IsPatent() );
    SPat_idMap& country = m_CountryMap[id.GetPatent().GetCit().GetCountry()];
    if ( id.GetPatent().GetCit().GetId().IsNumber() ) {
        SPat_idMap::TBySeqid& num = country.m_ByNumber[
            id.GetPatent().GetCit().GetId().GetNumber()];
        _ASSERT(num.find(id.GetPatent().GetSeqid()) == num.end());
        num[id.GetPatent().GetSeqid()] = handle;
    }
    else if ( id.GetPatent().GetCit().GetId().IsApp_number() ) {
        SPat_idMap::TBySeqid& app = country.m_ByApp_number[
            id.GetPatent().GetCit().GetId().GetApp_number()];
        _ASSERT(app.find(id.GetPatent().GetSeqid()) == app.end());
        app[id.GetPatent().GetSeqid()] = handle;
    }
    else {
        // Can not index empty patent number
        return;
    }
    x_AddToKeyMap(handle);
}


void CSeq_id_Patent_Tree::x_DropHandle(const CSeq_id_Handle& handle)
{
    const CSeq_id& id = x_GetSeq_id(handle);
    _ASSERT( id.IsPatent() );
    const CPatent_seq_id& pid = id.GetPatent();
    TByCountry::iterator country_it =
        m_CountryMap.find(pid.GetCit().GetCountry());
    _ASSERT(country_it != m_CountryMap.end());
    SPat_idMap& pats = country_it->second;
    if ( pid.GetCit().GetId().IsNumber() ) {
        SPat_idMap::TByNumber::iterator num_it =
            pats.m_ByNumber.find(pid.GetCit().GetId().GetNumber());
        _ASSERT(num_it != pats.m_ByNumber.end());
        SPat_idMap::TBySeqid::iterator seqid_it =
            num_it->second.find(pid.GetSeqid());
        _ASSERT(seqid_it != num_it->second.end());
        _ASSERT(seqid_it->second == handle);
        num_it->second.erase(seqid_it);
        if ( num_it->second.empty() )
            pats.m_ByNumber.erase(num_it);
    }
    else if ( pid.GetCit().GetId().IsApp_number() ) {
        SPat_idMap::TByNumber::iterator app_it =
            pats.m_ByApp_number.find(pid.GetCit().GetId().GetApp_number());
        _ASSERT(app_it == pats.m_ByApp_number.end());
        SPat_idMap::TBySeqid::iterator seqid_it =
            app_it->second.find(pid.GetSeqid());
        _ASSERT(seqid_it != app_it->second.end());
        _ASSERT(seqid_it->second == handle);
        app_it->second.erase(seqid_it);
        if ( app_it->second.empty() )
            pats.m_ByNumber.erase(app_it);
    }
    if (country_it->second.m_ByNumber.empty()  &&
        country_it->second.m_ByApp_number.empty())
        m_CountryMap.erase(country_it);
}



////////////////////////////////////////////////////////////////////
// e_PDB tree


class CSeq_id_PDB_Tree : public CSeq_id_Which_Tree
{
public:
    virtual TSeq_id_Info FindEqual(const CSeq_id& id) const;
    virtual void FindMatch(const CSeq_id& id,
                           TSeq_id_MatchList& id_list) const;
    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const;
    virtual void AddSeq_idMapping(CSeq_id_Handle& handle);

protected:
    virtual void x_DropHandle(const CSeq_id_Handle& handle);

private:
    // No index by chain or date - too complicated
    typedef list<CSeq_id_Handle>  TSubMolList;
    typedef map<string, TSubMolList, seqid_string_less> TMolMap;

    TMolMap m_MolMap;
};


TSeq_id_Info CSeq_id_PDB_Tree::FindEqual(const CSeq_id& id) const
{
    _ASSERT( id.IsPdb() );
    const CPDB_seq_id& pid = id.GetPdb();
    TMolMap::const_iterator mol_it = m_MolMap.find(pid.GetMol().Get());
    if (mol_it == m_MolMap.end())
        return TSeq_id_Info(0, 0);
    iterate(TSubMolList, it, mol_it->second) {
        if (SerialEquals<CPDB_seq_id>(pid, x_GetSeq_id(*it).GetPdb())) {
            return TSeq_id_Info(&x_GetSeq_id(*it), x_GetKey(*it));
        }
    }
    // Not found
    return TSeq_id_Info(0, 0);
}

void CSeq_id_PDB_Tree::FindMatch(const CSeq_id& id,
                                    TSeq_id_MatchList& id_list) const
{
    _ASSERT( id.IsPdb() );
    const CPDB_seq_id& pid = id.GetPdb();
    TMolMap::const_iterator mol_it = m_MolMap.find(pid.GetMol().Get());
    if (mol_it == m_MolMap.end())
        return;
    iterate(TSubMolList, it, mol_it->second) {
        const CPDB_seq_id& pid2 = x_GetSeq_id(*it).GetPdb();
        // Ignore date and chain if not set in id
        if ( pid.IsSetChain() ) {
            if ( !pid2.IsSetChain()  ||  pid.GetChain() != pid2.GetChain())
                continue;
        }
        if ( pid.IsSetRel() ) {
            if ( !pid2.IsSetRel()  ||
                !SerialEquals<CDate>(pid.GetRel(), pid2.GetRel()) )
                continue;
        }
        id_list.push_back(TSeq_id_Info(&x_GetSeq_id(*it), x_GetKey(*it)));
    }
}


void CSeq_id_PDB_Tree::FindMatchStr(string sid,
                                    TSeq_id_MatchList& id_list) const
{
    TMolMap::const_iterator mit = m_MolMap.find(sid);
    if (mit == m_MolMap.end())
        return;
    iterate(TSubMolList, sub_it, mit->second) {
        id_list.push_back(TSeq_id_Info(
            &x_GetSeq_id(*sub_it), x_GetKey(*sub_it)));
    }
}


void CSeq_id_PDB_Tree::AddSeq_idMapping(CSeq_id_Handle& handle)
{
    const CSeq_id& id = x_GetSeq_id(handle);
    _ASSERT( id.IsPdb() );
// this is an attempt to follow the undocumented rules of PDB
// ("documented" as code written elsewhere)
    string skey = id.GetPdb().GetMol().Get();
    switch (char chain = (char)id.GetPdb().GetChain()) {
    case '\0': skey += " ";   break;
    case '|':  skey += "VB";  break;
    default:   skey += chain; break;
    }
    TSubMolList& sub = m_MolMap[skey];
    iterate(TSubMolList, sub_it, sub) {
        _ASSERT(!SerialEquals<CPDB_seq_id>(
            x_GetSeq_id(handle).GetPdb(), x_GetSeq_id(*sub_it).GetPdb()));
    }
    sub.push_back(handle);
    x_AddToKeyMap(handle);
}


void CSeq_id_PDB_Tree::x_DropHandle(const CSeq_id_Handle& handle)
{
    const CSeq_id& id = x_GetSeq_id(handle);
    _ASSERT( id.IsPdb() );
    const CPDB_seq_id& pid = id.GetPdb();
    TMolMap::iterator mol_it = m_MolMap.find(pid.GetMol().Get());
    _ASSERT(mol_it == m_MolMap.end());
    non_const_iterate(TSubMolList, it, mol_it->second) {
        if (*it == handle) {
            _ASSERT(SerialEquals<CPDB_seq_id>(pid, x_GetSeq_id(*it).GetPdb()));
            mol_it->second.erase(it);
            break;
        }
    }
    if ( mol_it->second.empty() )
        m_MolMap.erase(mol_it);
}



////////////////////////////////////////////////////////////////////
//
//  CSeq_id_Mapper::
//


const size_t kKeyUsageTableSegmentSize =
    numeric_limits<TSeq_id_Key>().max() / kKeyUsageTableSize;

CSeq_id_Mapper::CSeq_id_Mapper(void)
    : m_NextKey(0)
{
    for (int i = 0; i < kKeyUsageTableSize; i++)
        m_KeyUsageTable[i] = 0;
// same order as in seq-id definition, see seqloc.asn
    m_IdMap[CSeq_id::e_Local] = CRef<CSeq_id_Which_Tree>
        (new CSeq_id_Local_Tree);
    m_IdMap[CSeq_id::e_Gibbsq] = CRef<CSeq_id_Which_Tree>
        (new CSeq_id_Gibbsq_Tree);
    m_IdMap[CSeq_id::e_Gibbmt] = CRef<CSeq_id_Which_Tree>
        (new CSeq_id_Gibbmt_Tree);
    m_IdMap[CSeq_id::e_Giim] = CRef<CSeq_id_Which_Tree>
        (new CSeq_id_Giim_Tree);
    // These three types share the same accessions space
    CRef<CSeq_id_Which_Tree> gb = new CSeq_id_GB_Tree;
    m_IdMap[CSeq_id::e_Genbank] = gb;
    m_IdMap[CSeq_id::e_Embl] = gb;
    m_IdMap[CSeq_id::e_Ddbj] = gb;
    m_IdMap[CSeq_id::e_Pir] = CRef<CSeq_id_Which_Tree>
        (new CSeq_id_Pir_Tree);
    m_IdMap[CSeq_id::e_Swissprot] = CRef<CSeq_id_Which_Tree>
        (new CSeq_id_Swissprot_Tree);
    m_IdMap[CSeq_id::e_Patent] = CRef<CSeq_id_Which_Tree>
        (new CSeq_id_Patent_Tree);
    m_IdMap[CSeq_id::e_Other] = CRef<CSeq_id_Which_Tree>
        (new CSeq_id_Other_Tree);
    m_IdMap[CSeq_id::e_General] = CRef<CSeq_id_Which_Tree>
        (new CSeq_id_General_Tree);
    m_IdMap[CSeq_id::e_Gi] = CRef<CSeq_id_Which_Tree>
        (new CSeq_id_Gi_Tree);
//// see above    m_IdMap[CSeq_id::e_Ddbj] = gb;
    m_IdMap[CSeq_id::e_Prf] = CRef<CSeq_id_Which_Tree>
        (new CSeq_id_Prf_Tree);
    m_IdMap[CSeq_id::e_Pdb] = CRef<CSeq_id_Which_Tree>
        (new CSeq_id_PDB_Tree);
    m_IdMap[CSeq_id::e_Tpg] = CRef<CSeq_id_Which_Tree>
        (new CSeq_id_Tpg_Tree);
    m_IdMap[CSeq_id::e_Tpe] = CRef<CSeq_id_Which_Tree>
        (new CSeq_id_Tpe_Tree);
    m_IdMap[CSeq_id::e_Tpd] = CRef<CSeq_id_Which_Tree>
        (new CSeq_id_Tpd_Tree);
}


CSeq_id_Handle CSeq_id_Mapper::GetHandle(const CSeq_id& id,
                                         bool do_not_create)
{
    TIdMap::iterator map_it = m_IdMap.find(id.Which());
    _ASSERT(map_it != m_IdMap.end()  &&  map_it->second.GetPointer());
    TSeq_id_Info info = map_it->second->FindEqual(id);
    // If found, return valid handle
    if ( !info.first.Empty() )
        return CSeq_id_Handle(*info.first, info.second);
    // Not found - should we create a new handle or return an empty one?
    if ( do_not_create )
        return CSeq_id_Handle();
    CSeq_id_Handle new_handle(id, GetNextKey());
    map_it->second->AddSeq_idMapping(new_handle);
    return new_handle;
}


void CSeq_id_Mapper::GetMatchingHandles(const CSeq_id& id,
                                        TSeq_id_HandleSet& h_set)
{
    TIdMap::iterator map_it = m_IdMap.find(id.Which());
    _ASSERT(map_it != m_IdMap.end()  &&  map_it->second.GetPointer());
    CSeq_id_Which_Tree::TSeq_id_MatchList m_list;
    map_it->second->FindMatch(id, m_list);
    iterate(CSeq_id_Which_Tree::TSeq_id_MatchList, it, m_list) {
        h_set.insert(CSeq_id_Handle(*it->first, it->second));
    }
}


void CSeq_id_Mapper::GetMatchingHandlesStr(string sid,
                                        TSeq_id_HandleSet& h_set)
{
    if (sid.find('|') != string::npos) {
        throw runtime_error(
            "CSeq_id_Mapper::GetMatchingHandlesStr() -- symbol \'|\'"
            " is not supported here");
    }
    CSeq_id_Which_Tree::TSeq_id_MatchList m_list;
    iterate(TIdMap, map_it, m_IdMap) {
        map_it->second->FindMatchStr(sid, m_list);
    }
    iterate(CSeq_id_Which_Tree::TSeq_id_MatchList, it, m_list) {
        h_set.insert(CSeq_id_Handle(*it->first, it->second));
    }
}


const CSeq_id& CSeq_id_Mapper::GetSeq_id(const CSeq_id_Handle& handle)
{
    return *handle.m_SeqId;
}


void CSeq_id_Mapper::AddHandleReference(const CSeq_id_Handle& handle)
{
    m_KeyUsageTable[handle.m_Value / kKeyUsageTableSegmentSize]++;
}


void CSeq_id_Mapper::ReleaseHandleReference(const CSeq_id_Handle& handle)
{
    TSeq_id_Key seg = handle.m_Value / kKeyUsageTableSegmentSize;
    _ASSERT(m_KeyUsageTable[seg] > 0);
    if (--m_KeyUsageTable[seg] == 0) {
        // Drop the handles segment from the proper seq-id trees
        non_const_iterate(TIdMap, it, m_IdMap) {
            it->second->DropKeysRange(seg*kKeyUsageTableSegmentSize,
                (seg+1)*kKeyUsageTableSegmentSize-1);
        }
    }
}


TSeq_id_Key CSeq_id_Mapper::GetNextKey(void)
{
    if (m_NextKey < kKeyUsageTableSegmentSize*kKeyUsageTableSize)
        m_NextKey++;
    else
        m_NextKey = 0;
    if (m_NextKey % kKeyUsageTableSegmentSize)
        return m_NextKey;
    // Crossing segment boundary - check if the segment is free
    if (m_KeyUsageTable[m_NextKey / kKeyUsageTableSegmentSize] == 0)
        return m_NextKey; // free segment, start using it
    // Find the first free segment
    for (int i = 0; i < kKeyUsageTableSize; i++) {
        if (m_KeyUsageTable[i] == 0) {
            m_NextKey = i*kKeyUsageTableSegmentSize;
            return m_NextKey;
        }
    }
    throw runtime_error(
        "CSeq_id_Mapper::GetNextKey() -- can not find free seq-id key");
}



END_SCOPE(objects)
END_NCBI_SCOPE
