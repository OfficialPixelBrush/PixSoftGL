#!/bin/bash
set -e
JAR=/home/torben/.mcb173/libraries/org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar
OUT=/home/torben/PixSoftGL/.gl11_probe.txt
{
  echo "=== strings not supported ==="
  unzip -p "$JAR" org/lwjgl/opengl/ContextCapabilities.class | strings | grep -n -A5 -B5 "not supported" || true
  echo
  echo "=== strings GL11 ==="
  unzip -p "$JAR" org/lwjgl/opengl/ContextCapabilities.class | strings | grep -n "GL11" || true
  echo
  echo "=== javap ==="
  which javap || true
  javap -version 2>&1 || true
} > "$OUT" 2>&1
wc -l "$OUT"