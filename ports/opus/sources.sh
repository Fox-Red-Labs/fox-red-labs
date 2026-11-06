# Source file lists for libopus 1.5.2, from its own *_sources.mk files. srcs fixed|float prints the absolute paths. Sourced by the build scripts.
srcs() {
  local O="$REPO/third-party/opus" kind=$1
  _list() { sed -n "/^$2 *= *\\\\/,/^\$/p" "$O/$1" | grep -o '[a-zA-Z0-9_/]*\.c' ; }
  { _list opus_sources.mk OPUS_SOURCES; _list celt_sources.mk CELT_SOURCES; _list silk_sources.mk SILK_SOURCES
    if [ "$kind" = fixed ]; then _list silk_sources.mk SILK_SOURCES_FIXED; else _list opus_sources.mk OPUS_SOURCES_FLOAT; _list silk_sources.mk SILK_SOURCES_FLOAT; fi
  } | sort -u | sed "s#^#$O/#" | grep -v 'projection\|multistream\|mapping_matrix'
}
