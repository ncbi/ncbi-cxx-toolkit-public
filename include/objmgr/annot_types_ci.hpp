#ifndef ANNOT_TYPES_CI__HPP
#define ANNOT_TYPES_CI__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*   Object manager iterators
*
*/

#include <objects/objmgr/bioseq_handle.hpp>
#include <objects/objmgr/annot_ci.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <corelib/ncbiobj.hpp>
#include <set>
#include <memory>
#include <deque>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class CTSE_Info;
class CSeq_loc;
class CAnnotObject;


class CAnnotObject_Less
{
public:
    // Compare CRef-s: if at least one is NULL, compare as pointers,
    // otherwise call non-inline x_CompareAnnot() method
    bool operator ()(const CRef<CAnnotObject>& x, const CRef<CAnnotObject>& y) const;
private:
    // Compare annot objects
    bool x_CompareAnnot(const CAnnotObject& x, const CAnnotObject& y) const;
};


// Base class for specific annotation iterators
class CAnnotTypes_CI
{
public:
    // Flag to indicate references resolution method
    enum EResolveMethod {
        eResolve_None, // Do not search annotations on segments
        eResolve_TSE,  // Search annotations only on sequences in the same TSE
        eResolve_All   // Search annotations for all referenced sequences
    };

    CAnnotTypes_CI(void);
    // Search all TSEs in all datasources, by default get annotations defined
    // on segments in the same TSE (eResolve_TSE method).
    CAnnotTypes_CI(CScope& scope,
                   const CSeq_loc& loc,
                   SAnnotSelector selector,
                   EResolveMethod resolve);
    // Search only in TSE, containing the bioseq, by default get annotations
    // defined on segments in the same TSE (eResolve_TSE method).
    CAnnotTypes_CI(CBioseq_Handle& bioseq,
                   TSeqPos start, TSeqPos stop,
                   SAnnotSelector selector,
                   EResolveMethod resolve);
    CAnnotTypes_CI(const CAnnotTypes_CI& it);
    virtual ~CAnnotTypes_CI(void);

    CAnnotTypes_CI& operator= (const CAnnotTypes_CI& it);

    typedef set< CRef<CTSE_Info> > TTSESet;

    const CSeq_annot& GetSeq_annot(void) const;

protected:
    // Check if a datasource and an annotation are selected.
    bool IsValid(void) const;
    // Move to the next valid position
    void Walk(void);
    // Return current annotation
    const CAnnotObject* Get(void) const;

private:
    struct SConvertionRec : public CObject {
        // Locations to search: must contain a single id. Locations
        // on this seq will be converted to locations on the master seq.
        auto_ptr<CHandleRangeMap>   m_Location;
        // Shift to the master sequence position:
        //   master_pos = annotation_pos + m_RefShift
        TSignedSeqPos               m_RefShift;
        // Min/max position allowed for features (to fit the map segment)
        // The positions are in the referenced sequence coordinates.
        TSeqPos                     m_RefMin;
        TSeqPos                     m_RefMax;
        ENa_strand                  m_Strand;
        // Convert references to this id
        CSeq_id_Handle              m_MasterId;
        // Master/segment flag. Do not convert references if this flag is set.
        bool                        m_Master;
    };
    typedef list< CRef<SConvertionRec> >      TIdConvList;
    typedef map<CSeq_id_Handle, TIdConvList>  TConvMap;
    typedef set<CRef<CAnnotObject>, CAnnotObject_Less> TAnnotSet;

    // Initialize the iterator
    void x_Initialize(const CSeq_loc& loc);
    // Search the location for annotations, add all to the annot-set,
    // lock all TSEs found. Do nothing with the convertions map.
    // Return "true" only if annotations were found on the location.
    void x_SearchLocation(CHandleRangeMap& loc);
    // Process the location, resolve references, add information
    // to the convertions map, search for annotations on references.
    // The master location needs to be mapped too, since iterations are made
    // over the map.
    // Return "true" only if annotations were found on referenced sequences.
    void x_ResolveReferences(CSeq_id_Handle master_idh, // master id
                             CSeq_id_Handle ref_idh,    // ref. id
                             TSeqPos rmin, TSeqPos rmax,// ref. interval
                             ENa_strand strand,         // ref. strand
                             TSignedSeqPos shift);      // shift to master
    // Convert an annotation to the master location coordinates
    CAnnotObject* x_ConvertAnnotToMaster(const CAnnotObject& annot_obj) const;
    // Convert seq-loc to the master location coordinates, return true
    // if any location was adjusted (used as Partial flag for features)
    bool x_ConvertLocToMaster(CSeq_loc& loc) const;

    SAnnotSelector               m_Selector;
    // Map of all convertions from references to the master location
    TConvMap                     m_ConvMap;
    // Set of all the annotations found
    TAnnotSet                    m_AnnotSet;
    // TSE set to keep all the TSEs locked
    TTSESet                      m_TSESet;
    // Current annotation
    TAnnotSet::const_iterator    m_CurAnnot;
    // Copy of the annot object (feature etc.) converted to the master seq
    mutable CRef<CAnnotObject>   m_AnnotCopy;
    mutable CRef<CScope>         m_Scope;
    // If non-zero, search annotations in the "native" TSE only
    CRef<CTSE_Info>              m_NativeTSE;
    // Reference resolving method
    EResolveMethod               m_ResolveMethod;
};


inline
bool CAnnotObject_Less::operator ()(const CRef<CAnnotObject>& x, const CRef<CAnnotObject>& y) const
{
    if ( !x.GetPointer()  ||  !y.GetPointer() )
        return x < y;
    return x_CompareAnnot(*x, *y);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.21  2002/11/04 21:28:58  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.20  2002/11/01 20:46:41  grichenk
* Added sorting to set< CRef<CAnnotObject> >
*
* Revision 1.19  2002/10/08 18:57:27  grichenk
* Added feature sorting to the iterator class.
*
* Revision 1.18  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.17  2002/05/31 17:52:58  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.16  2002/05/24 14:58:53  grichenk
* Fixed Empty() for unsigned intervals
* SerialAssign<>() -> CSerialObject::Assign()
* Improved performance for eResolve_None case
*
* Revision 1.15  2002/05/06 03:30:35  vakatov
* OM/OM1 renaming
*
* Revision 1.14  2002/05/03 21:28:01  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.13  2002/04/30 14:30:41  grichenk
* Added eResolve_TSE flag in CAnnot_Types_CI, made it default
*
* Revision 1.12  2002/04/22 20:06:15  grichenk
* Minor changes in private interface
*
* Revision 1.11  2002/04/17 21:11:58  grichenk
* Fixed annotations loading
* Set "partial" flag in features if necessary
* Implemented most seq-loc types in reference resolving methods
* Fixed searching for annotations within a signle TSE
*
* Revision 1.10  2002/04/11 12:07:28  grichenk
* Redesigned CAnnotTypes_CI to resolve segmented sequences correctly.
*
* Revision 1.9  2002/04/05 21:26:16  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.8  2002/03/07 21:25:31  grichenk
* +GetSeq_annot() in annotation iterators
*
* Revision 1.7  2002/03/05 16:08:12  grichenk
* Moved TSE-restriction to new constructors
*
* Revision 1.6  2002/03/04 15:07:46  grichenk
* Added "bioseq" argument to CAnnotTypes_CI constructor to iterate
* annotations from a single TSE.
*
* Revision 1.5  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.4  2002/02/15 20:36:29  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.3  2002/02/07 21:27:33  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.2  2002/01/16 16:26:35  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:03:59  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif  // ANNOT_TYPES_CI__HPP
