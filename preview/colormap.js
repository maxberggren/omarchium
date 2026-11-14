// Omarchium color-map evaluator for the preview.
//
// Mirrors core/browser/theme/omarchy_palette.cc and omarchy_color_map.cc
// exactly (same fallbacks, same blend/contrast math as ui/gfx/color_utils),
// so what the preview shows is what the browser paints. If you change the
// semantics on one side, change the other; core/tools/check_preview_parity.py
// compares both on every installed theme.

// Colors are [a, r, g, b], 0..255, like SkColor.
const rgb = (r, g, b) => [255, r, g, b];
const BLACK = rgb(0, 0, 0), WHITE = rgb(255, 255, 255);
const clampRound = (v) => Math.min(255, Math.max(0, Math.round(v)));

// color_utils::AlphaBlend(foreground, background, float alpha), in 32-bit
// float arithmetic like the C++ (Math.fround after every operation), so
// results match bit for bit.
const f = Math.fround;
export function mix(fg, bg, alphaIn) {
  const alpha = f(alphaIn);
  if (alpha === 0) return bg;
  if (alpha === 1) return fg;
  const inv = f(1 - alpha);
  const fa = f(fg[0] * alpha), ba = f(bg[0] * inv);
  const norm = f(fa + ba);
  if (norm === 0) return [0, 0, 0, 0];
  const fw = f(fa / norm), bw = f(ba / norm);
  const ch = (i) => clampRound(f(f(fg[i] * fw) + f(bg[i] * bw)));
  return [clampRound(norm), ch(1), ch(2), ch(3)];
}

const linearize = (c) => (c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4));
export const luminance = (c) =>
  0.2126 * linearize(c[1] / 255) + 0.7152 * linearize(c[2] / 255) + 0.0722 * linearize(c[3] / 255);
export function contrastRatio(a, b) {
  const la = luminance(a) + 0.05, lb = luminance(b) + 0.05;
  return la > lb ? la / lb : lb / la;
}
const isDark = (c) => luminance(c) < 0.211692036;

export function parseHex(v) {
  if (typeof v !== "string" || !v.startsWith("#")) return null;
  let h = v.slice(1);
  if (h.length === 3) h = [...h].map((x) => x + x).join("");
  else if (h.length === 8) h = h.slice(0, 6);
  if (!/^[0-9a-fA-F]{6}$/.test(h)) return null;
  const n = parseInt(h, 16);
  return rgb((n >> 16) & 255, (n >> 8) & 255, n & 255);
}

export const toCss = (c) =>
  c[0] === 255 ? `rgb(${c[1]} ${c[2]} ${c[3]})` : `rgb(${c[1]} ${c[2]} ${c[3]} / ${(c[0] / 255).toFixed(3)})`;
export const toHex = (c) =>
  "#" + c.slice(1).map((x) => x.toString(16).padStart(2, "0")).join("") +
  (c[0] === 255 ? "" : c[0].toString(16).padStart(2, "0"));

// Same flat-TOML reading as OmarchyPalette::Parse.
function parseFlatToml(text) {
  const out = {};
  for (let line of text.split("\n")) {
    line = line.trim();
    if (!line || line.startsWith("#") || line.startsWith("[")) continue;
    const eq = line.indexOf("=");
    if (eq < 0) continue;
    const key = line.slice(0, eq).trim();
    let value = line.slice(eq + 1).trim();
    if (value[0] === '"' || value[0] === "'") {
      const close = value.indexOf(value[0], 1);
      if (close < 0) continue;
      value = value.slice(1, close);
    } else {
      const hash = value.indexOf("#");
      value = (hash < 0 ? value : value.slice(0, hash)).trim();
    }
    out[key] = value;
  }
  return out;
}

export function parsePalette(toml) {
  const v = parseFlatToml(toml);
  const color = (k) => parseHex(v[k]);
  const bg = color("background"), fg = color("foreground");
  if (!bg || !fg) return null;
  const dark = "mode" in v ? v.mode.toLowerCase() !== "light" : isDark(bg);
  const or = (c, fallback) => c ?? fallback;
  const p = { dark, background: bg, foreground: fg };
  p.accent = or(color("accent"), or(color("blue"), fg));
  p.selection = or(color("selection"), mix(p.accent, bg, 0.3));
  p.muted = or(color("muted"), mix(fg, bg, 0.35));
  p.dark_background = or(color("dark_background"), mix(BLACK, bg, 0.12));
  p.darker_background = or(color("darker_background"), mix(BLACK, bg, 0.25));
  p.lighter_background = or(color("lighter_background"),
                            dark ? mix(WHITE, bg, 0.08) : mix(BLACK, bg, 0.06));
  p.dark_foreground = or(color("dark_foreground"), mix(fg, bg, 0.5));
  p.light_foreground = or(color("light_foreground"), mix(fg, bg, 0.8));
  p.red = or(color("red"), rgb(0xd0, 0x4a, 0x4a));
  p.yellow = or(color("yellow"), rgb(0xd8, 0xa6, 0x57));
  p.green = or(color("green"), rgb(0x6a, 0xa8, 0x4f));
  p.blue = or(color("blue"), p.accent);
  p.cyan = or(color("cyan"), p.blue);
  p.magenta = or(color("magenta"), p.accent);
  p.black = BLACK;
  p.white = WHITE;
  return p;
}

