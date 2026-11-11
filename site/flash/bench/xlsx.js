// A small .xlsx writer: sheets of cells with values or formulas, defined names, a few styles, column widths.
// No dependencies, so the page builds the workbook itself and nothing is sent anywhere. Runs in the browser and
// under node. Its output opens in any spreadsheet program.
//
// Cell: { v: number|string, f: "formula without =", c: cached result, s: style name }. A formula may carry the
// value the page computed (c), so a preview that does not calculate (mail, Quick Look) still shows numbers; the
// workbook asks for a full recalculation on load, so a spreadsheet program recomputes every one.
// An input with v "" is written as an empty, styled cell. sheet.flags = "E12:E30" colours FAIL, CHECK and PASS.
(function (root) {
  "use strict";
  const STYLES = ["plain", "bold", "title", "input", "out", "note", "head", "sci", "pct", "warn", "flag", "outpct"];
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
  const col = (n) => { let s = ""; n++; while (n) { const m = (n - 1) % 26; s = String.fromCharCode(65 + m) + s; n = Math.floor((n - 1) / 26); } return s; };

  function sheetXml(sh) {
    const rows = sh.rows.map((r, ri) => {
      if (!r) return "";
      const cells = r.map((c, ci) => {
        if (c === null || c === undefined || c === "") return "";
        const cell = typeof c === "object" ? c : { v: c };
        const ref = col(ci) + (ri + 1), st = Math.max(0, STYLES.indexOf(cell.s || (cell.f ? "out" : "plain")));
        if (cell.f) {
          if (typeof cell.c === "number" && isFinite(cell.c)) return `<c r="${ref}" s="${st}"><f>${esc(cell.f)}</f><v>${cell.c}</v></c>`;
          if (typeof cell.c === "string") return `<c r="${ref}" s="${st}" t="str"><f>${esc(cell.f)}</f><v>${esc(cell.c)}</v></c>`;
          return `<c r="${ref}" s="${st}"><f>${esc(cell.f)}</f></c>`;
        }
        if (cell.v === "") return `<c r="${ref}" s="${st}"/>`;
        if (typeof cell.v === "number" && isFinite(cell.v)) return `<c r="${ref}" s="${st}"><v>${cell.v}</v></c>`;
        return `<c r="${ref}" s="${st}" t="inlineStr"><is><t xml:space="preserve">${esc(cell.v)}</t></is></c>`;
      }).join("");
      return `<row r="${ri + 1}">${cells}</row>`;
    }).join("");
    const cols = sh.widths ? `<cols>${sh.widths.map((w, i) => `<col min="${i + 1}" max="${i + 1}" width="${w}" customWidth="1"/>`).join("")}</cols>` : "";
    const pane = sh.freeze ? `<sheetViews><sheetView workbookViewId="0"><pane ySplit="${sh.freeze}" topLeftCell="A${sh.freeze + 1}" activePane="bottomLeft" state="frozen"/></sheetView></sheetViews>` : "";
    const cf = sh.flags ? `<conditionalFormatting sqref="${sh.flags}">${["FAIL", "CHECK", "PASS"].map((t, i) => `<cfRule type="cellIs" dxfId="${i}" priority="${i + 1}" operator="equal"><formula>"${t}"</formula></cfRule>`).join("")}</conditionalFormatting>` : "";
    return `<?xml version="1.0" encoding="UTF-8" standalone="yes"?><worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">${pane}${cols}<sheetData>${rows}</sheetData>${cf}</worksheet>`;
  }

  const STYLES_XML = `<?xml version="1.0" encoding="UTF-8" standalone="yes"?><styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
<numFmts count="2"><numFmt numFmtId="164" formatCode="##0.000E+0"/><numFmt numFmtId="165" formatCode="0.00%"/></numFmts>
<fonts count="6"><font><sz val="11"/><name val="Calibri"/></font><font><b/><sz val="11"/><name val="Calibri"/></font><font><b/><sz val="15"/><name val="Calibri"/></font><font><sz val="11"/><color rgb="FF0000CC"/><name val="Calibri"/></font><font><i/><sz val="10"/><color rgb="FF666666"/><name val="Calibri"/></font><font><b/><sz val="11"/><color rgb="FFB42418"/><name val="Calibri"/></font></fonts>
<fills count="4"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill><fill><patternFill patternType="solid"><fgColor rgb="FFFFF6D5"/></patternFill></fill><fill><patternFill patternType="solid"><fgColor rgb="FFEDE7E1"/></patternFill></fill></fills>
<borders count="1"><border/></borders><cellStyleXfs count="1"><xf/></cellStyleXfs>
<cellXfs count="12"><xf/><xf fontId="1" applyFont="1"/><xf fontId="2" applyFont="1"/><xf fontId="3" fillId="2" applyFont="1" applyFill="1"/><xf numFmtId="164" applyNumberFormat="1"/><xf fontId="4" applyFont="1" applyAlignment="1"><alignment wrapText="1" vertical="top"/></xf><xf fontId="1" fillId="3" applyFont="1" applyFill="1"/><xf numFmtId="164" applyNumberFormat="1"/><xf numFmtId="165" applyNumberFormat="1"/><xf fontId="5" applyFont="1"/><xf fontId="1" applyFont="1" applyAlignment="1"><alignment horizontal="center"/></xf><xf numFmtId="165" applyNumberFormat="1"/></cellXfs>
<cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles>
<dxfs count="3"><dxf><font><b/><color rgb="FF9C0006"/></font><fill><patternFill><bgColor rgb="FFFFC7CE"/></patternFill></fill></dxf><dxf><font><b/><color rgb="FF7A5200"/></font><fill><patternFill><bgColor rgb="FFFFE9A8"/></patternFill></fill></dxf><dxf><font><color rgb="FF1E6B2E"/></font><fill><patternFill><bgColor rgb="FFDDF0DD"/></patternFill></fill></dxf></dxfs>
</styleSheet>`;

  // zip, stored (no compression): small and simple; CRC32 per file
  const CRC = (() => { const t = new Uint32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; } return t; })();
  const crc32 = (b) => { let c = 0xffffffff; for (let i = 0; i < b.length; i++) c = CRC[(c ^ b[i]) & 0xff] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };
  const utf8 = (s) => new TextEncoder().encode(s);
  function zip(files) {
    const parts = [], central = []; let off = 0;
    const u16 = (n) => [n & 0xff, (n >>> 8) & 0xff], u32 = (n) => [n & 0xff, (n >>> 8) & 0xff, (n >>> 16) & 0xff, (n >>> 24) & 0xff];
    for (const [name, text] of files) {
      const nb = utf8(name), data = utf8(text), c = crc32(data);
      const head = new Uint8Array([...u32(0x04034b50), ...u16(20), ...u16(0x0800), ...u16(0), ...u16(0), ...u16(0x21), ...u32(c), ...u32(data.length), ...u32(data.length), ...u16(nb.length), ...u16(0)]);
      parts.push(head, nb, data);
      central.push(new Uint8Array([...u32(0x02014b50), ...u16(20), ...u16(20), ...u16(0x0800), ...u16(0), ...u16(0), ...u16(0x21), ...u32(c), ...u32(data.length), ...u32(data.length), ...u16(nb.length), ...u16(0), ...u16(0), ...u16(0), ...u16(0), ...u32(0), ...u32(off)]), nb);
      off += head.length + nb.length + data.length;
    }
    const csize = central.reduce((a, b) => a + b.length, 0);
    const end = new Uint8Array([...u32(0x06054b50), ...u16(0), ...u16(0), ...u16(files.length), ...u16(files.length), ...u32(csize), ...u32(off), ...u16(0)]);
    const all = parts.concat(central, [end]); const out = new Uint8Array(all.reduce((a, b) => a + b.length, 0)); let p = 0;
    for (const b of all) { out.set(b, p); p += b.length; }
    return out;
  }

  // book: { sheets: [{name, rows, widths, freeze}], names: [[name, "Sheet!$B$3"]], title }
  function build(book) {
    const sheets = book.sheets;
    const files = [
      ["[Content_Types].xml", `<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>${sheets.map((_, i) => `<Override PartName="/xl/worksheets/sheet${i + 1}.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>`).join("")}<Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/></Types>`],
      ["_rels/.rels", `<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties" Target="docProps/core.xml"/></Relationships>`],
      ["docProps/core.xml", `<?xml version="1.0" encoding="UTF-8" standalone="yes"?><cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties" xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>${esc(book.title || "workbook")}</dc:title><dc:creator>Fox Red Labs</dc:creator></cp:coreProperties>`],
      ["xl/workbook.xml", `<?xml version="1.0" encoding="UTF-8" standalone="yes"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets>${sheets.map((s, i) => `<sheet name="${esc(s.name)}" sheetId="${i + 1}" r:id="rId${i + 1}"/>`).join("")}</sheets>${book.names && book.names.length ? `<definedNames>${book.names.map(([n, r]) => `<definedName name="${esc(n)}">${esc(r)}</definedName>`).join("")}</definedNames>` : ""}<calcPr calcId="191029" fullCalcOnLoad="1"/></workbook>`],
      ["xl/_rels/workbook.xml.rels", `<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">${sheets.map((_, i) => `<Relationship Id="rId${i + 1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet${i + 1}.xml"/>`).join("")}<Relationship Id="rId${sheets.length + 1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>`],
      ["xl/styles.xml", STYLES_XML],
    ].concat(sheets.map((s, i) => [`xl/worksheets/sheet${i + 1}.xml`, sheetXml(s)]));
    return zip(files);
  }

  const api = { build, col };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.XLSX_FRL = api;
})(typeof window !== "undefined" ? window : this);
