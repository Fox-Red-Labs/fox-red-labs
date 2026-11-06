# source this: sets EXTRA_SRC for our CoreMark port (EEMBC files unmodified from third-party/coremark, our core_portme.c)
C="$HOME/tmp/fox-red-labs/third-party/coremark"
export EXTRA_SRC="$C/core_list_join.c $C/core_main.c $C/core_matrix.c $C/core_state.c $C/core_util.c $HOME/tmp/fox-red-labs/am263p/bench/coremark/core_portme.c"
