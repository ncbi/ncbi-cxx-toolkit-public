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

/// @file seqdbalias.cpp
/// Code which manages a hierarchical tree of alias file data.
/// 
/// Defines classes:
///     CSeqDB_TitleWalker 
///     CSeqDB_MaxLengthWalker 
///     CSeqDB_MinLengthWalker 
///     CSeqDB_NSeqsWalker 
///     CSeqDB_NOIDsWalker 
///     CSeqDB_TotalLengthWalker 
///     CSeqDB_VolumeLengthWalker 
///     CSeqDB_MembBitWalker 
/// 
/// Implemented for: UNIX, MS-Windows

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbifile.hpp>
#include <algorithm>
#include <sstream>

#include "seqdbalias.hpp"
#include <objtools/blast/seqdb_reader/impl/seqdbfile.hpp>

BEGIN_NCBI_SCOPE

CSeqDBAliasFile::CSeqDBAliasFile(CSeqDBAtlas     & atlas,
                                 const string    & name_list,
                                 char              prot_nucl,
                                 bool              expand_links)
    : m_AliasSets        (atlas),
      m_IsProtein        (prot_nucl == 'p'),
      m_MinLength        (-1),
      m_NumSeqs          (-1),
      m_NumSeqsStats     (-1),
      m_NumOIDs          (-1),
      m_TotalLength      (-1),
      m_TotalLengthStats (-1),
      m_VolumeLength     (-1),
      m_MembBit          (-1),
      m_HasTitle         (false),
      m_NeedTotalsScan   (-1),
      m_HasFilters       (0)
{
    if (name_list.size() && prot_nucl != '-') {
        m_Node.Reset(new CSeqDBAliasNode(atlas,
                                         name_list,
                                         prot_nucl,
                                         m_AliasSets,
                                         expand_links));
        
        m_Node->FindVolumePaths(m_VolumeNames, &m_AliasNames, true);
    }
}

void CSeqDBAliasNode::x_Tokenize(const string & dbnames)
{
    vector<CSeqDB_Substring> dbs;
    SeqDB_SplitQuoted(dbnames, dbs);
    
    m_DBList.resize(dbs.size());
    m_SkipLocal.resize(dbs.size(),false);
    
    for(size_t i = 0; i<dbs.size(); i++) {
        m_DBList[i].Assign(dbs[i]);
        m_DBList[i].FixDelimiters();
    }
}

