import array
from pathlib import Path
import re
import unittest
import wave

ROOT = Path(__file__).resolve().parents[1]


class AudioAssetTests(unittest.TestCase):
    def assert_embedded_clip(self, wav_name, symbol):
        with wave.open(str(ROOT / 'firmware/audio' / wav_name)) as wav:
            self.assertEqual((wav.getnchannels(), wav.getsampwidth(), wav.getframerate()), (1, 2, 16000))
            self.assertGreater(wav.getnframes(), 0)
            self.assertLess(wav.getnframes() / wav.getframerate(), 3)
            pcm = wav.readframes(wav.getnframes())
        text = (ROOT / 'firmware/ui_sounds.h').read_text()
        body = re.search(rf'{symbol}\[\] = \{{(.*?)\}};', text, re.S).group(1)
        self.assertEqual(bytes(map(int, re.findall(r'\d+', body))), pcm)
        samples = array.array('h', pcm)
        self.assertLessEqual(max(abs(v) for v in samples), round(0.7 * 32767))
        self.assertGreater(max(abs(v) for v in samples), 20000)
        self.assertEqual(samples[0], 0)
        self.assertEqual(samples[-1], 0)

    def test_embedded_menu_sound_matches_supported_pcm(self):
        self.assert_embedded_clip('menu_selection.wav', 'MENU_SELECTION_SOUND')

    def test_embedded_action_sound_matches_supported_pcm(self):
        self.assert_embedded_clip('control_action.wav', 'CONTROL_ACTION_SOUND')
