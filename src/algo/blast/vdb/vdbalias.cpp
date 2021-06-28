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
 * Author: Amelia Fong
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbifile.hpp>
#include <algorithm>
#include <sstream>

#include <internal/blast/vdb/vdb2blast_util.hpp>
#include <internal/blast/vdb/vdbalias.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbfile.hpp>

BEGIN_NCBI_SCOPE


/// CVDBAliasNode class
///
/// This is one node of the alias node tree, an n-ary tree which
/// represents the relationships of the alias files and vdb used.
/// The children of this node are the other alias files mentioned
/// in this node's VDBLIST key.  Each node may have vdbs which
/// are the non-alias objects mentioned in the VDBLIST.
///


class CVDBAliasNode : public CObject {
    /// Type of set used for KEY/VALUE pairs within each node
    typedef map<string, string> TVarList;

public:
    /// Public Constructor
    ///
    /// This is the user-visible constructor, which builds the top level
    /// node in the dbalias node tree.  This design effectively treats the
    /// user-input database list as if it were an alias file containing
    /// only the VDBLIST specification.
    ///
    /// @param name_list
    ///   The space delimited list of database names.
    /// @param prot_nucl
    ///   The type of sequences stored here.
    CVDBAliasNode(const string & name_list,
                  char prot_nucl,
                  bool	expand_link,
                  bool verify_vdbs);

    void GetExpandedPaths(set<string> & vdbs,
    				  set<string> & db_alias,
    				  set<string> & vdb_alias);


    void GetNodePaths(set<string> & vdbs,
    				  set<string> & db_alias,
    				  set<string> & vdb_alias);

    vector<string> & GetNodeVDBs() {return m_VDBNames;}
    vector<string> & GetNodeVDBList() {return m_VDBList; }
    vector<string> & GetNodeDBList() {return m_DBList; }
    // Return blast db list from the orignal list, filter out any vdb dbs
    // Only applicable for the top node
    string GetBlastDBList() {return m_OrigBlastDBList; }

private:
    /// Private Constructor
    ///
    /// This constructor is used to build the alias nodes other than
    /// the topmost node.  It is private, because such nodes are only
    /// constructed via internal mechanisms of this class.  One
    /// potential issue for alias node hierarchies is that it is easy
    /// to (accidentally) construct mutually recursive alias
    /// configurations.  To prevent an infinite recursion in this
    /// case, this constructor takes a set of strings, which indicate
    /// all the nodes that have already been constructed.  It is
    /// passed by value (copied) because the same node can be used,
    /// legally and safely, in more than one branch of the same alias
    /// node tree.  If the node to build is already in this set, the
    /// constructor will throw an exception.  As a special case, if a
    /// name in a DBLIST line is the same as the node it is in, it is
    /// assumed to be a volume name (even though an alias file exists
    /// with that name), so this will not trigger the cycle detection
    /// exception.
    ///
    /// @param dbpath
    ///   The working directory for relative paths in this node
    /// @param dbname
    ///   The name of this node
    /// @param prot_nucl
    ///   Indicates whether protein or nucletide sequences will be used
    /// @param expand_link
    ///	  expand link if true
    /// @para, vdb_alias
    ///	  true if node is vdb alias

    CVDBAliasNode(const string & node,
    			  char prot_nucl,
            	  bool expand_links,
            	  bool vdb_alias,
            	  bool verify_vdbs);

    /// Read the alias file
    ///
    /// This function read the alias file from the atlas, parsing the
    /// lines and storing the KEY/VALUE pairs in this node.  It
    /// ignores KEY values that are not supported in SeqDB, although
    /// currently SeqDB should support all of the defined KEYs.
    ///
    /// @param fn
    ///   The name of the alias file
    void x_ReadValues(const string & fn);

    /// Read one line of the alias file
    ///
    /// This method parses the specified character string, storing the
    /// results in the KEY/VALUE map in this node.  The input string
    /// is specified as a begin/end pair of pointers.  If the string
    /// starts with "#", the function has no effect.  Otherwise, if
    /// there are tabs in the input string, they are silently
    /// converted to spaces, and the part of the string before the
    /// first space after the first nonspace is considered to be the
    /// key.  The rest of the line (with initial and trailing spaces
    /// removed) is taken as the value.
    ///
    /// @param bp
    ///   A pointer to the first character of the line
    /// @param ep
    ///   A pointer to (one past) the last character of the line
    /// @param name_s
    ///   The variable name from the file
    /// @param value_s
    ///   The value from the file
    void x_ReadLine(const char * bp,
                    const char * ep,
                    string     & name_s,
                    string     & value_s);

