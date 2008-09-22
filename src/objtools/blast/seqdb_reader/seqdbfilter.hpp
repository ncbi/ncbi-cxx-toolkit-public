#ifndef OBJTOOLS_READERS_SEQDB__SEQDBFILTER_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBFILTER_HPP

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

/// @file seqdbfilter.hpp
/// Implementation for some assorted ID list filtering code.
/// 
/// Defines classes:
///     CSeqDB_AliasMask
///     CSeqDB_FilterTree
/// 
/// Implemented for: UNIX, MS-Windows

#include "seqdbvol.hpp"

BEGIN_NCBI_SCOPE

/// Import definitions from the ncbi::objects namespace.
USING_SCOPE(objects);


/// Something else yet again etc.
class CSeqDB_AliasMask : public CObject {
public:
    /// Various types of masking.
    enum EMaskType {
        eGiList,  ///< GI list.
        eTiList,  ///< TI list.
        eOidList, ///< OID list.
        eOidRange ///< OID Range [start, end).
    };
    
    /// Constructor for file-based filtering.
    /// @param Type of file-based filtering to apply.
    /// @param fn Name of file containing included IDs info.
    CSeqDB_AliasMask(EMaskType mask_type, const CSeqDB_Path & fn)
        : m_MaskType(mask_type),
          m_Path    (fn),
          m_Begin   (0),
          m_End     (0)
    {
    }
    
    /// Constructor for OID range filter.
    /// @param begin First included OID.
    /// @param begin OID after last included OID.
    CSeqDB_AliasMask(int begin, int end)
        : m_MaskType(eOidRange),
          m_Begin   (begin),
          m_End     (end)
    {
    }
    
    /// Build string describing the filtering action.
    /// @return string describing the filtering action.
    string ToString() const
    {
        const char *p = 0;
        bool r = false;
        
        switch(m_MaskType) {
        case eGiList:   p="eGiList";   break;
        case eTiList:   p="eTiList";   break;
        case eOidList:  p="eOidList";  break;
        case eOidRange: p="eOidRange";
            r = true;
            break;
        }
        
        string oss;
        oss = oss + "{ " + p + ", ";
        
        if (r) {
            oss = oss + NStr::IntToString(m_Begin) + ":" + NStr::IntToString(m_End);
        } else {
            oss = oss + m_Path.GetPathS();
        }
        oss += " }";
        return oss;
    }
    
    /// Get type of filtering applied.
    /// @return type of filtering applied.
    EMaskType GetType() const
    {
        return m_MaskType;
    }
    
    /// Get path of file-based filter.
    /// @return path of file-based filter.
    CSeqDB_Path GetPath() const
    {
        return m_Path;
    }
    
    /// Get first included OID.
    /// @return First included OID.
    int GetBegin() const
    {
        return m_Begin;
    }
    
    /// Get OID after last included OID.
    /// @return OID after last included OID.
    int GetEnd() const
    {
        return m_End;
    }
    
private:
    /// Type of filtering to apply.
    EMaskType m_MaskType;
    
    /// Path of file describing included IDs.
    CSeqDB_Path m_Path;
    
    /// First included OID.
    int m_Begin;
    
    /// OID after last included OID.
    int m_End;
};


/// Tree of nodes describing filtering of database sequences.
class CSeqDB_FilterTree : public CObject {
public:
    /// Type used to store lists of filters found here.
    typedef vector< CRef<CSeqDB_AliasMask> > TFilters;
    
    /// Construct.
    CSeqDB_FilterTree()
    {
    }
    
    /// Set the node name.
    /// @param name Name of alias node generating this filter node.
    void SetName(string name)
    {
        m_Name = name;
    }
    
    /// Add filters to this node.
    /// @param filters Filters to add here.
    void AddFilters(const TFilters & filters)
    {
        m_Filters.insert(m_Filters.end(), filters.begin(), filters.end());
    }
    
    /// Get filters from this node.
    /// @return Filters attached here.
    const TFilters & GetFilters() const
    {
        return m_Filters;
    }
    
    /// Add a child node to this node.
    /// @param node Child node to add here.
    void AddNode(CRef<CSeqDB_FilterTree> node)
    {
        m_SubNodes.push_back(node);
    }
    
