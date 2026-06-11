#!/usr/bin/env python3
"""Static GUI layout audit for Borato Morphium.

Cross-references the JUCE control bounds (the svg(x,y,w,h) calls in
Source/PluginEditor.cpp) against the absolute geometry of the panel SVG
(Resources/morphium_panel.svg), then writes tools/layout_report.html — the
panel rendered inline with a canvas overlay showing every control bound,
its matched SVG anchor, and a findings table.

Feedback loop: edit coordinates -> python tools/layout_audit.py -> refresh
the report in the browser. No need to launch the standalone.
"""

from __future__ import annotations

import json
import re
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field, asdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EDITOR_CPP = ROOT / "Source" / "PluginEditor.cpp"
EDITOR_H = ROOT / "Source" / "PluginEditor.h"
SVG_FILE = ROOT / "Resources" / "morphium_panel.svg"
REPORT = ROOT / "tools" / "layout_report.html"

SVG_W, SVG_H = 1280.0, 760.0


# ---------------------------------------------------------------------------
# 1. Parse the JUCE bounds out of PluginEditor.cpp
# ---------------------------------------------------------------------------

def _cpp_expr_to_python(expr: str) -> str:
    expr = re.sub(r"\(\s*(?:float|int|size_t)\s*\)", "", expr)
    expr = re.sub(r"(?<=[\d.])f\b", "", expr)
    return expr.strip()


def _split_args(argstr: str) -> list[str]:
    args, depth, cur = [], 0, ""
    for ch in argstr:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            args.append(cur)
            cur = ""
        else:
            cur += ch
    if cur.strip():
        args.append(cur)
    return args


def _find_svg_call(text: str, start: int) -> tuple[str, int] | None:
    """Return the argument string of the svg( ... ) call beginning at/after start."""
    m = re.compile(r"svg\s*\(").search(text, start)
    if not m:
        return None
    i, depth = m.end(), 1
    while i < len(text) and depth:
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
        i += 1
    return text[m.end():i - 1], i


def parse_juce_bounds() -> list[dict]:
    cpp = EDITOR_CPP.read_text(encoding="utf-8")
    hdr = EDITOR_H.read_text(encoding="utf-8")

    constants: dict[str, int] = {}
    for name, value in re.findall(r"int\s+(\w+)\s*=\s*(\d+)", hdr + cpp):
        constants.setdefault(name, int(value))

    controls: list[dict] = []
    consumed: list[tuple[int, int]] = []

    # --- loop-generated bounds -------------------------------------------
    loop_re = re.compile(
        r"for\s*\(\s*int\s+(\w+)\s*=\s*0\s*;\s*\1\s*<\s*(\w+)\s*;[^)]*\)\s*\{",
        re.S,
    )
    for lm in loop_re.finditer(cpp):
        var, limit_name = lm.group(1), lm.group(2)
        count = constants.get(limit_name, None)
        if count is None:
            continue
        # body = balanced braces from lm.end()-1
        i, depth = lm.end(), 1
        while i < len(cpp) and depth:
            if cpp[i] == "{":
                depth += 1
            elif cpp[i] == "}":
                depth -= 1
            i += 1
        body = cpp[lm.end():i - 1]
        sb = re.search(r"(\w+)\s*\[[^\]]*\]\s*\.setBounds", body)
        call = _find_svg_call(body, 0)
        if not sb or not call:
            continue
        consumed.append((lm.start(), i))
        argstr, _ = call
        local_defs = re.findall(r"const\s+int\s+(\w+)\s*=\s*([^;]+);", body)
        for n in range(count):
            env = {var: n}
            for dname, dexpr in local_defs:
                env[dname] = int(eval(_cpp_expr_to_python(dexpr), {}, env))  # noqa: S307
            vals = [float(eval(_cpp_expr_to_python(a), {}, env)) for a in _split_args(argstr)]  # noqa: S307
            controls.append({"name": f"{sb.group(1)}[{n}]", "x": vals[0], "y": vals[1],
                             "w": vals[2], "h": vals[3]})

    # --- simple direct bounds --------------------------------------------
    for m in re.finditer(r"(\w+)\s*\.setBounds\s*\(\s*svg\s*\(", cpp):
        if any(a <= m.start() < b for a, b in consumed):
            continue
        call = _find_svg_call(cpp, m.start())
        if not call:
            continue
        argstr, _ = call
        try:
            vals = [float(eval(_cpp_expr_to_python(a), {}, {})) for a in _split_args(argstr)]  # noqa: S307
        except Exception:
            print(f"  ! could not evaluate bounds for {m.group(1)}: svg({argstr})")
            continue
        controls.append({"name": m.group(1), "x": vals[0], "y": vals[1],
                         "w": vals[2], "h": vals[3]})

    for c in controls:
        c["cx"] = c["x"] + c["w"] / 2
        c["cy"] = c["y"] + c["h"] / 2
        n = c["name"]
        if n.endswith("Slider") and abs(c["w"] - c["h"]) < 0.5:
            c["kind"] = "rotary"
        elif n.endswith("Slider") and c["h"] > c["w"] * 1.5:
            c["kind"] = "fader"
        elif "Button" in n or "button" in n:
            c["kind"] = "button"
        else:
            c["kind"] = "display"
    return controls


