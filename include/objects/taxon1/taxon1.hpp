#ifndef NCBI_TAXON1_HPP
#define NCBI_TAXON1_HPP

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
 *     NCBI Taxonomy information retreival library
 *
 */


#include <objects/taxon1/taxon1__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <serial/serialdef.hpp>
#include <connect/ncbi_types.h>
#include <corelib/ncbi_limits.hpp>

#include <list>
#include <vector>
#include <map>


BEGIN_NCBI_SCOPE

class CObjectOStream;
class CConn_ServiceStream;


BEGIN_objects_SCOPE

class COrgRefCache;
class ITaxon1Node;
class ITreeIterator;

class NCBI_TAXON1_EXPORT CTaxon1 {
public:
    typedef list< string > TNameList;
    typedef vector< int > TTaxIdList;

    CTaxon1();
    ~CTaxon1();

    //---------------------------------------------
    // Taxon1 server init
    // Returns: TRUE - OK
    //          FALSE - Can't open connection to taxonomy service
    ///
    bool Init(void);  // default:  120 sec timeout, 5 reconnect attempts,
                      // cache for 10 org-refs
    bool Init(unsigned cache_capacity);
    bool Init(const STimeout* timeout, unsigned reconnect_attempts=5,
	      unsigned cache_capacity=10);

    //---------------------------------------------
    // Taxon1 server fini (closes connection, frees memory)
    ///
    void Fini(void);

    //---------------------------------------------
    // Get organism by tax_id
    // Returns: pointer to Taxon2Data if organism exists
    //          NULL - if tax_id wrong
    //
    // NOTE:
    // Caller gets his own copy of Taxon2Data structure.
    ///
    CRef< CTaxon2_data > GetById(int tax_id);

    //----------------------------------------------
    // Get organism by OrgRef
    // Returns: pointer to Taxon2Data if organism exists
    //          NULL - if no such organism in taxonomy database
    //
    // NOTE:
    // 1. These functions uses the following data from inp_orgRef to find
    //    organism in taxonomy database. It uses taxname first. If no organism
    //    was found (or multiple nodes found) then it tries to find organism
    //    using common name. If nothing found, then it tries to find organism
    //    using synonyms. Lookup never uses tax_id to find organism.
    // 2. LookupMerge function modifies given OrgRef to correspond to the
    //    found one and returns constant pointer to the Taxon2Data structure
    //    stored internally.
    ///
    CRef< CTaxon2_data > Lookup(const COrg_ref& inp_orgRef);
    CConstRef< CTaxon2_data > LookupMerge(COrg_ref& inp_orgRef);

    //-----------------------------------------------
    // Get tax_id by OrgRef
    // Returns: tax_id - if organism found
    //               0 - no organism found
    //              -1 - error during processing occured
    //         -tax_id - if multiple nodes found
    //                   (where tax_id > 1 is id of one of the nodes)
    // NOTE:
    // This function uses the same information from inp_orgRef as Lookup
    ///
    int GetTaxIdByOrgRef(const COrg_ref& inp_orgRef);

    enum EOrgRefStatus {
        eStatus_Ok = 0,
        eStatus_WrongTaxId      = 0x001,
        eStatus_WrongGC         = 0x002,
        eStatus_WrongMGC        = 0x004,
        eStatus_NoOrgname       = 0x008,
        eStatus_WrongTaxname    = 0x010,
        eStatus_WrongLineage    = 0x020,
        eStatus_WrongCommonName = 0x040,
        eStatus_WrongOrgname    = 0x080,
        eStatus_WrongDivision   = 0x100,
        eStatus_WrongOrgmod     = 0x200,
	eStatus_WrongPGC        = 0x400
    };
    typedef unsigned TOrgRefStatus;
    //-----------------------------------------------
    // Checks whether OrgRef is valid
    // Returns: false on any error, stat_out filled with status flags
    // (see above)
    ///
    bool CheckOrgRef( const COrg_ref& orgRef, TOrgRefStatus& stat_out );

    enum ESearch {
        eSearch_Exact,
        eSearch_TokenSet,
        eSearch_WildCard, // shell-style wildcards, i.e. *,?,[]
        eSearch_Phonetic
    };
    //----------------------------------------------
    // Get tax_id by organism name
    // Returns: tax_id - if organism found
    //               0 - no organism found
    //              -1 - error during processing occured
    //         -tax_id - if multiple nodes found
    //                   (where tax_id > 1 is id of one of the nodes)
    ///
    int GetTaxIdByName(const string& orgname);

