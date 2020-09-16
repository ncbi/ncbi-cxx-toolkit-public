#ifndef FTAMED_H
#define FTAMED_H

BEGIN_NCBI_SCOPE

typedef struct findpubstr {
    bool    always_look;                /* if TRUE, look up even if muid in
                                           Pub-equiv */
    bool    replace_cit;                /* if TRUE, replace Cit-art w/ replace
                                           from MEDLINE */
    Int4    lookups_attempted;          /* citartmatch tries */
    Int4    lookups_succeeded;          /* citartmatch worked */
    Int4    fetches_attempted;          /* FetchPubs tried */
    Int4    fetches_succeeded;          /* FetchPubs that worked */
    bool    merge_ids;                  /* If TRUE then merges Cit-art.ids from
                                           input Cit-sub and one gotten from
                                           med server. */
} FindPubOption, *FindPubOptionPtr;

CRef<objects::CCit_art> FetchPubPmId(Int4 pmid);

void FixPub(TPubList& pub_list, FindPubOptionPtr fpop);
void FixPubEquiv(TPubList& pub_list, FindPubOptionPtr fpop);
bool MedArchInit(void);
void MedArchFini(void);

END_NCBI_SCOPE

#endif // FTAMED_H