# ---------------------------------------------------------------------------
# 2. Parse the SVG into absolute geometry
# ---------------------------------------------------------------------------

@dataclass
class SvgGeometry:
    circles: list[dict] = field(default_factory=list)
    rects: list[dict] = field(default_factory=list)
    texts: list[dict] = field(default_factory=list)
    classes: dict = field(default_factory=dict)


def _strip_ns(tag: str) -> str:
    return tag.split("}")[-1]


def _translate_of(el) -> tuple[float, float]:
    t = el.get("transform", "")
    m = re.search(r"translate\(\s*(-?[\d.]+)[\s,]+(-?[\d.]+)\s*\)", t)
    if m:
        return float(m.group(1)), float(m.group(2))
    m = re.search(r"translate\(\s*(-?[\d.]+)\s*\)", t)
    if m:
        return float(m.group(1)), 0.0
    return 0.0, 0.0


def parse_svg() -> SvgGeometry:
    geo = SvgGeometry()
    raw = SVG_FILE.read_text(encoding="utf-8")

    style = re.search(r"<style>(.*?)</style>", raw, re.S)
    if style:
        for cls, body in re.findall(r"\.([\w-]+)\s*\{([^}]*)\}", style.group(1)):
            fs = re.search(r"font:[^;]*?(\d+(?:\.\d+)?)px", body)
            ls = re.search(r"letter-spacing:\s*(-?[\d.]+)px", body)
            geo.classes[cls] = {"size": float(fs.group(1)) if fs else 10.0,
                                "ls": float(ls.group(1)) if ls else 0.0}

    root = ET.fromstring(raw)

    def walk(el, ox: float, oy: float):
        tag = _strip_ns(el.tag)
        if tag in ("defs", "style"):
            return
        tx, ty = _translate_of(el)
        ax, ay = ox + tx, oy + ty
        if tag == "circle":
            geo.circles.append({"cx": ax + float(el.get("cx", 0)), "cy": ay + float(el.get("cy", 0)),
                                "r": float(el.get("r", 0))})
        elif tag == "rect":
            geo.rects.append({"x": ax + float(el.get("x", 0)), "y": ay + float(el.get("y", 0)),
                              "w": float(el.get("width", 0)), "h": float(el.get("height", 0)),
                              "fill": el.get("fill", ""), "stroke": el.get("stroke", "")})
        elif tag == "text":
            content = "".join(el.itertext()).strip()
            cls = el.get("class", "")
            metrics = geo.classes.get(cls, {"size": 10.0, "ls": 0.0})
            size = float(el.get("font-size", metrics["size"]))
            ls = metrics["ls"]
            n = max(len(content), 1)
            width = n * size * 0.62 + (n - 1) * ls
            x = ax + float(el.get("x", 0))
            y = ay + float(el.get("y", 0))
            anchor = el.get("text-anchor", "start")
            if anchor == "middle":
                x -= width / 2
            elif anchor == "end":
                x -= width
            geo.texts.append({"x": x, "y": y - size * 0.8, "w": width, "h": size,
                              "text": content, "cls": cls})
        for child in el:
            walk(child, ax, ay)

    walk(root, 0.0, 0.0)
    return geo


# ---------------------------------------------------------------------------
# 3. Cross-checks
# ---------------------------------------------------------------------------

