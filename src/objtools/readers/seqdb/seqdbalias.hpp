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

#include <iostream>

#include <objtools/readers/seqdb/seqdb.hpp>
#include "seqdboidlist.hpp"
#include "seqdbfile.hpp"
#include "seqdbvol.hpp"
#include "seqdbvolset.hpp"
#include "seqdbgeneral.hpp"

BEGIN_NCBI_SCOPE

using namespace ncbi::objects;


class CSeqDB_AliasWalker {
public:
    virtual ~CSeqDB_AliasWalker() {}
    
    virtual const char * GetFileKey(void) const        = 0;
    virtual void         Accumulate(const CSeqDBVol &) = 0;
    virtual void         AddString (const string &)    = 0;
};

class CSeqDBAliasNode : public CObject {
public:
    CSeqDBAliasNode(CSeqDBAtlas    & atlas,
                    const string   & name_list,
                    char             prot_nucl,
                    bool             use_mmap);
    
    // Add our volumes and our subnode's volumes to the end of "vols".
    void GetVolumeNames(vector<string> & vols) const;
    
    // Compute title by recursive appending of subnodes values until
    // value specification or volume is reached.
    string GetTitle(const CSeqDBVolSet & volset) const;
    
    // Compute sequence count by recursive appending of subnodes
    // values until value specification or volume is reached.
    Uint4 GetNumSeqs(const CSeqDBVolSet & volset) const;
    
    // Compute sequence count by recursive appending of subnodes
    // values until value specification or volume is reached.
    Uint8 GetTotalLength(const CSeqDBVolSet & volset) const;
    
    void WalkNodes(CSeqDB_AliasWalker * walker,
                   const CSeqDBVolSet & volset) const;
    
    void SetMasks(CSeqDBVolSet & volset);
    
private:
    // To be called only from this class.  Note that the recursion
    // prevention list is passed by value.
    CSeqDBAliasNode(CSeqDBAtlas    & atlas,
                    const string   & dbpath,
                    const string   & dbname,
                    char             prot_nucl,
                    bool             use_mmap,
                    set<string>      recurse,
                    CSeqDBLockHold & locked);
    
    // Actual construction of the node
    // Reads file as a list of values.
    void x_ReadValues(const string & fn, bool use_mmap, CSeqDBLockHold & locked);
    
    // Reads one line, if it is a value pair it is added to the list.
    void x_ReadLine(const char * bp, const char * ep);
    
    // Expand all DB members as aliases if they exist.
    void x_ExpandAliases(const string   & this_name,
                         char             ending,
                         bool             use_mmap,
                         set<string>    & recurse,
                         CSeqDBLockHold & locked);
    
    string x_MkPath(const string & dir, const string & name, char prot_nucl) const
    {
        return SeqDB_CombinePath(dir, name) + "." + prot_nucl + "al";
    }
    
    // Add our volumes and our subnode's volumes to the end of "vols".
    void x_GetVolumeNames(set<string> & vols) const;
    
    void x_ResolveNames(string & dbname_list,
                        string & dbname_path,
                        char     prot_nucl);
    
    // --- Data ---
    
    typedef map<string, string>             TVarList;
    typedef vector<string>                  TVolNames;
    typedef vector< CRef<CSeqDBAliasNode> > TSubNodeList;
    
    CSeqDBAtlas & m_Atlas;
    string        m_DBPath;
    TVarList      m_Values;
    TVolNames     m_VolNames;
    TSubNodeList  m_SubNodes;
};


class CSeqDBAliasFile {
public:
    CSeqDBAliasFile(CSeqDBAtlas    & atlas,
                    const string   & name_list,
                    char             prot_nucl,
                    bool             use_mmap)
        : m_Node (atlas, name_list, prot_nucl, use_mmap)
    {
        m_Node.GetVolumeNames(m_VolumeNames);
    }
    
    const vector<string> & GetVolumeNames(void) const
    {
        return m_VolumeNames;
    }
    
    // Add our volumes and our subnode's volumes to the end of "vols".
    string GetTitle(const CSeqDBVolSet & volset) const
    {
        return m_Node.GetTitle(volset);
    }
    
    // Add our volumes and our subnode's seq counts.
    Uint4 GetNumSeqs(const CSeqDBVolSet & volset) const
    {
        return m_Node.GetNumSeqs(volset);
    }
    
    // Add our volumes and our subnode's base lengths.
    Uint8 GetTotalLength(const CSeqDBVolSet & volset) const
    {
        return m_Node.GetTotalLength(volset);
    }
    
    void SetMasks(CSeqDBVolSet & volset)
    {
        m_Node.SetMasks(volset);
    }
    
private:
    CSeqDBAliasNode m_Node;
    vector<string>  m_VolumeNames;
};


END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBALIAS_HPP


