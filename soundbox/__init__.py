from soundbox import soundbox as _core


class AudioData:
    def __init__(self, data: bytes, frequency: int, channels: int):
        self.data = data
        self.frequency = frequency
        self.channels = channels

    def qoa(self, filename=None) -> bytes:
        data = _core.qoa_encode(self.data, self.frequency, self.channels)
        if filename is not None:
            with open(filename, 'wb') as f:
                f.write(data)
        return data


def load(data: bytes | str) -> AudioData:
    if isinstance(data, str):
        with open(data, 'rb') as f:
            data = f.read()
    decoder = {
        b'qoaf': _core.qoa_decode,
        b'OggS': _core.ogg_decode,
    }[data[:4]]
    audio_data, frequency, channels = decoder(data)
    return AudioData(audio_data, frequency, channels)


def init():
    import os
    dll_path = os.path.abspath(os.path.normpath(os.path.join(os.path.dirname(__file__), 'soft_oal.dll')))
    _core.init(dll_path)
