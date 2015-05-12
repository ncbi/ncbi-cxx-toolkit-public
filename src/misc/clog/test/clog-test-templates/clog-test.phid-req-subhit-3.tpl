# include _clog.tpl

^${GetStartVars}
^${pid}/${tid}/0000/P  ${std_unk} extra         ncbi_phid=SOME_ENV_PHID
^${pid}/${tid}/0000/P  ${std_unk} Info: SOME_ENV_PHID\.1

// Request 1 -- with user-set PHID
^${pid}/${tid}/0001/RB ${std_sid} request-start
^${pid}/${tid}/0001/R  ${std_sid} extra         ncbi_phid=REQUEST1
^${pid}/${tid}/0001/R  ${std_sid} Info: REQUEST1\.1
^${pid}/${tid}/0001/RE ${std_sid} request-stop  200 ${timespan_re} 1 2

// App-level in-between requests sub-hit-id
^${pid}/${tid}/0000/P  ${std_unk} Info: SOME_ENV_PHID\.2

// Request 2 -- with inherited PHID
^${pid}/${tid}/0002/RB ${std_sid} request-start
^${pid}/${tid}/0002/R  ${std_sid} extra         ncbi_phid=SOME_ENV_PHID
^${pid}/${tid}/0002/R  ${std_sid} Info: SOME_ENV_PHID\.3
^${pid}/${tid}/0002/RE ${std_sid} request-stop  200 ${timespan_re} 1 2

// Stop
^${pid}/${tid}/0000/P  ${std_unk} Info: SOME_ENV_PHID\.4
^${pid}/${tid}/0000/PE ${std_unk} stop          0 ${timespan_re}