CSeqDBAliasNode::CSeqDBAliasNode(CSeqDBAtlas     & atlas,
                                 const string    & dbname_list,
                                 char              prot_nucl,
                                 CSeqDBAliasSets & alias_sets,
                                 bool              expand_links)
    : m_Atlas    (atlas),
      m_DBPath   ("."),
      m_ThisName ("-"),
      m_HasGiMask(true),
      m_AliasSets(alias_sets),
      m_ExpandLinks(expand_links)
{
    CSeqDBLockHold locked(atlas);
    m_Atlas.Verify(locked);
    
    m_Values["DBLIST"] = dbname_list;
    
    x_Tokenize(dbname_list);

    // Skip gi mask if more than one DBs are specified.
    if (m_DBList.size() != 1) {
        m_HasGiMask = false;
    }

    x_ResolveNames(prot_nucl, locked);
    
    CSeqDBAliasStack recurse;
    
    x_ExpandAliases(CSeqDB_BasePath("-"), prot_nucl, recurse, locked);
    
    m_Atlas.Unlock(locked);
    
    _ASSERT(recurse.Size() == 0);
   
    // When we get here, the subnodes tree has been built
    if (m_HasGiMask) {
        if (m_SubNodes.size() != 1 ||
            m_SubNodes[0]->m_Values.find("MASKLIST") 
            == m_SubNodes[0]->m_Values.end()) {
            m_HasGiMask = false;
        }
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

CSeqDBAliasNode::CSeqDBAliasNode(CSeqDBAtlas           & atlas,
                                 const CSeqDB_DirName  & dbpath,
                                 const CSeqDB_BaseName & dbname,
                                 char                    prot_nucl,
                                 CSeqDBAliasStack      & recurse,
                                 CSeqDBLockHold        & locked,
                                 CSeqDBAliasSets       & alias_sets,
                                 bool                    expand_links)
    : m_Atlas     (atlas),
      m_DBPath    (dbpath),
      m_ThisName  (m_DBPath, dbname, prot_nucl, 'a', 'l'),
      m_AliasSets (alias_sets),
      m_ExpandLinks (expand_links)
{
    recurse.Push(m_ThisName);
    
    x_ReadValues(m_ThisName, locked);
    x_Tokenize(m_Values["DBLIST"]);
    
    CSeqDB_BasePath basepath(m_ThisName.FindBasePath());
    
    x_ExpandAliases(basepath, prot_nucl, recurse, locked);
    
    recurse.Pop();
}

bool
CSeqDBAliasSets::x_FindBlastDBPath(const string   & dbname,
                                   char             dbtype,
                                   bool             exact,
                                   string         & resolved,
                                   CSeqDBLockHold & locked)
{
    map<string,string>::iterator i = m_PathLookup.find(dbname);
    
    if (i == m_PathLookup.end()) {
        resolved = SeqDB_FindBlastDBPath(dbname,
                                         dbtype,
                                         0,
                                         exact,
                                         m_Atlas,
                                         locked);
        
        m_PathLookup[dbname] = resolved;
    } else {
        resolved = (*i).second;
    }
    
    return ! resolved.empty();
}


bool
CSeqDBAliasSets::FindAliasPath(const CSeqDB_Path & dbpath,
                               CSeqDB_Path       * resolved,
                               CSeqDBLockHold    & locked)
{
    CSeqDB_Path     aset_path;
    CSeqDB_FileName alias_fname;
    
    x_DbToIndexName(dbpath, aset_path, alias_fname);
    
    CSeqDB_Path resolved_aset;
    
    if (! FindBlastDBPath(aset_path, resolved_aset, locked)) {
        return false;
    }
    
    CSeqDB_Path afpath(resolved_aset.FindDirName(),
                       alias_fname.GetFileNameSub());
    
    // This is not ideal.  If the alias file is found, but does not
    // contain the alias in question, we punt, allowing normal alias
    // file reading to take over.  The correct technique would be to
    // try the next location in the database search path.
    //
    // Solving this correctly means cracking FindBlastDBPath() into
    // three pieces, one that builds a list of paths, one that tries a
    // specified path, and a third that calls the first, then iterates
    // over the list, calling the second.
    //
    // This can be done later - punting could be inefficient in some
    // cases but should work correctly.
    
    if (! ReadAliasFile(afpath, 0, 0, locked)) {
        return false;
    }
    
    if (resolved) {
        *resolved = afpath;
    }
    
    return true;
}


void CSeqDBAliasNode::x_ResolveNames(char prot_nucl, CSeqDBLockHold & locked)
{
    m_DBPath = CSeqDB_DirName(".");
    
    size_t i = 0;
    
    for(i = 0; i < m_DBList.size(); i++) {
        // skip local DB search only if absolute path is given
        if(m_DBList[i].GetBasePathS()[0] == CDirEntry::GetPathSeparator()) {
            m_SkipLocal[i] = true;
        }

        const CSeqDB_Path db_path( (CSeqDB_BasePath(m_DBList[i])), prot_nucl, 'a', 'l' );
        
        CSeqDB_Path resolved_path;
        
        // search for X/kSeqDBGroupAliasFileName
        if (! m_AliasSets.FindAliasPath(db_path, & resolved_path, locked)) {
            CSeqDB_BasePath base(db_path.FindBasePath());
            CSeqDB_BasePath resolved_bp;
            
            // search for X/base.nal/nin
            if (m_AliasSets.FindBlastDBPath(base, prot_nucl, resolved_bp, locked)) {
                resolved_path = CSeqDB_Path(resolved_bp, prot_nucl, 'a', 'l');
            }
        }
        
        if (! resolved_path.Valid()) {
            string p_or_n;
            
            switch(prot_nucl) {
            case 'p':
                p_or_n = "protein";
                break;
                
            case 'n':
                p_or_n = "nucleotide";
                break;
                
            default:
                string msg("SeqDB: Internal error: bad sequence type for database [");
                msg += m_DBList[i].GetBasePathS() + "]";
                
                NCBI_THROW(CSeqDBException,
                           eFileErr,
                           msg);
            }
            
            // Do over (to get the search path).  This doesnt use the
            // resolution map, since speed is not of the essence.
            
            string search_path;
            string input_path;
            
            db_path.FindBasePath().GetString(input_path);
            
            SeqDB_FindBlastDBPath(input_path,
                                  prot_nucl,
                                  & search_path,
                                  false,
                                  m_Atlas,
                                  locked);
            
            ostringstream oss;
            oss << "No alias or index file found for " << p_or_n
                << " database [" << m_DBList[i].GetBasePathS()
                << "] in search path [" << search_path << "]";
            
            string msg(oss.str());
            
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       msg);
        } else {
            // full dereferenced name but without suffix /X/base
            if (m_ExpandLinks) {
                string dir_name, base_name;
                resolved_path.FindDirName().GetString(dir_name);
                resolved_path.FindBaseName().GetString(base_name);
                m_DBList[i].Assign(CSeqDB_Substring(
                    CDirEntry::NormalizePath(dir_name, eFollowLinks) + 
                    CDirEntry::GetPathSeparator() +
                    base_name  ));
            } else {
                m_DBList[i].Assign(resolved_path.FindBasePath());
            }
        }
    }
    
    // Everything from here depends on m_DBList[0] existing.
    if (m_DBList.empty())
        return;
    
    size_t common = m_DBList[0].GetBasePathS().size();
    
    // Reduce common length to length of min db path.
    for(i = 1; common && (i < m_DBList.size()); i++) {
        if (m_DBList[i].GetBasePathS().size() < common) {
            common = m_DBList[i].GetBasePathS().size();
        }
    }
    
    if (common) {
        --common;
    }
    
    // Reduce common length to largest universal prefix.
    const string & first = m_DBList[0].GetBasePathS();
    
    for(i = 1; common && (i < m_DBList.size()); i++) {
        // Reduce common prefix length until match is found.
        while(memcmp(first.c_str(),
                     m_DBList[i].GetBasePathS().c_str(),
                     common)) {
            --common;
        }
    }
    
    // Adjust back to whole path component.
    while(common && (first[common] != CFile::GetPathSeparator())) {
        --common;
    }
    
    if (common) {
        // Factor out common path components.
        m_DBPath.Assign( CSeqDB_Substring(first.data(), first.data() + common) );
        
        for(i = 0; i < m_DBList.size(); i++) {
            CSeqDB_Substring sub(m_DBList[i].GetBasePathS());
            sub.EraseFront((int) common + 1);
            
            m_DBList[i].Assign(sub);
        }
    }
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


void CSeqDBAliasNode::x_ReadLine(const char * bp,
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


void CSeqDBAliasSets::x_DbToIndexName(const CSeqDB_Path & dbpath,
                                      CSeqDB_Path       & index_path,
                                      CSeqDB_FileName   & alias_fname)
{
    index_path.ReplaceFilename(dbpath,
                               CSeqDB_Substring(kSeqDBGroupAliasFileName));
    alias_fname.Assign(dbpath.FindFileName());
}



/// Find starting points of included data in the group alias file.
///
/// This function scans the memory region containing the group alias
/// file's data, looking for the string provided as the key.  The key
/// marks the start of each alias file included in the group alias
/// file.  This code compiles a list of pointers representing the
/// starts and ends of the interesting data within the group alias
/// file memory region.
///
/// The first pointer returned here is the start of a line containing
/// the alias file string, then a pointer to the end of that line,
/// then a pointer to the start of the next line containing the key,
/// and so on, repeating.  Finally, the pointer to the end of the data
/// is returned.  Therefore, to find the names of all the alias files,
/// you would examine the range from p0 to p1, p2 to p3, and so on.
/// To find the contents of the alias files, you would examine p1 to
/// p2, p3 to p4, and so on.  The last pointer is appended because it
/// makes it easier to write the loop in the recieving code.
///
/// @param bp The memory region starts here. [in]
/// @param ep The end of the memory region. [in]
/// @param key The seperating string. [out]
/// @param offsets [out]
static void
s_SeqDB_FindOffsets(const char   * bp,
                    const char   * ep,
                    const string & key,
                    vector<const char *> & offsets)
{
    size_t keylen = key.size();
    
    const char * last_keyp = ep - keylen;
    
    for(const char * p = bp; p < last_keyp; p++) {
        bool found = true;
        
        for(size_t i = 0; i < keylen; i++) {
            if (p[i] != key[i]) {
                found = false;
                break;
            }
        }
        
        if (found) {
            // This snippet of code verifies that the key found by the
            // above loop is either at the start of the memory region,
            // or is the first non-whitespace on the line it inhabits.
            // If a database title includes the phrase ALIAS_FILE, we
            // don't treat it as the start of a new alias file.
            
            const char * p2 = p - 1;
            
            while((p2 >= bp) && !SEQDB_ISEOL(*p2)) {
                if ((*p2) != ' ' && (*p2) != '\t') {
                    found = false;
                    break;
                }
                
                p2 --;
            }
            
            if (found) {
                // Push back start of "ALIAS_FILE" string.
                
                offsets.push_back(p);
                
                for(p2 = p + keylen; p2 < ep && !SEQDB_ISEOL(*p2); p2++)
                    ;
                
                // And end of that line (or of the file).
                offsets.push_back(p2);
                
                p = p2;
            }
        }
    }
    
    // As with ISAM files, we append an additional pointer, to
    // indicate the end of the last entry's contents.
    
    offsets.push_back(ep);
}


void CSeqDBAliasSets::x_ReadAliasSetFile(const CSeqDB_Path & aset_path,
                                         CSeqDBLockHold    & locked)
{
    string key("ALIAS_FILE");
    
    CSeqDBMemLease lease(m_Atlas);
    
    CSeqDBAtlas::TIndx length(0);
    m_Atlas.GetFile(lease, aset_path, length, locked);
    
    const char * bp = lease.GetPtr(0);
    const char * ep = bp + (size_t) length;
    
    vector<const char *> offsets;
    
    s_SeqDB_FindOffsets(bp, ep, key, offsets);
    
    // Now, for each offset, read the "ALIAS_FILE" line and store the
    // contents of that (virtual) file in the alias set.
    
    if (offsets.size() > 2) {
        size_t last_start = offsets.size() - 2;
        
        string name, value;
        
        TAliasGroup & group = m_Groups[aset_path.GetPathS()];
        
        for(size_t i = 0; i < last_start; i += 2) {
            // The line being read here is "ALIAS_FILE <filename>"
            
            s_SeqDB_ReadLine(offsets[i],
                             offsets[i+1],
                             name,
                             value);
            
            if (name != key || value.empty()) {
                string msg("Alias set file: syntax error near offset "
                           + NStr::NumericToString(offsets[i] - bp) + ".");
                
                NCBI_THROW(CSeqDBException, eFileErr, msg);
            }
            
            group[value].assign(offsets[i+1], offsets[i+2]);
        }
    }
    
    m_Atlas.RetRegion(lease);
}


bool CSeqDBAliasSets::ReadAliasFile(const CSeqDB_Path  & dbpath,
                                    const char        ** bp,
                                    const char        ** ep,
                                    CSeqDBLockHold     & locked)
{
    // Compute name of combined alias file.
    
    CSeqDB_Path     aset_path;
    CSeqDB_FileName alias_fname;
    
    x_DbToIndexName(dbpath, aset_path, alias_fname);
    
    // Check whether we already have this combined alias file.
    
    if (m_Groups.find(aset_path.GetPathS()) == m_Groups.end()) {
        if (! m_Atlas.DoesFileExist(aset_path, locked)) {
            return false;
        }
        
        x_ReadAliasSetFile(aset_path, locked);
    }
    
    // Find and read the specific, included, alias file.
    
    TAliasGroup & group = m_Groups[aset_path.GetPathS()];
    
    if (group.find(alias_fname.GetFileNameS()) == group.end()) {
        return false;
    }
        
    // It would be simpler to move the if (bp||ep) test out to here,
    // and instead just not add any empty files to the map.  In fact,
    // it may already avoid adding empty files...
    
    // Also, it would probably be a good idea to trim whitespace from
    // the top and bottom of alias file contents, since it is nearly
    // free to do so before the strings are actually constructed.
    
    const string & file_data = group[alias_fname.GetFileNameS()];
    
    if (file_data.empty()) {
        return false;
    }
    
    if (bp || ep) {
        _ASSERT(bp && ep);
        
        *bp = file_data.data();
        *ep = file_data.data() + file_data.size();
    }
    
    return true;
}


void CSeqDBAliasNode::x_ReadAliasFile(CSeqDBMemLease    & lease,
                                      const CSeqDB_Path & path,
                                      const char       ** bp,
                                      const char       ** ep,
                                      CSeqDBLockHold    & locked)
{
    bool has_group_file = false;
    
    has_group_file = m_AliasSets.ReadAliasFile(path, bp, ep, locked);
    
    if (! has_group_file) {
        CSeqDBAtlas::TIndx length(0);
        m_Atlas.GetFile(lease, path, length, locked);
        
        *bp = lease.GetPtr(0);
        *ep = (*bp) + length;
    }
}


void CSeqDBAliasNode::x_ReadValues(const CSeqDB_Path & path,
                                   CSeqDBLockHold    & locked)
{
    m_Atlas.Lock(locked);
    
    CSeqDBMemLease lease(m_Atlas);
    
    const char * bp(0);
    const char * ep(0);
    
    x_ReadAliasFile(lease, path, & bp, & ep, locked);
    
    const char * p  = bp;
    
    // Existence should already be verified.
    _ASSERT(bp);
    
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
    
    m_Atlas.RetRegion(lease);
}


void CSeqDBAliasNode::x_AppendSubNode(CSeqDB_BasePath  & node_path,
                                      char               prot_nucl,
                                      CSeqDBAliasStack & recurse,
                                      CSeqDBLockHold   & locked)
{
    CSeqDB_DirName  dirname (node_path.FindDirName());
    CSeqDB_BaseName basename(node_path.FindBaseName());
    
    CRef<CSeqDBAliasNode>
        subnode( new CSeqDBAliasNode(m_Atlas,
                                     dirname,
                                     basename,
                                     prot_nucl,
                                     recurse,
                                     locked,
                                     m_AliasSets,
                                     m_ExpandLinks) );
    
    m_SubNodes.push_back(subnode);
}


void CSeqDBAliasNode::x_ExpandAliases(const CSeqDB_BasePath & this_name,
                                      char                    prot_nucl,
                                      CSeqDBAliasStack      & recurse,
                                      CSeqDBLockHold        & locked)
{
    if (m_DBList.empty()) {
        string situation;
        
        if (this_name.GetBasePathS() == "-") {
            situation = "passed to CSeqDB::CSeqDB().";
        } else {
            situation = string("found in alias file [")
                + this_name.GetBasePathS() + "].";
        }
        
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   string("No database names were ") + situation);
    }
    
    for(size_t i = 0; i < m_DBList.size(); i++) {
        // Inquiry: Is the following comparison correct for all
        // combinations of alias file and database name and path?
        // Which is to say, does it correctly deal with names of alias
        // files and names of volumes that collide?
        
        // If there is a directory on the mentioned name, we assume
        // this is NOT an overriding alias file, and skip the test
        // that treats it as a volume name.

        // If this an alias file refers to a volume of the same name
        // using "../<cwd>", it will detect and fail with an alias
        // file cyclicality message at this point.
        
        // If the base name of the alias file is also listed in
        // "dblist", it is assumed to refer to a volume instead of
        // to itself.  In this case, we do NOT search local copy

        if (m_DBList[i].FindDirName().Empty()) {
            if (m_DBList[i].FindBaseName() == this_name.FindBaseName()) {
                
                string normal_name;
                if (m_ExpandLinks) {
                    // normalize this_name
                    string dir_name, base_name;
                    this_name.FindDirName().GetString(dir_name);
                    this_name.FindBaseName().GetString(base_name);
                    normal_name = CDirEntry::NormalizePath(dir_name, eFollowLinks) + 
                      CDirEntry::GetPathSeparator() + base_name;
                } else {
                    normal_name = this_name.GetBasePathS();
                }

                bool found = false;
                for (int i = 0; i < (int) m_VolNames.size(); i++) {
                    if (m_VolNames[i].GetBasePathS() == normal_name) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    m_VolNames.push_back(CSeqDB_BasePath(normal_name));
                }
                continue;
            }
        }

        // Join the "current" directory (location of this alias node)          
        // to the path specified in the alias file.                            
                                                                               
        CSeqDB_BasePath base(m_DBPath, m_DBList[i]);                           
        CSeqDB_Path new_db_path( base, prot_nucl, 'a', 'l' );                  
                                                                               
        if ( recurse.Exists(new_db_path) ) {                                   
            NCBI_THROW(CSeqDBException,                                        
                       eFileErr,                                               
                       "Illegal configuration: DB alias files are mutually recursive.");     
        }                                                                      
                                                                               
        // If we find the new name in the combined alias file or one           
        // of the individual ones, build a subnode.                            
                                                                               
        if ( m_AliasSets.FindAliasPath(new_db_path, 0, locked) ||              
             m_Atlas.DoesFileExist(new_db_path, locked) ) {                    
                                                                               
            x_AppendSubNode(base, prot_nucl, recurse, locked);
            continue;                                                          
        }                                                               
        
        // The name was not found as an alias file, so check for the
        // existence of a volume file at the same location.
        
        bool found = false;
        CSeqDB_BasePath bp;

        // Always check local copy first unless full path is given
        if (!m_SkipLocal[i]) {
            // GCC 3.0.4 requires the extra parentheses below.
            CSeqDB_BasePath local_base((CSeqDB_DirName(CDir::GetCwd())),
                                   CSeqDB_BaseName(m_DBList[i].FindBaseName()));
            CSeqDB_Path new_local_vol_path(local_base, prot_nucl, 'i', 'n' );
            if (m_Atlas.DoesFileExist(new_local_vol_path, locked)) {
                bp = CSeqDB_BasePath(new_local_vol_path.FindBasePath());
                found = true;
            }
        } 

        if (!found) {
            CSeqDB_Path new_vol_path( base, prot_nucl, 'i', 'n' );
            if (m_Atlas.DoesFileExist(new_vol_path, locked)) {
                bp = CSeqDB_BasePath(new_db_path.FindBasePath() );
                found = true;
            }
        }

        if (found) {
            string normal_name;
            if (m_ExpandLinks) {
                // normalize this_name
                string dir_name, base_name;
                bp.FindDirName().GetString(dir_name);
                bp.FindBaseName().GetString(base_name);
                normal_name = CDirEntry::NormalizePath(dir_name, eFollowLinks) + 
                  CDirEntry::GetPathSeparator() + base_name;
            } else {
                normal_name = bp.GetBasePathS();
            }
            found = false;
            for (int i = 0; i < (int) m_VolNames.size(); i++) {
                if (m_VolNames[i].GetBasePathS() == normal_name) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                m_VolNames.push_back(CSeqDB_BasePath(normal_name));
            }

            continue;
        }

        // If all that failed, "restart" the search using the blast DB
        // path configuration.  This ensures always finding the local
        // copy of a database even if the alias file resides on remote site.
        
        CSeqDB_BasePath result;
        CSeqDB_BasePath restart(m_DBList[i]);
        
        if (m_AliasSets.FindBlastDBPath(restart, prot_nucl, result, locked)) {
            // either alias or volume file exists for this entry
            CSeqDB_Path new_alias( result, prot_nucl, 'a', 'l' );
            CSeqDB_Path new_volume( result, prot_nucl, 'i', 'n' );

            if (m_Atlas.DoesFileExist(new_alias, locked)) {
                x_AppendSubNode( result, prot_nucl, recurse, locked );
            } else if (m_Atlas.DoesFileExist(new_volume, locked)) {
                string normal_name;
                if (m_ExpandLinks) {
                    // normalize this_name
                    string dir_name, base_name;
                    result.FindDirName().GetString(dir_name);
                    result.FindBaseName().GetString(base_name);
                    normal_name = CDirEntry::NormalizePath(dir_name, eFollowLinks) + 
                      CDirEntry::GetPathSeparator() + base_name;
                } else {
                    normal_name = result.GetBasePathS();
                }
                bool found = false;
                for (int i = 0; i < (int) m_VolNames.size(); i++) {
                    if (m_VolNames[i].GetBasePathS() == normal_name) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    m_VolNames.push_back(CSeqDB_BasePath(normal_name));
                }
            }
            continue;
        }
        

        ostringstream oss;
        oss << "Could not find volume or alias file ("
            << m_DBList[i].GetBasePathS() << ") referenced in alias file ("
            << this_name.GetBasePathS() << ").";
        
        NCBI_THROW(CSeqDBException, eFileErr, oss.str());
    }
}

void CSeqDBAliasNode::FindVolumePaths(vector<string> & vols, vector<string> * alias, bool recursive) const
{

    set<string> volset;
    set<string> aliset;

    if (recursive) {
        x_FindVolumePaths(volset, aliset);
    } else {
        // No alias file list is populated in the non-recursive mode
        ITERATE(vector<CSeqDB_BasePath>, path, m_VolNames) {
            volset.insert(path->GetBasePathS());
        }
        ITERATE(TSubNodeList, iter, m_SubNodes) {
            ITERATE(vector<CSeqDB_BasePath>, path, (*iter)->m_VolNames) {
                volset.insert(path->GetBasePathS());
            }
            ITERATE(TSubNodeList, sub, (*iter)->m_SubNodes) {
                volset.insert(((*sub)->m_ThisName).GetPathS());
            }
        }
    }
    
    vols.clear();
    ITERATE(set<string>, iter, volset) {
        vols.push_back(*iter);
    }
    
    // Sort to insure deterministic order.
    sort(vols.begin(), vols.end(), SeqDB_CompareVolume);

    if (alias) {
        alias->clear();
        ITERATE(set<string>, iter, aliset) {
            alias->push_back(*iter);
        }
        sort(alias->begin(), alias->end(), SeqDB_CompareVolume);
    }
}


void CSeqDBAliasNode::x_FindVolumePaths(set<string> & vols, set<string> & alias) const
{
    ITERATE(TVolNames, iter, m_VolNames) {
        vols.insert(iter->GetBasePathS());
    }

    string alias_base = m_ThisName.GetPathS();
    if (alias_base != "-") {
        alias.insert(m_ThisName.GetPathS());
    }
    
    ITERATE(TSubNodeList, iter, m_SubNodes) {
        (*iter)->x_FindVolumePaths(vols, alias);
    }
}


/// Walker for TITLE field of alias file
///
/// The TITLE field of the alias file is a string describing the set
/// of sequences collected by that file.  The title is reported via
/// the "CSeqDB::GetTitle()" method.

class CSeqDB_TitleWalker : public CSeqDB_AliasWalker {
public:
    /// This provides the alias file key used for this field.
    virtual const char * GetFileKey() const
    {
        return "TITLE";
    }
    
    /// Collect data from a volume
    ///
    /// If the TITLE field is not specified in an alias file, we can
    /// use the title(s) in the database volume(s).  Values from alias
    /// node tree siblings are concatenated with "; " used as a
    /// delimiter.
    ///
    /// @param vol
    ///   A database volume
    virtual void Accumulate(const CSeqDBVol & vol)
    {
        AddString( vol.GetTitle() );
    }
    
    /// Collect data from an alias file
    ///
    /// If the TITLE field is specified in an alias file, it will be
    /// used unmodified.  Values from alias node tree siblings are
    /// concatenated with "; " used as a delimiter.
    ///
    /// @param value
    ///   A database volume
    virtual void AddString(const string & value)
    {
        SeqDB_JoinDelim(m_Value, value, "; ");
    }
    
    /// Returns the database title string.
    string GetTitle()
    {
        return m_Value;
    }
    
private:
    /// The title string we are accumulating.
    string m_Value;
};


/// Walker for MAX_SEQ_LENGTH field of alias file
///
/// This functor encapsulates the specifics of the MAX_SEQ_LENGTH
/// field of the alias file.  The NSEQ fields specifies the number of
/// sequences to use when reporting information via the
/// "CSeqDB::GetNumSeqs()" method.  It is not the same as the number
/// of OIDs unless there are no filtering mechanisms in use.
/// (Note: this seems to be unused.)

class CSeqDB_MaxLengthWalker : public CSeqDB_AliasWalker {
public:
    /// Constructor
    CSeqDB_MaxLengthWalker()
    {
        m_Value = 0;
    }
    
    /// This provides the alias file key used for this field.
    virtual const char * GetFileKey() const
    {
        return "MAX_SEQ_LENGTH";
    }
    
    /// Collect data from the volume
    ///
    /// If the MAX_SEQ_LENGTH field is not specified in an alias file,
    /// the maximum values of all contributing volumes is used.
    ///
    /// @param vol
    ///   A database volume
    virtual void Accumulate(const CSeqDBVol & vol)
    {
        int new_max = vol.GetMaxLength();
        
        if (new_max > m_Value)
            m_Value = new_max;
    }
    
    /// Collect data from an alias file
    ///
    /// Values from alias node tree siblings are compared, and the
    /// maximum value is used as the result.
    ///
    /// @param value
    ///   A database volume
    virtual void AddString(const string & value)
    {
        int new_max = NStr::StringToUInt(value);
        
        if (new_max > m_Value)
            m_Value = new_max;
    }
    
    /// Returns the maximum sequence length.
    int GetMaxLength()
    {
        return m_Value;
    }
    
private:
    /// The maximum sequence length.
    int m_Value;
};

class CSeqDB_MinLengthWalker : public CSeqDB_AliasWalker {
public:
    /// Constructor
    CSeqDB_MinLengthWalker()
    {
        m_Value = INT4_MAX;
    }
    
    /// This provides the alias file key used for this field.
    virtual const char * GetFileKey() const
    {
        return "MIN_SEQ_LENGTH";
    }
    
    /// Collect data from the volume
    ///
    /// If the MAX_SEQ_LENGTH field is not specified in an alias file,
    /// the maximum values of all contributing volumes is used.
    ///
    /// @param vol
    ///   A database volume
    virtual void Accumulate(const CSeqDBVol & vol)
    {
        int new_min = vol.GetMinLength();
        
        if (new_min < m_Value) {
            m_Value = new_min;
        }
    }
    
    /// Collect data from an alias file
    ///
    /// Values from alias node tree siblings are compared, and the
    /// maximum value is used as the result.
    ///
    /// @param value
    ///   A database volume
    virtual void AddString(const string & value)
    {
        int new_min = NStr::StringToUInt(value);
        
        if (new_min < m_Value) {
            m_Value = new_min;
        }
    }
    
    /// Returns the maximum sequence length.
    int GetMinLength()
    {
        return m_Value;
    }
    
private:
    /// The maximum sequence length.
    int m_Value;
};

/// Walker for NSEQ field of alias file
///
/// The NSEQ field of the alias file specifies the number of sequences
/// to use when reporting information via the "CSeqDB::GetNumSeqs()"
/// method.  It is not the same as the number of OIDs unless there are
/// no filtering mechanisms in use.

class CSeqDB_NSeqsWalker : public CSeqDB_AliasWalker {
public:
    /// Constructor
    CSeqDB_NSeqsWalker()
    {
        m_Value = 0;
    }
    
    /// This provides the alias file key used for this field.
    virtual const char * GetFileKey() const
    {
        return "NSEQ";
    }
    
    /// Collect data from the volume
    ///
    /// If the NSEQ field is not specified in an alias file, the
    /// number of OIDs in the volume is used instead.
    ///
    /// @param vol
    ///   A database volume
    virtual void Accumulate(const CSeqDBVol & vol)
    {
        m_Value += vol.GetNumOIDs();
    }
    
    /// Collect data from an alias file
    ///
    /// If the NSEQ field is specified in an alias file, it will be
    /// used.  Values from alias node tree siblings are summed.
    ///
    /// @param value
    ///   A database volume
    virtual void AddString(const string & value)
    {
        m_Value += NStr::StringToInt8(value);
    }
    
    /// Returns the accumulated number of OIDs.
    Int8 GetNum() const
    {
        return m_Value;
    }
    
private:
    /// The accumulated number of OIDs.
    Int8 m_Value;
};


/// Walker for OID count accumulation.
///
/// The number of OIDs should be like the number of sequences, but
/// without the value adjustments made by alias files.  To preserve
/// this relationship, this class inherits from CSeqDB_NSeqsWalker.

class CSeqDB_NOIDsWalker : public CSeqDB_NSeqsWalker {
public:
    /// This disables the key; the spaces would not be preserved, so
    /// this is a non-matchable string in this context.
    virtual const char * GetFileKey() const
    {
        return " no key ";
    }
};

/// Walker for STATS_NSEQ field of alias file
///
/// The STATS_NSEQ field of the alias file specifies the number of
/// sequences to use when reporting information via the
/// "CSeqDB::GetNumSeqsStats()" method.  It is not the same as the
/// number of OIDs unless there are no filtering mechanisms in use.

class CSeqDB_NSeqsStatsWalker : public CSeqDB_NSeqsWalker {
public:
    /// This does the same calculation as above but uses another key.
    virtual const char * GetFileKey() const
    {
        return "STATS_NSEQ";
    }
    
    /// STATS_NSEQ field
    ///
    /// The STATS_* versions of these walkers do not return volume
    /// lengths, instead the value zero will be returned if the field
    /// is not specified.
    ///
    /// @param vol
    ///   A database volume
    virtual void Accumulate(const CSeqDBVol & vol)
    {
        // only alias file data is included.
    }
};


/// Walker for total length accumulation.
///
/// The total length of the database is the sum of the lengths of all
/// volumes of the database (measured in bases).

class CSeqDB_TotalLengthWalker : public CSeqDB_AliasWalker {
public:
    /// Constructor
    CSeqDB_TotalLengthWalker()
    {
        m_Value = 0;
    }
    
    /// This provides the alias file key used for this field.
    virtual const char * GetFileKey() const
    {
        return "LENGTH";
    }
    
    /// Collect data from the volume
    ///
    /// If the LENGTH field is not specified in an alias file, the
    /// sum of the volume lengths will be used.
    ///
    /// @param vol
    ///   A database volume
    virtual void Accumulate(const CSeqDBVol & vol)
    {
        m_Value += vol.GetVolumeLength();
    }
    
    /// Collect data from an alias file
    ///
    /// If the LENGTH field is specified in an alias file, it will be
    /// used.  Values from alias node tree siblings are summed.
    ///
    /// @param value
    ///   A database volume
    virtual void AddString(const string & value)
    {
        m_Value += NStr::StringToUInt8(value);
    }
    
    /// Returns the accumulated volume length.
    Uint8 GetLength() const
    {
        return m_Value;
    }
    
private:
    /// The accumulated volume length.
    Uint8 m_Value;
};


/// Walker for volume length accumulation.
///
/// The volume length should be like total length, but without the
/// value adjustments made by alias files.  To preserve this
/// relationship, this class inherits from CSeqDB_TotalLengthWalker.
/// (Note: this seems to be unused.)

class CSeqDB_VolumeLengthWalker : public CSeqDB_TotalLengthWalker {
public:
    /// This disables the key; the spaces would not be preserved, so
    /// this is a non-matchable string in this context.
    virtual const char * GetFileKey() const
    {
        return " no key ";
    }
};


/// Walker for total length stats accumulation.
///
/// The total length of the database is the sum of the lengths of all
/// volumes of the database (measured in bases).

class CSeqDB_TotalLengthStatsWalker : public CSeqDB_TotalLengthWalker {
public:
    /// This does the same calculation as above but uses another key.
    virtual const char * GetFileKey() const
    {
        return "STATS_TOTLEN";
    }
    
    /// STATS_TOTLEN field
    ///
    /// The STATS_* versions of these walkers do not return volume
    /// lengths, instead the value zero will be returned if the field
    /// is not specified.
    ///
    /// @param vol
    ///   A database volume
    virtual void Accumulate(const CSeqDBVol & vol)
    {
        // only alias file data is included.
    }
};


/// Walker for membership bit
///
/// This just searches alias files for the membership bit if one is
/// specified.

class CSeqDB_MembBitWalker : public CSeqDB_AliasWalker {
public:
    /// Constructor
    CSeqDB_MembBitWalker()
    {
        m_Value = 0;
    }
    
    /// This provides the alias file key used for this field.
    virtual const char * GetFileKey() const
    {
        return "MEMB_BIT";
    }
    
    /// Collect data from the volume
    ///
    /// If the MEMB_BIT field is not specified in an alias file, then
    /// it is not needed.  This field is intended to allow filtration
    /// of deflines by taxonomic category, which is only needed if an
    /// alias file reduces the taxonomic scope.
    virtual void Accumulate(const CSeqDBVol &)
    {
        // Volumes don't have this data, only alias files.
    }
    
    /// Collect data from an alias file
    ///
    /// If the MEMB_BIT field is specified in an alias file, it will
    /// be used unmodified.  No attempt is made to combine or collect
    /// bit values - currently, only one can be used at a time.
    ///
    /// @param value
    ///   A database volume
    virtual void AddString(const string & value)
    {
        m_Value = NStr::StringToUInt(value);
    }
    
    /// Returns the membership bit.
    int GetMembBit() const
    {
        return m_Value;
    }
    
private:
    /// The membership bit.
    int m_Value;
};


/// Test for completeness of GI list alias file values.
///
/// This searches alias files to determine whether NSEQS and LENGTH
/// are specified in all of the cases where they should be.  If any
/// volume has a GI list but the number of included sequences or
/// length is not specified, then SeqDB must scan the database to
/// compute this length.

class CSeqDB_IdListValuesTest : public CSeqDB_AliasExplorer {
public:
    /// Constructor
    CSeqDB_IdListValuesTest()
    {
        m_NeedScan = false;
    }
    
    /// Collect data from the volume
    ///
    /// Volume data is not used by this class.
    virtual void Accumulate(const CSeqDBVol &)
    {
        // Volumes don't have this data, only alias files.
    }
    
    /// Explore the values in this alias file
    ///
    /// If the NSEQ and LENGTH fields are specified, this method can
    /// close this branch of the traversal tree.  Otherwise, if the
    /// GILIST or TILIST is specified, then this branch of the
    /// traversal will fail to produce accurate totals information,
    /// therefore an oid scan is required, and we are done.
    /// 
    /// @param vars
    ///   The name/value mapping for this node.
    /// @return
    ///   True if the traversal should cease descent.
    virtual bool Explore(const TVarList & vars)
    {
        // If we already know that a scan is needed, we can skip all
        // further analysis (by returning true at all points).
        
        if (m_NeedScan)
            return true;
        
        // If we find both NSEQ and LENGTH, then this branch of the
        // alias file is covered.
        
        if (vars.find("NSEQ")   != vars.end() &&
            vars.find("LENGTH") != vars.end()) {
            
            return true;
        }
        
        // If we we have an attached GILIST (but don't have both NSEQ
        // and LENGTH), then we need to scan the entire database.
        
        if (vars.find("GILIST") != vars.end()) {
            m_NeedScan = true;
            return true;
        }
        
        // Ditto for an attached TILIST.
        
        if (vars.find("TILIST") != vars.end()) {
            m_NeedScan = true;
            return true;
        }
        
        // Ditto for an attached SILIST.
        
        if (vars.find("SEQIDLIST") != vars.end()) {
            m_NeedScan = true;
            return true;
        }
        
        // If none of those conditions is met, traversal proceeds.
        return false;
    }
    
    /// Returns true if a scan is required.
    bool NeedScan() const
    {
        return m_NeedScan;
    }
    
private:
    /// True unless/until a node with incomplete totals was found.
    bool m_NeedScan;
};


void
CSeqDBAliasNode::WalkNodes(CSeqDB_AliasWalker * walker,
                           const CSeqDBVolSet & volset) const
{
    TVarList::const_iterator value =
        m_Values.find(walker->GetFileKey());
    
    if (value != m_Values.end()) {
        walker->AddString( (*value).second );
        return;
    }
    
    ITERATE(TSubNodeList, node, m_SubNodes) {
        (*node)->WalkNodes( walker, volset );
    }
    
    ITERATE(TVolNames, volname, m_VolNames) {
        if (const CSeqDBVol * vptr = volset.GetVol(volname->GetBasePathS())) {
            walker->Accumulate( *vptr );
        }
    }
}


void
CSeqDBAliasNode::WalkNodes(CSeqDB_AliasExplorer * explorer,
                           const CSeqDBVolSet   & volset) const
{
    if (explorer->Explore(m_Values)) {
        return;
    }
    
    ITERATE(TSubNodeList, node, m_SubNodes) {
        (*node)->WalkNodes( explorer, volset );
    }
    
    ITERATE(TVolNames, volname, m_VolNames) {
        if (const CSeqDBVol * vptr = volset.GetVol(volname->GetBasePathS())) {
            explorer->Accumulate( *vptr );
        }
    }
}

// This could be changed to use ComputeMasks, then apply the masks for
// each node in a second traversal.  However, it probably makes more
// sense to ignore this because eventually this functionality can be
// scrapped once the filter tree based OID wrangling is ready.

string CSeqDBAliasNode::GetTitle(const CSeqDBVolSet & volset) const
{
    CSeqDB_TitleWalker walk;
    WalkNodes(& walk, volset);
    
    return walk.GetTitle();
}

Int4 CSeqDBAliasNode::GetMinLength(const CSeqDBVolSet & vols) const
{
    CSeqDB_MinLengthWalker walk;
    WalkNodes(& walk, vols);

    return walk.GetMinLength();
}

Int8 CSeqDBAliasNode::GetNumSeqs(const CSeqDBVolSet & vols) const
{
    CSeqDB_NSeqsWalker walk;
    WalkNodes(& walk, vols);
    
    return walk.GetNum();
}

Int8 CSeqDBAliasNode::GetNumSeqsStats(const CSeqDBVolSet & vols) const
{
    CSeqDB_NSeqsStatsWalker walk;
    WalkNodes(& walk, vols);
    
    return walk.GetNum();
}

Int8 CSeqDBAliasNode::GetNumOIDs(const CSeqDBVolSet & vols) const
{
    CSeqDB_NOIDsWalker walk;
    WalkNodes(& walk, vols);
    
    return walk.GetNum();
}

Uint8 CSeqDBAliasNode::GetTotalLength(const CSeqDBVolSet & volset) const
{
    CSeqDB_TotalLengthWalker walk;
    WalkNodes(& walk, volset);
    
    return walk.GetLength();
}

Uint8 CSeqDBAliasNode::GetTotalLengthStats(const CSeqDBVolSet & volset) const
{
    CSeqDB_TotalLengthStatsWalker walk;
    WalkNodes(& walk, volset);
    
    return walk.GetLength();
}

// (Note: this seems to be unused.)
Uint8 CSeqDBAliasNode::GetVolumeLength(const CSeqDBVolSet & volset) const
{
    CSeqDB_VolumeLengthWalker walk;
    WalkNodes(& walk, volset);
    
    return walk.GetLength();
}

int CSeqDBAliasNode::GetMembBit(const CSeqDBVolSet & volset) const
{
    CSeqDB_MembBitWalker walk;
    WalkNodes(& walk, volset);
    
    return walk.GetMembBit();
}


bool CSeqDBAliasNode::NeedTotalsScan(const CSeqDBVolSet & volset) const
{
    CSeqDB_IdListValuesTest explore;
    WalkNodes(& explore, volset);
    
    return explore.NeedScan();
}


void CSeqDBAliasNode::
CompleteAliasFileValues(const CSeqDBVolSet & volset)
{
    // First, complete the values stored in the child nodes.
    
    NON_CONST_ITERATE(TSubNodeList, node, m_SubNodes) {
        (**node).CompleteAliasFileValues(volset);
    }
    
    // Then, get the various values for this node.
    
    if (m_Values.find("TITLE") == m_Values.end()) {
        m_Values["TITLE"] = GetTitle(volset);
    }
}


void CSeqDBAliasNode::
GetAliasFileValues(TAliasFileValues & afv) const
{
    _ASSERT(m_ThisName.Valid());
    
    afv[m_ThisName.GetPathS()].push_back(m_Values);
    
    ITERATE(TSubNodeList, node, m_SubNodes) {
        (**node).GetAliasFileValues(afv);
    }
}

void CSeqDBAliasNode::
GetMaskList(vector <string> & mask_list)
{
    if (!m_HasGiMask) {
        return;
    }

    mask_list.clear();

    // parse the white spaces...
    vector <CTempString> masks;
    SeqDB_SplitQuoted(m_SubNodes[0]->m_Values["MASKLIST"], masks);
    ITERATE(vector <CTempString>, mask, masks) {
        mask_list.push_back(string(*mask));
    }
}

void CSeqDBAliasFile::GetAliasFileValues(TAliasFileValues   & afv,
                                         const CSeqDBVolSet & volset)
{
    m_Node->CompleteAliasFileValues(volset);
    
    // Now complete the 'volume' values.
    for(int i = 0; i < volset.GetNumVols(); i++) {
        const CSeqDBVol * v = volset.GetVol(i);
        
        string key = v->GetVolName();
        
        if (afv.find(key) != afv.end()) {
            // If this name already corresponds to an alias file,
            // don't replace it with a volume.
            continue;
        }
        
        // Add the title of the volume.
        map<string,string> values;
        values["TITLE"] = v->GetTitle();
        
        string extn = (m_IsProtein ? ".pin" : ".nin");
        
        afv[v->GetVolName() + extn].push_back(values);
    }
    
    m_Node->GetAliasFileValues(afv);
}


int CSeqDBAliasFile::GetMembBit(const CSeqDBVolSet & volset) const
{
    // Default is zero; -1 means not-computed-yet.
    if (m_MembBit == -1) {
        m_MembBit = m_Node->GetMembBit(volset);
    }
    
    return m_MembBit;
}


string CSeqDBAliasFile::GetTitle(const CSeqDBVolSet & volset) const
{
    if (! m_HasTitle)
        m_Title = m_Node->GetTitle(volset);
    
    return m_Title;
}

Int4 CSeqDBAliasFile::GetMinLength(const CSeqDBVolSet & volset) const
{
    if (m_MinLength == -1)
        m_MinLength = m_Node->GetMinLength(volset);

    return m_MinLength;
}

Int8 CSeqDBAliasFile::GetNumSeqs(const CSeqDBVolSet & volset) const
{
    if (m_NumSeqs == -1)
        m_NumSeqs = m_Node->GetNumSeqs(volset);
    
    return m_NumSeqs;
}


Int8 CSeqDBAliasFile::GetNumSeqsStats(const CSeqDBVolSet & volset) const
{
    if (m_NumSeqsStats == -1)
        m_NumSeqsStats = (int)m_Node->GetNumSeqsStats(volset);
    
    return m_NumSeqsStats;
}


Int8 CSeqDBAliasFile::GetNumOIDs(const CSeqDBVolSet & volset) const
{
    if (m_NumOIDs == -1)
        m_NumOIDs = m_Node->GetNumOIDs(volset);
    
    return m_NumOIDs;
}


Uint8 CSeqDBAliasFile::GetTotalLength(const CSeqDBVolSet & volset) const
{
    if (m_TotalLength == -1)
        m_TotalLength = m_Node->GetTotalLength(volset);
    
    return m_TotalLength;
}


Uint8 CSeqDBAliasFile::GetTotalLengthStats(const CSeqDBVolSet & volset) const
{
    if (m_TotalLengthStats == -1)
        m_TotalLengthStats = m_Node->GetTotalLengthStats(volset);
    
    return m_TotalLengthStats;
}


Uint8 CSeqDBAliasFile::GetVolumeLength(const CSeqDBVolSet & volset) const
{
    if (m_VolumeLength == -1)
        m_VolumeLength = m_Node->GetVolumeLength(volset);
    
    return m_VolumeLength;
}


bool CSeqDBAliasFile::NeedTotalsScan(const CSeqDBVolSet & volset) const
{
    if (m_NeedTotalsScan == -1) {
        bool need = m_Node->NeedTotalsScan(volset);
        m_NeedTotalsScan = need ? 1 : 0;
    }
    return m_NeedTotalsScan == 1;
}

void CSeqDBAliasNode::BuildFilterTree(CSeqDB_FilterTree & ftree) const
{
    ftree.SetName(m_ThisName.GetPathS());
    ftree.AddFilters(m_NodeMasks);
    
    ITERATE(TSubNodeList, node, m_SubNodes) {
        CRef<CSeqDB_FilterTree> subtree(new CSeqDB_FilterTree);
        
        (*node)->BuildFilterTree( *subtree );
        ftree.AddNode(subtree);
    }
    
    ITERATE(TVolNames, volname, m_VolNames) {
        ftree.AddVolume(*volname);
    }
}

CRef<CSeqDB_FilterTree> CSeqDBAliasFile::GetFilterTree()
{
    if (m_TopTree.Empty()) {
        x_ComputeMasks();
        
        m_TopTree.Reset(new CSeqDB_FilterTree);
        m_Node->BuildFilterTree(*m_TopTree);
    }
    
    return m_TopTree;
}

void CSeqDBAliasNode::ComputeMasks(bool & has_filters)
{
    if (! m_NodeMasks.empty()) {
        return;
    }
    
    typedef CSeqDB_AliasMask TMask;
    
    TVarList::iterator gil_iter   = m_Values.find(string("GILIST"));
    TVarList::iterator til_iter   = m_Values.find(string("TILIST"));
    TVarList::iterator sil_iter   = m_Values.find(string("SEQIDLIST"));
    TVarList::iterator oid_iter   = m_Values.find(string("OIDLIST"));
    TVarList::iterator f_oid_iter = m_Values.find(string("FIRST_OID"));
    TVarList::iterator l_oid_iter = m_Values.find(string("LAST_OID"));
    TVarList::iterator mbit_iter  = m_Values.find(string("MEMB_BIT"));
    
    if (! m_DBList.empty()) {
        if (oid_iter   != m_Values.end() ||
            gil_iter   != m_Values.end() ||
            til_iter   != m_Values.end() ||
            sil_iter   != m_Values.end() ||
            f_oid_iter != m_Values.end() ||
            l_oid_iter != m_Values.end() ||
            mbit_iter  != m_Values.end()) {
            
            has_filters = true;
            
            int first_oid = 0;
            int last_oid  = INT_MAX;
            bool has_range = false;
            
            if (f_oid_iter != m_Values.end()) {
                first_oid = NStr::StringToUInt(f_oid_iter->second);
                
                // Starts at one, adjust to zero-indexed.
                if (first_oid)
                    first_oid--;
                
                has_range = true;
            }
            
            if (l_oid_iter != m_Values.end()) {
                // Zero indexing and post notation adjustments cancel.
                last_oid = NStr::StringToUInt(l_oid_iter->second);
                has_range = true;
            }
            
            if (has_range) {
                CRef<TMask> mask(new TMask(first_oid, last_oid));
                m_NodeMasks.push_back(mask);
            }
            
            if (oid_iter != m_Values.end()) {
                CSeqDB_FileName lst(oid_iter->second);
                CSeqDB_Path lst_path(m_DBPath, lst);
                
                CRef<TMask> mask(new TMask(TMask::eOidList, lst_path));
                m_NodeMasks.push_back(mask);
            }
            
            if (gil_iter != m_Values.end()) {
                const string & gilname = gil_iter->second;
                
                if (gilname.find(" ") != gilname.npos) {
                    string msg =
                        string("Alias file (") + m_DBPath.GetDirNameS() +
                        ") has multiple GI lists (" + gilname + ").";
                    
                    NCBI_THROW(CSeqDBException, eFileErr, msg);
                }
                
                CSeqDB_FileName lst(gilname);
                CSeqDB_Path lst_path(m_DBPath, lst);
                
                CRef<TMask> mask(new TMask(TMask::eGiList, lst_path));
                m_NodeMasks.push_back(mask);
            }
            
            if (til_iter != m_Values.end()) {
                const string & tilname = til_iter->second;
                
                if (tilname.find(" ") != tilname.npos) {
                    string msg =
                        string("Alias file (") + m_DBPath.GetDirNameS() +
                        ") has multiple TI lists (" + tilname + ").";
                    
                    NCBI_THROW(CSeqDBException, eFileErr, msg);
                }
                
                CSeqDB_FileName lst(tilname);
                CSeqDB_Path lst_path(m_DBPath, lst);
                
                CRef<TMask> mask(new TMask(TMask::eTiList, lst_path));
                m_NodeMasks.push_back(mask);
            }

            if (sil_iter != m_Values.end()) {
                const string & silname = sil_iter->second;
                
                if (silname.find(" ") != silname.npos) {
                    string msg =
                        string("Alias file (") + m_DBPath.GetDirNameS() +
                        ") has multiple SEQID lists (" + silname + ").";
                    
                    NCBI_THROW(CSeqDBException, eFileErr, msg);
                }
                
                CSeqDB_FileName lst(silname);
                CSeqDB_Path lst_path(m_DBPath, lst);
                
                CRef<TMask> mask(new TMask(TMask::eSiList, lst_path));
                m_NodeMasks.push_back(mask);
            }

            if (mbit_iter != m_Values.end()) {
                int mbit = NStr::StringToUInt(mbit_iter->second);
                CRef<TMask> mask(new TMask(mbit));
                m_NodeMasks.push_back(mask);
            }
            
        }
    }
    
    NON_CONST_ITERATE(TSubNodeList, sn, m_SubNodes) {
        (**sn).ComputeMasks(has_filters);
    }
}

END_NCBI_SCOPE

