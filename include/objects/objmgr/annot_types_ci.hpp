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
* ---------------------------------------------------------------------------
* $Log$
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

#include <objects/objmgr1/bioseq_handle.hpp>
#include <objects/objmgr1/annot_ci.hpp>
#include <corelib/ncbiobj.hpp>
#include <set>
#include <memory>
#include <deque>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class CTSE_Info;
class CSeq_loc;



// Base class for specific annotation iterators
class CAnnotTypes_CI
{
public:
    // Flag to indicate references resolution method
    enum EResolveMethod {
        eResolve_None,
        eResolve_All
    };

    CAnnotTypes_CI(void);
    // Search all TSEs in all datasources, by default do not get
    // annotations defined for segmented bioseq parts.
    CAnnotTypes_CI(CScope& scope,
                   const CSeq_loc& loc,
                   SAnnotSelector selector,
                   EResolveMethod resolve);
    // Search only in TSE, containing the bioseq, by default do not get
    // annotations defined for segmented bioseq parts.
    CAnnotTypes_CI(CBioseq_Handle& bioseq,
                   int start, int stop,
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
    CAnnotObject* Get(void) const;

private:
    struct SConvertionRec : public CObject {
        // Locations to search: must contain a single id. Locations
        // on this seq will be converted to locations on the master seq.
        auto_ptr<CHandleRangeMap>   m_Location;
        // Shift to the master sequence position:
        //   master_pos = annotation_pos + m_RefShift
        int                         m_RefShift;
        // Min/max position allowed for features (to fit the map segment)
        // -1 means there is no limit. The positions are in the referenced
        // sequence coordinates.
        int                         m_RefMin;
        int                         m_RefMax;
        // Convert references to this id
        CSeq_id_Handle              m_MasterId;
        // Master/segment flag. Do not convert references if this flag is set.
        bool                        m_Master;
    };
    typedef list< CRef<SConvertionRec> >      TIdConvList;
    typedef map<CSeq_id_Handle, TIdConvList>  TConvMap;
    typedef set< CRef<CAnnotObject> >         TAnnotSet;

    // Initialize the iterator
    void x_Initialize(const CSeq_loc& loc, EResolveMethod resolve);
    // Search the location for annotations, add all to the annot-set,
    // lock all TSEs found. Do nothing with the convertions map.
    void x_SearchLocation(CHandleRangeMap& loc);
    // Process the location, resolve references, add information
    // to the convertions map, search for annotations on references.
    // The master location needs to be mapped too, since iterations are made
    // over the map.
    void x_ResolveReferences(CSeq_id_Handle master_idh, // master id
                             CSeq_id_Handle ref_idh,    // ref. id
                             int rmin, int rmax,        // ref. interval
                             int shift,                 // shift to master
                             bool resolve);
    // Convert an annotation to the master location coordinates
    CAnnotObject* x_ConvertAnnotToMaster(CAnnotObject& annot_obj) const;
    // Convert seq-loc to the master location coordinates
    void x_ConvertLocToMaster(CSeq_loc& loc) const;

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
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ANNOT_TYPES_CI__HPP