def _rect_overlap(a, b) -> float:
    w = min(a["x"] + a["w"], b["x"] + b["w"]) - max(a["x"], b["x"])
    h = min(a["y"] + a["h"], b["y"] + b["h"]) - max(a["y"], b["y"])
    return max(w, 0.0) * max(h, 0.0)


def run_checks(controls: list[dict], geo: SvgGeometry) -> list[dict]:
    findings: list[dict] = []

    def add(level, control, message, anchor=None):
        findings.append({"level": level, "control": control, "message": message,
                         "anchor": anchor})

    # 3a. anchor matching ---------------------------------------------------
    for c in controls:
        best, best_d = None, 1e9
        if c["kind"] == "rotary":
            for circ in geo.circles:
                if not (0.4 * c["w"] <= 2 * circ["r"] <= 1.5 * c["w"]):
                    continue
                d = ((circ["cx"] - c["cx"]) ** 2 + (circ["cy"] - c["cy"]) ** 2) ** 0.5
                if d < best_d:
                    best, best_d = circ, d
            if best is None or best_d > 60:
                add("info", c["name"], "sem bezel circular correspondente no SVG (controle custom-drawn?)")
            else:
                dx, dy = best["cx"] - c["cx"], best["cy"] - c["cy"]
                dd = 2 * best["r"] - c["w"]
                c["anchor"] = {"type": "circle", **best}
                if abs(dx) > 1 or abs(dy) > 1:
                    add("error", c["name"],
                        f"centro desalinhado do bezel: dx={dx:+.1f}, dy={dy:+.1f} (unid. SVG)",
                        c["anchor"])
                else:
                    note = f" (bezel ⌀{2*best['r']:.0f} vs bounds {c['w']:.0f})" if abs(dd) > 2 else ""
                    add("ok", c["name"], "centrado no bezel" + note, c["anchor"])
        else:
            for r in geo.rects:
                if not (0.6 * c["w"] <= r["w"] <= 1.6 * c["w"] and 0.6 * c["h"] <= r["h"] <= 1.6 * c["h"]):
                    continue
                rcx, rcy = r["x"] + r["w"] / 2, r["y"] + r["h"] / 2
                d = ((rcx - c["cx"]) ** 2 + (rcy - c["cy"]) ** 2) ** 0.5
                if d < best_d:
                    best, best_d = r, d
            if best is None or best_d > 60:
                add("info", c["name"], "sem rect âncora correspondente no SVG")
            else:
                rcx, rcy = best["x"] + best["w"] / 2, best["y"] + best["h"] / 2
                dx, dy = rcx - c["cx"], rcy - c["cy"]
                c["anchor"] = {"type": "rect", **{k: best[k] for k in ("x", "y", "w", "h")}}
                tol = 1.0
                if abs(dx) > tol or abs(dy) > tol:
                    lvl = "error" if c["kind"] != "fader" else "info"
                    add(lvl, c["name"],
                        f"centro deslocado do slot SVG: dx={dx:+.1f}, dy={dy:+.1f} (unid. SVG)",
                        c["anchor"])
                else:
                    add("ok", c["name"], "alinhado ao slot SVG", c["anchor"])

    # 3b. control <-> control overlaps -------------------------------------
    for i, a in enumerate(controls):
        for b in controls[i + 1:]:
            ov = _rect_overlap(a, b)
            if ov > 0:
                add("error", f"{a['name']} × {b['name']}",
                    f"bounds se sobrepõem em {ov:.0f} unid.² SVG")

    # 3c. control <-> label collisions --------------------------------------
    def _label_belongs_to(c, t) -> bool:
        # A label drawn by the SVG inside the control's own anchor (e.g. the
        # resonator button captions) is by design, not a collision.
        tcx, tcy = t["x"] + t["w"] / 2, t["y"] + t["h"] / 2
        a = c.get("anchor")
        if a and a["type"] == "rect":
            return a["x"] <= tcx <= a["x"] + a["w"] and a["y"] <= tcy <= a["y"] + a["h"]
        return False

    for c in controls:
        for t in geo.texts:
            if t["cls"] in ("brand", "brandHi") or _label_belongs_to(c, t):
                continue
            ov = _rect_overlap(c, t)
            if ov > 4:  # estimated text bbox: small overlaps are noise
                add("warn", c["name"],
                    f"pode colidir com o label \"{t['text']}\" (~{ov:.0f} unid.²; bbox estimado)")

    # 3d. row / column alignment -------------------------------------------
    by_kind: dict[str, list[dict]] = {}
    for c in controls:
        by_kind.setdefault(c["kind"], []).append(c)
    for kind, group in by_kind.items():
        for axis, center in (("linha", "cy"), ("coluna", "cx")):
            buckets: list[list[dict]] = []
            for c in sorted(group, key=lambda c: c[center]):
                if buckets and abs(buckets[-1][0][center] - c[center]) <= 6:
                    buckets[-1].append(c)
                else:
                    buckets.append([c])
            for bucket in buckets:
                if len(bucket) < 2:
                    continue
                vals = [c[center] for c in bucket]
                spread = max(vals) - min(vals)
                if 0.5 < spread <= 6:
                    names = ", ".join(c["name"] for c in bucket)
                    add("warn", names, f"quase alinhados na mesma {axis}, mas com desvio de {spread:.1f} unid.")

    # 3e. spacing consistency ------------------------------------------------
    for kind, group in by_kind.items():
        rows: dict[float, list[dict]] = {}
        for c in group:
            key = round(c["cy"] / 6) * 6
            rows.setdefault(key, []).append(c)
        for row in rows.values():
            if len(row) < 3:
                continue
            row.sort(key=lambda c: c["cx"])
            # split into clusters: a gap much larger than the control width
            # means a different panel section, not an irregularity
            clusters: list[list[dict]] = [[row[0]]]
            for a, b in zip(row, row[1:]):
                if b["cx"] - a["cx"] > 2.5 * max(a["w"], b["w"]):
                    clusters.append([b])
                else:
                    clusters[-1].append(b)
            for cluster in clusters:
                if len(cluster) < 3:
                    continue
                gaps = [round(b["cx"] - a["cx"], 1) for a, b in zip(cluster, cluster[1:])]
                if max(gaps) - min(gaps) > 1:
                    names = ", ".join(c["name"] for c in cluster)
                    add("warn", names, f"espaçamento horizontal irregular: gaps {gaps}")

    return findings


