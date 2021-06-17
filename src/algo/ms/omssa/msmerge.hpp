/* 
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
 *  and reliability of the software and data, the NLM 
 *  Although all reasonable efforts have been taken to ensure the accuracyand the U.S.
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
 * Author:  Lewis Y. Geer
 *
 * File Description:
 *   Code for loading in spectrum datasets
 *
 */

#ifndef MSMERGE_HPP
#define MSMERGE_HPP



#include <iostream>
#include <deque>
#include <map>

#include <corelib/ncbistr.hpp>
#include <corelib/ncbiexpt.hpp>
#include <objects/omssa/MSSearch.hpp>
#include "msms.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(omssa)

/** type that hold a list of oids */
typedef std::set <int> TOid;


class NCBI_XOMSSA_EXPORT COMSSASearch : public CMSSearch {

    typedef CMSSearch Tparent;

public:

    // constructor
    COMSSASearch(void) {}
    // destructor
    ~COMSSASearch(void) {}


    /**
     * copy a search onto this search
     * 
     * @param OldSearch the search to be copied
     */
    void CopyCMSSearch(CRef <COMSSASearch> OldSearch);

    /**
      * copy a spectra from old search into this search
      * 
      * @param OldSearch the search to be copied
      */
    void CopySpectra(CRef <COMSSASearch> OldSearch); 

    /**
      * copy Hitsets from old search into new this search
      * 
      * @param OldSearch the search to be copied
      */
    void CopyHitsets(CRef <COMSSASearch> OldSearch);

    /**
      * copy Settings from old search into new this search
      * 
      * @param OldSearch the search to be copied
      */
    void CopySettings(CRef <COMSSASearch> OldSearch);

    /**
     * return a file ending based on encoding type
     * 
     * @param FileType encoding type
     * @return file extension without separator
     */
    const string FileEnding(const ESerialDataFormat FileType) const;

    /**
     * find the maximum and minimum search setting id in this search
     *
     * @param Min minimum
     * @param Max maximum
     */
    void FindMinMaxSearchSettingId(int& Min, int& Max) const;

    /**
     * find the maximum and minimum spectrum number in this search
     * 
     * @param Min minimum
     * @param Max maximum
     */
    void
        FindMinMaxSpectrumNumber(
            int& Min,
            int& Max
            ) const;


    /**
     * check for matching library name and size in all search settings
     * 
     * @return true if all match
     */
    const bool CheckLibraryNameAndSize(const string Name, const int Size) const;


    /**
     * renumber search settings by adding a minimum value
     * 
     * @param Min minimum value to add
     * 
     */
    void RenumberSearchSettingId(const int Min);

    /** 
     * renumber spectrum numbers by adding a minimum value
     * 
     * @param Min minimum value to add
     */
    void RenumberSpectrumNumber(const int Min);

    /**
     * get the oid list
     */
    const TOid& GetOids(void) const;

    /**
     * set the oid list
     */
    TOid& SetOids(void);

    /**
     * fill in the oid list from existing object
     */
    void PopulateOidList(void);

    /**
     * add a bioseq to the bioseq list
     */
    void AppendBioseq(const int oid, CConstRef <CBioseq> Bioseq);

    /**
     * add a search to this object
     */
    void AppendSearch(CRef <COMSSASearch> OldSearch);

protected:

    /** helper function for FindMinMaxSearchSettingId */
    void 
        FindMinMaxForOneSetting(
            const CMSSearchSettings& SearchSettings,
            int& Min,
            int& Max
            ) const;

    /**
     * renumber one search setting
     */
    void 
        RenumberOneSearchSettingId(
            CMSSearchSettings& SearchSettings,
            const int Min);
    
private:
    // Prohibit copy constructor and assignment operator
    COMSSASearch(const COMSSASearch& value);
    COMSSASearch& operator=(const COMSSASearch& value);

    /** list of oids in used in search */
    TOid Oids;

};

inline
const TOid& 
COMSSASearch::GetOids(void) const
{
    return Oids;
}

inline
TOid& 
COMSSASearch::SetOids(void)
{
    return Oids;
}

/** 
 * sets up output file based on DataFormat
 * in particular, initializes xml stream appropriately
 * 
 * @param os output stream
 * @param DataFormat format of file type
 * @return output stream
 */
NCBI_XOMSSA_EXPORT CObjectOStream* SetUpOutputFile(
    CObjectOStream* os, 
    ESerialDataFormat DataFormat);

END_SCOPE(omssa)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif // MSMERGE_HPP
