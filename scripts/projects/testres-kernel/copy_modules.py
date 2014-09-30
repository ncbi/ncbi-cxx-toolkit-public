#!/opt/python-2.7/bin/python
import os
import shutil
import sys

MODULES_PATH = "src/internal/cppcore/testres/modules"
GENERATE_CONFIG = "generate_config"
DEFAULT_RUN = "default_run"

try:
    src_dir = sys.argv[1]
    bin_dir = sys.argv[2]
    install_dir = sys.argv[3]

    #locate all modules
    modules_dir = os.path.join(src_dir, MODULES_PATH)
    modules_dest_dir = os.path.join(install_dir, MODULES_PATH)

    for dir_entry in os.listdir(modules_dir):
        module_path = os.path.join(modules_dir, dir_entry)
        if os.path.isdir(module_path):
            gen_conf_path = os.path.join(module_path, GENERATE_CONFIG)
            def_run_path = os.path.join(module_path, DEFAULT_RUN)

            if os.path.exists(gen_conf_path) and os.path.exists(def_run_path):

                module_dest_dir = os.path.join(modules_dest_dir, dir_entry)
                os.makedirs(module_dest_dir)
                shutil.copy(gen_conf_path, module_dest_dir)
                shutil.copy(def_run_path, module_dest_dir)
except Exception as excpt:
    print excpt
    exit(1)

exit(0)