# ---------------------------------------------------------------------------
# 4. HTML report (inline SVG + canvas overlay)
# ---------------------------------------------------------------------------

def write_report(controls, geo, findings):
    svg_markup = SVG_FILE.read_text(encoding="utf-8")
    payload = json.dumps({"controls": controls, "findings": findings,
                          "texts": geo.texts, "w": SVG_W, "h": SVG_H},
                         ensure_ascii=False)
    html = f"""<!doctype html>
<html lang="pt-br"><head><meta charset="utf-8">
<title>Morphium — Layout Audit</title>
<style>
  body {{ background:#111; color:#ddd; font:14px/1.5 system-ui, sans-serif; margin:24px; }}
  h1 {{ font-size:18px; }} small {{ color:#888; }}
  #stage {{ position:relative; width:1280px; max-width:100%; }}
  #stage svg {{ width:100%; height:auto; display:block; }}
  #overlay {{ position:absolute; inset:0; width:100%; height:100%; }}
  label {{ margin-right:16px; user-select:none; }}
  table {{ border-collapse:collapse; margin-top:16px; width:100%; max-width:1280px; }}
  td, th {{ border:1px solid #333; padding:4px 10px; text-align:left; vertical-align:top; }}
  tr.ok td:first-child    {{ color:#3ddc84; }}
  tr.info td:first-child  {{ color:#5ab4ff; }}
  tr.warn td:first-child  {{ color:#ffc24b; }}
  tr.error td:first-child {{ color:#ff5d5d; font-weight:bold; }}
  tr:hover {{ background:#1d1d2a; cursor:pointer; }}
</style></head><body>
<h1>Borato Morphium — auditoria de layout
  <small>(bounds JUCE × geometria SVG · regenere com <code>python tools/layout_audit.py</code>)</small></h1>
<p>
  <label><input type="checkbox" id="cbBounds" checked> bounds JUCE</label>
  <label><input type="checkbox" id="cbAnchors" checked> âncoras SVG casadas</label>
  <label><input type="checkbox" id="cbLabels"> bboxes de texto (estimados)</label>
  <label><input type="checkbox" id="cbNames"> nomes</label>
</p>
<div id="stage">
{svg_markup}
<canvas id="overlay" width="1280" height="760"></canvas>
</div>
<table id="tbl"><thead>
<tr><th>nível</th><th>controle</th><th>diagnóstico</th></tr></thead><tbody></tbody></table>
<script>
const DATA = {payload};
const canvas = document.getElementById('overlay');
const ctx = canvas.getContext('2d');
let highlight = null;

function draw() {{
  ctx.clearRect(0, 0, DATA.w, DATA.h);
  if (document.getElementById('cbLabels').checked)
    for (const t of DATA.texts) {{
      ctx.strokeStyle = 'rgba(255,255,255,0.35)'; ctx.lineWidth = 0.6;
      ctx.strokeRect(t.x, t.y, t.w, t.h);
    }}
  for (const c of DATA.controls) {{
    const hot = highlight && highlight.includes(c.name);
    if (document.getElementById('cbAnchors').checked && c.anchor) {{
      ctx.strokeStyle = hot ? '#fff' : 'rgba(61,220,132,0.9)';
      ctx.lineWidth = hot ? 2.5 : 1.2;
      ctx.setLineDash([4, 3]);
      if (c.anchor.type === 'circle') {{
        ctx.beginPath(); ctx.arc(c.anchor.cx, c.anchor.cy, c.anchor.r, 0, 7); ctx.stroke();
      }} else
        ctx.strokeRect(c.anchor.x, c.anchor.y, c.anchor.w, c.anchor.h);
      ctx.setLineDash([]);
    }}
    if (document.getElementById('cbBounds').checked) {{
      const bad = DATA.findings.some(f => f.level === 'error' && f.control.includes(c.name));
      ctx.strokeStyle = hot ? '#fff' : (bad ? 'rgba(255,93,93,0.95)' : 'rgba(0,240,255,0.85)');
      ctx.lineWidth = hot ? 3 : 1.4;
      ctx.strokeRect(c.x, c.y, c.w, c.h);
      ctx.beginPath(); ctx.moveTo(c.cx - 4, c.cy); ctx.lineTo(c.cx + 4, c.cy);
      ctx.moveTo(c.cx, c.cy - 4); ctx.lineTo(c.cx, c.cy + 4); ctx.stroke();
      if (document.getElementById('cbNames').checked || hot) {{
        ctx.fillStyle = '#fff'; ctx.font = '11px monospace';
        ctx.fillText(c.name, c.x + 2, c.y - 3);
      }}
    }}
  }}
}}

const tbody = document.querySelector('#tbl tbody');
const order = {{ error: 0, warn: 1, info: 2, ok: 3 }};
for (const f of [...DATA.findings].sort((a, b) => order[a.level] - order[b.level])) {{
  const tr = document.createElement('tr');
  tr.className = f.level;
  tr.innerHTML = `<td>${{f.level.toUpperCase()}}</td><td><code>${{f.control}}</code></td><td>${{f.message}}</td>`;
  tr.onmouseenter = () => {{ highlight = f.control; draw(); }};
  tr.onmouseleave = () => {{ highlight = null; draw(); }};
  tbody.appendChild(tr);
}}
for (const id of ['cbBounds', 'cbAnchors', 'cbLabels', 'cbNames'])
  document.getElementById(id).onchange = draw;
draw();
</script></body></html>
"""
    REPORT.write_text(html, encoding="utf-8")


# ---------------------------------------------------------------------------

def main() -> int:
    controls = parse_juce_bounds()
    geo = parse_svg()
    findings = run_checks(controls, geo)
    write_report(controls, geo, findings)

    counts = {lvl: sum(1 for f in findings if f["level"] == lvl)
              for lvl in ("error", "warn", "info", "ok")}
    print(f"Controles extraídos do PluginEditor.cpp : {len(controls)}")
    print(f"Geometria SVG: {len(geo.circles)} círculos, {len(geo.rects)} rects, {len(geo.texts)} textos")
    print(f"Achados: {counts['error']} erro(s), {counts['warn']} aviso(s), "
          f"{counts['info']} info(s), {counts['ok']} ok")
    for f in findings:
        if f["level"] in ("error", "warn"):
            print(f"  [{f['level'].upper():5}] {f['control']}: {f['message']}")
    print(f"\nRelatório: {REPORT}")
    return 1 if counts["error"] else 0


if __name__ == "__main__":
    sys.exit(main())