    /// Expand a node of the alias node tree recursively
    ///
    /// This method expands a node of the alias node tree, recursively
    /// building the tree from the specified node downward.  (This
    /// method and the private version of the constructor are mutually
    /// recursive.)  The cyclic tree check is done, and paths of these
    /// components are resolved.  The alias file is parsed, and for
    /// each member of the DBLIST set, a subnode is constructed or a
    /// volume name is stored (if the element is the same as this
    /// node's name).
    ///
    /// @param prot_nucl
    ///   Indicates whether this is a protein or nucleotide database.
    void x_ExpandAliases(char prot_nucl);

    /// Check for and resolve path for blast alias file(s)
    ///
    /// This fucntion checks and finds the path for blast alias file
    ///
    /// @param dblist
    ///	  Input db list (can be a mixed of blast dbs, blast alias files,
    ///   vdbs or vdb alias
    /// @ prot_nucl
    ///   Indicates whether this is a protein or nucleotide database.
    /// @ non_blast_alias_list
    ///   If not NULL, funtion will retrun non blast alias files in the
    ///   input db list
    void x_ResolveDBList(vector<string> & dblist, char prot_nucl, vector<string> & vdb_list);


    /// Resolve path for vdbs and vdb alias file(s)
    ///
    /// This fucntion resolves path for vdbs and vdb alias files
    /// An exception is thrown if not everthing in the input list
    /// can be resolved.
    ///
    /// @param vdblist
    ///	  Input vdb list (vdbs or vdb alias files only)
    /// @ prot_nucl
    ///   Indicates whether this is a protein or nucleotide database.
    void x_ResolveVDBList(const vector<string> & vdblist, char prot_nucl);

    /// Append a subnode to this alias node.
    ///
    /// This method appends a new subnode to this node of the alias
    /// node tree.  It is called by the x_ExpandAliases method.
    ///
    /// @param node_path
    ///   The base path of the new node's volume.
    /// @param prot_nucl
    ///   Indicates whether this is a protein or nucleotide database.
    void x_AppendSubNode(const string & node, char prot_nucl, bool isVDB);


    /// Type used to store the set of subnodes for this node
    typedef vector< CRef<CVDBAliasNode> > TSubNodeList;

    /// Filename of this alias file
    CSeqDB_Path m_ThisName;

    /// The common prefix for the DB paths.
    CSeqDB_DirName m_DBPath;

    /// Do not expand link when resolving paths
    bool m_ExpandLinks;

    /// List of KEY/VALUE pairs from this alias file
    TVarList m_Values;

    /// Set of vdb names associated with this node
    vector<string> m_VDBNames;

    /// List of subnodes contained by this node
    TSubNodeList m_SubNodes;

    /// Alias file from VDBLIST
    vector<string> m_VDBList;
    /// Alias Files from DBLIST
    vector<string> m_DBList;
    string m_OrigBlastDBList;
    bool m_VerifyVDBs;

    /// Disable copy operator.
    CVDBAliasNode & operator =(const CVDBAliasNode &);

    /// Disable copy constructor.
    CVDBAliasNode(const CVDBAliasNode &);
};

static void s_Tokenize(const string & dbnames, vector<string> & db_list)
{
	vector <CSeqDB_Substring> tmp;
    SeqDB_SplitQuoted(dbnames, tmp);
    db_list.clear();
    for(unsigned int i=0; i < tmp.size(); i++) {
    	string t;
    	tmp[i].GetString(t);
    	SeqDB_ConvertOSPath(t);
    	db_list.push_back(t);
    }
}

static string s_Join (const vector<string> & list, unsigned int max_size)
{
	if(list.empty())
		return kEmptyStr;

	string join_str = kEmptyStr;
	join_str.reserve(max_size + 1);
	for(unsigned int i=0; i < list.size(); i++)
	{
		if(list[i] == kEmptyStr)
			continue;

		join_str += list[i];
		join_str += " ";
	}
	if (join_str != kEmptyStr) {
		join_str.resize(join_str.size() -1);
	}
	return join_str;

}

