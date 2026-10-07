# Sourced by the scripts that play network games in the real game (tools/mp-*-test.sh): a network game saves itself
# every month as autosave.mpsav in the data directory (T5.3, D-074), where Alexandre's own games keep theirs. The
# scripts keep his aside before the test (mp_keep_autosave) and put it back after (mp_restore_autosave).
# Needs DATA_DIR.
MP_KEPT_AUTOSAVE="$DATA_DIR/autosave.mpsav.kept-by-test"

mp_keep_autosave() {
    if [ -f "$DATA_DIR/autosave.mpsav" ] && [ ! -f "$MP_KEPT_AUTOSAVE" ]; then
        mv "$DATA_DIR/autosave.mpsav" "$MP_KEPT_AUTOSAVE"
    fi
}

mp_restore_autosave() {
    rm -f "$DATA_DIR/autosave.mpsav" "$DATA_DIR"/mp-autosave-*
    if [ -f "$MP_KEPT_AUTOSAVE" ]; then
        mv "$MP_KEPT_AUTOSAVE" "$DATA_DIR/autosave.mpsav"
    fi
}
