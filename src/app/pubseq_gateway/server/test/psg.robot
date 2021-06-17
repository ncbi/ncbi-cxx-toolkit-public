| *** Settings *** |
| Test Template | UDC Test Case |
| Resource  | robot_keywords.robot |
| Documentation | UDC tests for PubSeq. |

| *** Variables *** |
| ${INPUT_DIR_ROOT}        | ${CURDIR}/input                | # root directory for input files    |
| ${BASELINE_DIR_ROOT}     | ${CURDIR}/baseline             | # root directory for baseline files |
| ${NO_INPUT_FILE}         | true                           | # No input file is required for the test |
| ${TEST_APPS_DIR}         | ${CURDIR}/                     | # Location of executables |

# Local application
| ${IS_LOCAL}              | true                           | # Enable testing the application locally (with command line) |
| ${APP_NAME}              | psg.bash -server ${PSG_SERVER} -https ${PSG_HTTPS} | # Name of the application |
| ${PARAMS}                | ${EMPTY}                       | # Parameters for finding server
| ${APP_BN}                | ${TEST_APPS_DIR}${APP_NAME} ${PARAMS} | # Path to the application |
| ${RUN_FROM_APP_DIR}      | false                          | # Run the application to be tested from its own directory |
| ${USE_DASHI_DASHO}       | true                           | # Automatically pass "-i" and "-o" parameters to the application |
| ${IS_GRID_WORKER}        | false                          | # Automatically set "-offline-input-dir" and "-offline-output-dir" flags |

# Remote application
| ${IS_REMOTE}             | false                          | # Enable testing application remotely (via HTTP request) |

# Other processing
| ${PRE_PROCESSOR}         | ${EMPTY}                       | # The pre-processor app |
| ${PRE_PROCESSOR_ARGS}    | ${EMPTY}                       | # Arguments for the preprocessor |
#| ${POST_PROCESSOR}        | ${CURDIR}/postprocess          | # The post-processor app |
| ${POST_PROCESSOR}        | ${EMPTY}                       | # The post-processor app |
| ${POST_PROCESSOR_ARGS}   | ${EMPTY}                       | # Arguments for the preprocessor |

| *** Test Cases *** |
| use_manifest_txt |
|    | ${EMPTY} |
