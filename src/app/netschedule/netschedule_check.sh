# $Id$
#!/bin/sh
#
# Check netschedule services

services="
     NS_test        \
     NS_AppLogCGI   \
     NS_AppLog_VolMerge \
     NS_AppLog_VolMerge_Test \
     NS_FluAnnot \
     NS_FrameShifts \
     NS_GBSubmission \
     NS_GWSeqView NS_GeneMark \
     NS_GeoSubmission \
     NS_GeoSubmission_Dev \
     NS_GeoSubmission_Test \
     NS_Glimmer \
     NS_IgBlast \
     NS_Influenza \
     NS_Influenza_Dev \
     NS_Influenza_QA \
     NS_Influenza_Test \
     NS_MapView_Dev \
     NS_PCAssayGraph \
     NS_Pasc \
     NS_QMBP \
     NS_QMBP_Dev \
     NS_SeqComp \
     NS_Splign \
     NS_TmpProdMon \
     NS_TreeBuilder \
     NS_TreeBuilder_Dev \
     NS_TreeBuilder_QA \
     NS_TreeBuilder_Test \
     NS_WebLog \
     NS_gds_ab \
     NS_gds_kmeans \
     NS_sage_gmap"

hosts="
     serviceqa:9051 \
     service1:9051 \
     service2:9051 \
     service3:9051"

res_file="/tmp/`basename $0`.$$"
trap 'rm -f $res_file' 1 2 15

lbsmc='/opt/machine/lbsm/bin/lbsmc'
lbsm_feedback='/opt/machine/lbsm/sbin/lbsm_feedback'

n_ok=0
n_err=0
sum_list=''

os=`env | grep "OS=" | grep -i "Windows"`
if test $? -ne 0; then
    service_machines="service0 service1 service2 service3 serviceqa"
    mask="NS_.*"
    for s in $service_machines ; do
       list=`lbsmc -a -h $s 0 2>&1 | sed 's/  */ /g' | cut -f1 -d' ' | sed '1,/^Service/d' | sed 'g;N;/[*]/{x;q;}' | grep -si "^$mask"`
       #list=`$lbsmc -g $s 0 2>&1 | tail +7 | cut -f1 -d' ' | sed '/-/{x;q;}' | grep -s "$mask"`
       test -n list  ||  list=""
       wholelist=`echo $wholelist $list` 
    done
    wholelist=`for s in $wholelist; do echo $s; done | sort | uniq`

    for s in $wholelist; do
        echo " $services " | grep " $s " 2>&1 >/dev/null
        test $? -ne 0 && missing_services=`echo $missing_services $s`
    done 
fi


for service in `echo $services $hosts`; do
   echo "-------------------------------------------------------"
   printf '%-40s' "Testing service '$service'"
   $CHECK_EXEC netschedule_check $service > $res_file 2>&1
   if test $? -eq 0 ; then
      echo ":OK"
      n_ok=`expr $n_ok + 1`
   else
      echo ":ERR"
      cat $res_file
      n_err=`expr $n_err + 1`
   fi
done

rm -f $res_file

cat <<EOF

*******************************************************

SUCCEEDED:  $n_ok
FAILED:     $n_err
MISSING SERVICES: $missing_services

EOF

exit $n_err
