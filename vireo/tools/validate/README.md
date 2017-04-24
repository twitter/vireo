Validate
=========

* Usage:

    validate -threads 2,4,.. < infile
    - Decode all frames and sounds from movie fed via stdin
    
    validate --help
    - Show usage
    
    validate --version
    - Show version

* Build on Mac:

    CC=/usr/bin/clang xcodebuild

* Build on Linux:

    make

* Build executable is in twitter-media/vireo/build/release

* To deploys on Aurora job defined in scrips/deploy.aurora with a given cpu count (default: 1):

    CPUs=1,2,.. ./scripts/deploy.sh
    