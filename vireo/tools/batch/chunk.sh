#! /bin/bash -e

TOOL_NAME=chunk
NUM_ARGS=$#

if [ ${NUM_ARGS} -lt 2 -a ${NUM_ARGS} -ne 0 ]; then
    printf "Illegal number of parameters\n\n"
fi

SCRIPT_DIR=$(cd `dirname "${BASH_SOURCE[0]}"` && pwd)
. ${SCRIPT_DIR}/.setup ${TOOL_NAME}

if [ ${NUM_ARGS} -lt 2 ]; then
    printf "Usage: $0 [options] input_dir output_dir\n"
    printf "\nOptions: for options check ${TOOL_NAME} tool usage below\n"
    printf "\n --- ${TOOL_NAME} tool usage --- \n\n"
    ${EXEC}
    exit -1
fi

NUM_OPTS=$[NUM_ARGS - 2]
OPTS=${@: 1:${NUM_OPTS}}
IN_FOLDER=${@: -2:1}
OUT_FOLDER=${@: -1:1}

COMMAND=${EXEC}' '${OPTS}' "{}" '${OUT_FOLDER}

. ${SCRIPT_DIR}/.run
