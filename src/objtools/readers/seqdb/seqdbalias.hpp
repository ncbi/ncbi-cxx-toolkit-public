#ifndef OBJTOOLS_READERS_SEQDB__SEQDBALIAS_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBALIAS_HPP

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

/// CSeqDBAlias class
/// 
/// This object defines access to one database aliasume.

#include <assert.h>
#include <iostream>

#include <objtools/readers/seqdb/seqdb.hpp>
#include "seqdbfile.hpp"
#include "seqdbvol.hpp"

/*
typedef struct _readdb_alias_file {
    CharPtr title,        // title of the database. 
        dblist,        // list of databases. 
        gilist,        // a gilist to be used with the database. 
        oidlist;    // an ordinal id list to be used with this database. 
    Int8    len;        // length of the database 
    Uint4    nseq;        // number of seqs of the database 
        Int4    first_oid;      // ordinal id range 
        Int4    last_oid;
        Int4    membership;
} ReadDBAlias, PNTR ReadDBAliasPtr;
*/

BEGIN_NCBI_SCOPE

using namespace ncbi;
using namespace ncbi::objects;


class CSeqDB_AliasWalker {
public:
    virtual ~CSeqDB_AliasWalker() {}
    
    virtual const char * GetFileKey(void)           = 0;
    virtual void         Accumulate(CSeqDBVol &)    = 0;
    virtual void         AddString (const string &) = 0;
};

class CSeqDBAliasNode : public CObject {
public:
    CSeqDBAliasNode(const string & dbpath,
                    const string & name_list,
                    char           prot_nucl,
                    bool           use_mmap);
    
    // Add our volumes and our subnode's volumes to the end of "vols".
    void GetVolumeNames(vector<string> & vols);
    
    // Compute title by recursive appending of subnodes values until
    // value specification or volume is reached.
    string GetTitle(vector< CRef<CSeqDBVol> > & vols);
    
    // Compute sequence count by recursive appending of subnodes
    // values until value specification or volume is reached.
    Uint4 GetNumSeqs(vector< CRef<CSeqDBVol> > & vols);
    
    // Compute sequence count by recursive appending of subnodes
    // values until value specification or volume is reached.
    Uint8 GetTotalLength(vector< CRef<CSeqDBVol> > & vols);
    
    void WalkNodes(CSeqDB_AliasWalker * walker, vector< CRef<CSeqDBVol> > & vols);
    
    void BuildMaskList(vector< CRef<CSeqDBVol> > & vols);
    
private:
    // To be called only from this class.  Note that the recursion
    // prevention list is passed by value.
    CSeqDBAliasNode(const string & dbpath,
                    const string & dbname,
                    char           prot_nucl,
                    bool           use_mmap,
                    set<string>    recurse);
    
    // Actual construction of the node
    // Reads file as a list of values.
    void x_ReadValues(const string & fn, bool use_mmap);
    
    // Reads one line, if it is a value pair it is added to the list.
    void x_ReadLine(const char * bp, const char * ep);
    
    // Expand all DB members as aliases if they exist.
    void x_ExpandAliases(const string & this_name,
                         char           ending,
                         bool           use_mmap,
                         set<string>  & recurse);
    
    string x_MkPath(const string & dir, const string & name, char prot_nucl)
    {
        return SeqDB_CombinePath(dir, name, '/') + "." + prot_nucl + "al";
    }
    
    // Add our volumes and our subnode's volumes to the end of "vols".
    void x_GetVolumeNames(set<string> & vols);
    
    // --- Data ---
    
    typedef map<string, string>             TVarList;
    typedef vector<string>                  TVolNames;
    typedef vector< CRef<CSeqDBAliasNode> > TSubNodeList;
    
    string       m_DBPath;
    TVarList     m_Values;
    TVolNames    m_VolNames;
    TSubNodeList m_SubNodes;
};


class CSeqDBAliasFile {
public:
    CSeqDBAliasFile(const string & dbpath, const string & name_list, char prot_nucl, bool use_mmap)
        : m_Node      (dbpath, name_list, prot_nucl, use_mmap)
//           m_Len       (0),
//           m_Nseq      (0),
//           m_FirstOid  (0),
//           m_LastOid   (0),
//           m_Membership(0)
    {
        m_Node.GetVolumeNames(m_VolumeNames);
    }
    
    const vector<string> & GetVolumeNames(void)
    {
        return m_VolumeNames;
    }
    
    // Add our volumes and our subnode's volumes to the end of "vols".
    string GetTitle(vector< CRef<CSeqDBVol> > & vols)
    {
        return m_Node.GetTitle(vols);
    }
    
    // Add our volumes and our subnode's seq counts.
    Uint4 GetNumSeqs(vector< CRef<CSeqDBVol> > & vols)
    {
        return m_Node.GetNumSeqs(vols);
    }
    
    // Add our volumes and our subnode's base lengths.
    Uint8 GetTotalLength(vector< CRef<CSeqDBVol> > & vols)
    {
        return m_Node.GetTotalLength(vols);
    }
    
    void BuildMaskList(vector< CRef<CSeqDBVol> > & vols)
    {
        cout << "Walking volume list (size " << vols.size()
             << "), using OIDLIST." << endl;
        
        m_Node.BuildMaskList(vols);
    }
    
private:
    CSeqDBAliasNode m_Node;
    vector<string>  m_VolumeNames;
    
//     string m_Dblist;
//     string m_Gilist;
//     string m_Oidlist;
    
//     Int8   m_Len;
//     Uint4  m_Nseq;
//     Int4   m_FirstOid;
//     Int4   m_LastOid;
//     Int4   m_Membership;
};


END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBALIAS_HPP