    /// Add several child nodes to this node.
    /// @param node Child nodes to add here.
    void AddNodes(const vector< CRef<CSeqDB_FilterTree> > & nodes)
    {
        m_SubNodes.insert(m_SubNodes.end(), nodes.begin(), nodes.end());
    }
    
    /// Get child nodes attached to this node.
    /// @return Child nodes attached here.
    const vector< CRef<CSeqDB_FilterTree> > & GetNodes() const
    {
        return m_SubNodes;
    }
    
    /// Attach a volume to this node.
    /// @param vol Path to new volume.
    void AddVolume(const CSeqDB_BasePath & vol)
    {
        m_Volumes.push_back(vol);
    }
    
    /// Attach several volumes to this node.
    /// @param vols Paths to new volumes.
    void AddVolumes(const vector<CSeqDB_BasePath> & vols)
    {
        m_Volumes.insert(m_Volumes.end(), vols.begin(), vols.end());
    }
    
    /// Get volumes attached to this node.
    /// @return Paths to attached volumes.
    const vector<CSeqDB_BasePath> & GetVolumes() const
    {
        return m_Volumes;
    }
    
    /// Print a formatted description of this tree.
    ///
    /// This is very useful for maintainability, e.g. debugging and
    /// for analysis of system behavior.  It prints an indented tree
    /// of filter tree nodes with volumes and mask information.
    void Print() const
    {
        int indent = 0;
        x_Print(indent);
    }
    
    /// Specialized this tree for the indicated volume.
    ///
    /// This method returns a copy of this filter tree, specialized on
    /// the specified volume.  Filter Tree specialization removes all
    /// volumes except the one matching the provided name, and cleans
    /// up any unnecessary or ineffective elements.  This tree is not
    /// changed in place, but the new tree will share any subelements
    /// of this tree that did not need to change.
    /// 
    /// Because the OID list is constructed recursively from this tree
    /// structure, inefficiencies or redundancies here can result in
    /// unnecessary and possibly very expensive extra work.  Thus, the
    /// goal here is to produce the simplest tree that can correctly
    /// represent all filtering for the given volume.
    ///
    /// @param volname The name of the volume to specialize on.
    /// @return A specialized and simplified tree.
    CRef<CSeqDB_FilterTree> Specialize(string volname) const;
    
    /// Check whether this tree represents any volume filtering.
    /// @return True iff any volumes included here are filtered.
    bool HasFilter() const;
    
private:
    /// "Pretty-print" this tree in symbolic form.
    /// @param indent The amount of spaces to indent each line.
    void x_Print(int indent) const
    {
        string tab1(indent, ' ');
        string tab2(indent+4, ' ');
        
        cout << tab1 << "Node(" << m_Name << ")\n";
        cout << tab1 << "{\n";
        ITERATE(TFilters, iter, m_Filters) {
            cout << tab2 << "Filter -> " << (**iter).ToString() << "\n";
        }
        
        if (m_Filters.size() && m_Volumes.size())
            cout << "\n";
        
        ITERATE(vector<CSeqDB_BasePath>, vol_iter, m_Volumes) {
            cout << tab2 << "Volume: " << vol_iter->GetBasePathS() << "\n";
        }
        
        if ((m_Filters.size() || m_Volumes.size()) && m_SubNodes.size())
            cout << "\n";
        
        bool first = true;
        
        ITERATE(vector< CRef<CSeqDB_FilterTree> >, iter, m_SubNodes) {
            if (first) {
                first = false;
            } else {
                cout << "\n";
            }
            (**iter).x_Print(indent + 4);
        }
        cout << tab1 << "}\n";
    }
    
    /// Prevent copy-construction of this object.
    CSeqDB_FilterTree(CSeqDB_FilterTree & other);
    
    /// Prevent assignment of this class.
    CSeqDB_FilterTree & operator=(CSeqDB_FilterTree & other);
    
    /// The node name.
    string m_Name;
    
    /// List of sequence inclusion filters.
    TFilters m_Filters;
    
    /// Other nodes included by this node.
    vector< CRef<CSeqDB_FilterTree> > m_SubNodes;
    
    /// Database volumes attached at this level.
    vector<CSeqDB_BasePath> m_Volumes;
};


END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBFILTER_HPP


