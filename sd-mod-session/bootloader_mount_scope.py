"""Keep the writable-mount audit strict when D.S.B hides its config bind.

D.S.B first bind-mounts card-root mpcqol.conf at autorun/os/mpcqol.conf,
then covers autorun with AM's directory. mountinfo retains the hidden row.
It grants no reachable write access in the clone's private mount namespace.
"""
from pathlib import Path
PREAMBLE='''    # D.S.B keeps a config bind below the autorun overmount. Exempt only
    # that exact row when even its parent directory is absent: the file is
    # unreachable, not an extra writable configuration path. Visible files,
    # symlink parents and every other writable disk still fail the audit.
    boot_config="${save_path%/*}/autorun/os/mpcqol.conf"
    boot_config_hidden=0
    if [ ! -e "${boot_config%/*}" ] && [ ! -L "${boot_config%/*}" ]; then
        boot_config_hidden=1
    fi
'''
def transform(launcher):
 if 'hidden_boot_config++' in launcher:return launcher
 anchor='    awk -v save="$save_path" -v save_fs="$save_fs" -v data_type="$data_type" -v data_id="$data_mount_id" '
 assert launcher.count(anchor)==1,'No unique current storage-scope audit'
 launcher=launcher.replace(anchor,PREAMBLE+'    awk -v boot_config="$boot_config" -v boot_config_hidden="$boot_config_hidden" -v save="$save_path" -v save_fs="$save_fs" -v data_type="$data_type" -v data_id="$data_mount_id" ',1)
 anchor='          else { print "UNEXPECTED_WRITABLE_DISK: "'
 assert launcher.count(anchor)==1
 launcher=launcher.replace(anchor,'          else if (boot_config_hidden==1 && target==boot_config && fs==save_fs) {hidden_boot_config++; print "HS_BOOT_HIDDEN_CONFIG: " target}\n'+anchor,1)
 anchor='        exit bad || allowed_projects!=1'
 assert launcher.count(anchor)==1
 return launcher.replace(anchor,'        exit bad || hidden_boot_config>1 || allowed_projects!=1',1)
