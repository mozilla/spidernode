#!/bin/bash -ex

function usage() {
    echo "Usage: $0 [--project <shell|browser>] <workspace-dir> flags..."
    echo "flags are treated the same way as a commit message would be"
    echo "(as in, they are scanned for directives just like a try: ... line)"
}

PROJECT=shell
WORKSPACE=
DO_TOOLTOOL=1
while [[ $# -gt 0 ]]; do
    if [[ "$1" == "-h" ]] || [[ "$1" == "--help" ]]; then
        usage
        exit 0
    elif [[ "$1" == "--project" ]]; then
        shift
        PROJECT="$1"
        shift
    elif [[ "$1" == "--no-tooltool" ]]; then
        shift
        DO_TOOLTOOL=
    elif [[ -z "$WORKSPACE" ]]; then
        WORKSPACE=$( cd "$1" && pwd )
        shift
        break
    fi
done

SCRIPT_FLAGS=$*

# Ensure all the scripts in this dir are on the path....
DIRNAME=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
PATH=$DIRNAME:$PATH

# Use GECKO_BASE_REPOSITORY as a signal for whether we are running in automation.
export AUTOMATION=${GECKO_BASE_REPOSITORY:+1}

: "${GECKO_DIR:="$DIRNAME"/../../..}"
: "${TOOLTOOL_CACHE:=$WORKSPACE/tt-cache}"

if ! [ -d "$GECKO_DIR" ]; then
    echo "GECKO_DIR must be set to a directory containing a gecko source checkout" >&2
    exit 1
fi
GECKO_DIR="$( cd "$GECKO_DIR" && pwd )"

# Directory to populate with tooltool-installed tools
export TOOLTOOL_DIR="$WORKSPACE"

# Directory to hold the (useless) object files generated by the analysis.
export MOZ_OBJDIR="$WORKSPACE/obj-analyzed"
mkdir -p "$MOZ_OBJDIR"

if [ -n "$DO_TOOLTOOL" ]; then (
    cd "$TOOLTOOL_DIR"
    "$GECKO_DIR"/mach artifact toolchain -v\
                ${TOOLTOOL_MANIFEST:+ --tooltool-url https://tooltool.mozilla-releng.net/ \
                                      --tooltool-manifest "$GECKO_DIR/$TOOLTOOL_MANIFEST"} \
                --cache-dir "$TOOLTOOL_CACHE"${MOZ_TOOLCHAINS:+ ${MOZ_TOOLCHAINS}}
) fi

export NO_MERCURIAL_SETUP_CHECK=1

if [[ "$PROJECT" = "browser" ]]; then (
    cd "$WORKSPACE"
    set "$WORKSPACE"
    # Mozbuild config:
    export MOZBUILD_STATE_PATH=$WORKSPACE/mozbuild/
    # Create .mozbuild so mach doesn't complain about this
    mkdir -p "$MOZBUILD_STATE_PATH"
) fi
. hazard-analysis.sh

build_js_shell
analysis_self_test

# Artifacts folder is outside of the cache.
mkdir -p "$HOME"/artifacts/ || true

function onexit () {
    grab_artifacts "$WORKSPACE/analysis" "$HOME/artifacts"
}

trap onexit EXIT

configure_analysis "$WORKSPACE/analysis"
run_analysis "$WORKSPACE/analysis" "$PROJECT"

check_hazards "$WORKSPACE/analysis"

################################### script end ###################################
