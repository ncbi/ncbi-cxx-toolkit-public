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
#include "ctreecont.hpp"
#include <map>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


class CTaxon1Node;


class COrgRefCache
{
public:
    COrgRefCache( CTaxon1& host );

    bool Init();

    bool LookupAndAdd( int tax_id, CTaxon1Node** ppData );
    bool LookupAndInsert( int tax_id, CTaxon1_data** ppData );
    bool LookupAndInsert( int tax_id, CTaxon2_data** ppData );
    bool Lookup( int tax_id, CTaxon1_data** ppData );
    bool Lookup( int tax_id, CTaxon2_data** ppData );

    bool Insert1( CTaxon1Node& node );
    bool Insert2( CTaxon1Node& node );

    // Rank stuff
    const char* GetRankName( int rank ) const;

    int GetFamilyRank() const { return m_nFamilyRank; }
    int GetOrderRank() const { return m_nOrderRank; }
    int GetClassRank() const { return m_nClassRank; }
    int GetGenusRank() const { return m_nGenusRank; }
    int GetSubgenusRank() const { return m_nSubgenusRank; }
    int GetSpeciesRank() const { return m_nSpeciesRank; }
    int GetSubspeciesRank() const { return m_nSubspeciesRank; }
    int GetFormaRank() const { return m_nFormaRank; }
    int GetVarietyRank() const { return m_nVarietyRank; }

    const char* GetNameClassName( short nc ) const;
    short GetPreferredCommonNameClass() const { return m_ncPrefCommon; }
    short GetCommonNameClass() const { return m_ncCommon; }
    short GetSynonymNameClass() const { return m_ncSynonym; }

    const char* GetDivisionName( short div_id ) const;
    const char* GetDivisionCode( short div_id ) const;
    short GetVirusesDivision() const { return m_divViruses; }
    short GetPhagesDivision() const { return m_divPhages; }

    CTreeCont& GetTree() { return m_tPartTree; }

    void  SetIndexEntry( int id, CTaxon1Node* pNode );

private:
    friend class CTaxon1Node;
    struct SCacheEntry {
        friend class CTaxon1Node;
        CRef< CTaxon1_data > m_pTax1;
        CRef< CTaxon2_data > m_pTax2;

        CTaxon1Node*  m_pTreeNode;
        // 	COrg_ref* p_org_ref;
        // 	bool is_uncultured;
        // 	bool is_species;
        // 	bool has_modif;
        // 	list< string > blast_name;
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
                                       list< CRef<COrgMod> >& lMod,
                                       CTaxon1Node* pParent = NULL );
    bool             SetBinomialName( CTaxon1Node& node, COrgName& on );
    bool             SetPartialName( CTaxon1Node& node, COrgName& on );
    // Rank stuff
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
    int      FindRankByName( const char* pchName ) const;

    // Name classes stuff
    short m_ncPrefCommon;
    short m_ncCommon;
    short m_ncSynonym;

    typedef map<short, string> TNameClassMap;
    typedef TNameClassMap::const_iterator TNameClassMapCI;
    typedef TNameClassMap::iterator TNameClassMapI;
    TNameClassMap m_ncStorage;

    bool     InitNameClasses();
    short    FindNameClassByName( const char* pchName ) const;

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
    short    FindDivisionByCode( const char* pchCode ) const;
};



class CTaxon1Node : public CTreeContNodeBase
{
public:
    CTaxon1Node( const CRef< CTaxon1_name >& ref )
        : m_ref( ref ), m_cacheEntry( NULL ) {}

    int                GetTaxId() const { return m_ref->GetTaxid(); }
    const string&      GetName() const { return m_ref->GetOname(); }
    const string&      GetBlastName() const{ return m_ref->GetUname(); }
    short              GetRank() const;
    short              GetDivision() const;
    short              GetGC() const;
    short              GetMGC() const;
                       
    bool               IsUncultured() const;
    bool               IsGBHidden() const;

    COrgRefCache::SCacheEntry* GetEntry() { return m_cacheEntry; }

    CTaxon1Node*               GetParent()
    { return static_cast<CTaxon1Node*>(Parent()); }
private:
    friend class COrgRefCache;
    CRef< CTaxon1_name >       m_ref;

    COrgRefCache::SCacheEntry* m_cacheEntry;
};


END_objects_SCOPE
END_NCBI_SCOPE



/*
 * $Log$
 * Revision 6.2  2002/01/31 00:31:26  vakatov
 * Follow the renaming of "CTreeCont.hpp" to "ctreecont.hpp".
 * Get rid of "std::" which is unnecessary and sometimes un-compilable.
 * Also done some source identation/beautification.
 *
 * Revision 6.1  2002/01/28 19:56:10  domrach
 * Initial checkin of the library implementation files
 *
 * ===========================================================================
 */

#endif // NCBI_TAXON1_CACHE_HPP
