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
 * Author:  Kevin Bealer
 *
 */

#include <corelib/ncbistr.hpp>
#include <algorithm>

#include "seqdbalias.hpp"
#include "seqdbfile.hpp"

BEGIN_NCBI_SCOPE

/// Index file.
///
/// Index files (extension nin or pin) contain information on where to
/// find information in other files.  The OID is the (implied) key.


// Public Constructor
//
// This is the user-visible constructor, which builds the top level
// node in the dbalias node tree.  This design effectively treats the
// user-input database list as if it were an alias file containing
// only the DBLIST specification.

CSeqDBAliasNode::CSeqDBAliasNode(const string & dbpath,
                                 const string & dbname_list,
                                 char           prot_nucl,
                                 bool           use_mmap)
    : m_DBPath(dbpath)
{
    set<string> recurse;
    
    if (seqdb_debug_class && debug_alias) {
        cout << "user list((" << dbname_list << "))<>";
    }
    
    m_Values["DBLIST"] = dbname_list;
    
    x_ExpandAliases("-", prot_nucl, use_mmap, recurse);
}

// Private Constructor
// 
// This is the constructor for nodes other than the top-level node.
// As such it is private and only called from this class.
// 
// This constructor constructs subnodes by calling x_ExpandAliases,
// which calls this constructor again with the subnode's arguments.
// But no node should be its own ancestor.  To prevent this kind of
// recursive loop, each file adds its full path to a set of strings
// and does not create a subnode for any path already in that set.
// 
// The set (recurse) is passed BY VALUE so that two branches of the
// same file can contain equivalent nodes.  A more efficient method
// for allowing this kind of sharing might be to pass by reference,
// removing the current node path from the set after construction.

CSeqDBAliasNode::CSeqDBAliasNode(const string & dbpath,
                                 const string & dbname,
                                 char           prot_nucl,
                                 bool           use_mmap,
                                 set<string>    recurse)
    : m_DBPath(dbpath)
{
    if (seqdb_debug_class && debug_alias) {
        bool comma = false;
        
        cout << dbname << "<";
        for(set<string>::iterator i = recurse.begin(); i != recurse.end(); i++) {
            if (comma) {
                cout << ",";
            }
            comma = true;
            cout << SeqDB_GetFileName(*i);
        }
        cout << ">";
    }
    
    string full_filename( x_MkPath(m_DBPath, dbname, prot_nucl) );
    recurse.insert(full_filename);
    
    x_ReadValues(full_filename, use_mmap);
    x_ExpandAliases(dbname, prot_nucl, use_mmap, recurse);
}

void CSeqDBAliasNode::x_ReadLine(const char * bp,
                                 const char * ep)
{
    const char * p = bp;
    
    // If first nonspace char is '#', line is a comment, so skip.
    if (*p == '#') {
        return;
    }
    
    // Find name
    const char * spacep = p;
    
    while((spacep < ep) && (*spacep != ' '))
        spacep ++;
    
    string name(p, spacep);
    
    // Find value
    while((spacep < ep) && ((*spacep == ' ') || (*spacep == '\t')))
        spacep ++;
    
    string value(spacep, ep);
    
    // Store in this nodes' dictionary.
    m_Values[name] = value;
}

void CSeqDBAliasNode::x_ReadValues(const string & fn, bool use_mmap)
{
    CSeqDBRawFile af(use_mmap);
    af.Open(fn);
    
    Uint4 file_length = af.GetFileLength();
    
    const char * bp = af.GetRegion(0, file_length);
    const char * ep = bp + file_length;
    const char * p  = bp;
    
    while(p < ep) {
        // Skip spaces
        while((p < ep) && (*p == ' ')) {
            p++;
        }
        
        const char * eolp = p;
        
        while((eolp < ep) && (*eolp != '\n')) {
            eolp++;
        }
        
        // Non-empty line, so read it.
        if (eolp != p) {
            x_ReadLine(p, eolp);
        }
        
        p = eolp + 1;
    }
}

