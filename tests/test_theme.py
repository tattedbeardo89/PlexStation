#!/usr/bin/env python3
"""Guard the shared theme boundary; controller tests exercise actual draw lists."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
for filename in ['plex_ui.cc','plex_audio_ui.h','plex_video_ui.h']:
    text=(root/'src'/filename).read_text()
    assert not re.search(r'IM_COL32\s*\(',text),filename+' bypasses shared colors'
    assert 'Theme::' in text
assert (root/'assets/plexstation.svg').is_file()
print('Native UI theme boundary passed: all linked Plex screens use shared color tokens')
