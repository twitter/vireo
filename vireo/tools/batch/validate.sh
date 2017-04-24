#! /bin/bash -e

TOOL_NAME=validate
NUM_ARGS=$#

SCRIPT_DIR=$(cd `dirname "${BASH_SOURCE[0]}"` && pwd)
. ${SCRIPT_DIR}/.setup ${TOOL_NAME}

if [ ${NUM_ARGS} -lt 1 ]; then
    printf "Usage: $0 [options] input_dir\n"
    printf "\nOptions: for options check ${TOOL_NAME} tool usage below\n"
    printf "\n --- ${TOOL_NAME} tool usage --- \n\n"
    ${EXEC} --help
    exit -1
fi

NUM_OPTS=$[NUM_ARGS - 1]
OPTS=${@: 1:${NUM_OPTS}}
IN_FOLDER=${@: -1:1}

COMMAND=${EXEC}' '${OPTS}' < "{}"'

. ${SCRIPT_DIR}/.run
