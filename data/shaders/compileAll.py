#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os, subprocess
from pathlib import Path

glslExtensions = ['.vert', '.tesc', '.tese', '.geom', '.frag', '.comp', '.rmiss', '.rchit', '.rgen', '.rahit']
command = ['glslangValidator', '--quiet', '-V', '--target-env', 'vulkan1.3', '--target-env', 'spirv1.6', '', '-o', '']
# command = ['glslangValidator', '-g', '-V', '--target-env', 'vulkan1.3', '--target-env', 'spirv1.6', '', '-o', '']

def find_glsl_dep(src, depSet):
    f = open(src, 'r')
    while True:
        l = f.readline()
        if not l:
            break
        if (l.startswith('#include "')):
            dep = Path(l.split('"')[1])
            if dep not in depSet:
                depSet.add(dep)
                find_glsl_dep(dep, depSet)
    f.close()
    
def need_compilation(spvFile):
    if not spvFile.exists():
        return True, []
        
    depSet = { srcFile }
    find_glsl_dep(srcFile, depSet)
    updated = False
    updatedList = []
    for d in depSet:
        if spvFile.stat().st_mtime <= d.stat().st_mtime:
            updated = True
            updatedList.append(d)
    return updated, updatedList

for f in os.listdir('.'):
    srcFile = Path(f)
    if srcFile.suffix in glslExtensions:
        command[-3] = f
        command[-1] = ''.join([f, '.spv'])

        # skip already compiled files
        spvFile = Path(command[-1])

        doCompile, depList = need_compilation(Path(command[-1]))
        if doCompile:
            print(' Compiling:\t', spvFile.name)
            if depList:
                print('   updated:\t', [ src.name for src in depList ])
            subprocess.run(command)
        else:
            print(' Ready:\t\t', spvFile.name)
            
print('\nSPVs compilation finished.')
