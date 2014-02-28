#ifndef NCBI_TAXON1_CACHE_HPP
#define NCBI_TAXON1_CACHE_HPP

/* $Id$
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
 * Author:  Vladimir Soussov, Michael Domrachev
 *
 * File Description:
 *     NCBI Taxonomy information retreival library caching mechanism
 *
 */

#include <objects/taxon1/taxon1.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include "ctreecont.hpp"

#include <map>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


class CTaxon1Node;


class COrgRefCache
{
public:
    COrgRefCache( CTaxon1& host );
    ~COrgRefCache();

    bool Init( unsigned nCapacity = 10 );

    bool Lookup( int tax_id, CTaxon1Node** ppNode );
    bool LookupAndAdd( int tax_id, CTaxon1Node** ppData );
    bool LookupAndInsert( int tax_id, CTaxon1_data** ppData );
    bool LookupAndInsert( int tax_id, CTaxon2_data** ppData );
    bool Lookup( int tax_id, CTaxon1_data** ppData );
    bool Lookup( int tax_id, CTaxon2_data** ppData );

    bool Insert1( CTaxon1Node& node );
    bool Insert2( CTaxon1Node& node );

    // Rank stuff
    const char* GetRankName( int rank );

    int GetSuperkingdomRank() const { return m_nSuperkingdomRank; }
    int GetFamilyRank() const { return m_nFamilyRank; }
    int GetOrderRank() const { return m_nOrderRank; }
    int GetClassRank() const { return m_nClassRank; }
    int GetGenusRank() const { return m_nGenusRank; }
    int GetSubgenusRank() const { return m_nSubgenusRank; }
    int GetSpeciesRank() const { return m_nSpeciesRank; }
    int GetSubspeciesRank() const { return m_nSubspeciesRank; }
    int GetFormaRank() const { return m_nFormaRank; }
    int GetVarietyRank() const { return m_nVarietyRank; }

    const char* GetNameClassName( short nc );
    short GetPreferredCommonNameClass() const { return m_ncPrefCommon; }
    short GetCommonNameClass() const { return m_ncCommon; }
    short GetSynonymNameClass() const { return m_ncSynonym; }
    short GetGBAcronymNameClass() const { return m_ncGBAcronym; }
    short GetGBSynonymNameClass() const { return m_ncGBSynonym; }
    short GetGBAnamorphNameClass() const { return m_ncGBAnamorph; }

    const char* GetDivisionName( short div_id );
    const char* GetDivisionCode( short div_id );
    short GetVirusesDivision() const { return m_divViruses; }
    short GetPhagesDivision() const { return m_divPhages; }

    CTreeCont& GetTree() { return m_tPartTree; }
    const CTreeCont& GetTree() const { return m_tPartTree; }

    void  SetIndexEntry( int id, CTaxon1Node* pNode );

    COrgMod::ESubtype GetSubtypeFromName( string& sName );

private:
    friend class CTaxon1Node;
    friend class CTaxon1;
    struct SCacheEntry {
        friend class CTaxon1Node;
        CRef< CTaxon1_data > m_pTax1;
        CRef< CTaxon2_data > m_pTax2;

        CTaxon1Node*  m_pTreeNode;

        CTaxon1_data* GetData1();
        CTaxon2_data* GetData2();
    };

    CTaxon1&           m_host;

    unsigned           m_nMaxTaxId;
    CTaxon1Node**      m_ppEntries; // index by tax_id
    CTreeCont          m_tPartTree; // Partial tree

    unsigned           m_nCacheCapacity; // Max number of elements in cache
    list<SCacheEntry*> m_lCache; // LRU list

    bool             BuildOrgRef( CTaxon1Node& node, COrg_ref& org,
                                  bool& is_species );
    bool             BuildOrgModifier( CTaxon1Node* pNode,
                                       COrgName& on,
                                       CTaxon1Node* pParent = NULL );
    bool             SetBinomialName( CTaxon1Node& node, COrgName& on );
    bool             SetPartialName( CTaxon1Node& node, COrgName& on );
    // Rank stuff
    int m_nSuperkingdomRank;
    int m_nFamilyRank;
    int m_nOrderRank;
    int m_nClassRank;
    int m_nGenusRank;
    int m_nSubgenusRank;
    int m_nSpeciesRank;
    int m_nSubspeciesRank;
    int m_nFormaRank;
    int m_nVarietyRank;

