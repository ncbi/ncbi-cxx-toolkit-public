#ifndef _PROT_MATCH_UTILS_HPP_
#define _PROT_MATCH_UTILS_HPP_

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

struct SMatchIdInfo {

    string update_nuc_id;
    list<string> update_prot_ids;

    list<string> replaced_nuc_ids;
    list<string> replaced_prot_ids;
   // map<string, list<string>> replaced_prot_ids;

    list<string> db_nuc_ids;
    map<string, list<string>> db_prot_ids;


    bool IsReplacedNucId(const string& id) const {
        return (find(replaced_nuc_ids.begin(), replaced_nuc_ids.end(), id) != replaced_nuc_ids.end());
    }

    bool DBEntryHasProteins(const string& db_nuc_id) const {
        return  db_prot_ids.find(db_nuc_id) != db_prot_ids.end();
    }

    bool ReplacesEntry(void) const {
        return !replaced_nuc_ids.empty();
    }
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _PROT_MATCH_UTILS_