    //----------------------------------------------
    // Get tax_id by organism "unique" name
    // Returns: tax_id - if organism found
    //               0 - no organism found
    //              -1 - error during processing occured
    //         -tax_id - if multiple nodes found
    //                   (where tax_id > 1 is id of one of the nodes)
    ///
    int FindTaxIdByName(const string& orgname);

    //----------------------------------------------
    // Get tax_id by organism name using fancy search modes. If given a pointer
    // to the list of names then it'll return all found names (one name per 
    // tax id). Previous content of name_list_out will be destroyed.
    // Returns: tax_id - if organism found
    //               0 - no organism found
    //              -1 - if multiple nodes found
    //              -2 - error during processing occured
    ///
    int SearchTaxIdByName(const string& orgname,
			  ESearch mode = eSearch_TokenSet,
			  list< CRef< CTaxon1_name > >* name_list_out = NULL);

    //----------------------------------------------
    // Get ALL tax_id by organism name
    // Returns: number of organisms found (negative value on error), 
    // id list appended with found tax ids
    ///
    int GetAllTaxIdByName(const string& orgname, TTaxIdList& lIds);

    //----------------------------------------------
    // Get organism by tax_id
    // Returns: pointer to OrgRef structure if organism found
    //          NULL - if no such organism in taxonomy database or error occured 
    //                 (check GetLastError() for the latter)
    // NOTE:
    // This function does not make a copy of OrgRef structure but returns
    // pointer to internally stored OrgRef.
    ///
    CConstRef< COrg_ref > GetOrgRef(int tax_id,
				    bool& is_species,
				    bool& is_uncultured,
				    string& blast_name,
				    bool* is_specified = NULL);

    //---------------------------------------------
    // Set mode for synonyms in OrgRef
    // Returns: previous mode
    // NOTE:
    // Default value: false (do not copy synonyms to the new OrgRef)
    ///
    bool SetSynonyms(bool on_off);

    //---------------------------------------------
    // Get parent tax_id
    // Returns: tax_id of parent node or 0 if error
    // NOTE:
    //   Root of the tree has tax_id of 1
    ///
    int GetParent(int id_tax);

    //---------------------------------------------
    // Get species tax_id (id_tax should be below species).
    // There are 2 species search modes: one finds the nearest ancestor
    // whose rank is 'species' while another finds the highest ancestor in 
    // the node's lineage having true value of flag 'is_species' defined
    // in the Taxon2_data structure.
    // Returns: tax_id of species node (> 1)
    //       or 0 if no species above (maybe id_tax above species level)
    //       or -1 if error
    // NOTE:
    //   Root of the tree has tax_id of 1
    ///
    enum ESpeciesMode {
	eSpeciesMode_RankOnly,
	eSpeciesMode_Flag
    };
    int GetSpecies(int id_tax, ESpeciesMode mode = eSpeciesMode_Flag);

    //---------------------------------------------
    // Get genus tax_id (id_tax should be below genus)
    // Returns: tax_id of genus or
    //               0 - no genus in the lineage
    //              -1 - if error
    ///
    int GetGenus(int id_tax);

    //---------------------------------------------
    // Get superkingdom tax_id (id_tax should be below superkingdom)
    // Returns: tax_id of superkingdom or
    //               0 - no superkingdom in the lineage
    //              -1 - if error
    ///
    int GetSuperkingdom(int id_tax);

    //---------------------------------------------
    // Get taxids for all children of specified node.
    // Returns: number of children, id list appended with found tax ids
    //          -1 - in case of error
    ///
    int GetChildren(int id_tax, TTaxIdList& children_ids);

    //---------------------------------------------
    // Get genetic code name by genetic code id
    ///
    bool GetGCName(short gc_id, string& gc_name_out );

    //---------------------------------------------
    // Get taxonomic rank name by rank id
    ///
    bool GetRankName(short rank_id, string& rank_name_out );

    //---------------------------------------------
    // Get taxonomic division name by division id
    ///
    bool GetDivisionName(short div_id, string& div_name_out, string* div_code_out = NULL );

