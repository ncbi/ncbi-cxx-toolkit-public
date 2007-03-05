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
* Authors:  Paul Thiessen
*
* File Description:
*      class to manage different root ASN data types
*
* ===========================================================================
*/

#ifndef CN3D_DATA_MANAGER__HPP
#define CN3D_DATA_MANAGER__HPP

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/serialdef.hpp>

#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/cdd/Cdd.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/mmdb1/Biostruc.hpp>
#include <objects/mmdb1/Biostruc_annot_set.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/cdd/Update_align.hpp>
#include <objects/cn3d/Cn3d_style_dictionary.hpp>
#include <objects/cn3d/Cn3d_user_annotations.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/mmdb2/Model_type.hpp>
#include <objects/mmdb2/Biostruc_model.hpp>
#include <objects/mmdb2/Model_type.hpp>
#include <objects/mmdb1/Biostruc_graph.hpp>
#include <objects/mmdb1/Molecule_graph.hpp>

#include <list>
#include <vector>

#include "structure_set.hpp"


BEGIN_SCOPE(Cn3D)

class Sequence;
class AlignmentSet;

class ASNDataManager
{
public:
    typedef std::list < ncbi::CRef < ncbi::objects::CSeq_entry > > SeqEntryList;
    typedef std::list < ncbi::CRef < ncbi::objects::CBiostruc > > BiostrucList;
    typedef std::list < ncbi::CRef < ncbi::objects::CSeq_annot > > SeqAnnotList;
    typedef std::list < ncbi::CRef < ncbi::objects::CUpdate_align > > UpdateAlignList;

private:
    // top-level data, supplied to constructor
    ncbi::CRef < ncbi::objects::CNcbi_mime_asn1 > mimeData;
    ncbi::CRef < ncbi::objects::CCdd > cddData;

    mutable unsigned int dataChanged;
    ncbi::objects::CCdd * GetInternalCDDData(void);
    const ncbi::objects::CCdd * GetInternalCDDData(void) const;
    bool ConvertMimeToGeneral(void);
    void RemoveConsensusFromCDD(void);

    // pointers to lower-level data, filled in by Load
    void Load(void);
    SeqEntryList *seqEntryList;
    ncbi::objects::CBiostruc *masterBiostruc;
    BiostrucList *biostrucList;
    bool isSingleStructure;     // IFF single MMDB is viewed, then we can show alt confs
    ncbi::objects::EModel_type biostrucModelType;
    ncbi::objects::CBiostruc_annot_set *structureAlignments;
    SeqAnnotList *sequenceAlignments;
    SeqAnnotList *bundleImports;
    UpdateAlignList bundleImportsFaked; // bundle imports re-cast as Update-aligns
    UpdateAlignList *cddUpdates;

public:
    ASNDataManager(ncbi::objects::CNcbi_mime_asn1 *mime);
    ASNDataManager(ncbi::objects::CCdd *cdd);
    ~ASNDataManager(void);

    // validate alignments (as per CDTree2)
    bool MonitorAlignments(void) const;

    // dump data to file
    bool WriteDataToFile(const char *filename, bool isBinary,
        std::string *err, ncbi::EFixNonPrint fixNonPrint = ncbi::eFNP_Default) const;

    // retrieve sequences
    SeqEntryList * GetSequences(void) const { return seqEntryList; }

    // retrieve structures
    bool IsSingleStructure(void) const { return isSingleStructure; }
    bool IsGeneralMime(void) const { return (mimeData.NotEmpty() && mimeData->IsGeneral()); }
    const ncbi::objects::CBiostruc * GetMasterStructure(void) const { return masterBiostruc; }
    const BiostrucList * GetStructureList(void) const { return biostrucList; }
    ncbi::objects::EModel_type GetBiostrucModelType(void) const { return biostrucModelType; }

    // store new structure, if appropriate
    bool AddBiostrucToASN(ncbi::objects::CBiostruc *biostruc);

    // retrieve structure alignments
    ncbi::objects::CBiostruc_annot_set * GetStructureAlignments(void) const { return structureAlignments; }
    void SetStructureAlignments(ncbi::objects::CBiostruc_annot_set *structureAlignments);

    // retrieve sequence alignments
    SeqAnnotList * GetSequenceAlignments(void) const { return sequenceAlignments; }
    // return it, or create the list if not present (and if the data type can hold it)
    SeqAnnotList * GetOrCreateSequenceAlignments(void);

    // retrieve updates
    const UpdateAlignList * GetUpdates(void) const
        { return (cddUpdates ? cddUpdates : (bundleImports ? &bundleImportsFaked : NULL)); }
    void ReplaceUpdates(UpdateAlignList& newUpdates);

    // style dictionary and user annotations
    const ncbi::objects::CCn3d_style_dictionary * GetStyleDictionary(void) const;
    void SetStyleDictionary(ncbi::objects::CCn3d_style_dictionary& styles);
    void RemoveStyleDictionary(void);
    const ncbi::objects::CCn3d_user_annotations * GetUserAnnotations(void) const;
    void SetUserAnnotations(ncbi::objects::CCn3d_user_annotations& annots);
    void RemoveUserAnnotations(void);

    // add sequence to CDD rejects list
    void AddReject(ncbi::objects::CReject_id *reject);
    const StructureSet::RejectList * GetRejects(void) const;

    // updates sequences in the asn, to remove any sequences
    // that are not used by the current alignmentSet or updates
    typedef std::list < const Sequence * > SequenceList;
    void RemoveUnusedSequences(const AlignmentSet *alignmentSet, const SequenceList& updateSequences);

    // convert underlying data from mime to cdd
    bool ConvertMimeDataToCDD(const std::string& cddName);

    // CDD-specific data accessors
    bool IsCDD(void) const;
    bool IsCDDInMime(void) const;
    const std::string& GetCDDName(void) const;
    bool SetCDDName(const std::string& name);
    const std::string& GetCDDDescription(void) const;
    bool SetCDDDescription(const std::string& descr);
    typedef std::vector < std::string > TextLines;
    bool GetCDDNotes(TextLines *lines) const;
    bool SetCDDNotes(const TextLines& lines);
    ncbi::objects::CCdd_descr_set * GetCDDDescrSet(void);
    ncbi::objects::CAlign_annot_set * GetCDDAnnotSet(void);
    const ncbi::objects::CSeq_id * GetCDDMaster3d(void) const;

    // to flag data changes
    bool HasDataChanged(void) const { return (dataChanged > 0); }
    void SetDataChanged(unsigned int what) const { dataChanged |= what; }
    void SetDataUnchanged(void) const { dataChanged = 0; }
    unsigned int GetDataChanged(void) const { return dataChanged; }
};

extern ncbi::objects::CNcbi_mime_asn1 * CreateMimeFromBiostruc(const std::string& filename,
    ncbi::objects::EModel_type model);
extern ncbi::objects::CNcbi_mime_asn1 * CreateMimeFromBiostruc(ncbi::CRef < ncbi::objects::CBiostruc >& biostruc,
    ncbi::objects::EModel_type model);

END_SCOPE(Cn3D)

#endif // CN3D_DATA_MANAGER__HPP
