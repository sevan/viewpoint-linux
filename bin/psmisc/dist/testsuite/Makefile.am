AUTOMAKE_OPTIONS = dejagnu
export DEJAGNU

# Programs that are expected across the board.
DEJATOOL = killall
DEJATOOL += pslog

if WANT_FUSER
  DEJATOOL += fuser
endif


EXTRA_DIST = \
    config/unix.exp \
    fuser.test/fuser.exp \
    pslog.test/pslog.exp \
    killall.test/killall.exp
