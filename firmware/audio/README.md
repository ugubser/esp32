# Controller sounds

## Current: TNG Hail Beep (version 1.4.3)

Source: TrekCore's [Hail Beep 1](https://www.trekcore.com/audio/), downloaded
2026-09-13 from https://www.trekcore.com/audio/computer/hailbeep_clean.mp3.
The original sound's timing is preserved. `hail.wav` is 0.312 seconds of mono
16 kHz signed 16-bit PCM, with 4 ms boundary fades and a 70% peak.
`../hail_sound.h` embeds its 9,984 PCM bytes. SYSTEM's HAIL button replaces
DOOR SWISH; saved volume, touch, and playback handling remain unchanged.
Version 1.4.3 also plays this hail on every enabled touchscreen button click.
Rapid taps keep executing their actions while an existing clip finishes;
they do not queue additional clips. Disabled buttons, incoming state updates
and the brightness slider stay quiet. Saved volume applies to all feedback,
including mute at zero. SYSTEM's HAIL button plays the same clip once.
The previous door assets below remain available locally but are not compiled
into the current firmware.

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
