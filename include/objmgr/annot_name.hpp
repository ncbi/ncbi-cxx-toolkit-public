#ifndef ANNOT_NAME__HPP
#define ANNOT_NAME__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Annotations selector structure.
*
*/


#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CAnnotName
{
public:
    CAnnotName(void)
        : m_Named(false)
        {
        }
    CAnnotName(const string& name)
        : m_Named(true), m_Name(name)
        {
        }
    CAnnotName(const char* name)
        : m_Named(true), m_Name(name)
        {
        }

    bool IsNamed(void) const
        {
            return m_Named;
        }
    const string& GetName(void) const
        {
            _ASSERT(m_Named);
            return m_Name;
        }

    void SetUnnamed(void)
        {
            m_Named = false;
            m_Name.erase();
        }
    void SetNamed(const string& name)
        {
            m_Name = name;
            m_Named = true;
        }

    bool operator<(const CAnnotName& name) const
        {
            return name.m_Named && (!m_Named || name.m_Name > m_Name);
        }
    bool operator==(const CAnnotName& name) const
        {
            return name.m_Named == m_Named && name.m_Name == m_Name;
        }
    bool operator!=(const CAnnotName& name) const
        {
            return name.m_Named != m_Named || name.m_Name != m_Name;
        }

private:
    bool   m_Named;
    string m_Name;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2004/08/05 18:23:25  vasilche
* CAnnotName and CAnnotTypeSelector are moved in separate headers.
*
* Revision 1.30  2004/04/05 15:56:13  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.29  2004/03/17 16:03:42  vasilche
* Removed Windows EOL
*
* Revision 1.28  2004/03/16 15:47:25  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.27  2004/02/26 14:41:40  grichenk
* Fixed types excluding in SAnnotSelector and multiple types search
* in CAnnotTypes_CI.
*
* Revision 1.26  2004/02/11 22:19:23  grichenk
* Fixed annot type initialization in iterators
*
* Revision 1.25  2004/02/05 19:53:39  grichenk
* Fixed type matching in SAnnotSelector. Added IncludeAnnotType().
*
* Revision 1.24  2004/02/04 18:05:31  grichenk
* Added annotation filtering by set of types/subtypes.
* Renamed *Choice to *Type in SAnnotSelector.
*
* Revision 1.23  2004/01/23 16:14:45  grichenk
* Implemented alignment mapping
*
* Revision 1.22  2003/11/13 19:12:51  grichenk
* Added possibility to exclude TSEs from annotations request.
*
* Revision 1.21  2003/10/09 20:20:59  vasilche
* Added possibility to include and exclude Seq-annot names to annot iterator.
* Fixed adaptive search. It looked only on selected set of annot names before.
*
* Revision 1.20  2003/10/07 13:43:22  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.19  2003/10/01 15:08:48  vasilche
* Do not reset feature type if feature subtype is set to 'any'.
*
* Revision 1.18  2003/09/30 16:21:59  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.17  2003/09/16 14:21:46  grichenk
* Added feature indexing and searching by subtype.
*
* Revision 1.16  2003/09/11 17:45:06  grichenk
* Added adaptive-depth option to annot-iterators.
*
* Revision 1.15  2003/08/22 14:58:55  grichenk
* Added NoMapping flag (to be used by CAnnot_CI for faster fetching).
* Added GetScope().
*
* Revision 1.14  2003/07/17 21:01:43  vasilche
* Fixed ambiguity on Windows
*
* Revision 1.13  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.12  2003/06/25 20:56:29  grichenk
* Added max number of annotations to annot-selector, updated demo.
*
* Revision 1.11  2003/06/17 20:34:02  grichenk
* Added flag to ignore sorting
*
* Revision 1.10  2003/04/28 14:59:58  vasilche
* Added missing initialization.
*
* Revision 1.9  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.8  2003/03/31 21:48:17  grichenk
* Added possibility to select feature subtype through SAnnotSelector.
*
* Revision 1.7  2003/03/18 14:46:35  grichenk
* Set limit object type to "none" if the object is null.
*
* Revision 1.6  2003/03/14 20:39:26  ucko
* return *this from SetIdResolvingXxx().
*
* Revision 1.5  2003/03/14 19:10:33  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.4  2003/03/10 16:55:16  vasilche
* Cleaned SAnnotSelector structure.
* Added shortcut when features are limited to one TSE.
*
* Revision 1.3  2003/03/05 20:56:42  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.2  2003/02/25 14:48:06  vasilche
* Added Win32 export modifier to object manager classes.
*
* Revision 1.1  2003/02/24 19:36:19  vasilche
* Added missing annot_selector.hpp.
*
*
* ===========================================================================
*/

#endif  // ANNOT_NAME__HPP
