# Per-thread CPU use of a running process over a window: name (SetThreadDescription), % of one core.
# On the Switch every game thread gets at most one core at ~0.15 of a PC core (docs/switch.md "Switch-speed test
# on PC"), so a thread above ~15 % of a PC core at the PC frame rate cannot keep that frame rate on the console.
# usage: python -I tools/thread_cpu.py <pid> [seconds=10]
import ctypes, ctypes.wintypes as w, sys, time

k32 = ctypes.WinDLL('kernel32', use_last_error=True)
TH32CS_SNAPTHREAD = 4
THREAD_QUERY_LIMITED_INFORMATION = 0x0800


class THREADENTRY32(ctypes.Structure):
    _fields_ = [('dwSize', w.DWORD), ('cntUsage', w.DWORD), ('th32ThreadID', w.DWORD),
                ('th32OwnerProcessID', w.DWORD), ('tpBasePri', w.LONG), ('tpDeltaPri', w.LONG),
                ('dwFlags', w.DWORD)]


def threads(pid):
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    te = THREADENTRY32()
    te.dwSize = ctypes.sizeof(te)
    ok = k32.Thread32First(snap, ctypes.byref(te))
    out = []
    while ok:
        if te.th32OwnerProcessID == pid:
            out.append(te.th32ThreadID)
        ok = k32.Thread32Next(snap, ctypes.byref(te))
    k32.CloseHandle(snap)
    return out


def sample(pid):
    res = {}
    for tid in threads(pid):
        h = k32.OpenThread(THREAD_QUERY_LIMITED_INFORMATION, False, tid)
        if not h:
            continue
        c, e, kt, ut = (w.FILETIME() for _ in range(4))
        name = ctypes.c_wchar_p()
        k32.GetThreadTimes(h, ctypes.byref(c), ctypes.byref(e), ctypes.byref(kt), ctypes.byref(ut))
        k32.GetThreadDescription(h, ctypes.byref(name))
        t = sum((x.dwHighDateTime << 32 | x.dwLowDateTime) for x in (kt, ut)) / 1e7
        res[tid] = (name.value or '', t)
        k32.CloseHandle(h)
    return res


pid = int(sys.argv[1])
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 10
a = sample(pid)
time.sleep(secs)
b = sample(pid)
rows = sorted(((b[t][1] - a[t][1]) / secs * 100, b[t][0], t) for t in b if t in a)
for pct, name, tid in reversed(rows):
    if pct >= 0.5:
        print(f'{pct:6.1f}%  {tid:6d}  {name}')
