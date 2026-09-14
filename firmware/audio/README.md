# Controller sounds

## Current feedback sounds (version 1.6.0)

Menu navigation uses [`commship.wav`](https://stdimension.org/MediaLib/effects/technology/federation/commship.wav)
from STDimension. This applies to LIGHTS, SYSTEM, TRANSIT, BACK, each room's
MENU button and the scene-page arrows. The original is retained as
`commship_source.wav`; `menu_selection.wav` is the embedded 16 kHz version.

All other enabled buttons use [`communicator1.wav`](https://stdimension.org/MediaLib/effects/technology/federation/communicator1.wav).
The original is retained as `communicator1_source.wav`; `control_action.wav`
is the embedded 16 kHz version. The SYSTEM page's CONTROL SOUND button tests
this action clip.

Both processed clips are mono signed 16-bit PCM at 16 kHz with 4 ms boundary
fades and peaks below 70%. `../ui_sounds.h` embeds them in flash. Playback is
nonblocking: rapid taps still execute their actions, but do not queue or overlap
sounds. Disabled buttons, incoming state updates and the brightness slider stay
quiet. The saved volume setting applies to both clips, including mute at zero.

The sound effects are used for the requested personal controller; no ownership
or general redistribution licence is claimed.

## Previous: TNG Hail Beep (version 1.4.3)

Source: TrekCore's [Hail Beep 1](https://www.trekcore.com/audio/), downloaded
2026-09-13 from https://www.trekcore.com/audio/computer/hailbeep_clean.mp3.
The original sound's timing is preserved. `hail.wav` is 0.312 seconds of mono
16 kHz signed 16-bit PCM, with 4 ms boundary fades and a 70% peak.
`../hail_sound.h` embeds its 9,984 PCM bytes. It remains in the repository but
is not compiled into the current firmware.

## Previous: Door swish

Source: TrekCore's [TNG Swoosh](https://www.trekcore.com/audio/), downloaded
2026-09-13 from https://www.trekcore.com/audio/doors/tng_swoosh_clean.mp3.
This is a Star Trek sound effect used for the requested personal controller;
no ownership or general redistribution licence is claimed.

`door_swish.wav` is mono 16 kHz signed 16-bit PCM, with short fades and a
normalised peak of 70%. The first trial was too quiet; version 1.4.1 adjusts
the clip level and adds persistent 0–100% volume controls in 10% steps.
`../door_swish.h` embeds its 31,488 PCM bytes in flash. Playback
uses 60% speaker volume initially, nonblocking chunks and a five-second failure
deadline. No network request, SD card, MP3 decoder or media server is required
at playback time.

Pins match Freenove's Sketch_16.1_LVGL_Music/music.h: BCLK 0, LRCLK 18, SDATA 17.
Its lvgl_display.h sets the GT911 interrupt to -1. Our touch configuration
likewise polls without an interrupt pin, leaving GPIO18 for audio.