CVDBAliasNode::CVDBAliasNode( const string & name_str, char prot_nucl, bool expand_links, bool verify_vdbs)
    : m_ThisName ("-"),
      m_DBPath   ("."),
      m_ExpandLinks(expand_links),
      m_OrigBlastDBList(kEmptyStr),
      m_VerifyVDBs(verify_vdbs)
{
	vector<string> name_list;
    vector<string> vdb_list;
   	if (name_str.empty()) {
   	     NCBI_THROW(CException, eInvalid, "Empty input list ");
   	}
    s_Tokenize(name_str, name_list);
    x_ResolveDBList(name_list, prot_nucl, vdb_list);
    m_Values["DBLIST"] = NStr::Join(m_DBList, " ");
    m_OrigBlastDBList = s_Join(name_list, name_str.size());
    x_ResolveVDBList(vdb_list, prot_nucl);
    m_Values["VDBLIST"] = NStr::Join(vdb_list, " ");
   	if (m_DBList.empty() && m_VDBList.empty() && m_VDBNames.empty() && (m_OrigBlastDBList != name_str)) {
   	     NCBI_THROW(CException, eInvalid, "No valid db entry found: " + name_str + "].");
   	}
	x_ExpandAliases(prot_nucl);
}

void CVDBAliasNode::x_ResolveDBList(vector<string> & dblist, char prot_nucl, vector<string> & vdblist )
{
    size_t i = 0;

   	for(i = 0; i < dblist.size(); i++) {
		_TRACE("Input File :" + dblist[i]);
		string resolved_path = kEmptyStr;

		{
			const CSeqDB_Path path_alias ( m_DBPath, (CSeqDB_BaseName(dblist[i])), prot_nucl, 'a', 'l' );
			resolved_path= SeqDB_ResolveDbPath(path_alias.GetPathS());
			if (resolved_path != kEmptyStr) {
				_TRACE("Local alias db :" + resolved_path);
				m_DBList.push_back(resolved_path);
				continue;
			}
			const CSeqDB_Path path_db ( m_DBPath, (CSeqDB_BaseName(dblist[i])), prot_nucl, 's', 'q');
			resolved_path= SeqDB_ResolveDbPath(path_db.GetPathS());
			if (resolved_path != kEmptyStr) {
				_TRACE("Local path db :" + resolved_path);
				continue;
			}
		}

		const CSeqDB_Path path_alias( (CSeqDB_BasePath(dblist[i])), prot_nucl, 'a', 'l' );
		resolved_path= SeqDB_ResolveDbPath(path_alias.GetPathS());
		if (resolved_path != kEmptyStr) {
			_TRACE("Blast Alias File :" + resolved_path);
			m_DBList.push_back(resolved_path);
			continue;
		}

 		resolved_path= SeqDB_ResolveDbPathNoExtension(dblist[i], prot_nucl);
  		if (resolved_path != kEmptyStr) {
  			_TRACE("Blast db :" + resolved_path);
  			continue;
  		}
       	_TRACE("VDBs :" + resolved_path);
       	vdblist.push_back(dblist[i]);
       	dblist[i] = kEmptyStr;
    }
}

void CVDBAliasNode::x_ResolveVDBList(const vector<string> & vdblist, char prot_nucl)
{
    size_t i = 0;

    for(i=0; i < vdblist.size(); i++) {
		bool check_local = (vdblist[i].find(CDirEntry::GetPathSeparator()) == string::npos);
		string resolved_path = kEmptyStr;
		string vdb = vdblist[i];

		if(check_local) {
			const CSeqDB_Path path_alias (m_DBPath, CSeqDB_BaseName(vdb), prot_nucl, 'v', 'l' );
			resolved_path= SeqDB_ResolveDbPath(path_alias.GetPathS());
			if(resolved_path != kEmptyStr) {
				_TRACE("VDB Local Alias File :" + resolved_path);
				m_VDBList.push_back(resolved_path);
				continue;
			}

    		string local;
    		SeqDB_CombinePath(m_DBPath.GetDirNameSub(),CSeqDB_Substring(vdb), NULL, local);
    		CFile local_file(local);
    		if(local_file.Exists()) {
    			if(m_ExpandLinks) {
     				local_file.DereferenceLink();
     			}
				_TRACE("VDB Local File :" + resolved_path);
     			m_VDBNames.push_back(local_file.GetPath());
     			continue;
     		}
   		}

		const CSeqDB_Path path_alias (CSeqDB_BasePath(vdb), prot_nucl, 'v', 'l' );
		resolved_path= SeqDB_ResolveDbPath(path_alias.GetPathS());
		if(resolved_path != kEmptyStr) {
			_TRACE("VDB Alias File :" + resolved_path);
			m_VDBList.push_back(resolved_path);
			continue;
		}
		const CSeqDB_Path path_db (vdb);
		resolved_path= SeqDB_ResolveDbPath(path_db.GetPathS());
		if(resolved_path != kEmptyStr) {
			CFile file(resolved_path);
			if(m_ExpandLinks) {
     			file.DereferenceLink();
     		}
     		m_VDBNames.push_back(file.GetPath());
			_TRACE("VDB File (blast config) :" + resolved_path);
     		continue;
     	}

   		m_VDBNames.push_back(vdb);
    }

    if(!m_VDBNames.empty() && m_VerifyVDBs) {
    	// check if files are valid
       	CVDBBlastUtil::CheckVDBs(m_VDBNames);
    }
}