void CSeqDBAliasNode::x_ExpandAliases(const string & this_name,
                                      char           prot_nucl,
                                      bool           use_mmap,
                                      set<string>  & recurse)
{
    vector<string> namevec;
    string dblist( m_Values["DBLIST"] );
    NStr::Tokenize(dblist, " ", namevec, NStr::eMergeDelims);
    
    bool parens = false;
    
    for(Uint4 i = 0; i<namevec.size(); i++) {
        if (namevec[i] == this_name) {
            // If the base name of the alias file is also listed in
            // "dblist", it is assumed to refer to a volume instead of
            // to itself.
            
            m_VolNames.push_back(this_name);
            continue;
        }
        
        string new_db_loc( x_MkPath(m_DBPath, namevec[i], prot_nucl) );
        
        if (recurse.find(new_db_loc) != recurse.end()) {
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       "Illegal configuration: DB alias files are mutually recursive.");
        }
        
        if ( CFile(new_db_loc).Exists() ) {
            if (parens == false && seqdb_debug_class && debug_alias) {
                parens = true;
                cout << " {" << endl;
            }
            
            string newpath = SeqDB_GetDirName(new_db_loc);
            string newfile = SeqDB_GetBaseName(new_db_loc);
            
            CRef<CSeqDBAliasNode>
                subnode( new CSeqDBAliasNode(newpath,
                                             newfile,
                                             prot_nucl,
                                             use_mmap,
                                             recurse) );
            
            m_SubNodes.push_back(subnode);
        } else {
            // If the name is not found as an alias file, it is
            // considered to be a volume.
            
            m_VolNames.push_back( SeqDB_GetBasePath(new_db_loc) );
        }
    }
    
    if (seqdb_debug_class && debug_alias) {
        if (parens) {
            cout << "}" << endl;
        } else {
            cout << ";" << endl;
        }
    }
}

void CSeqDBAliasNode::GetVolumeNames(vector<string> & vols)
{
    set<string> volset;
    x_GetVolumeNames(volset);
    
    vols.clear();
    for(set<string>::iterator i = volset.begin(); i != volset.end(); i++) {
        vols.push_back(*i);
    }
    
    // Sort to insure deterministic order.
    sort(vols.begin(), vols.end());
}

void CSeqDBAliasNode::x_GetVolumeNames(set<string> & vols)
{
    Uint4 i = 0;
    
    for(i = 0; i < m_VolNames.size(); i++) {
        vols.insert(m_VolNames[i]);
    }
    
    for(i = 0; i < m_SubNodes.size(); i++) {
        m_SubNodes[i]->x_GetVolumeNames(vols);
    }
}

// Uint4 CSeqDBAliasNode::GetNumSeqs(vector< CRef<CSeqDBVol> > & vols)
// {
//     if (m_Values.find("NSEQ") != m_Values.end()) {
//         return m_Values["NSEQ"];
//     }
    
//     Uint4 nseqs = 0;
    
//     Uint4 i,j;
    
//     for(i = 0; i < m_SubNodes.size(); i++) {
//         Uint4 node_nseqs = m_SubNodes[i]->GetNumSeqs(vols);
        
//         if (node_nseqs > 0) {
//             nseqs += node_nseqs;
//         }
//     }
    
//     for(i = 0; i < m_VolNames.size(); i++) {
//         string volnseqs = -1;
        
//         for(j = 0; j < vols.size(); j++) {
//             // Find volume "j".
//             if (vols[j]->GetVolName() == m_VolNames[i]) {
//                 volnseqs = vols[j]->GetNumSeqs();
//                 break;
//             }
//         }
        
//         if (volnseqs) {
//             nseqs += nseqs;
//         }
//     }
    
//     return title;
// }

class CSeqDB_TitleWalker : public CSeqDB_AliasWalker {
public:
    virtual const char * GetFileKey(void)
    {
        return "TITLE";
    }
    
    virtual void Accumulate(CSeqDBVol & vol)
    {
        AddString( vol.GetTitle() );
    }
    
    virtual void AddString(const string & value)
    {
        if (! value.empty()) {
            if (! m_Value.empty()) {
                m_Value += "; ";
            }
            m_Value += value;
        }
    }
    
    string GetTitle(void)
    {
        return m_Value;
    }
    
private:
    string m_Value;
};


// A slightly more clever approach (might) track the contributions
// from each volume or alias file and trim the final total by the
// amount of provable overcounting detected.
// 
// Since this is probably impossible in most cases, it is not done.
// The working assumption then is that the specified databases are
// disjoint.  This design should prevent undercounting but allows
// overcounting in some cases.

class CSeqDB_NSeqsWalker : public CSeqDB_AliasWalker {
public:
    CSeqDB_NSeqsWalker(void)
    {
        m_Value = 0;
    }
    
