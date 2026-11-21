"""Stage the D.S.B USB bind transaction together with its pinned host helper."""
import re
def transform(launcher,drive_sha,boot_sha):
 old='for drive_name in usb-drive.sh usb-drive-screen; do'
 new='for drive_name in usb-drive.sh usb-drive-screen usb-drive-boot.sh; do'
 if new not in launcher:
  assert launcher.count(old)==1;launcher=launcher.replace(old,new,1)
 pattern=r'case "\$drive_name" in usb-drive.sh\) drive_sha=[0-9a-f]{64} ;; usb-drive-screen\) drive_sha=([0-9a-f]{64}) ;;(?: usb-drive-boot.sh\) drive_sha=[0-9a-f]{64} ;;)? esac'
 def pins(match):return f'case "$drive_name" in usb-drive.sh) drive_sha={drive_sha} ;; usb-drive-screen) drive_sha={match[1]} ;; usb-drive-boot.sh) drive_sha={boot_sha} ;; esac'
 launcher,count=re.subn(pattern,pins,launcher);assert count==1
 launcher=launcher.replace('head -c 64 "$R/usb-drive/error" > "$R/app/usb-drive-error"','dd if="$R/usb-drive/error" bs=64 count=1 2>/dev/null > "$R/app/usb-drive-error"')
 return launcher