void CVDBAliasNode::x_ExpandAliases(char  prot_nucl)
{
    for(size_t i = 0; i < m_DBList.size(); i++) {
    	x_AppendSubNode(m_DBList[i], prot_nucl, false);
    }

   	for(size_t i = 0; i < m_VDBList.size(); i++) {
   		x_AppendSubNode(m_VDBList[i], prot_nucl, true);
   	}
}

void CVDBAliasNode::x_AppendSubNode(const string & node_str,
                                    char  prot_nucl,
                                    bool isVDBAliasFile)
{

	 CRef<CVDBAliasNode>
	        subnode( new CVDBAliasNode(node_str,
	        						   prot_nucl,
	                                   m_ExpandLinks,
	                                   isVDBAliasFile,
	                                   m_VerifyVDBs));

	 m_SubNodes.push_back(subnode);
}

/// Parse a name-value pair.
///
/// The specified section of memory, corresponding to a line from an
/// alias file or group alias file, is read, and the name and value
/// are returned in the provided strings, whose capacity is managed
/// via the quick assignment function.
///
/// @param bp The memory region starts here. [in]
/// @param ep The end of the memory region. [in]
/// @param name The field name is returned here. [out]
/// @param value The field value is returned here. [out]

static void s_SeqDB_ReadLine(const char * bp,
                             const char * ep,
                             string     & name,
                             string     & value)
{
    name.erase();
    value.erase();

    const char * p = bp;

    // If first nonspace char is '#', line is a comment, so skip.
    if (*p == '#') {
        return;
    }

    // Find name
    const char * spacep = p;

    while((spacep < ep) && ((*spacep != ' ') && (*spacep != '\t')))
        spacep ++;

    s_SeqDB_QuickAssign(name, p, spacep);

    // Skip spaces, tabs, to find value
    while((spacep < ep) && ((*spacep == ' ') || (*spacep == '\t')))
        spacep ++;

    // Strip spaces, tabs from end
    while((spacep < ep) && ((ep[-1] == ' ') || (ep[-1] == '\t')))
        ep --;

    s_SeqDB_QuickAssign(value, spacep, ep);

    for(size_t i = 0; i<value.size(); i++) {
        if (value[i] == '\t') {
            value[i] = ' ';
        }
    }
}


void CVDBAliasNode::x_ReadLine(const char * bp,
                                 const char * ep,
                                 string     & name,
                                 string     & value)
{
    s_SeqDB_ReadLine(bp, ep, name, value);

    if (name.size()) {
        // Store in this nodes' dictionary.
        m_Values[name].swap(value);
    }
}



void CVDBAliasNode::x_ReadValues(const string & file)
{
    const char * bp(0);
    const char * ep(0);

    //Expect filename with full path
    CMemoryFile aliasFile(file);
    bp = (char *) aliasFile.Map();
    if(bp == NULL) {
    	NCBI_THROW(CException, eInvalid, "Cannot find or open " + file);
    }

    ep = bp + aliasFile.GetSize();
    const char * p  = bp;

    // These are kept here to reduce allocations.
    string name_s, value_s;

    while(p < ep) {
        // Skip spaces
        while((p < ep) && (*p == ' ')) {
            p++;
        }

        const char * eolp = p;

        while((eolp < ep) && !SEQDB_ISEOL(*eolp)) {
            eolp++;
        }

        // Non-empty line, so read it.
        if (eolp != p) {
            x_ReadLine(p, eolp, name_s, value_s);
        }

        p = eolp + 1;
    }
}

// Private Constructor
//
// This is the constructor for nodes other than the top-level node.
// As such it is private and only called from this class.
//
// This constructor constructs subnodes by calling x_ExpandAliases,
// which calls this constructor again with the subnode's arguments.
// But no node should be its own ancestor.  To prevent this kind of
// recursive loop, each file adds its full path to a stack of strings
// and will not create a subnode for any path already in that set.