    typedef map<int, string> TRankMap;
    typedef TRankMap::const_iterator TRankMapCI;
    typedef TRankMap::iterator TRankMapI;

    TRankMap m_rankStorage;

    bool     InitRanks();
    int      FindRankByName( const char* pchName );

    // Name classes stuff
    short m_ncPrefCommon; // now called "genbank common name"
    short m_ncCommon;
    short m_ncSynonym;
    short m_ncGBAcronym;
    short m_ncGBSynonym;
    short m_ncGBAnamorph;

    typedef map<short, string> TNameClassMap;
    typedef TNameClassMap::const_iterator TNameClassMapCI;
    typedef TNameClassMap::iterator TNameClassMapI;
    TNameClassMap m_ncStorage;

    bool     InitNameClasses();
    short    FindNameClassByName( const char* pchName );
    // Division stuff
    short m_divViruses;
    short m_divPhages;
    struct SDivision {
        string m_sCode;
        string m_sName;
    };
    typedef map<short, struct SDivision> TDivisionMap;
    typedef TDivisionMap::const_iterator TDivisionMapCI;
    typedef TDivisionMap::iterator TDivisionMapI;
    TDivisionMap m_divStorage;

    bool     InitDivisions();
    short    FindDivisionByCode( const char* pchCode );

    // forbidden
    COrgRefCache(const COrgRefCache&);
    COrgRefCache& operator=(const COrgRefCache&);
};


class CTaxon1Node : public CTreeContNodeBase, public ITaxon1Node {
public:
    CTaxon1Node( const CRef< CTaxon1_name >& ref )
        : m_ref( ref ), m_cacheEntry( NULL ), m_flags( 0 ) {}
    explicit CTaxon1Node( const CTaxon1Node& node )
        : CTreeContNodeBase(), m_ref( node.m_ref ),
	  m_cacheEntry( NULL ), m_flags( 0 ) {}
    virtual ~CTaxon1Node() {}

    virtual int           GetTaxId() const { return m_ref->GetTaxid(); }
    virtual const string& GetName() const { return m_ref->GetOname(); }
    virtual const string& GetBlastName() const
    { return m_ref->CanGetUname() ? m_ref->GetUname() : kEmptyStr; }
    virtual short         GetRank() const;
    virtual short         GetDivision() const;
    virtual short         GetGC() const;
    virtual short         GetMGC() const;
                       
    virtual bool          IsUncultured() const;
    virtual bool          IsGenBankHidden() const;

    virtual bool          IsRoot() const
    { return CTreeContNodeBase::IsRoot(); }

    COrgRefCache::SCacheEntry* GetEntry() { return m_cacheEntry; }

    bool                  IsJoinTerminal() const
    { return m_flags&mJoinTerm ? true : false; }
    void                  SetJoinTerminal() { m_flags |= mJoinTerm; }
    bool                  IsSubtreeLoaded() const
    { return m_flags&mSubtreeLoaded ? true : false; }
    void                  SetSubtreeLoaded( bool b )
    { if( b ) m_flags |= mSubtreeLoaded; else m_flags &= ~mSubtreeLoaded; }

    CTaxon1Node*          GetParent()
    { return static_cast<CTaxon1Node*>(Parent()); }
private:
    friend class COrgRefCache;
    enum EFlags {
	mJoinTerm      = 0x1,
	mSubtreeLoaded = 0x2
    };

    CRef< CTaxon1_name >       m_ref;

    COrgRefCache::SCacheEntry* m_cacheEntry;
    unsigned                   m_flags;
};

class CTaxTreeConstIterator : public ITreeIterator {
public:
    CTaxTreeConstIterator( CTreeConstIterator* pIt, CTaxon1::EIteratorMode m )
	: m_it( pIt ), m_itMode( m )  {}
    virtual ~CTaxTreeConstIterator() {
	delete m_it;
    }

    virtual CTaxon1::EIteratorMode GetMode() const { return m_itMode; }
    virtual const ITaxon1Node* GetNode() const
    { return CastCI(m_it->GetNode()); }
    const ITaxon1Node* operator->() const { return GetNode(); }
    virtual bool IsLastChild() const;
    virtual bool IsFirstChild() const;
    virtual bool IsTerminal() const;
    // Navigation
    virtual void GoRoot() { m_it->GoRoot(); }
    virtual bool GoParent();
    virtual bool GoChild();
    virtual bool GoSibling();
    virtual bool GoNode(const ITaxon1Node* pNode);
    // move cursor to the nearest common ancestor
    // between node pointed by cursor and the node
    // with given node_id
    virtual bool GoAncestor(const ITaxon1Node* pNode);
    // check if node pointed by cursor
    // is belong to subtree wich root node
    // has given node_id
    virtual bool BelongSubtree(const ITaxon1Node* subtree_root) const;
    // check if node with given node_id belongs
    // to subtree pointed by cursor
    virtual bool AboveNode(const ITaxon1Node* node) const;
protected:
    virtual bool IsVisible( const CTreeContNodeBase* p ) const = 0;
    // Moves m_it to the next visible for this parent
    bool NextVisible( const CTreeContNodeBase* pParent ) const;

