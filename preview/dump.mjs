// node preview/dump.mjs <colors.toml> [<map.json>]
// Same output shape as out/Release/omarchium_color_dump, from the preview's
// evaluator. scripts/check-preview diffs the two.
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { evaluateMap, parsePalette, scaleRadius, toHex } from "./colormap.js";

const [tomlPath, mapPath] = process.argv.slice(2);
const map = JSON.parse(readFileSync(mapPath ?? fileURLToPath(new URL("../core/resources/omarchy_color_map.json", import.meta.url)), "utf8"));
const palette = parsePalette(readFileSync(tomlPath, "utf8"));
const ev = evaluateMap(map, palette);
const hex = (o) => Object.fromEntries(Object.entries(o).map(([k, c]) => [k, toHex(c)]));
const scale = Object.fromEntries([4, 8, 10, 12, 16, 17, 24].map((r) => [String(r), scaleRadius(map, 6, r)]));
console.log(JSON.stringify({ dark: palette.dark, sys: hex(ev.sys), chrome: hex(ev.chrome), scale_at_6: scale, errors: ev.errors }));
