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
* Authors:  Chris Lanczycki
*
* File Description:
*           Functions and classes to handle CD annotations and biostruc features.
*
* ===========================================================================
*/

#ifndef CU_CDANNOTATION__HPP
#define CU_CDANNOTATION__HPP 

#include <map>
#include <vector>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiapp.hpp>
#include <objects/cdd/Cdd.hpp>
#include <objects/cdd/Cdd_book_ref.hpp>
#include <objects/cdd/Align_annot.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <algo/structure/cd_utils/cuCdCore.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

// Use vector to reference Seq_aligns vs. native list container
//typedef vector< CRef< CSeq_align > > TSeqAlignVec;

typedef pair<unsigned int, unsigned int> FromToPair;

// Annotation map:  indexed by zero-based feature ID from an 
// Align_annot into that set of locations (in zero-based residue number from/to pairs 
// on master) for which an annotation exists
typedef map<unsigned int, vector<FromToPair> > CdAnnotMap;
typedef CdAnnotMap::iterator CdAnnotMapIt;
typedef CdAnnotMap::const_iterator CdAnnotMapCit;
typedef CdAnnotMap::value_type CdAnnotMapVT;

// Annotation position map:  indexed by a valid annotation position from an Align_annot
// into the set of residue IDs (zero-based).  The index is by definition the position
// on the master; the mapped vector contains entries for the N-1 remaining rows,
// or kMax_UInt if there is no longer an aligned residue at the position.
typedef map<unsigned int, vector<unsigned int> > CdAnnotPosMap;
typedef CdAnnotPosMap::iterator CdAnnotPosMapIt;
typedef CdAnnotPosMap::const_iterator CdAnnotPosMapCit;
typedef CdAnnotPosMap::value_type CdAnnotPosMapVT;

//  A handful of convenience functions for CAlign_annot objects.
NCBI_CDUTILS_EXPORT
bool IsCommentEvidenceInAlignAnnot(const CAlign_annot& feature);    //  e_Comment type evidence
NCBI_CDUTILS_EXPORT
bool IsStructureEvidenceInAlignAnnot(const CAlign_annot& feature);  //  e_Bsannot type evidence
NCBI_CDUTILS_EXPORT
bool IsReferenceEvidenceInAlignAnnot(const CAlign_annot& feature);  //  e_Reference type evidence
NCBI_CDUTILS_EXPORT
bool IsBookRefEvidenceInAlignAnnot(const CAlign_annot& feature);    //  e_Book_ref type evidence
NCBI_CDUTILS_EXPORT
bool IsSeqfeatEvidenceInAlignAnnot(const CAlign_annot& feature);    //  e_Seqfeat type evidence
NCBI_CDUTILS_EXPORT
string GetAlignAnnotDescription(const CAlign_annot& feature);
//  If the 'type' is unset, return -1.
NCBI_CDUTILS_EXPORT
CAlign_annot::TType GetAlignAnnotType(const CAlign_annot& feature);

//  Even if an empty string, 'descr' replaces an existing description.
NCBI_CDUTILS_EXPORT
void SetAlignAnnotDescription(const string& descr, CAlign_annot& feature);
//  By the convention described in the Align-annot ASN.1 spec, only values in the interval [0,6] are allowed.
//  Return true if the value of 'type' is allowed and the type was set; false if it not, and the type
//  field was unchanged.
NCBI_CDUTILS_EXPORT
bool SetAlignAnnotType(CAlign_annot::TType type, CAlign_annot& feature);


NCBI_CDUTILS_EXPORT
unsigned int GetAlignAnnotFromToPairs(const CAlign_annot& feature, vector<FromToPair>& pairs);
NCBI_CDUTILS_EXPORT
unsigned int GetAlignAnnotEvidenceFromToPairs(const CAlign_annot& feature, vector<FromToPair>& pairs);

//  This reports all positions implied by the from/to pairs
NCBI_CDUTILS_EXPORT
unsigned int GetAlignAnnotPositions(const CAlign_annot& feature, vector<unsigned int>& positions);
//  This reports all positions implied by the from/to pairs
NCBI_CDUTILS_EXPORT
unsigned int GetAlignAnnotEvidencePositions(const CAlign_annot& feature, vector<unsigned int>& positions);

//  If from/to positions are included, they are zero-based.
NCBI_CDUTILS_EXPORT
string CAlignAnnotToString(const CRef< CAlign_annot>& feature, bool includeFromTo, bool includeEvidence, bool hyphenateFromTo = true);
NCBI_CDUTILS_EXPORT
string CAlignAnnotToString(const CAlign_annot& feature, bool includeFromTo, bool includeEvidence, bool hyphenateFromTo = true);

//  Dump the CAlign_annot as ASN, with the option to omit the evidence field.
NCBI_CDUTILS_EXPORT
string CAlignAnnotToASNString(const CRef< CAlign_annot>& feature, bool includeEvidence);
NCBI_CDUTILS_EXPORT
string CAlignAnnotToASNString(const CAlign_annot& feature, bool includeEvidence);

