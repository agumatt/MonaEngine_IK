"""
This code comes from https://github.com/rubenvillegas/cvpr2018nkn/blob/master/datasets/fbx2bvh.py
"""
import bpy
import numpy as np

import os
import sys
sys.path.append(os.getcwd())
sys.path.append('../')
from os import listdir
import math

data_path = './'

files = sorted([f for f in listdir(data_path) if f.endswith(".bvh")])

for f in files:
    sourcepath = data_path + f
    dumppath = data_path + f.split(".bvh")[0] + ".fbx"

    bpy.ops.import_anim.bvh(filepath=sourcepath, rotate_mode='NATIVE', axis_forward='Y', axis_up='Z')

    bpy.ops.export_scene.fbx(filepath=dumppath, axis_forward='Y', axis_up='Z', add_leaf_bones=False, bake_anim_simplify_factor=0, apply_unit_scale=False)
    
    
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()

    print(data_path + f + " processed.")

bpy.ops.wm.quit_blender()