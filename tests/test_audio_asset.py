import array
from pathlib import Path
import re
import unittest
import wave

ROOT = Path(__file__).resolve().parents[1]


class AudioAssetTests(unittest.TestCase):
    def test_embedded_hail_matches_supported_pcm(self):
        with wave.open(str(ROOT / 'firmware/audio/hail.wav')) as wav:
            self.assertEqual((wav.getnchannels(), wav.getsampwidth(), wav.getframerate()), (1, 2, 16000))
            self.assertGreater(wav.getnframes(), 0)
            self.assertLess(wav.getnframes() / wav.getframerate(), 3)
            pcm = wav.readframes(wav.getnframes())
        text = (ROOT / 'firmware/hail_sound.h').read_text()
        body = re.search(r'HAIL_SOUND\[\] = \{(.*?)\};', text, re.S).group(1)
        self.assertEqual(bytes(map(int, re.findall(r'\d+', body))), pcm)
        samples = array.array('h', pcm)
        self.assertLessEqual(max(abs(v) for v in samples), round(0.7 * 32767))
        self.assertGreater(max(abs(v) for v in samples), 20000)
        self.assertEqual(samples[0], 0)
        self.assertEqual(samples[-1], 0)
