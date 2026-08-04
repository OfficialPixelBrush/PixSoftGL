#!/bin/bash
JAVAP=/home/torben/PixSoftGL/x64_jdk1.8.0_202/bin/javap
OUT=/home/torben/PixSoftGL/.gl11_init_method.txt
# Extract just that method from full disassembly
python3 - <<'PY'
text=open('/tmp/cc_full.txt').read().splitlines()
start=None
for i,l in enumerate(text):
    if 'GL11_initNativeFunctionAddresses(boolean)' in l and 'Code:' not in l:
        start=i
        break
print('start', start, text[start] if start is not None else None)
if start is None:
    raise SystemExit(1)
# find next method at same indent level starting with "  private" or "  public" after Code of this method
end=len(text)
for j in range(start+1, len(text)):
    if text[j].startswith('  private ') or text[j].startswith('  public ') or text[j].startswith('  static ') or text[j].startswith('  protected '):
        end=j
        break
chunk=text[start:end]
open('/home/torben/PixSoftGL/.gl11_init_method.txt','w').write('\n'.join(chunk)+'\n')
print('lines', len(chunk))
# Extract all function name strings looked up
import re
names=[]
for l in chunk:
    m=re.search(r'// String (gl\w+)', l)
    if m: names.append(m.group(1))
print('FUNCTION COUNT', len(names))
print('FUNCTIONS:')
for n in names:
    print(n)
# Also note boolean forwardCompatible usage
for i,l in enumerate(chunk):
    if 'iload_1' in l or 'forward' in l.lower() or 'ifeq' in l and i<30:
        pass
# Print first 80 lines of method for structure
print('==== HEAD ====')
print('\n'.join(chunk[:100]))
print('==== TAIL ====')
print('\n'.join(chunk[-40:]))
PY
wc -l "$OUT"