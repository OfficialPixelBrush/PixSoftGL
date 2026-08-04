#!/bin/bash
JAR=/home/torben/.mcb173/libraries/org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar
JAVAP=/home/torben/PixSoftGL/x64_jdk1.8.0_202/bin/javap
OUT=/home/torben/PixSoftGL/.gl11_javap.txt
mkdir -p /tmp/lwjgl_cc
unzip -o -q "$JAR" org/lwjgl/opengl/ContextCapabilities.class -d /tmp/lwjgl_cc
# Full disassembly is huge; extract methods of interest
"$JAVAP" -c -p -classpath /tmp/lwjgl_cc org.lwjgl.opengl.ContextCapabilities > /tmp/cc_full.txt 2>&1
wc -l /tmp/cc_full.txt
# Find GL11 not supported and surrounding bytecode
grep -n "GL11 not supported\|GL11_initNative\|OpenGL11\|initAllStubs\|entry point" /tmp/cc_full.txt | head -60
echo "==== extract around GL11 not supported ===="
# get line numbers
ln=$(grep -n 'GL11 not supported' /tmp/cc_full.txt | head -1 | cut -d: -f1)
if [ -n "$ln" ]; then
  start=$((ln-80)); [ $start -lt 1 ] && start=1
  end=$((ln+40))
  sed -n "${start},${end}p" /tmp/cc_full.txt
fi
echo "==== initAllStubs method header region ===="
# Find method initAllStubs
awk '
  /initAllStubs/ {print NR":"$0}
' /tmp/cc_full.txt | head -20
# Extract the private static initAllStubs method - from its declaration to next method
python3 - <<'PY'
import re
text=open('/tmp/cc_full.txt').read().splitlines()
# find "static java.util.Set initAllStubs" or similar
starts=[]
for i,l in enumerate(text):
    if 'initAllStubs' in l and ('Set' in l or 'Method' in l or l.strip().startswith('static') or 'private' in l or 'final' in l):
        starts.append(i)
        print('START CAND', i, l)
# Also look for method signature lines containing initAllStubs(
for i,l in enumerate(text):
    if re.search(r'\binitAllStubs\s*\(', l):
        print('SIG', i, l)
# Dump from first occurrence of method definition pattern
for i,l in enumerate(text):
    if 'initAllStubs(boolean' in l.replace(' ','') or 'initAllStubs(boolean' in l:
        print('FOUND', i, l)
# search more loosely
for i,l in enumerate(text):
    if 'initAllStubs' in l:
        print(i, l)
        if i>0: print(' prev', text[i-1])
PY
# Save a focused excerpt around first "GL11 not supported" ldc and GL11_init
python3 - <<'PY'
text=open('/tmp/cc_full.txt').read().splitlines()
# Find all "GL11 not supported"
idxs=[i for i,l in enumerate(text) if 'GL11 not supported' in l]
print('GL11 not supported lines', idxs)
for i in idxs:
    open('/home/torben/PixSoftGL/.gl11_javap.txt','w').write('\n'.join(text[max(0,i-120):i+60])+'\n')
    break
# Also find GL11_initNativeFunctionAddresses invocations in initAllStubs area
# Print method that contains the throw
# Walk backwards from first idx to find method header (line with '(' and not starting with spaces number:)
i=idxs[0]
while i>0:
    if re.match(r'^\s*(public|private|protected|static|final|synchronized|native|abstract).*', text[i]) and 'Code:' not in text[i] and not text[i].strip().startswith('//') and not re.match(r'^\s+\d+:', text[i]):
        # might be descriptor
        pass
    if re.match(r'^(public|private|protected|static).*initAllStubs', text[i].strip()) or (text[i].strip().endswith(');') is False and 'initAllStubs' in text[i] and '(' in text[i] and 'Code:' not in text[i]):
        print('method at', i, text[i])
        break
    i-=1
import re
i=idxs[0]
while i>=0:
    line=text[i]
    if line.startswith('  ')==False and line.strip() and not line.startswith('Compiled') and not line.startswith('Classfile') and '(' in line and ')' in line and 'throws' not in line.lower() or (line.strip().startswith('static') or line.strip().startswith('private') or line.strip().startswith('public')):
        if not re.match(r'^\s+\d+:', line) and 'Code:' not in line and 'LineNumber' not in line and 'LocalVariable' not in line and 'Exception table' not in line and 'StackMap' not in line and 'Descriptor:' not in line and 'Signature:' not in line and 'flags:' not in line and 'Constant pool' not in line:
            if any(k in line for k in ['initAllStubs','ContextCapabilities','GL11_','boolean','Set']):
                print('maybe', i, line)
    if re.search(r'^\s*(private|public|protected|static).*\binitAllStubs\b', line):
        print('METHOD DEF', i, line)
        open('/home/torben/PixSoftGL/.gl11_method_start.txt','w').write('\n'.join(text[i:idxs[0]+80]))
        break
    i-=1
else:
    print('no method def found walking back')
# Simpler: grep -n for method signatures
for i,l in enumerate(text):
    if re.match(r'^  (private|public|protected|static)', l) and 'initAllStubs' in l:
        print('DEF2', i, l)
PY