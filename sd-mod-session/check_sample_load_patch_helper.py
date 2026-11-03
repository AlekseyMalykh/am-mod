"""Test the shipped patch helper at its fixed disposable RAM path (WSL root)."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys

R = Path(__file__).resolve().parent
O = R / ('build/workspace-fix' if '--workspace-fix' in sys.argv else 'build/sample-load')
target_dir = Path('/run/mpc-clone-trial-v1')
assert os.geteuid() == 0 and not target_dir.exists()
target_dir.mkdir(mode=0o700)
target = target_dir / 'MPC-Sample'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
try:
    shutil.copyfile(R.parent/'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample', target)
    helper = ['qemu-aarch64', '-L', '/home/user/mpc-rootfs-user-20260808', str(O/'apply-session')]
    result = subprocess.run(helper, check=True, capture_output=True, text=True)
    assert result.stdout.strip() == 'SESSION_RAM_PATCH_OK'
    expected = (O/'patched-image-sha256.txt').read_text().strip()
    assert sha(target) == expected
    # Already-patched input must be rejected before any further writes.
    rejected = subprocess.run(helper, capture_output=True, text=True)
    assert rejected.returncode != 0 and 'original bytes' in rejected.stderr
    assert sha(target) == expected
    report = dict(passed=True, native_patch_sha256=expected,
                  exact_patched_image=True, already_patched_input_rejected_unchanged=True)
    (O/'patch-helper-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
finally:
    if target.exists():
        target.unlink()
    target_dir.rmdir()
