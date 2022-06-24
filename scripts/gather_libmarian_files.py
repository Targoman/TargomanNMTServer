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
    target_path = os.path.join(nmt_server_path, 'libmarian/include/marian', target_relative_path)
    os.makedirs(target_path, exist_ok=True)
    shutil.copy2(header_file, target_path)

print('Copying the libraries ...')
target_path = os.path.join(nmt_server_path, 'libmarian/lib')
os.makedirs(target_path, exist_ok=True)
for static_lib_file in glob(f'{marian_path}/build/**/*.a', recursive=True):
    shutil.copy2(static_lib_file, target_path)

print('Copying additional files ...')
additional_files = [
    (f'src/3rd_party/spdlog/details/format.cc', 'include/marian/3rd_party/spdlog/details/'),
    (f'src/3rd_party/half_float/umHalf.inl', 'include/marian/3rd_party/half_float/')
]
for source, destination in additional_files:
    shutil.copy2(f'{marian_path}/{source}', f'{nmt_server_path}/libmarian/{destination}')