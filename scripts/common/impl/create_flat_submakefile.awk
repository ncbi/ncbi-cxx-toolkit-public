# $Id$

BEGIN                        { stage = 0; target = ""; in_all_projects = 0 }
(stage == 0  &&  /^[ \t]*$/) { stage = 1 }
(stage == 1  &&  /^all_/)    { stage = 2 }
(stage == 1)                 { print; next }
/^all_projects[ \t]*=/       { in_all_projects = 1; next }
/^all_dataspec[ \t]*=/       { in_all_dataspec = 1; next }
/^all_dirs[ \t]*=/           { in_all_dirs = 1; last_dir = ""; print; next }
(in_all_projects == 1)       {
    orig_all_projects[$1] = 1
    in_all_projects = ($NF == "\\")
    next
}
(in_all_dataspec == 1)       {
    orig_all_dataspec[$1] = 1
    in_all_dataspec = ($NF == "\\")
    next
}
(in_all_dirs == 1)           {
    if ($1 == subdir) {
        last_dir = "."
    } else if (sub("^" subdir "/", "", $1)) {
        if (last_dir != "") {
            print "    " last_dir " \\"
        }
        last_dir = $1
    }
    if ($NF != "\\") {
        print "    " last_dir
        in_all_dirs = 0
        stage = 1
    }
    next
}
/\.real[ \t]*:/              {
    n = split($0, a, "\\.real[ :]*")
    target = a[1]
    deps = ""
    for (i = 2;  i <= n;  ++i) {
        dep = a[i]
        sub("^" subdir "/", "", dep)
        deps = deps " " dep
    }
    sub("^" subdir "/", "", target)
    all_deps[target] = deps
    rules = ""
    next
}
(length(target)  &&  /^\t/)  { rules = rules "\n" $0; next }
(length(target)  &&  !/^\t/) {
    if (gsub("cd " subdir "/", "cd ./", rules)) {
        split(rules, split_rules, /[ \t;]/)
        if (split_rules[2] ~ /^[+-]*cd$/) {
            dir = split_rules[3]
            all_dirs[dir] = 1
        }
        all_rules[target] = rules
        all_targets = all_targets " " target
        if (target ~ /\.files$/ && target in orig_all_dataspec) {
            all_dataspec = all_dataspec " " target
            if (! (dir in spec_bearing_dirs) ) {
                spec_bearing_dirs[dir] = target
            }
        } else if (target in orig_all_projects) {
            all_projects = all_projects " " target
            if (target ~ /\.(lib|dll)$/) {
                all_libraries = all_libraries " " target
            }
        }
        if (rules ~ /\t\+?cd /) {
            non_expendable_dirs[dir] = 1
        }
    } else if (sub("^" subdir "/", "", target)  &&  target != "") {
        all_rules[target] = rules
        all_targets = all_targets " " target
    }
    target = ""
    next
}
END {
    make = "$(MAKE) $(MFLAGS_NR) -f $(MINPUT)"
    sep  = " \\\n    "
    all = all_projects
    gsub(" ", sep, all)
    print "all_projects =" all "\n"
    print "ptb_all :"
    print "\t" make " ptb_all.real MTARGET=$(MTARGET)\n"
    print "ptb_all.real : $(all_projects:%=%.real)\n"
    all = all_libraries
    gsub(" ", sep, all)
    print "all_libraries = " all "\n"
    print "all_libs :"
    print "\t" make " all_libs.real MTARGET=$(MTARGET)\n"
    print "all_libs.real : $(all_libraries:%=%.real)\n"
    all = all_dataspec
    gsub(" ", sep, all)
    print "all_dataspec = " all "\n"
    print "all_files :"
    print "\t" make " all_files.real MTARGET=$(MTARGET)\n"
    print "all_files.real : $(all_dataspec:%=%.real)\n"
    np = split(all_targets, p, " ")
    for (i = 1;  i <= np;  ++i) {
        if (p[i] ~ /\/$/) {
            print ".PHONY : " p[i] "\n"
        }
        print p[i] " :"
        print "\t" make " " p[i] ".real MTARGET=$(MTARGET)\n"
        printf "%s.real :", p[i]
        rules = all_rules[p[i]]
        if (("/" p[i]) !~ /\/\.files$/  &&  all_deps[p[i]] !~ /\/\.files/ \
            &&  match(rules, /cd [^ ;]+/)) {
            dir = substr(rules, RSTART + 3, RLENGTH - 3)
            printf " %s.files.real", dir
        } else {
            dir = ""
        }
        nd = split(all_deps[p[i]], d, " ")
        for (j = 1;  j <= np;  ++j) {
            if (d[j] in all_rules) {
                printf " %s.real", d[j]
            }
        }
        gsub("cd \\./; ", "", rules)
        print rules
        if (dir != "" && ! (dir in seen_dirs)) {
            print
            if (dir in spec_bearing_dirs) {
                print dir ".files.real: " spec_bearing_dirs[dir] ".real ;"
                print "spec_bearing_dirs += " dir
            } else if (dir in non_expendable_dirs) {
                print "plain_dirs += " dir
            } else {
                print "expendable_dirs += " dir
            }
            seen_dirs[dir] = 1
        }
        print ""
    }
    print "include $(top_srcdir)/src/build-system/Makefile.flat_tuneups"
}