CVDBAliasNode::CVDBAliasNode(const string & node,
							 char prot_nucl,
                             bool expand_links,
                             bool vdb_alias,
                             bool verify_vdbs)
	:m_ThisName(node),
	 m_DBPath(m_ThisName.FindDirName()),
	 m_ExpandLinks(expand_links),
     m_OrigBlastDBList(kEmptyStr),
     m_VerifyVDBs(verify_vdbs)
{
   	_TRACE("DB Path : " + m_ThisName.GetPathS());

    x_ReadValues(m_ThisName.GetPathS());

    vector<string> db_list;
    vector<string> vdb_list;
	s_Tokenize(m_Values["DBLIST"], db_list);
	if(vdb_alias && !db_list.empty()) {
    	NCBI_THROW(CException, eInvalid,
    			   "DBLIST is not allowed in vdb alais file: " +  m_ThisName.GetPathS()+ ".");
	}
    x_ResolveDBList(db_list, prot_nucl, vdb_list);

    if(!vdb_list.empty()) {
    	string invalid = NStr::Join(vdb_list, " ");
    	NCBI_THROW(CException, eInvalid, "Invalid db entry in alais file: " + invalid + ".");
    }
   	s_Tokenize(m_Values["VDBLIST"], vdb_list);
   	x_ResolveVDBList(vdb_list, prot_nucl);

   	x_ExpandAliases(prot_nucl);
}

void CVDBAliasNode::GetNodePaths(set<string> & vdbs, set<string> & db_alias, set<string> & vdb_alias)
{
	vdbs.insert(m_VDBNames.begin(), m_VDBNames.end());

	NON_CONST_ITERATE(TSubNodeList, node, m_SubNodes) {
		vector<string> & node_vdbs = (*node)->GetNodeVDBs();
		vdbs.insert( node_vdbs.begin(), node_vdbs.end());
		vector<string> & node_db_list = (*node)->GetNodeDBList();
		db_alias.insert(node_db_list.begin(), node_db_list.end());
		vector<string> & node_vdb_list = (*node)->GetNodeVDBList();
		vdb_alias.insert(node_vdb_list.begin(), node_vdb_list.end());
	}
}

void CVDBAliasNode::GetExpandedPaths(set<string> & vdbs, set<string> & db_alias, set<string> & vdb_alias)
{
	vdbs.insert(m_VDBNames.begin(), m_VDBNames.end());
	db_alias.insert(m_DBList.begin(), m_DBList.end());
	vdb_alias.insert(m_VDBList.begin(), m_VDBList.end());

	NON_CONST_ITERATE(TSubNodeList, node, m_SubNodes) {
		(*node)->GetExpandedPaths( vdbs, db_alias, vdb_alias);
	}
}


void CVDBAliasUtil::FindVDBPaths(const string   & dbname,
								 bool             isProtein,
								 vector<string> & paths,
								 vector<string> * db_alias_list ,
								 vector<string> * vdb_alias_list,
								 bool             recursive ,
								 bool             expand_links,
								 bool			  verify_dbs)
{
    char prot_nucl = (isProtein) ? 'p':'n';
    CVDBAliasNode node(dbname, prot_nucl, expand_links, verify_dbs);
    set<string> vdbs;
    set<string> vdb_alias;
    set<string> db_alias;

    paths.clear();
    if(recursive) {
    	node.GetExpandedPaths(vdbs, db_alias, vdb_alias);
    	paths.insert(paths.begin(), vdbs.begin(), vdbs.end());
    }
    else {
    	node.GetNodePaths(vdbs, db_alias, vdb_alias);
    	paths.insert(paths.begin(), vdbs.begin(), vdbs.end());
    	paths.insert(paths.end(), vdb_alias.begin(), vdb_alias.end());
    	paths.insert(paths.end(), db_alias.begin(), db_alias.end());
    }

    if(db_alias_list != NULL) {
    	db_alias_list->clear();
    	db_alias_list->insert(db_alias_list->begin(),db_alias.begin(), db_alias.end());
    }
    if(vdb_alias_list != NULL) {
    	vdb_alias_list->clear();
    	vdb_alias_list->insert(vdb_alias_list->begin(),vdb_alias.begin(), vdb_alias.end());
    }

}

void CVDBAliasUtil::ProcessMixedDBsList(const string   & input_list,
								 	    bool             isProtein,
								 	    vector<string> & vdb_list,
								 	    string         & blastdb_list,
								 	    bool             verify_vdbs)
{
	  char prot_nucl = (isProtein) ? 'p':'n';
	  set<string> vdbs;
	  set<string> vdb_alias;
	  set<string> db_alias;
	  CVDBAliasNode node(input_list, prot_nucl, true, verify_vdbs);
	  node.GetExpandedPaths(vdbs, db_alias, vdb_alias);
	  blastdb_list = node.GetBlastDBList();

	  vdb_list.clear();
	  vdb_list.insert(vdb_list.begin(), vdbs.begin(), vdbs.end());
}

END_NCBI_SCOPE

