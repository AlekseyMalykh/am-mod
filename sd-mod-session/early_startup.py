"""Apply owned early-startup changes to the frozen, previously tested loader."""
from pathlib import Path
R=Path(__file__).resolve().parent
def transform_entry(text):
 assert text.count('sync || exit 1')==2
 # The verified controller flushes the pinned SD at its checkpoints. Avoid
 # two whole-system syncs before the early stop while stock is still loading.
 return text.replace('sync || exit 1', ': # SD startup logs are flushed by the verified controller.')
def transform_launcher(text,splash_sha):
 def replace(old,new):
  nonlocal text
  assert text.count(old)==1,old;text=text.replace(old,new,1)
 fragment=(R/'src/early_startup.sh').read_text().replace('EARLY_SPLASH_SHA',splash_sha)
 replace('flush_report_sd() {',fragment+'\nflush_report_sd() {')
 replace('case "${1:-main}" in\n','case "${1:-main}" in\n  early-watch) early_watch; exit 0 ;;\n')
 replace('log PRECHECK_BEGIN\n','early_begin\nlog PRECHECK_BEGIN\n')
 replace('# No daemon or stock state changes until identity and sandbox checks have passed.',
         '# Early stop is protected by its own recovery service; full validation follows under the splash.')
 old='''systemctl is-active --quiet "$STOCK_UNIT" || fail STOCK_NOT_ACTIVE
pid=$(prop "$STOCK_UNIT" MainPID)
[ "$pid" -gt 1 ] || fail STOCK_PID_MISSING
[ "$(stat -Lc '%d:%i' "/proc/$pid/exe")" = "$(stat -Lc '%d:%i' "$STOCK")" ] || fail UNKNOWN_RUNNING_APP'''
 replace(old,'''if [ -f "$R/early-stopped" ]; then
    [ "$(prop "$STOCK_UNIT" MainPID)" = 0 ] || fail STOCK_RESTARTED_DURING_PREPARATION
else
'''+old+'\nfi')
 replace('    systemctl is-active --quiet "$STOCK_UNIT" || fail STOCK_NOT_RUNNING',
 '''    if [ -f "$R/early-stopped" ]; then
        [ "$(prop "$STOCK_UNIT" MainPID)" = 0 ] || fail STOCK_RESTARTED_BEFORE_HANDOFF
    else
        systemctl is-active --quiet "$STOCK_UNIT" || fail STOCK_NOT_RUNNING
    fi''')
 replace('    systemctl stop "$STOCK_UNIT"\n    [ "$(prop "$STOCK_UNIT" MainPID)" = 0 ] || fail STOCK_STILL_RUNNING',
 '''    [ -f "$R/early-stopped" ] || systemctl stop "$STOCK_UNIT"
    [ "$(prop "$STOCK_UNIT" MainPID)" = 0 ] || fail STOCK_STILL_RUNNING
    "$R/boot-splash" --show >> "$R/events.txt" 2>&1 || true''')
 # The permanent guard takes responsibility only after its stock-return
 # override exists. Until then the early watcher remains independently armed.
 old='''systemctl start "$U-guard.timer" || fail GUARD_START_FAILED
systemctl is-active --quiet "$U-guard.timer" || fail GUARD_NOT_ACTIVE
'''
 replace(old,'')
 replace('log "RECOVERY_ARMED; manual Mod Off, no session time limit; independent controller health check"',
 '''touch "$R/switch-intent"
systemctl start "$U-guard.timer" || fail GUARD_START_FAILED
systemctl is-active --quiet "$U-guard.timer" || fail GUARD_NOT_ACTIVE
touch "$R/early-handoff"
[ ! -f "$R/early-watch-systemd" ] || systemctl --no-block stop "$U-early-watch.service" || true
log "RECOVERY_ARMED; normal power copy, no session time limit; independent controller health check"''')
 replace('on_exit() {\n', '''on_exit() {
    if [ "$1" -ne 0 ] && [ "${exit_mode:-main}" = main ] && [ -f "$R/early-handoff" ]; then
        systemctl --no-block start "$U-recover.service" || true
    fi
''')
 return text