    const ITaxon1Node* CastCI( const CTreeContNodeBase* p ) const
    { return static_cast<const ITaxon1Node*>
	  (static_cast<const CTaxon1Node*>(p)); }
    const CTreeContNodeBase* CastIC( const ITaxon1Node* p ) const
    { return static_cast<const CTreeContNodeBase*>
	  (static_cast<const CTaxon1Node*>(p)); }
    mutable CTreeConstIterator* m_it;
    CTaxon1::EIteratorMode      m_itMode;
};

class CFullTreeConstIterator : public CTaxTreeConstIterator {
public:
    CFullTreeConstIterator( CTreeConstIterator* pIt )
	: CTaxTreeConstIterator( pIt, CTaxon1::eIteratorMode_FullTree ) {}
    virtual ~CFullTreeConstIterator() {}

    virtual bool IsLastChild() const
    { return m_it->GetNode() && m_it->GetNode()->IsLastChild(); }
    virtual bool IsFirstChild() const
    { return m_it->GetNode() && m_it->GetNode()->IsFirstChild(); }
    virtual bool IsTerminal() const
    { return m_it->GetNode() && m_it->GetNode()->IsTerminal(); }
    // Navigation
    virtual bool GoParent() { return m_it->GoParent(); }
    virtual bool GoChild() { return m_it->GoChild(); }
    virtual bool GoSibling() { return m_it->GoSibling(); }
    virtual bool GoNode(const ITaxon1Node* pNode)
    { return m_it->GoNode(CastIC(pNode)); }
    // move cursor to the nearest common ancestor
    // between node pointed by cursor and the node
    // with given node_id
    virtual bool GoAncestor(const ITaxon1Node* pNode)
    { return m_it->GoAncestor(CastIC(pNode)); }
    // check if node pointed by cursor
    // is belong to subtree wich root node
    // has given node_id
    virtual bool BelongSubtree(const ITaxon1Node* subtree_root) const
    { return m_it->BelongSubtree(CastIC(subtree_root)); }	
    // check if node with given node_id belongs
    // to subtree pointed by cursor
    virtual bool AboveNode(const ITaxon1Node* node) const
    { return m_it->AboveNode(CastIC(node)); }
protected:
    virtual bool IsVisible( const CTreeContNodeBase* ) const { return true; }
};

class CTreeLeavesBranchesIterator : public CTaxTreeConstIterator {
public:
    CTreeLeavesBranchesIterator( CTreeConstIterator* pIt ) :
	CTaxTreeConstIterator( pIt, CTaxon1::eIteratorMode_LeavesBranches ) {}
    virtual ~CTreeLeavesBranchesIterator() {}

    virtual bool IsTerminal() const
    { return m_it->GetNode() && m_it->GetNode()->IsTerminal(); }
protected:
    virtual bool IsVisible( const CTreeContNodeBase* p ) const;
};

class CTreeBestIterator : public CTaxTreeConstIterator {
public:
    CTreeBestIterator( CTreeConstIterator* pIt ) :
	CTaxTreeConstIterator( pIt, CTaxon1::eIteratorMode_Best ) {}
    virtual ~CTreeBestIterator() {}

    virtual bool IsTerminal() const
    { return m_it->GetNode() && m_it->GetNode()->IsTerminal(); }
protected:
    virtual bool IsVisible( const CTreeContNodeBase* p ) const;
};

class CTreeBlastIterator : public CTaxTreeConstIterator {
public:
    CTreeBlastIterator( CTreeConstIterator* pIt ) :
	CTaxTreeConstIterator( pIt, CTaxon1::eIteratorMode_Blast ) {}
    virtual ~CTreeBlastIterator() {}

protected:
    virtual bool IsVisible( const CTreeContNodeBase* p ) const;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // NCBI_TAXON1_CACHE_HPP
