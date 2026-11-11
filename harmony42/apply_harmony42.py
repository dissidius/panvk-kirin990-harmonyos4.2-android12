#!/usr/bin/env python3
"""Guarded diagnostic patch for PanVK JM kbase submission on HarmonyOS 4.2.
This is an EXPERIMENTAL build, not a verified fix.
"""
from pathlib import Path
import re
import sys
p = Path(sys.argv[1])
s = p.read_text()
start = s.find('static bool\nkbase_try_atom_stride(int fd, unsigned stride, bool seq_nr)')
if start < 0:
    sys.exit("Expected kbase_try_atom_stride probe missing; refusing to patch.")
brace = s.index("{", start)
depth = 0
end = None
for i in range(brace, len(s)):
    if s[i] == "{": depth += 1
    if s[i] == "}":
        depth -= 1
        if depth == 0:
            end = i + 1
            break
if end is None:
    sys.exit("Probe braces unbalanced")
old = s[start:end]
for token in ['KBASE_IOCTL_JOB_SUBMIT', 'return true;', 'struct base_jd_atom']:
    if token not in old:
        sys.exit(f"Unrecognized upstream probe (missing {token})")
# Keep original probe behavior for this first diagnostic build. Do NOT
# change atom layout until rejection parameters reveal the actual mismatch.
# Kbase uses 56/64 layouts depending on kernel, and naive probes are unsafe.

start = s.find('kbase_kmod_job_submit_dep(')
if start < 0:
    sys.exit("Missing dependency submit function")
end = s.find('\n}\n', start)
if end < 0:
    sys.exit("Unterminated dependency submit function")
section = s[start:end]
needle = 'mesa_loge("kbase: JOB_SUBMIT(dep) failed: %s", strerror(errno));'
if section.count(needle) != 1:
    sys.exit("Unexpected dependency submit logging; refusing to patch")
replacement = """/* HarmonyOS diagnostics; preserve the real errno before logging. */
      static unsigned h42_rejects;
      int h42_errno = errno;
      unsigned h42_n = __atomic_add_fetch(&h42_rejects, 1, __ATOMIC_RELAXED);
      if (h42_n <= 32 || (h42_n % 4096) == 0)
         mesa_loge("kbase: H42 JOB_SUBMIT(dep) errno=%d count=%u stride=%u "
                   "core_req=0x%x jc=0x%llx atom=%u "
                   "dep0=%u/type%u dep1=%u/type%u",
                   h42_errno, h42_n, kd->atom_stride,
                   (unsigned)core_req, (unsigned long long)jc,
                   (unsigned)atom.atom_number,
                   (unsigned)atom.pre_dep[0].atom_id,
                   (unsigned)atom.pre_dep[0].dependency_type,
                   (unsigned)atom.pre_dep[1].atom_id,
                   (unsigned)atom.pre_dep[1].dependency_type);
      errno = h42_errno;"""
section = section.replace(needle,replacement)
s=s[:start]+section+s[end:]
p.write_text(s)
print("H42 diagnostic patch applied to:",p)
