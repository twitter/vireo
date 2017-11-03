#! /usr/bin/env bash

set -e

# check if PREFIX is defined -- this is the directory where vireo is installed
if [ -z ${PREFIX+x} ]; then
  echo "ERROR: Unable to build Scala. Please set PREFIX to where vireo is installed and rerun."
  exit -1
fi

# setup
PLATFORM=`uname -s`
if [ "${PLATFORM}" = "Darwin" ] ;  then
  EXT=dylib
else
  EXT=so
fi

VIREO_LIB_PATH=${PREFIX}/lib
LIB_BINARY=${VIREO_LIB_PATH}/libvireo.${EXT}

SCRIPT_NAME=`basename "${BASH_SOURCE[0]}"`
SCRIPT_DIR=$(cd `dirname "${BASH_SOURCE[0]}"` && pwd)
VIREO_DIR=${SCRIPT_DIR}

pushd ${VIREO_DIR}

# read vireo version
VERSION_NUMBER=`grep -E 'VIREO_VERSION \"(.*)\"' version.h | cut -d\" -f2`

# change directory to scala directory
cd scala/vireo-scala

# replace VIREO_VERSION string in templates with actual version number and save
TEMPLATES="pom.xml.template"
COMMAND="cat {} | sed 's/VIREO_VERSION/${VERSION_NUMBER}/g' > \`dirname {}\`/\`basename {} .template\`"
find . -name ${TEMPLATES} -exec sh -c "${COMMAND}" \;

# build Scala wrappers
mvn install

# copy binary and rename
cp ${LIB_BINARY} libvireo-scala.${EXT}

# add binary to jar file
jar uvf target/vireo-scala_2.11-${VERSION_NUMBER}.jar libvireo-scala.dylib

# copy compiled jar file to PREFIX
cp target/vireo-scala_2.11-${VERSION_NUMBER}.jar ${VIREO_LIB_PATH}/

echo "Scala jar file is built successfully @" ${VIREO_LIB_PATH}/vireo-scala_2.11-${VERSION_NUMBER}.jar

popd

exit 0