    //---------------------------------------------
    // Get taxonomic name class name by name class id
    ///
    bool GetNameClass(short nameclass_id, string& class_name_out );

    //---------------------------------------------
    // Get name class id by name class name
    // Returns: value < 0 - Incorrect class name or error
    // NOTE: Currently there are following name classes in Taxonomy:
    //  scientific name
    //  synonym
    //  genbank synonym
    //  common name
    //  genbank common name
    //  blast name
    //  acronym
    //  genbank acronym
    //  anamorph
    //  genbank anamorph
    //  teleomorph
    //  equivalent name
    //  includes
    //  in-part
    //  misnomer
    //  equivalent name
    //  misspelling
    //
    // Scientific name is always present for each taxon. Note 'genbank'
    // variants for some name classes (e.g. all common names for taxon
    // is an union of names having both 'common name' and 'genbank common
    // name' classes).
    ///
    short GetNameClassId( const string& class_name );

    //---------------------------------------------
    // Get the nearest common ancestor for two nodes
    // Returns: id of this ancestor (id == 1 means that root node only is
    // ancestor)
    //          -1 - in case of an error
    ///
    int Join(int taxid1, int taxid2);

    //---------------------------------------------
    // Get all names for tax_id
    // Returns: number of names, name list appended with ogranism's names
    //          -1 - in case of an error
    // NOTE:
    // If unique is true then only unique names will be stored
    ///
    int GetAllNames(int tax_id, TNameList& lNames, bool unique);

    //---------------------------------------------
    // Get list of all names for tax_id.
    // Clears the previous content of the list.
    // Returns: TRUE - success
    //          FALSE - failure
    ///
    bool GetAllNamesEx(int tax_id, list< CRef< CTaxon1_name > >& lNames);

    //---------------------------------------------
    // Dump all names of the particular class
    // Replaces the list of Taxon1_name with returned values
    // Returns: TRUE - success
    //          FALSE - failure
    ///
    bool DumpNames( short name_class, list< CRef< CTaxon1_name > >& out );

    //---------------------------------------------
    // Find out is taxonomy lookup system alive or not
    // Returns: TRUE - alive
    //          FALSE - dead
    ///

    bool IsAlive(void);

    //--------------------------------------------------
    // Get tax_id for given gi
    // Returns:
    //       true   if ok
    //       false  if error
    // tax_id_out contains:
    //       tax_id if found
    //       0      if not found
    ///
    bool GetTaxId4GI(TGi gi, int& tax_id_out);

    //--------------------------------------------------
    // Get "blast" name for id
    // Returns: false if some error (blast_name_out not changed)
    //          true  if Ok
    //                blast_name_out contains first blast name at or above
    //                this node in the lineage or empty if there is no blast
    //                name above
    ///
    bool GetBlastName(int tax_id, string& blast_name_out);

    //--------------------------------------------------
    // Get error message after latest erroneous operation
    // Returns: error message, or empty string if no error occurred
    ///
    const string& GetLastError() const { return m_sLastError; }

    //--------------------------------------------------
    // This function constructs minimal common tree from the given tax id
    // set (ids_in) treated as tree's leaves. It then returns a residue of 
    // this tree node set and the given tax id set in ids_out.
    // Returns: false if some error
    //          true  if Ok
    ///
    bool GetPopsetJoin( const TTaxIdList& ids_in, TTaxIdList& ids_out );

    //--------------------------------------------------
    // This function updates cached partial tree and insures that node
    // with given tax_id and all its ancestors will present in this tree.
    // Returns: false if error
    //          true  if Ok, *ppNode is pointing to the node
    ///
    bool LoadNode( int tax_id, const ITaxon1Node** ppNode = NULL )
    { return LoadSubtreeEx( tax_id, 0, ppNode ); }

    //--------------------------------------------------
    // This function updates cached partial tree and insures that node
    // with given tax_id and all its ancestors and immediate children (if any)
    // will present in this tree.
    // Returns: false if error
    //          true  if Ok, *ppNode is pointing to the subtree root
    ///
    bool LoadChildren( int tax_id, const ITaxon1Node** ppNode = NULL )
    { return LoadSubtreeEx( tax_id, 1, ppNode ); }

