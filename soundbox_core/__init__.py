import os


def load(data, core):
    if isinstance(data, tuple):
        return data

    if isinstance(data, str):
        with open(data, 'rb') as f:
            data = f.read()

    decoder = {
        b'qoaf': core.qoa_decode,
        b'OggS': core.ogg_decode,
    }[data[:4]]

    return decoder(data)


def find_openal():
    return os.path.abspath(os.path.normpath(os.path.join(os.path.dirname(__file__), 'soft_oal.dll')))