//NCBI_CDUTILS_EXPORT
//string CAlignAnnotToFormattedString(const CRef< CAlign_annot>& feature);

//bool GetFeatureFromCD(CCdCore& cd, unsigned int featureIndex, CRef<CAlign_annot>& feature);
//bool GetFeatureFromCD(const CCdCore& cd, unsigned int featureIndex, CAlign_annot_set::Tdata::const_iterator& returnedCit);
//bool GetFeatureFromCD(const CCdCore& cd, unsigned int featureIndex, const CRef<CAlign_annot>& feature);

class NCBI_CDUTILS_EXPORT CCdAnnotationInfo
{

public:

    //  Annotation ID mapped to description.
    typedef map<unsigned int, string> AnnotDescrMap;
    typedef AnnotDescrMap::iterator AnnotDescrIt;
    typedef AnnotDescrMap::const_iterator AnnotDescrCit;

    //  Requires user to manually set member variables.
    CCdAnnotationInfo() : m_cd(NULL), m_cdMappedFrom(NULL) {};

    //  Uses the CD passed to define the member variables.
    //  When onlyMasterPos is true, do not initialize m_slaveAnnotMap.
    CCdAnnotationInfo(const cd_utils::CCdCore* cd, bool onlyMasterPos = true);

    unsigned int GetNumAnnotations() const { return m_masterAnnotMap.size();}

    //  Return a copy of the map from the annotation ID to the corresponding descriptions.
    AnnotDescrMap GetDescriptions() const {return m_annotDescrMap;}

    //  Return a copy of the map from the annotation ID to the corresponding master sequence from/to positions.
    CdAnnotMap GetFeaturePositions() const {return m_masterAnnotMap;}

    //  Return a map from the annotation ID to the corresponding master sequence from/to
    //  positions for the evidence of that annotation.
    CdAnnotMap GetEvidencePositions() const;

    //  Map the from-to positions onto the other CD of the guide, changing m_cd to that other CD.  
    //  (This is used to support assigning generic annotations from a CD higher in a hierarchy.)
    //  Assumes the master row of current CD was used to make the guide alignment object;
    //  don't care what row used for the other CD although by convention it should also be the master of that CD.
    //  If the current CD is not in the guide, or any from/to range can't be mapped NULL is returned.
//    CCdAnnotationInfo* MapAnnotations(CGuideAlignment_Base& guide, string& err, string& warn) const;

    //  The pointer to the CD in whose coordinates the positions of the annotations in
    //  this object are given.  Was not the source of the annotations, if
    //  'IsMappedAnnotation' returns true.
    const CCdCore* GetCD() const { return m_cd; }
    void SetCD(const CCdCore* cd) { m_cd = cd; }

    //  NULL if this CD does not represent a mapping.  Otherwise, returned pointer is
    //  to the source of the annotations that were mapped.
    const CCdCore* GetCDMappedFrom() const { return m_cdMappedFrom; }
    void SetCDMappedFrom(const CCdCore* cd) { m_cdMappedFrom = cd; }

    const CdAnnotMap& GetMasterAnnotMap() const {return m_masterAnnotMap;}
    CdAnnotMap& SetMasterAnnotMap() {return m_masterAnnotMap;}
    void SetMasterAnnotMap(const CdAnnotMap& masterAnnotMap) {m_masterAnnotMap = masterAnnotMap;}

    const CdAnnotPosMap& GetSlaveAnnotMap() const {return m_slaveAnnotMap;}

    const AnnotDescrMap& GetAnnotDescrMap() const {return m_annotDescrMap;}
    AnnotDescrMap& SetAnnotDescrMap() {return m_annotDescrMap;}
    void SetAnnotDescrMap(const AnnotDescrMap& annotDescrMap) {m_annotDescrMap = annotDescrMap;}

    //  Does this instance represent a mapped set of annotations?
    bool IsMappedAnnotation() const { return (m_cdMappedFrom == NULL);}

    void MakeSlaveAnnotMap();


    //  A one-line summary of each annotation formatted as:
    //
    //  0|1 accession|pssmId description_string [from-to, from-to, ..., from-to] 
    //
    //    ... or (if not using hyphenated ranges) ...
    //
    //  0|1 accession|pssmId description_string [from1, ..., to1, from2, ..., to2, ..., fromN, ..., toN] 
    //
    //  There is one line for each annotation, in the same order as they appear in the CD.
    //  The first character is either '0' or '1', to indicate either a mapped (i.e., non-native) 
    //  annotation or a native annotation specific to this CD.
    //  The 'useAccession' argument controls whether the cd Accession or a pssm-id is used in the output.
    //  ** Note:  from/to positions are listed in 0-based coordinates on the master **
    //  Return value is number of annotations.

