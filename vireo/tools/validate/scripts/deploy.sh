#!/bin/sh -e

DIR=$(cd `dirname "${BASH_SOURCE[0]}"` && pwd)
PROJECT=validate

if [ -z "$CPUs" ]; then
  export CPUs=1
fi
echo "Deploying with $CPUs cpu(s)"

zip -jr "$DIR"/"$PROJECT-$CPUs".zip "$DIR"/../720-30s.mp4 "$DIR"/../../../build/release/$PROJECT
aurora package_add_version --cluster=smf1 $USER "$PROJECT-$CPUs" "$DIR"/"$PROJECT-$CPUs".zip
aurora deployment create smf1/$USER/devel/"$PROJECT-$CPUs" "$DIR"/deploy.aurora -r
rm "$DIR"/"$PROJECT-$CPUs".zip
