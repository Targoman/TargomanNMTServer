import os
import sys
import re
import shutil

from glob import glob

marian_path = os.path.abspath(sys.argv[1])
nmt_server_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

print('Copying header files ...')
for header_file in glob(f'{marian_path}/src/**/*.h*', recursive=True):
    target_relative_path = os.path.dirname(header_file[len(marian_path) + 5:])
    target_path = os.path.join(nmt_server_path, 'libmarian/include', target_relative_path)
    os.makedirs(target_path, exist_ok=True)
    shutil.copy2(header_file, target_path)

print('Copying the libraries ...')
target_path = os.path.join(nmt_server_path, 'libmarian/lib')
os.makedirs(target_path, exist_ok=True)
for static_lib_file in glob(f'{marian_path}/build/**/*.a', recursive=True):
    shutil.copy2(static_lib_file, target_path)