    //  Default is that hyphenated ranges are used.  Must explicitly turn them off.
    static void OutputHyphenatedRanges(bool hyphenatedRanges) { m_useHyphenatedRanges = hyphenatedRanges;}


    unsigned int ToString(vector<string>& annotations, bool useAccession) const;
    static unsigned int ToString(const CCdCore& cd, vector<string>& annotations, bool useAccession);

    //  Same format as ToString(...) above, but package all annotations into a single string. 
    //  Each annotation's line newline-delimited.
    string ToString(bool useAccession) const;
    static string ToString(const CCdCore& cd, bool useAccession);

    //  Dump information on coordinates on the slave rows.  Either by individual row, or as a group
    //  of structures-only or all rows.  Makes the slave-annotation map if doesn't already exist.
    //  Returns a concatenated string for all annotations.
    string MappedToSlaveString(unsigned int row) const;
    //  Returns a string for each annotation.  
    //  If 'addRow' is true, the row number (but not sequence ID) is added to the front of each string.
    void MappedToSlaveString(unsigned int row, vector<string>& individualAnnotStrings, bool addRowToAll = false) const;
    //  Returns only a collected string, grouping the mappings for each annotation together if 'groupByRow' is false.
    string MappedToSlaveString(bool structuresOnly = false, bool groupByRow = true) const;
    //  A string for the row, including sequence identifier.
    string MakeRowInfoString(unsigned int row) const;

    //  Get each annotation as an ASN Align-annot text blob.  Use other class methods to
    //  determine if the reported annotations are mapped, and the desired accession/pssmId.
    //  NOTE:  if this object represents a mapped annotation, only coordinates in the 'location' 
    //         field of the CAlign_annot are mapped in the output ASN.  Coordinates in the
    //         evidence field are unaltered and refer to positions in  m_cdMappedFrom!
    unsigned int ToASNString(vector<string>& annotations, bool withEvidence) const;
    static unsigned int ToASNString(const CCdCore& cd, vector<string>& annotations, bool withEvidence);

    //  Output as a single ASN Align-annot-set text blob.  Use other class methods to
    //  determine if the reported annotations are mapped, and the desired accession/pssmId.
    //  NOTE:  if this object represents a mapped annotation, only coordinates in the 'location' 
    //         field of the CAlign_annot are mapped in the output ASN.  Coordinates in the
    //         evidence field are unaltered and refer to positions in  m_cdMappedFrom!
    string ToASNString(bool withEvidence) const;
    static string ToASNString(const CCdCore& cd, bool withEvidence);


    //  A summary of all annotations in the format distributed on the CDD FTP site.
    //  Items on the line are *tab separated*.
    //  ** Note:  positions are listed in 0-based coordinates on the master, unless requested otherwise **
    //  Example:
    //  28772   cd01334 Lyase_I 1       active sites    1       1       1       82,83,128,129,262
    //  28772   cd01334 Lyase_I 2       tetramer interface      1       1       1       127,128,129, ... 282,285,300
    string ToFtpDumpString(bool zeroBasedCoords = true) const;
    static string ToFtpDumpString(const CCdCore& cd, bool zeroBasedCoords = true);

    //  A more verbose summary, possibly including evidence details.
    //  Uses CAlignAnnotToString to format text-based annotation output, but adds all annotated coordinates.
    //  The 'useAccession' argument controls whether the cd Accession or a pssm-id is used in the output.
    //  ** Note:  positions are listed in 0-based coordinates on the master, unless requested otherwise **
    string ToLongString(bool withEvidence, bool useAccession, bool zeroBasedCoords = true) const;
    static string ToLongString(const CCdCore& cd, bool withEvidence, bool useAccession, bool zeroBasedCoords = true);

private:

    static bool m_useHyphenatedRanges;

    const CCdCore* m_cd;

    //  NULL unless this instance was created by mapping another instance using a guide alignment.  
    //  In that case, m_cdMappedFrom points to the original CD and m_cd is the CD the annotation
    //  was mapped to.  
    const CCdCore* m_cdMappedFrom;  

    //  Extracted annotation info from the CD.
    CdAnnotMap m_masterAnnotMap;
    CdAnnotPosMap m_slaveAnnotMap;
    AnnotDescrMap m_annotDescrMap;

    void Initialize(bool onlyMasterPos);

    void GenerateMappedAnnotSet(CAlign_annot_set& alignAnnotSet) const;

    //  Map the ranges of coordinates on the master into ranges for the indicated row.
    //  Returns false if any of the positions could not be mapped, and that position
    //  is set to kMax_UInt in the returned map.
    //  NOTE:  it is possible that the ranges on the slave are not the same size as
    //         on the master if there is not a consistent block model and there are
    //         inserts/deletions on the slave row.
    bool MapRangesForRow(unsigned int row, CdAnnotMap& slaveRanges) const;

    void RangeStringsForAnnots(const CdAnnotMap& rowAnnots, vector<string>& rangeStrings) const;

};


END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif // CU_CDANNOTATION__HPP