// ---- expressions -----------------------------------------------------------

function tokenize(src) {
  const re = /\s*(#[0-9a-fA-F]+|[A-Za-z_][A-Za-z0-9_]*|\d*\.?\d+|[(),])/y;
  const out = [];
  let m;
  re.lastIndex = 0;
  while (re.lastIndex < src.length) {
    if (/^\s*$/.test(src.slice(re.lastIndex))) break;
    if (!(m = re.exec(src))) throw new Error(`bad expression near "${src.slice(re.lastIndex)}"`);
    out.push(m[1]);
  }
  return out;
}

// Parses to a tree: {kind: "name"|"color"|"num"|"call", ...}
export function parseExpr(src) {
  const t = tokenize(src);
  let i = 0;
  const expr = () => {
    const tok = t[i++];
    if (tok === undefined) throw new Error("unexpected end of expression");
    if (tok.startsWith("#")) {
      const c = parseHex(tok);
      if (!c) throw new Error(`bad color ${tok}`);
      return { kind: "color", c };
    }
    if (/^[\d.]/.test(tok)) return { kind: "num", n: parseFloat(tok) };
    if (t[i] === "(") {
      i++;
      const args = [];
      if (t[i] !== ")") {
        for (;;) {
          args.push(expr());
          if (t[i] === ",") { i++; continue; }
          break;
        }
      }
      if (t[i++] !== ")") throw new Error(`missing ) in ${src}`);
      return { kind: "call", fn: tok, args };
    }
    return { kind: "name", name: tok };
  };
  const tree = expr();
  if (i !== t.length) throw new Error(`trailing input in ${src}`);
  return tree;
}

const ARITY = { mix: 3, alpha: 2, contrast: 1, dark: 2 };

export function evalExpr(node, palette, vars) {
  const color = (n) => {
    const v = evalExpr(n, palette, vars);
    if (!Array.isArray(v)) throw new Error("expected a color");
    return v;
  };
  const num = (n) => {
    if (n.kind !== "num" || n.n < 0 || n.n > 1) throw new Error("expected a number in 0..1");
    return n.n;
  };
  switch (node.kind) {
    case "color": return node.c;
    case "num": return node.n;
    case "name":
      if (vars.has(node.name)) return vars.get(node.name);
      if (node.name in palette && Array.isArray(palette[node.name])) return palette[node.name];
      throw new Error(`unknown name ${node.name}`);
    case "call": {
      if (ARITY[node.fn] !== node.args.length) throw new Error(`${node.fn}() takes ${ARITY[node.fn] ?? "?"} arguments`);
      const a = node.args;
      switch (node.fn) {
        case "mix": return mix(color(a[0]), color(a[1]), num(a[2]));
        case "alpha": { const c = color(a[0]); return [clampRound(f(f(num(a[1])) * 255)), c[1], c[2], c[3]]; }
        case "contrast": {
          const c = color(a[0]);
          return contrastRatio(c, palette.foreground) >= contrastRatio(c, palette.background)
            ? palette.foreground : palette.background;
        }
        case "dark": return palette.dark ? color(a[0]) : color(a[1]);
      }
    }
  }
  throw new Error("bad expression");
}

// Evaluates a whole map against a palette. Vars resolve lazily (on first
// reference), like the C++ side, so their order in the file doesn't matter.
// Returns {vars, sys, chrome, errors}.
export function evaluateMap(map, palette) {
  const errors = [];
  const varSrc = map.vars ?? {};
  const values = {}, resolving = new Set();
  const vars = {
    has: (n) => n in values || n in varSrc,
    get: (n) => {
      if (n in values) return values[n];
      if (resolving.has(n)) throw new Error(`var cycle through ${n}`);
      resolving.add(n);
      try { return (values[n] = evalExpr(parseExpr(varSrc[n]), palette, vars)); }
      finally { resolving.delete(n); }
    },
  };
  for (const n of Object.keys(varSrc)) {
    try { vars.get(n); } catch (e) { errors.push(`vars.${n}: ${e.message}`); }
  }
  const section = (obj, label) => {
    const into = {};
    for (const [k, src] of Object.entries(obj ?? {})) {
      try { into[k] = evalExpr(parseExpr(src), palette, vars); }
      catch (e) { errors.push(`${label}.${k}: ${e.message}`); }
    }
    return into;
  };
  return { vars: values, sys: section(map.sys, "sys"), chrome: section(map.chrome, "chrome"), errors };
}

// Mirrors OmarchyColorMap::Shape: the address bar gets the target radius,
// everything else scales by the same factor. Returns null for stock radii.
export function shapeTarget(map, hyprlandRounding) {
  const v = map.shape?.address_bar_radius ?? "hyprland";
  if (typeof v === "number" && v >= 0) return Math.trunc(v);
  if (v === "stock") return null;
  return hyprlandRounding ?? null;  // off Hyprland: stock
}
export function scaleRadius(map, target, radius) {
  const stock = map.shape?.stock_address_bar_radius > 0 ? map.shape.stock_address_bar_radius : 17;
  if (target == null || radius <= 0) return radius;
  return Math.round((radius * target) / stock);
}

export { BLACK, WHITE, isDark };