    //--------------------------------------------------
    // This function updates cached partial tree and insures that all nodes
    // from subtree with given tax_id as a root will present in this tree.
    // Returns: false if error
    //          true  if Ok, *ppNode is pointing to the subtree root
    ///
    bool LoadSubtree( int tax_id, const ITaxon1Node** ppNode = NULL )
    { return LoadSubtreeEx( tax_id, -1, ppNode ); }

    enum EIteratorMode {
        eIteratorMode_FullTree,       // Iterator in this mode traverses all 
                                      // tree nodes
        eIteratorMode_LeavesBranches, // traverses only leaves and branches
        eIteratorMode_Best,           // leaves and branches plus 
                                      // nodes right below branches
        eIteratorMode_Blast,          // nodes with non-empty blast names
        eIteratorMode_Default = eIteratorMode_FullTree
    };
    //--------------------------------------------------
    // This function returnes an iterator of a cached partial tree positioned
    // at the tree root. Please note that the tree is PARTIAL. To traverse the
    // full taxonomy tree invoke LoadSubtree(1) first.
    // Returns: NULL if error
    ///
    CRef< ITreeIterator > GetTreeIterator( EIteratorMode mode
					   = eIteratorMode_Default );

    //--------------------------------------------------
    // This function returnes an iterator of a cached partial tree positioned
    // at the tree node with tax_id.
    // Returns: NULL if node doesn't exist or some other error occurred
    ///
    CRef< ITreeIterator > GetTreeIterator( int tax_id, EIteratorMode mode
					   = eIteratorMode_Default );

    //--------------------------------------------------
    // These functions retreive the "properties" of the taxonomy nodes. Each
    // "property" is a (name, value) pair where name is a string and value
    // could be of integer, boolean, or string type.
    // Returns: true  when success and last parameter is filled with value,
    //          false when call failed
    ///
    bool GetNodeProperty( int tax_id, const string& prop_name,
                          bool& prop_val );
    bool GetNodeProperty( int tax_id, const string& prop_name,
                          int& prop_val );
    bool GetNodeProperty( int tax_id, const string& prop_name,
                          string& prop_val );

    //---------------------------------------------------
    // This function returns the list of "type materials" for the node with taxid given.
    // The list consists of names with class type material found at the species
    // or subspecies ancestor of the node. 
    // Returns: true  when success and last parameter is filled with type material list,
    //          false when call failed
    ///
    bool GetTypeMaterial( int tax_id, TNameList& type_material_list_out );

    //---------------------------------------------------
    // This function returns the maximal value for taxid
    // or -1 in case of error
    ///
    int GetMaxTaxId( void );

    //---------------------------------------------------
    // This function constructs the "display common name" for the taxid following this algorithm:
    //  Return first non-empty value from the following sequence:
    //  1) the GenBank common name
    //  2) the common name if there is only one
    //  3) if taxid is below species level
    //    a) the corresponding species GenBank common name
    //    b) the corresponding species common name if there is only one
    //  4) the Blast inherited blast name
    // Returns: true on success, false in case of error
    ///
    bool GetDisplayCommonName( int tax_id, string& disp_name_out );

private:
    friend class COrgRefCache;

    ESerialDataFormat        m_eDataFormat;
    const char*              m_pchService;
    STimeout*                m_timeout;  // NULL, or points to "m_timeout_value"
    STimeout                 m_timeout_value;

    CConn_ServiceStream*     m_pServer;

    CObjectOStream*          m_pOut;
    CObjectIStream*          m_pIn;

    unsigned                 m_nReconnectAttempts;

    COrgRefCache*            m_plCache;

    bool                     m_bWithSynonyms;
    string                   m_sLastError;

    typedef map<short, string> TGCMap;
    TGCMap                   m_gcStorage;

    void             Reset(void);
    bool             SendRequest(CTaxon1_req& req, CTaxon1_resp& resp, bool bShouldReconnect = true);
    void             SetLastError(const char* err_msg);
    void             PopulateReplaced(COrg_ref& org, COrgName::TMod& lMods);
    bool             LookupByOrgRef(const COrg_ref& inp_orgRef, int* pTaxid,
                                    COrgName::TMod& hitMods);
    void             OrgRefAdjust( COrg_ref& inp_orgRef,
                                   const COrg_ref& db_orgRef,
                                   int tax_id );
    bool             LoadSubtreeEx( int tax_id, int type,
                                    const ITaxon1Node** ppNode );
};

