/* $Id$
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
 * Author:  Chris Lanczycki
 *
 * File Description:
 *
 *       Subclass of CCdCore to build & represent a CD instantiated via mFASTA.
 *
 * ===========================================================================
 */

#ifndef CU_CDFROMFASTA_HPP
#define CU_CDFROMFASTA_HPP

#include <algo/structure/cd_utils/cuCdCore.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class CBasicFastaWrapper;

class NCBI_CDUTILS_EXPORT CCdFromFasta : public CCdCore
{

public:
    typedef CSeqAnnotFromFasta::MasteringMethod TMasterMethod;
    typedef struct Fasta2CdParams {
        string cdAcc;   //  accession of CD to create
        string cdName;  //  shortname of CD to create (use accession if missing)
        bool   useLocalIds;  //  all identifiers treated as 'local'; don't parse identifier
        bool   useAsIs;      //  make alignments 'as-is', choosing the "best" master, and no IBM run
        TMasterMethod masterMethod; //  corresponds to the CSeqAnnotFromFasta::MasteringMethod enum
        unsigned int masterIndex;  //  explicitly set the master index (ignored if masterMethod != eSpecifiedSequence)

        string Print() {
            string s = "Cd accession = " + cdAcc + "\n";
            s.append("Cd name = " + cdName + "\n");
            s.append("use local ids? " + ((useLocalIds) ? string("Yes\n") : string("No\n")));
            s.append("use as is?  " + ((useAsIs) ? string("Yes\n") : string("No\n")));
            s.append("masterMethod = " + NStr::UIntToString((unsigned int) masterMethod) + "\n");
            s.append("master index = " + NStr::UIntToString(masterIndex) + "\n");
            return s;
        }

    } Fasta2CdParams;

    CCdFromFasta();
    CCdFromFasta(const Fasta2CdParams& params);
    CCdFromFasta(const string& fastaFile, const Fasta2CdParams& params, CBasicFastaWrapper* fastaIOWrapper = NULL);
    virtual ~CCdFromFasta(void);

    void SetParameters(const Fasta2CdParams& params) {InitializeParameters(&params);}

    //  Get the data from the fasta file and instantiate the CD from it.
    //  When 'cleanUp' is true and this has ownership of the m_fastaIO object, which caches the raw data read
    //  from the file, m_fastaIO is deleted on exiting ths method.  Thereafter, methods that try to get at
    //  the missing data will not work.
    bool ImportAlignmentData(const string& fastaFile, bool cleanUp = false);

    //  These add a specific type of Cdd-descr to the CD.
    //  Typically, duplicates will not be added; functions return
    //  'false' when attempting to add a duplicate description.
    bool AddComment(const string& comment);
    bool AddOthername(const string& othername);
    bool AddTitle(const string& title);
    bool AddPmidReference(unsigned int pmid);
    bool AddSource(const string& source, bool removeExistingSources = true);
    bool AddCreateDate();  //  uses the current time

    // update any existing source-ids with this one
    bool UpdateSourceId(const string& sourceId, int version = 0);  

    // false if there was an error
    bool WriteToFile(const string& outputFile) const;

    bool   WasInputError() const { return m_fastaInputErrorMsg.length() > 0; }
    string GetFastaInputErrorMsg() const { return m_fastaInputErrorMsg;}

    //  **  The following methods rely on the cached data in the m_fastaIO object.
    //  **  They will fail if ImportAlignmentData was called with 'cleanUp' option set to true.

    //  Access the data from the FASTA file used to initialize the CD; the file 
    //  is not re-read as the fasta reader object maintains a record of what it read.
    //  Any gap characters in the input sequence are preserved.
    //  If the index is out of bounds (based on the number of sequences originally read),
    //  an empty string is returned.
    string GetDeflineReadFromFile(unsigned int index) const;
    string GetSequenceReadFromFile(unsigned int index) const;

    //  Access the data from the FASTA file used to initialize the CD; the file 
    //  is not re-read as the fasta reader object maintains a record of what it read.
    //  If the column index is out of bounds on any particular row, a gap character is
    //  inserted in its place.  If column index is out of bounds on all rows, an empty string is returned.
    string GetColumnReadFromFile(unsigned int column) const;

    //  Return all columns, indexed by position on the first row of the FASTA used to create this CD.
    //  Return value is the numbers of such columns.
    unsigned int GetAllColumnsReadFromFile(map<unsigned int, string>& columns) const;

    //  Return all gapless columns, indexed by position on the first row of the FASTA used to create this CD.
    //  Return value is the numbers of such columns.
    //  A gapless column is one containing only letters.
    unsigned int GetGaplessColumnsReadFromFile(map<unsigned int, string>& gaplessColumns) const;

    //  This only refers to the master sequence index when initial created.  If the object
    //  has been edited (e.g., by remastering, delete rows) this value becomes obsolete.
    //  When not defined or has been reset, return CSeqAnnotFromFasta::eUnassignedMaster.
    unsigned int GetInitialMasterSequenceIndex() const {return m_initialMasterSequenceIndex;}
    void ResetInitialMasterSequenceIndex();

private:

    string m_fastaInputErrorMsg;
    Fasta2CdParams m_parameters;

    //  When there are repeats, the mapping can be ambiguous.  Set from the corresponding parameter
    //  in CSeqAnnotFromFasta when the alignment is imported -- not subsequently modified if the CD
    //  changes!!!!
    unsigned int m_initialMasterSequenceIndex;  

    //  Fasta I/O object that uses NCBI C++ toolkit ReadFasta and CFastaReader classes.
    bool m_ownsFastaIO;
    CBasicFastaWrapper* m_fastaIO;

    //  Only cleans up the object if this object has ownership.
    void CleanUpFastaIO() {
        if (m_ownsFastaIO) {
            delete m_fastaIO;
            m_fastaIO = NULL;
        }
    }

    //  Also sets the accession and shortname.
    void InitializeParameters(const Fasta2CdParams* params = NULL);

    //  Return true only if 'descr' was added.
    bool AddCddDescr(CRef< CCdd_descr >& descr);

    //  Removes any CCdd_descr of the specified choice type.
    bool RemoveCddDescrsOfType(int cddDescrChoice);

    unsigned int GetColumnsReadFromFile(map<unsigned int, string>& columns, bool skipGappedColumns) const;

    // Prohibit copy constructor and assignment operator
    CCdFromFasta(const CCdFromFasta& value);
    CCdFromFasta& operator=(const CCdFromFasta& value);
};


END_SCOPE(cd_utils)
END_NCBI_SCOPE


#endif //  CU_CDFROMFASTA_HPP