    virtual const char * GetFileKey(void)
    {
        return "NSEQ";
    }
    
    virtual void Accumulate(CSeqDBVol & vol)
    {
        m_Value += vol.GetNumSeqs();
    }
    
    virtual void AddString(const string & value)
    {
        m_Value += NStr::StringToUInt(value);
    }
    
    Uint4 GetNumSeqs(void)
    {
        return m_Value;
    }
    
private:
    Uint4 m_Value;
};


class CSeqDB_TotalLengthWalker : public CSeqDB_AliasWalker {
public:
    CSeqDB_TotalLengthWalker(void)
    {
        m_Value = 0;
    }
    
    virtual const char * GetFileKey(void)
    {
        return "LENGTH";
    }
    
    virtual void Accumulate(CSeqDBVol & vol)
    {
        m_Value += vol.GetTotalLength();
    }
    
    virtual void AddString(const string & value)
    {
        m_Value += NStr::StringToUInt8(value);
    }
    
    Uint8 GetTotalLength(void)
    {
        return m_Value;
    }
    
private:
    Uint8 m_Value;
};


void CSeqDBAliasNode::WalkNodes(CSeqDB_AliasWalker * walker, vector< CRef<CSeqDBVol> > & vols)
{
    TVarList::iterator iter = m_Values.find(walker->GetFileKey());
    
    if (iter != m_Values.end()) {
        walker->AddString( (*iter).second );
        return;
    }
    
    Uint4 i,j;
    
    for(i = 0; i < m_SubNodes.size(); i++) {
        m_SubNodes[i]->WalkNodes( walker, vols );
    }
    
    for(i = 0; i < m_VolNames.size(); i++) {
        for(j = 0; j < vols.size(); j++) {
            // Find volume "j".
            if (vols[j]->GetVolName() == m_VolNames[i]) {
                walker->Accumulate( *vols[j] );
                break;
            }
        }
    }
}

// Rather than m<s,s> ......


// What is communicated here?  One of two things.  First, that an
// OIDLIST subset of a volume should be used (specifically, combined
// with existing subsets via OR).  Alternately, we can communicate
// that a volume should be used, in its entirety.  So, a set of
// combineable partial volumes, or a whole volume.

// Thus, for each volume, there is either no filtering, or filtering
// by an OR of a set of mask files.

// volset.IncludeAll(volname);
// volset.AddMask(volname, filename);

void CSeqDBAliasNode::BuildMaskList(vector< CRef<CSeqDBVol> > & vols)
{
    TVarList::iterator oid_iter = m_Values.find(string("OIDLIST"));
    TVarList::iterator db_iter  = m_Values.find(string("DBLIST"));
    
    if (oid_iter != m_Values.end()) {
        //walker->AddString( (*iter).second );
        cout << "Mapping found: dbp(" << m_DBPath << "); DBLIST[" << (*db_iter).second << "], OIDLIST[" << (*oid_iter).second << "]" << endl;
        return;
    }
    
    Uint4 i,j;
    
    for(i = 0; i < m_SubNodes.size(); i++) {
        m_SubNodes[i]->BuildMaskList( vols );
    }
    
    for(i = 0; i < m_VolNames.size(); i++) {
        for(j = 0; j < vols.size(); j++) {
            // Find volume "j".
            if (vols[j]->GetVolName() == m_VolNames[i]) {
                cout << "Accumulate would be called: volume->GetVolName()=(" << vols[j]->GetVolName() << ")" << endl;
                //walker->Accumulate( *vols[j] );
                break;
            }
        }
    }
}

string CSeqDBAliasNode::GetTitle(vector< CRef<CSeqDBVol> > & vols)
{
    CSeqDB_TitleWalker walk;
    WalkNodes(& walk, vols);
    
    return walk.GetTitle();
}

Uint4 CSeqDBAliasNode::GetNumSeqs(vector< CRef<CSeqDBVol> > & vols)
{
    CSeqDB_NSeqsWalker walk;
    WalkNodes(& walk, vols);
    
    return walk.GetNumSeqs();
}

Uint8 CSeqDBAliasNode::GetTotalLength(vector< CRef<CSeqDBVol> > & vols)
{
    CSeqDB_TotalLengthWalker walk;
    WalkNodes(& walk, vols);
    
    return walk.GetTotalLength();
}

END_NCBI_SCOPE

