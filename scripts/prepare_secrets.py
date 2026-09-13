"""Prepare device-only ESPHome secrets; never include the Home Assistant token."""
import base64
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read_env(text):
    values = {}
    for number, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("export "):
            line = line[7:]
        name, separator, value = line.partition("=")
        name, value = name.strip(), value.strip()
        if not separator or not name or name in values:
            raise ValueError(f"Invalid or duplicate setting on line {number}")
        if value.startswith(('"', "'")):
            if len(value) < 2 or value[-1] != value[0]:
                raise ValueError(f"Unclosed quoted setting on line {number}")
            value = value[1:-1]
        values[name] = value
    return values


def device_secrets(env, existing=None):
    for key in ("WIFI_SSID", "WIFI_PASSWORD", "TRANSIT_API_KEY"):
        if not env.get(key):
            raise ValueError(f"Missing {key}")
    existing = existing or {}
    if any(c in env["TRANSIT_API_KEY"] for c in "\r\n"):
        raise ValueError("Invalid transit API key")
    return {
        "wifi_ssid": env["WIFI_SSID"],
        "wifi_password": env["WIFI_PASSWORD"],
        "transit_api_key": env["TRANSIT_API_KEY"],
        "transit_api_key_literal": json.dumps(env["TRANSIT_API_KEY"]),
        "api_encryption_key": existing.get("api_encryption_key")
        or base64.b64encode(os.urandom(32)).decode(),
        "ota_password": existing.get("ota_password") or os.urandom(32).hex(),
    }


def main():
    env_path = ROOT / ".env"
    env_path.chmod(0o600)
    target = ROOT / "firmware/secrets.yaml"
    existing = json.loads(target.read_text()) if target.exists() else None
    values = device_secrets(read_env(env_path.read_text()), existing)
    target.parent.mkdir(exist_ok=True)
    fd = os.open(target, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    os.fchmod(fd, 0o600)
    with os.fdopen(fd, "w") as file:
        json.dump(values, file, indent=2)
        file.write("\n")
    print("Device secrets prepared. Home Assistant token excluded.")


if __name__ == "__main__":
    main()
