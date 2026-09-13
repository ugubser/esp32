import base64
import json
import unittest
from scripts.prepare_secrets import read_env, device_secrets


class SecretsTests(unittest.TestCase):
    def test_transit_token_is_safely_encoded_for_firmware(self):
        token = 'test-"quoted"-\\token'
        values = device_secrets({"WIFI_SSID":"one", "WIFI_PASSWORD":"pass", "TRANSIT_API_KEY":token})
        self.assertEqual(json.loads(values["transit_api_key_literal"]), token)
        with self.assertRaises(ValueError):
            device_secrets({"WIFI_SSID":"one", "WIFI_PASSWORD":"pass", "TRANSIT_API_KEY":"bad\r\nheader"})

    def test_password_characters_are_literal(self):
        env = read_env('WIFI_SSID="My WiFi"\nWIFI_PASSWORD=abc#def$()\\x\nHA_TOKEN=private')
        env["TRANSIT_API_KEY"] = "test-transit-token"
        self.assertEqual(env["WIFI_PASSWORD"], 'abc#def$()\\x')
        result = device_secrets(env)
        self.assertNotIn("HA_TOKEN", result)
        self.assertNotIn("private", result.values())
        self.assertEqual(len(base64.b64decode(result["api_encryption_key"])), 32)

    def test_keys_survive_wifi_change(self):
        first = device_secrets({"WIFI_SSID": "one", "WIFI_PASSWORD": "pass", "TRANSIT_API_KEY": "test-key"})
        second = device_secrets({"WIFI_SSID": "two", "WIFI_PASSWORD": "new", "TRANSIT_API_KEY": "new-key"}, first)
        self.assertEqual(first["api_encryption_key"], second["api_encryption_key"])
        self.assertEqual(first["ota_password"], second["ota_password"])
        self.assertEqual(second["wifi_ssid"], "two")
        self.assertEqual(second["transit_api_key"], "new-key")

    def test_invalid_settings_fail(self):
        for text in ['A=1\nA=2', 'WIFI_SSID', 'WIFI_PASSWORD="unfinished']:
            with self.assertRaises(ValueError):
                read_env(text)
        for env in [{}, {"WIFI_SSID": "name"}, {"WIFI_SSID": "name", "WIFI_PASSWORD": ""}]:
            with self.assertRaises(ValueError):
                device_secrets(env)


if __name__ == "__main__":
    unittest.main()
