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
 *`
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   Privae classes and definition for the validator
 *   .......
 *
 */

#ifndef VALIDATOR___CACHE_IMPL__HPP
#define VALIDATOR___CACHE_IMPL__HPP

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)

// =============================================================================
//                            Caching classes
// =============================================================================

// for convenience
typedef CValidator::CCache CCache;

class CBioSourceKind
{
public:
    CBioSourceKind(const CBioSource& bsrc);
    CBioSourceKind();
    CBioSourceKind& operator=(const CBioSource& bsrc);

    bool IsOrganismBacteria() const;
    bool IsOrganismEukaryote() const;
    bool IsOrganismArchaea() const;
    bool IsSourceOrganelle() const;
    bool IsGood() const;
    void SetNotGood();
protected:
    bool m_bacteria : 1;
    bool m_eukaryote : 1;
    bool m_archaea : 1;
    bool m_organelle : 1;
};

inline
CBioSourceKind::CBioSourceKind()
{
    SetNotGood();
}

inline
CBioSourceKind::CBioSourceKind(const CBioSource& bsrc)
{
    operator=(bsrc);
}

inline
void CBioSourceKind::SetNotGood()
{
    m_bacteria = false;
    m_eukaryote = false;
    m_archaea = false;
    m_organelle = false;
}

inline
bool CBioSourceKind::IsGood() const
{
    return m_bacteria | m_eukaryote | m_archaea | m_organelle;
}

inline
bool CBioSourceKind::IsOrganismBacteria() const
{
    return m_bacteria;
}
inline
bool CBioSourceKind::IsOrganismEukaryote() const
{
    return m_eukaryote;
}
inline
bool CBioSourceKind::IsOrganismArchaea() const
{
    return m_archaea;
}
inline
bool CBioSourceKind::IsSourceOrganelle() const
{
    return m_organelle;
}

/// Cache various information for one validation run.
///
/// This is easy because the validator won't
/// change the contents of what it's validating.
///
/// Not thread-safe within one CCacheImpl, but it's
/// fine for each thread to have its own CCacheImpl.
class NCBI_VALIDATOR_EXPORT CValidator::CCacheImpl
{
public:

    //////////
    // cache information about pubdescs
    class NCBI_VALIDATOR_EXPORT CPubdescInfo : public CObject
    {
    public:
        vector<TEntrezId> m_pmids;
        vector<TEntrezId> m_muids;
        vector<int> m_serials;
        vector<string> m_published_labels;
        vector<string> m_unpublished_labels;
    };
    const CPubdescInfo & GetPubdescToInfo(CConstRef<CPubdesc> pub);

    // TODO: add a function for accessing the cache that fills it in if it's empty

    //////////
    // cache the features on each bioseq
    struct SFeatKey {
        SFeatKey(
        CSeqFeatData::E_Choice feat_type_arg,
            CSeqFeatData::ESubtype feat_subtype_arg,
            const CBioseq_Handle & bioseq_h_arg)
            : feat_type(feat_type_arg),
              feat_subtype(feat_subtype_arg),
              bioseq_h(bioseq_h_arg) { }

        // use AutoPtr so that NULL can represent
        // the concept of "doesn't matter"
        CSeqFeatData::E_Choice feat_type;
        CSeqFeatData::ESubtype feat_subtype;
        CBioseq_Handle bioseq_h;

        bool operator<(const SFeatKey & rhs) const;
        bool operator==(const SFeatKey & rhs) const;
    };
    typedef std::vector<CMappedFeat> TFeatValue;

    // user should go through here since it loads the cache if it's empty
    static const CSeqFeatData::E_Choice kAnyFeatType;
    static const CSeqFeatData::ESubtype kAnyFeatSubtype;
    const TFeatValue & GetFeatFromCache(const SFeatKey & featKey);

    // user should go through here since it loads the cache if it's empty, but:
    // slower than GetFeatFromCache because it iterates over all the
    // features on the bioseq, so try to avoid when possible.
    // All featkeys must point to the same bioseq.
    AutoPtr<TFeatValue> GetFeatFromCacheMulti(
        const vector<SFeatKey> &featKeys);

    // this should be used for any kind of "string -> feats" mapping
    enum EFeatKeyStr{
        eFeatKeyStr_Label,
        eFeatKeyStr_LocusTag,
        // more could be added
    };
    struct SFeatStrKey
    {
        // set bioseq to kAnyBioseq to get data on all bioseqs
        SFeatStrKey(EFeatKeyStr eFeatKeyStr,
                  const CBioseq_Handle & bioseq,
                  const string & feat_str) :
            m_eFeatKeyStr(eFeatKeyStr), m_bioseq(bioseq),
            m_feat_str(feat_str) { }

        EFeatKeyStr m_eFeatKeyStr;
        CBioseq_Handle m_bioseq;
        string m_feat_str;

        bool operator<(const SFeatStrKey & rhs) const;
        bool operator==(const SFeatStrKey & rhs) const;
    };

    // The "tse" is used to load the cache if it's empty
    // (for now just indexes genes, but more may be added in the future)
    static const CBioseq_Handle kAnyBioseq;
    const TFeatValue & GetFeatStrKeyToFeats(
        const SFeatStrKey & feat_str_key, const CTSE_Handle & tse);

    //////////
    // cache the bioseq(s) that each feature is on

    typedef CMappedFeat TFeatToBioseqKey;
    typedef set<CBioseq_Handle> TFeatToBioseqValue;

    // user should go through here since it loads the cache if it's empty
    const TFeatToBioseqValue & GetBioseqsOfFeatCache(
        const TFeatToBioseqKey & feat_to_bioseq_key,
        const CTSE_Handle & tse);

    //////////
    // cache the bioseq that each CSeq-id points to, since the
    // native implementation is too slow for our purposes
    typedef CConstRef<CSeq_id> TIdToBioseqKey;
    typedef CBioseq_Handle TIdToBioseqValue;

    const TIdToBioseqValue & GetIdToBioseq(
        const TIdToBioseqKey & key, const CTSE_Handle & tse);

    // feel free to add more types of caching.  Maybe bioseqs?  Bioseq sets?
    // etc.

    ////////////////////
    // helper functions on top of the basic cache functions above
    ////////////////////

    CBioseq_Handle GetBioseqHandleFromLocation(
        CScope *scope, const CSeq_loc& loc, const CTSE_Handle & tse);

    static const CTSE_Handle kEmptyTSEHandle;

    void Clear();

private:
    // values that do not correspond to any legitimate value
    // of the given enum to let us express the concept of "any"
    static const CBioseq_Handle kEmptyBioseqHandle;
    static const TFeatValue kEmptyFeatValue;

    typedef map<CConstRef<CPubdesc>,
                CConstRef<CPubdescInfo> > TPubdescCache;
    TPubdescCache m_pubdescCache;

    typedef map<SFeatKey, TFeatValue> TFeatCache;
    TFeatCache m_featCache;

    typedef map<SFeatStrKey, TFeatValue> TFeatStrKeyToFeatsCache;
    TFeatStrKeyToFeatsCache m_featStrKeyToFeatsCache;

    typedef map<TFeatToBioseqKey, TFeatToBioseqValue> TFeatToBioseqCache;
    TFeatToBioseqCache m_featToBioseqCache;

    typedef map<TIdToBioseqKey, TIdToBioseqValue> TIdToBioseqCache;
    TIdToBioseqCache m_IdToBioseqCache;
};

typedef CValidator::CCacheImpl CCacheImpl;


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___CACHE_IMPL__HPP */
