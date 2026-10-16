# Per-thread summary of tools/switch_gdbprof.sh output: running samples per thread, its usual stack, top frames.
import re, sys, collections
txt = open(sys.argv[1], errors='replace').read()
samples = txt.split('===SAMPLE ')[1:]
fn = re.compile(r'^#(\d+)\s+(?:0x[0-9a-f]+ in )?(.+?) \(')
per = collections.defaultdict(collections.Counter); busy = collections.Counter(); tot = collections.Counter(); names = {}
for s in samples:
    for b in re.split(r'\nThread ', s)[1:]:
        m = re.match(r'(\d+) \(Thread (\d+) "[^"]*" \(([^)]*)\)', b)
        if not m: continue
        tid, state = m.group(2), m.group(3)
        frames = [fm.group(2).strip() for fm in (fn.match(l.strip()) for l in b.split('\n')) if fm]
        tot[tid] += 1
        if 'Waiting' in state: continue
        busy[tid] += 1
        per[tid][(frames[0] if frames else '?')[:70]] += 1
        names.setdefault(tid, collections.Counter()).update(f[:50] for f in frames[1:6])
for tid, c in busy.most_common(12):
    print(f'thread {tid}: running {c}/{tot[tid]}  stack: {", ".join(k for k,_ in names[tid].most_common(4))}')
    for k, v in per[tid].most_common(6): print(f'     {v:4d} {k}')