//-------------------------------------------------
// This interface class represents a Taxonomy Tree node
class ITaxon1Node {
public:
    virtual ~ITaxon1Node() { }

    //-------------------------------------------------
    // Returns: taxonomy id of the node
    virtual int              GetTaxId() const = 0;

    //-------------------------------------------------
    // Returns: scientific name of the node. This name is NOT unique
    // To get unique name take the first one from the list after calling
    // CTaxon1::GetAllNames() with parameter unique==true.
    virtual const string&    GetName() const = 0;

    //-------------------------------------------------
    // Returns: blast name of the node if assigned or empty string otherwise.
    virtual const string&    GetBlastName() const = 0;

    //-------------------------------------------------
    // Returns: taxonomic rank id of the node
    virtual short            GetRank() const = 0;

    //-------------------------------------------------
    // Returns: taxonomic division id of the node
    virtual short            GetDivision() const = 0;

    //-------------------------------------------------
    // Returns: genetic code for the node
    virtual short            GetGC() const = 0;

    //-------------------------------------------------
    // Returns: mitochondrial genetic code for the node
    virtual short            GetMGC() const = 0;
                       
    //-------------------------------------------------
    // Returns: true if node is uncultured,
    //          false otherwise
    virtual bool             IsUncultured() const = 0;

    //-------------------------------------------------
    // Returns: true if node is root
    //          false otherwise
    virtual bool             IsRoot() const = 0;

};

//-------------------------------------------------
// This interface class represents an iterator to // partial taxonomy tree build by CTaxon1 object.
class NCBI_TAXON1_EXPORT ITreeIterator : public CObject {
public:
    //-------------------------------------------------
    // Returns: iterator operating mode
    //
    virtual CTaxon1::EIteratorMode GetMode() const = 0;

    //-------------------------------------------------
    // Get node pointed by this iterator
    // Returns: pointer to node
    //          or NULL if error
    virtual const ITaxon1Node* GetNode() const = 0;
    const ITaxon1Node* operator->() const { return GetNode(); }

    //-------------------------------------------------
    // Returns: true if node is terminal,
    //          false otherwise
    // NOTE: Although node is terminal in the partial tree
    // build by CTaxon object it might be NOT a terminal node
    // in the full taxonomic tree !
    virtual bool IsTerminal() const = 0;

    //-------------------------------------------------
    // Returns: true if node is last child in this partial tree,
    //          false otherwise
    virtual bool IsLastChild() const = 0;

    //-------------------------------------------------
    // Returns: true if node is last child in this partial tree,
    //          false otherwise
    virtual bool IsFirstChild() const = 0;

    //-------------------------------------------------
    // Move iterator to tree root
    // Returns: true if move is sucessful,
    //          false otherwise (e.g. node is root)
    virtual void GoRoot() = 0;

    //-------------------------------------------------
    // Move iterator to parent node
    // Returns: true if move is sucessful,
    //          false otherwise (e.g. node is root)
    virtual bool GoParent() = 0;

    //-------------------------------------------------
    // Move iterator to first child
    // Returns: true if move is sucessful,
    //          false otherwise (e.g. no children)
    virtual bool GoChild() = 0;

    //-------------------------------------------------
    // Move iterator to sibling
    // Returns: true if move is sucessful,
    //          false otherwise (e.g. last child)
    virtual bool GoSibling() = 0;

    //-------------------------------------------------
    // Move iterator to given node. Node MUST be previously obtained
    // using GetNode().
    // Returns: true if move is sucessful,
    //          false otherwise
    virtual bool GoNode(const ITaxon1Node* pNode) = 0;

    //-------------------------------------------------
    // Move iterator to the nearest common ancestor of the node pointed
    // by iterator and given node
    // Returns: true if move sucessful,
    //          false otherwise
    virtual bool GoAncestor(const ITaxon1Node* pNode) = 0; 

    enum EAction {
        eOk,   // Ok - Continue traversing
        eStop, // Stop traversing, exit immediately
               // (the iterator will stay on node which returns this code)
        eSkip  // Skip current node's subree and continue traversing
    };

