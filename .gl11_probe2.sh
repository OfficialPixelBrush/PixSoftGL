#!/bin/bash
OUT=/home/torben/PixSoftGL/.gl11_probe2.txt
JAR=/home/torben/.mcb173/libraries/org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar
{
  echo "=== find javap ==="
  ls /usr/lib/jvm 2>/dev/null || true
  find /usr/lib/jvm /home/torben -name javap 2>/dev/null | head -20
  type -a java 2>/dev/null || true
  echo
  echo "=== nearby strings ==="
  unzip -p "$JAR" org/lwjgl/opengl/ContextCapabilities.class | strings | grep -n -E 'initAllStubs|OpenGL11|GL11_init|not supported|entry point|reported as|supported' | head -80
  echo
  mkdir -p /tmp/lwjgl_cc
  unzip -o -q "$JAR" org/lwjgl/opengl/ContextCapabilities.class -d /tmp/lwjgl_cc
  ls -la /tmp/lwjgl_cc/org/lwjgl/opengl/ContextCapabilities.class
  python3 - <<'PY'
import subprocess
path='/tmp/lwjgl_cc/org/lwjgl/opengl/ContextCapabilities.class'
data=open(path,'rb').read()
for s in [b'GL11 not supported', b'GL11_initNativeFunctionAddresses', b'OpenGL11', b'initAllStubs', b'entry point is missing']:
    print(s, 'at', data.find(s))
out=subprocess.check_output(['strings','-n','4',path]).decode()
lines=out.splitlines()
for i,l in enumerate(lines):
    if 'GL11 not supported' in l or 'GL11_init' in l or l=='OpenGL11' or 'initAllStubs' in l:
        print('--- context', i)
        print('\n'.join(lines[max(0,i-20):i+20]))
        print()
PY
} > "$OUT" 2>&1
wc -l "$OUT"