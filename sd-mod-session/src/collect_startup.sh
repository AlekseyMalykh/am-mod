#!/bin/sh
# Read-only MPC startup diagnostics. Writes only its report on the inserted SD.
set -u
PATH=/usr/sbin:/usr/bin:/sbin:/bin
export PATH
umask 077
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
case "$DIR" in /media/*/autorun) ;; *) exit 1 ;; esac
case "$DIR" in *' '*|*'%'*|*'\'*) exit 1 ;; esac
SD=${DIR%/autorun}
case "${SD#/media/}" in ''|*[!A-Za-z0-9._-]*) exit 1 ;; esac
[ "$(readlink -f "$SD")" = "$SD" ] || exit 1
awk -v p="$SD" '$2==p && $3=="exfat" && $4 ~ /(^|,)rw(,|$)/ {n++} END {exit n!=1}' /proc/mounts || exit 1
OUT="$DIR/MPC_R9B_DIAGNOSTICS.txt"
[ ! -L "$OUT" ] || exit 1
exec > "$OUT" 2>&1 || exit 1
echo MPC_R9B_STARTUP_READ_ONLY_DIAGNOSTICS_V1
echo 'No service stop/start, mounts, firmware changes, application execution or project writes.'
date -u +%Y-%m-%dT%H:%M:%SZ
echo "UID=$(id -u) SCRIPT=$0 CWD=$(pwd)"
echo '=== Collector SHA256 ==='
sha256sum "$0"
echo '=== Boot identity and uptime ==='
cat /proc/sys/kernel/random/boot_id /proc/uptime
echo '=== Current trial files in RAM ==='
R=/run/mpc-clone-trial-v1
if [ -d "$R" ] && [ ! -L "$R" ] && [ "$(stat -f -c %T "$R")" = tmpfs ]; then
    ls -la "$R"
    for name in events.txt report-path sd-device autorun-id startup-deadline recovered switch-intent clone-verified data-staged app/probe.txt app/clone.txt proxy/proxy.txt storage-meta/data-image-type storage-meta/data-image-identity storage-meta/data-changes-identity storage-meta/original-data-identity storage-meta/save-path; do
        p="$R/$name"
        echo "--- $name ---"
        if [ -f "$p" ] && [ ! -L "$p" ]; then tail -c 65536 "$p"; else echo ABSENT_OR_NOT_REGULAR; fi
    done
    echo '=== Actual RAM launcher SHA256 ==='
    if [ -f "$R/launcher.sh" ] && [ ! -L "$R/launcher.sh" ]; then sha256sum "$R/launcher.sh"; fi
else
    echo TRIAL_RAM_DIRECTORY_ABSENT_OR_NOT_TMPFS
fi
echo '=== Service and mount status ==='
for unit in ac50.service az01-script-runner.service mpc-clone-trial-v1-probe.service mpc-clone-trial-v1-clone.service mpc-clone-trial-v1-supervisor.service mpc-clone-trial-v1-recover.service mpc-clone-trial-v1-bus-proxy.service mpc-clone-trial-v1-guard.timer 'run-mpc\x2dclone\x2dtrial\x2dv1-data\x2dimage.mount' 'run-mpc\x2dclone\x2dtrial\x2dv1-data\x2dlower.mount' 'run-mpc\x2dclone\x2dtrial\x2dv1-data\x2dchanges.mount'; do
    echo "--- $unit ---"
    systemctl show "$unit" -p LoadState -p ActiveState -p SubState -p Result -p MainPID -p ExecMainCode -p ExecMainStatus -p DropInPaths
done
echo '=== Running native application identity ==='
pid=$(systemctl show ac50.service -p MainPID --value)
case "$pid" in ''|0|*[!0-9]*) ;; *)
    readlink "/proc/$pid/exe"
    stat -Lc '%n %d:%i %s' "/proc/$pid/exe"
    ;;
esac
echo '=== Current mounts ==='
cat /proc/self/mountinfo
echo '=== Filesystems and memory ==='
cat /proc/filesystems
awk '/^(MemTotal|MemFree|MemAvailable|Shmem|SwapTotal|SwapFree):/ {print}' /proc/meminfo
df -k /run /data "$SD"
echo '=== Current-boot trial, mount and native launcher journal ==='
journalctl -b -u ac50.service -u az01-script-runner.service -u mpc-clone-trial-v1-probe.service -u mpc-clone-trial-v1-clone.service -u mpc-clone-trial-v1-supervisor.service -u mpc-clone-trial-v1-recover.service -u mpc-clone-trial-v1-bus-proxy.service -u mpc-clone-trial-v1-guard.service -u 'run-mpc\x2dclone\x2dtrial\x2dv1-data\x2dimage.mount' -u 'run-mpc\x2dclone\x2dtrial\x2dv1-data\x2dlower.mount' -u 'run-mpc\x2dclone\x2dtrial\x2dv1-data\x2dchanges.mount' -n 220 --no-pager
echo '=== Current-boot kernel journal tail ==='
journalctl -b -k -n 60 --no-pager
echo '=== Card launcher metadata ==='
ls -l "$DIR/autorun.sh" "$DIR/autorun.json" "$DIR/autorun.check" "$DIR/MPC_CLONE_TRIAL.txt"
echo DIAGNOSTICS_COMPLETE
sync
exit 0
