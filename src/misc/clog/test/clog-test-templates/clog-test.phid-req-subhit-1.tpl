# include _clog.tpl

^${GetStartVars}
^${pid}/${tid}/0000/P  ${std_unk} extra         ncbi_phid=${phidapp=${phid_re}}

// Request 1
^${pid}/${tid}/0001/RB ${std_sid} request-start
^${pid}/${tid}/0001/R  ${std_sid} extra         ncbi_phid=${phidreq1=${phid_re}}
^${pid}/${tid}/0001/R  ${std_sid} Info: ${phidreq1}\.1
^${pid}/${tid}/0001/RE ${std_sid} request-stop  200 ${timespan_re} 1 2

// Request 2
^${pid}/${tid}/0002/RB ${std_sid} request-start
^${pid}/${tid}/0002/R  ${std_sid} extra         ncbi_phid=${phidreq2=${phid_re}}
^${pid}/${tid}/0002/R  ${std_sid} Info: ${phidreq2}\.1
^${pid}/${tid}/0002/RE ${std_sid} request-stop  200 ${timespan_re} 1 2

// Stop
^${pid}/${tid}/0000/PE ${std_unk} stop          0 ${timespan_re}


// All PHIDs should be different

# test ${phidapp}!=${phidreq1}
# test ${phidapp}!=${phidreq2}
# test ${phidreq1}!=${phidreq2}
