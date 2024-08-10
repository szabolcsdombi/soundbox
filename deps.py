import io
import os
import zipfile

import requests

if not os.path.exists('soundbox_core/soft_oal.dll'):
    res = requests.get('https://github.com/kcat/openal-soft/releases/download/1.23.1/openal-soft-1.23.1-bin.zip')
    pack = zipfile.ZipFile(io.BytesIO(res.content))
    dll = pack.read('openal-soft-1.23.1-bin/bin/Win64/soft_oal.dll')
    with open('soundbox_core/soft_oal.dll', 'wb') as f:
        f.write(dll)
