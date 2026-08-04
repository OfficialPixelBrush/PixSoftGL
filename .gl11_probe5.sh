#!/bin/bash
python3 - <<'PY'
import re
chunk=open('/home/torben/PixSoftGL/.gl11_init_method.txt').read().splitlines()
# Parse pattern: optional "iload_1 / ifne skip" then getFunctionAddress String
always=[]; optional=[]
i=0
while i < len(chunk):
    l=chunk[i]
    m=re.search(r'// String (gl\w+)', l)
    if m:
        name=m.group(1)
        # look back a few lines for iload_1 before this block
        window='\n'.join(chunk[max(0,i-8):i+1])
        # Pattern for optional: iload_1; ifne <label>; aload_0; ldc name
        # Simpler: if within 6 lines before ldc there is iload_1 then ifne, it's optional
        prev=chunk[max(0,i-6):i]
        is_opt=any('iload_1'==p.strip().split(':',1)[-1].strip() or p.strip().endswith('iload_1') for p in prev)
        # more precise: last non-empty instruction before aload_0 chain
        back='\n'.join(prev)
        if re.search(r'iload_1\n\s+\d+:\s+ifne', back) or re.search(r'iload_1\s*$', '\n'.join(prev[-3:])):
            # check: immediately before aload for this func
            j=i-1
            while j>=0 and '// String' not in chunk[j]:
                if 'iload_1' in chunk[j]:
                    is_opt=True
                    break
                if 'iand' in chunk[j] and j < i-2:
                    break
                j-=1
            else:
                is_opt=False
                j=i-1
                seen_iload=False
                while j>= max(0,i-10):
                    if 'iload_1' in chunk[j]:
                        seen_iload=True
                        break
                    if re.search(r'// String gl', chunk[j]):
                        break
                    j-=1
                is_opt=seen_iload
        (optional if is_opt else always).append(name)
    i+=1
print('ALWAYS', len(always))
print('\n'.join(always))
print('OPTIONAL_FC', len(optional))
print('\n'.join(optional))
print('has glGetString always?', 'glGetString' in always, 'optional?', 'glGetString' in optional)
PY