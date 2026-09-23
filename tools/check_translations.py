#!/usr/bin/env python3
"""Every qsTrId/qtTrId used in the UI must be translated in every ui/src/app/i18n/locale_*.ts,
with the same %1-style placeholders as English. Run: python3 tools/check_translations.py"""
import pathlib, re, sys
import xml.etree.ElementTree as ET

app = pathlib.Path(__file__).resolve().parent.parent / "ui/src/app"
used = {m for f in [*app.glob("qml/**/*.qml"), *app.glob("cpp/*.cpp")]
        for m in re.findall(r'(?:qsTrId|qtTrId)\("([^"]+)"', f.read_text(encoding="utf-8"))}
placeholders = lambda s: sorted(re.findall(r"%\d", s or ""))

tables = {ts.stem: {m.get("id"): m.findtext("translation") for m in ET.parse(ts).iter("message")}
          for ts in sorted(app.glob("i18n/locale_*.ts"))}
errors = [f"{lang}: {i} {problem}"
          for lang, t in tables.items() for i in sorted(used)
          for problem in (["missing"] if not t.get(i) else
                          ["placeholders differ from English"]
                          if placeholders(t[i]) != placeholders(tables["locale_en"].get(i)) else [])]
print("\n".join(errors) or f"OK: {len(used)} ids in {', '.join(tables)}")
sys.exit(bool(errors))