    //-------------------------------------------------
    // "Callback" class for traversing the tree.
    // It features 3 virtual member functions: Execute(), LevelBegin(),
    // and LevelEnd(). Execute() is called with pointer of a node
    // to process it. LevelBegin() and LevelEnd() functions are called 
    // before and after processing of the children nodes respectively with
    // to-be-processed subtree root as an argument. They are called only
    // when the node has children. The order of execution of 3 functions
    // may differ but LevelBegin() always precedes LevelEnd().
    class I4Each {
    public:
        virtual ~I4Each() { }
        virtual EAction
        LevelBegin(const ITaxon1Node* /*pParent*/)
        { return eOk; }
        virtual EAction Execute(const ITaxon1Node* pNode)= 0;
        virtual EAction LevelEnd(const ITaxon1Node* /*pParent*/)
        { return eOk; }
    };
    
    //--------------------------------------------------
    // Here's a tree A drawing that will be used to explain trversing modes
    //              /| 
    //             B C
    //            /| 
    //           D E
    //
    // This function arranges 'downward' traverse mode when higher nodes are
    // processed first. The sequence of calls to I4Each functions for
    // iterator at the node A whould be:
    //   Execute( A ), LevelBegin( A )
    //     Execute( B ), LevelBegin( B )
    //       Execute( D ), Execute( E )
    //     LevelEnd( B )
    //     Execute( C )
    //   LevelEnd( A )
    // The 'levels' parameter specifies the depth of traversing the tree.
    // Nodes that are 'levels' levels below subtree root are considered
    // terminal nodes.
    // Returns: Action code (see EAction description)
    EAction TraverseDownward(I4Each&, unsigned levels = kMax_UInt);

    //--------------------------------------------------
    // This function arranges 'upward' traverse mode when lower nodes are
    // processed first. The sequence of calls to I4Each functions for
    // iterator at the node A whould be:
    //   LevelBegin( A )
    //     LevelBegin( B )
    //       Execute( D ), Execute( E )
    //     LevelEnd( B ), Execute( B )
    //     Execute( C )
    //   LevelEnd( A ), Execute( A )
    // The 'levels' parameter specifies the depth of traversing the tree.
    // Nodes that are 'levels' levels below subtree root are considered
    // terminal nodes.
    // Returns: Action code (see EAction description)
    EAction TraverseUpward(I4Each&, unsigned levels = kMax_UInt);

    //--------------------------------------------------
    // This function arranges 'level by level' traverse mode when nodes are 
    // guarantied to be processed after its parent and all of its 'uncles'.
    // The sequence of calls to I4Each functions for iterator at the node A
    // whould be:
    //   Execute( A ), LevelBegin( A )
    //     Execute( B ), Execute( C )
    //       LevelBegin( B )
    //         Execute( D ), Execute( E )
    //       LevelEnd( B )
    //   LevelEnd( A )
    // The 'levels' parameter specifies the depth of traversing the tree.
    // Nodes that are 'levels' levels below subtree root are considered
    // terminal nodes.
    // Returns: Action code (see EAction description)
    EAction TraverseLevelByLevel(I4Each&, unsigned levels = kMax_UInt);

    //--------------------------------------------------
    // This function arranges traverse of all ancestors of the node in  
    // ascending order starting from its parent (if there is one).
    // The sequence of calls to I4Each functions for iterator at the node D
    // whould be:
    //   Execute( B )
    //   Execute( A )
    // Note: The are NO LevelBegin(), levelEnd() calls performed.
    EAction TraverseAncestors(I4Each&);

    //--------------------------------------------------
    // Checks if node is belonging to subtree with subtree_root
    // Returns: true if it does,
    //          false otherwise
    virtual bool BelongSubtree(const ITaxon1Node* subtree_root) const = 0;

    //--------------------------------------------------
    // Checks if the given node belongs to subtree which root is 
    // pointed by iterator
    // Returns: true if it does,
    //          false otherwise
    virtual bool AboveNode(const ITaxon1Node* node) const = 0;

private:
    EAction TraverseLevelByLevelInternal(I4Each& cb, unsigned levels,
                                         vector< const ITaxon1Node* >& skp);
};


END_objects_SCOPE
END_NCBI_SCOPE

#endif //NCBI_TAXON1_HPP
