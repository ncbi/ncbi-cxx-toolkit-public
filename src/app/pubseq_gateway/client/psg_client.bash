#!/bin/bash

user_threads=(1 2 4 10 20 50 100)
io_threads=(2 4 8 16 32 64)
requests_per_io=(1 4 16 64 256 1000)
cache_options=("yes" "no")
services=("nctest11:2180" "PSG_Test")
delayed_options=("" "-delayed-completion")
binaries=("./psg_client.0" "./psg_client.1")

xaxis_ref="user_threads[@]"
yaxis_ref="io_threads[@]"
zaxis_ref="services[@]"
waxis_ref="binaries[@]"

zaxis=("${!zaxis_ref}")
waxis=("${!waxis_ref}")

overall_row=4

function run()
{
    local psg_client="$1";
    local requests_file="$2";
    local dir="$3";

    if [[ -d $dir ]]; then
        echo "$dir already exists" >&2;
        exit 1;
    else
        mkdir $dir;
    fi

    local req_io="1";
    local cache="yes";
    local flag="-delayed-completion";

    for x in "${!xaxis_ref}"; do
        for y in "${!yaxis_ref}"; do
            for z in "${!zaxis[@]}"; do
                for w in "${!waxis[@]}"; do
                    local user="$x";
                    local io="$y";
                    local service="${zaxis[$z]}";
                    local options="-service $service -user-threads $user -io-threads $io -use-cache=$cache -requests-per-io=$req_io $flag";
                    local psg_client="${waxis[$w]}";
                    $psg_client performance $options < $requests_file;

                    mv psg_client.table.txt $dir/$x.$y.$z.$w.txt;
                    echo "$x.$y.$z.$w done.";
                done;
            done;
        done;
    done;
}

function parse()
{
    local dir="$1";
    local metric="$2";
    local stat="$3";
    local tmp="$4";

    for z in "${!zaxis[@]}"; do
        for w in "${!waxis[@]}"; do
            for x in "${!xaxis_ref}"; do
                for y in "${!yaxis_ref}"; do
                    local file=$dir/$x.$y.$z.$w.txt;
                    {
                        echo "$x $y ";
                        grep -wi "^${stat}" $file |awk "{print \$${metric}}";
                        grep -w "^Overall" $file |awk "{print \$2}";
                    } |xargs
                done
            done > $tmp/$z.$w.txt;
        done;
    done
}

function gnuplot()
{
    local title="$1";
    local title1="$2";
    local file1="$3";
    local row1="$4";
    local title2="$5";
    local file2="$6";
    local row2="$7";

    if [ $row1 -eq $overall_row ]; then
        local zlabel="seconds";
    else
        local zlabel="milliseconds";
    fi

    local xlabel="${xaxis_ref:0: -3}"; local xlabel="${xlabel//_/ }";
    local ylabel="${yaxis_ref:0: -3}"; local ylabel="${ylabel//_/ }";
    local xaxis=("${!xaxis_ref}");
    local yaxis=("${!yaxis_ref}");
    local xtics="$(for i in "${!xaxis[@]}"; do echo -n "'${xaxis[$i]}' $i, "; done)";
    local ytics="$(for i in "${!yaxis[@]}"; do echo -n "'${yaxis[$i]}' $i, "; done)";
    local xsize="${#xaxis[@]}";
    local ysize="${#yaxis[@]}";

cat << EOF
set view 55,200

set title '$title' offset screen 0,-0.05
set dgrid3d $ysize,$xsize qnorm 2
set xlabel '$xlabel'
set ylabel '$ylabel'
set zlabel '$zlabel' offset 0,0
set xtics (${xtics:0: -2})
set ytics (${ytics:0: -2})

set xyplane 0.25

x(c)=int(c / $ysize)
y(c)=int(c) % $ysize

splot '$file1' using (x(\$0)):(y(\$0)):$row1 with surface lc 1 title '$title1' at 0.2,0.77, \\
      '$file2' using (x(\$0)):(y(\$0)):$row2 with surface lc 2 title '$title2' at 0.2,0.75, \\
      '$file1' using (x(\$0)):(y(\$0)):$row1 with impulses notitle, \\
      '$file2' using (x(\$0)):(y(\$0)):$row2 with impulses notitle, \\
      '$file1' using (x(\$0)):(y(\$0)):$row1:$row1 with labels tc ls 1 offset 0,-1 font ',7' notitle, \\
      '$file2' using (x(\$0)):(y(\$0)):$row2:$row2 with labels tc ls 2 offset 0,1 font ',7' notitle
EOF
}

function prepare()
{
    local stat="$1";
    local dir="$2";
    local request_info="$3";
    local metric_name="$4";

    local title1="stringstream";
    local title2="strstream";

    for z in "${!zaxis[@]}"; do
        local title="time for $request_info requests to ${zaxis[$z]}";
        local file1="./$z.0.txt";
        local file2="./$z.1.txt";

        gnuplot "$metric_name $stat $title" "$title1" "$file1" 3 "$title2" "$file2" 3 >"$dir/${stat}_${zaxis[$z]}.gnuplot"
        gnuplot "Overall $title" "$title1" "$file1" $overall_row "$title2" "$file2" $overall_row >"$dir/overall_${zaxis[$z]}.gnuplot"
    done;
}

function draw()
{
    local dir="$1";
    local metric="$2";
    local stat="$3";
    local request_info="$4";

    local tmp=$(mktemp -d);
    local files=($dir/*.txt);
    local metric_name=$(head -1 ${files[0]} |awk "{print \$${metric}}");

    parse "$dir" "$metric" "$stat" "$tmp"
    prepare "$stat" "$tmp" "$request_info" "$metric_name"
cat << EOF
Results were stored at:
$tmp

To adjust Z axis add:
set zrange [0:ZMAX]
e.g. set zrange [0:160]

To see graphs:
cat FILE - |gnuplot -
e.g. cat average_two.gnuplot - |gnuplot -

To create png files add:
set output 'PNG_FILE'
set terminal pngcairo size WIDTH,HEIGHT
e.g. set output '020519.blob_two_yes.png'; set terminal pngcairo size 1380,910
EOF
}

if [ $# -eq 4 ]; then
    if [ "$1" == "run" ]; then
        run $2 $3 $4
        exit 0
    fi
fi

if [ $# -eq 6 ]; then
    if [ "$1" == "draw" ]; then
        draw $2 $3 $4 "$6 $5"
        exit 0
    fi
fi

cat << EOF
Usage:
$0 run BINARY INPUT_FILE OUTPUT_DIR
e.g. $0 bin/psg_client json/blob.10k.json output/blob_10k/
OR
$0 draw INPUT_DIR METRIC STAT TYPE NUMBER
e.g. $0 draw output/blob_10k 3 median blob 10k
EOF

exit 1
