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

class CDomainStorage {
public:
    static const int kIllegalValue = 0x7fffffff;
    CDomainStorage();
    ~CDomainStorage() {};

    void SetId( int id ) { m_id = id; }
    int  GetId() const { return m_id; }

    void SetName( const string& n ) { m_name = n; }
    const string& GetName() const { return m_name; }

    void AddField( int field_no, int val_type, const string& name );
    bool HasField( const string& field_name ) const;

    void InsertFieldValue( int val_id, int str_len, const string& str );
    void InsertFieldValue( int val_id, int value );

    int  FindValueIdByField( const string& fieldName, const string& searchstring ) const;
    int  FindValueIdByField( const string& fieldName, int fieldValue ) const;

    int  FindFieldValueById( int value_id, const string& fieldName ) const;
    const string&  FindFieldStringById( int value_id, const string& fieldName ) const;

    bool empty() const { return m_values.empty(); }

private:
    int    m_id;
    string m_name;
    typedef map<string, size_t> TFieldMap;
    typedef vector<int> TValueTypes;
    typedef struct {
	int    m_int;
        string m_str;
    } TValue;
    typedef map< int, vector<TValue> > TValues;
    TFieldMap   m_fields;
    TValueTypes m_types;
    TValues     m_values;
};

class COrgRefCache
{
public:
    COrgRefCache( CTaxon1& host );
    ~COrgRefCache();

    bool Init( unsigned nCapacity = 10 );

    bool Lookup( TTaxId tax_id, CTaxon1Node** ppNode );
    bool LookupAndAdd( TTaxId tax_id, CTaxon1Node** ppData );
    bool LookupAndInsert( TTaxId tax_id, CTaxon2_data** ppData );
    bool Lookup( TTaxId tax_id, CTaxon2_data** ppData );

    bool Insert2( CTaxon1Node& node );

    // Rank stuff
    const char* GetRankName( int rank );

    TTaxRank GetSuperkingdomRank() const { return m_nSuperkingdomRank; }
    TTaxRank GetGenusRank() const { return m_nGenusRank; }
    TTaxRank GetSpeciesRank() const { return m_nSpeciesRank; }
    TTaxRank GetSubspeciesRank() const { return m_nSubspeciesRank; }

    const char* GetNameClassName( short nc );
    TTaxNameClass GetPreferredCommonNameClass() const { return m_ncPrefCommon; }
    TTaxNameClass GetCommonNameClass() const { return m_ncCommon; }

    const char* GetDivisionName( TTaxDivision div_id );
    const char* GetDivisionCode( TTaxDivision div_id );

    CTreeCont& GetTree() { return m_tPartTree; }
    const CTreeCont& GetTree() const { return m_tPartTree; }

    void  SetIndexEntry( int id, CTaxon1Node* pNode );

private:
    friend class CTaxon1Node;
    friend class CTaxon1;
    struct SCacheEntry {
        friend class CTaxon1Node;
        CRef< CTaxon2_data > m_pTax2;

        CTaxon1Node*  m_pTreeNode;

        CTaxon2_data* GetData() { return m_pTax2.GetPointer(); }
    };

    CTaxon1&           m_host;

    unsigned           m_nMaxTaxId;
    CTaxon1Node**      m_ppEntries; // index by tax_id
    CTreeCont          m_tPartTree; // Partial tree

    unsigned           m_nCacheCapacity; // Max number of elements in cache
    list<SCacheEntry*> m_lCache; // LRU list

    // Rank stuff
    TTaxRank m_nSuperkingdomRank;
    TTaxRank m_nGenusRank;
    TTaxRank m_nSpeciesRank;
    TTaxRank m_nSubspeciesRank;

    bool          InitDomain( const string& name, CDomainStorage& storage );

    CDomainStorage     m_rankStorage;

    bool          InitRanks();
    TTaxRank      FindRankByName( const char* pchName );

    // Name classes stuff
    TTaxNameClass m_ncPrefCommon; // now called "genbank common name"
    TTaxNameClass m_ncCommon;

    typedef map<TTaxNameClass, string> TNameClassMap;
    typedef TNameClassMap::const_iterator TNameClassMapCI;
    typedef TNameClassMap::iterator TNameClassMapI;
    TNameClassMap m_ncStorage;

    bool          InitNameClasses();
    TTaxNameClass FindNameClassByName( const char* pchName );
    // Division stuff
    CDomainStorage m_divStorage;

    bool          InitDivisions();
    TTaxDivision  FindDivisionByCode( const char* pchCode );
    TTaxDivision  FindDivisionByName( const char* pchName );
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

    virtual TTaxId           GetTaxId() const { return m_ref->GetTaxid(); }
    virtual const string&    GetName() const { return m_ref->GetOname(); }
    virtual const string&    GetBlastName() const
    { return m_ref->CanGetUname() ? m_ref->GetUname() : kEmptyStr; }
    virtual TTaxRank         GetRank() const;
    //virtual const CTaxRank&  GetRankEx() const;
    virtual TTaxDivision     GetDivision() const;
    virtual TTaxGeneticCode  GetGC() const;
    virtual TTaxGeneticCode  GetMGC() const;
                       
    virtual bool             IsUncultured() const;
    virtual bool             IsGenBankHidden() const;

    virtual bool             IsRoot() const
    { return CTreeContNodeBase::IsRoot(); }

    COrgRefCache::SCacheEntry* GetEntry() { return m_cacheEntry; }

    bool                     IsJoinTerminal() const
    { return m_flags&mJoinTerm ? true : false; }
    void                     SetJoinTerminal() { m_flags |= mJoinTerm; }
    bool                     IsSubtreeLoaded() const
    { return m_flags&mSubtreeLoaded ? true : false; }
    void                     SetSubtreeLoaded( bool b )
    { if( b ) m_flags |= mSubtreeLoaded; else m_flags &= ~mSubtreeLoaded; }

    CTaxon1Node*             GetParent()
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

// Orgref "properties"
class COrgrefProp {
public:
    static bool               HasOrgrefProp( const ncbi::objects::COrg_ref& org, const std::string& prop_name );
    static const std::string& GetOrgrefProp( const ncbi::objects::COrg_ref& org, const std::string& prop_name );
    // returns default value false if not found
    static bool               GetOrgrefPropBool( const ncbi::objects::COrg_ref& org, const std::string& prop_name );
    // returns default value 0 if not found
    static int                GetOrgrefPropInt( const ncbi::objects::COrg_ref& org, const std::string& prop_name );

    static void               SetOrgrefProp( ncbi::objects::COrg_ref& org, const std::string& prop_name,
					     const std::string& prop_val );
    static void               SetOrgrefProp( ncbi::objects::COrg_ref& org, const std::string& prop_name,
					     int prop_val );
    static void               SetOrgrefProp( ncbi::objects::COrg_ref& org, const std::string& prop_name,
					     bool prop_val );

    static void               RemoveOrgrefProp( ncbi::objects::COrg_ref& org, const std::string& prop_name );
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // NCBI_TAXON1_CACHE_HPP
