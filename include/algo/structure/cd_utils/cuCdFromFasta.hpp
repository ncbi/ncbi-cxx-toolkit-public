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
    bool ImportAlignmentData(const string& fastaFile);

    //  These add a specific type of Cdd-descr to the CD
    bool AddComment(const string& comment);
    bool AddPmidReference(unsigned int pmid);
    bool AddSource(const string& source, bool removeExistingSources = true);
    bool AddCreateDate();  //  uses the current time

    // update any existing source-ids with this one
    bool UpdateSourceId(const string& sourceId, int version = 0);  

    void WriteToFile(const string& outputFile) const;

    bool   WasInputError() const { return m_fastaInputErrorMsg.length() > 0; }
    string GetFastaInputErrorMsg() const { return m_fastaInputErrorMsg;}

private:

    string m_fastaInputErrorMsg;
    Fasta2CdParams m_parameters;

    //  Fasta I/O object that uses NCBI C++ toolkit ReadFasta and CFastaReader classes.
    CBasicFastaWrapper* m_fastaIO;

    //  Also sets the accession and shortname.
    void InitializeParameters(const Fasta2CdParams* params = NULL);

    //  Return true only if 'descr' was added.
    bool AddCddDescr(CRef< CCdd_descr >& descr);

    //  Removes any CCdd_descr of the specified choice type.
    bool RemoveCddDescrsOfType(int cddDescrChoice);

    // Prohibit copy constructor and assignment operator
    CCdFromFasta(const CCdFromFasta& value);
    CCdFromFasta& operator=(const CCdFromFasta& value);
};


END_SCOPE(cd_utils)
END_NCBI_SCOPE


#endif //  CU_CDFROMFASTA_HPP
