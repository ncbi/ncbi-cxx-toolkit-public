
// --- Constants

# set app=test_ncbi_clog_templates


// --- Useful regexp's for inline variables

# set d3=\d{3,}
# set d5=\d{5,}


// --- Regexp's for parts of logging string

# set guid_re=[0-9A-Z]{16}
# set time_re=\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}

// automatically generated PHID
# set phid_re=[0-9A-Z]{32}

// execution time for request/app (1.234)
# set timespan_re=\d{1,}\.\d{3}

// standard prefix for most records
# set std=${guid_re} \d{4,}/\d{4,} ${time_re} TESTHOST {7} UNK_CLIENT {5}

// prefix for app-level records with unknown session
# set std_unk=${std} UNK_SESSION {13} ${app}

// prefix for request records with known session
# set std_sid=${std} ${guid_re}_\d{4}SID ${app}



// Match 'start' line and get next variables from it:
// ${pid}, ${tid}, ${guid}
//
# set GetStartVars=${pid=${d5}}/${tid=${d3}}/0000/PB ${guid=${guid_re}} \d{4,}/\d{4,} ${time_re} TESTHOST {7} UNK_CLIENT {5} UNK_SESSION {13} ${app} start

