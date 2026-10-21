#!/usr/bin/env python3
"""Surface native build failures in Actions annotations as well as log artifacts."""
import json
import pathlib

root = pathlib.Path('artifacts')
report = json.loads((root/'validation.json').read_text())
if not report.get('success'):
    excerpt = []
    for name in ('native-build.log', 'container-run.log', 'container-build.log'):
        path = root/'logs'/name
        if path.is_file():
            excerpt = path.read_text(errors='replace').splitlines()[-60:]
            if excerpt:
                break
    detail = 'Stage: '+report.get('last_stage', 'container preparation')+'\n'+'\n'.join(excerpt)
    detail = detail[-12000:]
    escaped = detail.replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A')
    print('::error title=PlexStation native build failure::'+escaped)
