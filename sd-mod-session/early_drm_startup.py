"""Replace only the two fbdev show calls in the immutable r26z launcher."""
from pathlib import Path
R=Path(__file__).resolve().parent
def transform(text,old_sha,new_sha):
 assert text.count(old_sha)==2
 text=text.replace(old_sha,new_sha)
 helpers=(R/'src/early_drm_startup.sh').read_text()
 assert text.count('early_watch() {')==1
 text=text.replace('early_watch() {',helpers+'\nearly_watch() {',1)
 old='    "$R/boot-splash" --show >> "$R/events.txt" 2>&1 || log EARLY_SPLASH_UNAVAILABLE'
 assert text.count(old)==1;text=text.replace(old,'    boot_display_start || log EARLY_DRM_UNAVAILABLE',1)
 old='    "$R/boot-splash" --show >> "$R/events.txt" 2>&1 || true'
 assert text.count(old)==1;text=text.replace(old,'    boot_display_start || true',1)
 old='            systemctl --no-block start "$STOCK_UNIT"'
 assert text.count(old)==1;text=text.replace(old,'            boot_display_stop\n'+old,1)
 old='on_exit() {\n';assert text.count(old)==1;text=text.replace(old,old+'    [ "$1" = 0 ] || boot_display_stop\n',1)
 old='    log RECOVERY_BEGIN\n';assert text.count(old)==1;text=text.replace(old,old+'    boot_display_stop\n',1)
 assert '--show' not in text
 return text
