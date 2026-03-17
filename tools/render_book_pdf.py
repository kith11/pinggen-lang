from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import math
import textwrap


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "docs" / "book.md"
OUTPUT = ROOT / "docs" / "book.pdf"

# This renderer stays dependency-free on purpose. SVG/logo embedding is skipped
# because robust vector/image support would add a lot of complexity or external
# tooling; the title page is therefore text-only.

PAGE_WIDTH = 612
PAGE_HEIGHT = 792
LEFT = 56
RIGHT = 56
TOP = 730
BOTTOM = 64
FOOTER_Y = 32
TEXT_WIDTH = PAGE_WIDTH - LEFT - RIGHT
LINE_HEIGHT = 14
SECTION_GAP = 10


@dataclass
class StyledLine:
    kind: str
    text: str
    font: str
    size: int
    indent: int = 0


@dataclass
class Heading:
    level: int
    title: str
    page: int = 0


def escape_pdf_text(value: str) -> str:
    return value.replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)")


def approx_chars(width_scale: float) -> int:
    return max(24, int(TEXT_WIDTH / width_scale))


def wrap_text(text: str, width: int) -> list[str]:
    if not text:
        return [""]
    return textwrap.wrap(text, width=width, replace_whitespace=False, drop_whitespace=False) or [""]


def parse_markdown(markdown: str) -> tuple[str, str, list[Heading], list[StyledLine]]:
    title = "Puff Language Book"
    subtitle = ""
    headings: list[Heading] = []
    lines: list[StyledLine] = []
    in_code = False
    saw_title = False

    for raw in markdown.splitlines():
        if raw.startswith("```"):
            in_code = not in_code
            lines.append(StyledLine("spacer", "", "F1", 10))
            continue

        if in_code:
            lines.append(StyledLine("code", raw, "F2", 10, indent=12))
            continue

        stripped = raw.strip()
        if not saw_title and stripped.startswith("# "):
            title = stripped[2:].strip()
            saw_title = True
            continue

        if saw_title and not subtitle and stripped and not stripped.startswith("## "):
            subtitle = stripped
            lines.append(StyledLine("spacer", "", "F1", 10))
            continue

        if not stripped:
            lines.append(StyledLine("spacer", "", "F1", 10))
            continue

        if stripped.startswith("## "):
            heading_title = stripped[3:].strip()
            headings.append(Heading(1, heading_title))
            lines.append(StyledLine("chapter", heading_title, "F3", 18))
            continue

        if stripped.startswith("### "):
            heading_title = stripped[4:].strip()
            headings.append(Heading(2, heading_title))
            lines.append(StyledLine("section", heading_title, "F3", 13))
            continue

        if stripped.startswith("- "):
            lines.append(StyledLine("bullet", stripped[2:], "F1", 11, indent=14))
            continue

        if raw.startswith("```"):
            continue

        lines.append(StyledLine("body", raw, "F1", 11))

    return title, subtitle, headings, lines


def line_height_for(kind: str, size: int) -> int:
    if kind == "title":
        return 30
    if kind == "subtitle":
        return 18
    if kind == "chapter":
        return 24
    if kind == "section":
        return 18
    if kind == "code":
        return 13
    return max(LINE_HEIGHT, size + 3)


def wrap_styled_line(line: StyledLine) -> list[StyledLine]:
    if line.kind == "spacer":
        return [line]

    if line.kind == "chapter":
        width = approx_chars(7.8)
    elif line.kind == "section":
        width = approx_chars(7.0)
    elif line.kind == "code":
        width = approx_chars(6.2)
    elif line.kind == "bullet":
        width = approx_chars(6.3)
    else:
        width = approx_chars(6.4)

    wrapped = wrap_text(line.text, width)
    out: list[StyledLine] = []
    for index, piece in enumerate(wrapped):
        if line.kind == "bullet":
            if index == 0:
                out.append(StyledLine("bullet", "- " + piece, line.font, line.size, line.indent))
            else:
                out.append(StyledLine("bullet", "  " + piece, line.font, line.size, line.indent))
        else:
            out.append(StyledLine(line.kind, piece, line.font, line.size, line.indent))
    return out


def build_toc_lines(headings: list[Heading]) -> list[StyledLine]:
    lines: list[StyledLine] = [StyledLine("chapter", "Contents", "F3", 18), StyledLine("spacer", "", "F1", 10)]
    for heading in headings:
        indent = 0 if heading.level == 1 else 18
        dots = "." * max(3, 60 - len(heading.title))
        text = f"{heading.title} {dots} {heading.page}"
        lines.append(StyledLine("toc", text, "F1", 11, indent))
    return lines


def paginate(lines: list[StyledLine], start_page: int, heading_pages: dict[str, int] | None = None) -> tuple[list[list[StyledLine]], dict[str, int]]:
    pages: list[list[StyledLine]] = []
    current: list[StyledLine] = []
    y = TOP
    discovered: dict[str, int] = {}
    page_number = start_page

    def flush() -> None:
        nonlocal current, y, page_number
        pages.append(current)
        current = []
        y = TOP
        page_number += 1

    for line in lines:
        wrapped = wrap_styled_line(line)
        if line.kind in {"chapter", "section"}:
            title = line.text
            if title not in discovered:
                discovered[title] = page_number

        for wrapped_line in wrapped:
            height = line_height_for(wrapped_line.kind, wrapped_line.size)
            if wrapped_line.kind == "spacer":
                if y - SECTION_GAP < BOTTOM:
                    flush()
                y -= SECTION_GAP
                continue
            if y - height < BOTTOM:
                flush()
            current.append(wrapped_line)
            y -= height

        if line.kind in {"chapter", "section"}:
            if y - SECTION_GAP < BOTTOM:
                flush()
            else:
                y -= SECTION_GAP

    if current:
        pages.append(current)
    return pages, discovered if heading_pages is None else heading_pages


def render_page(lines: list[StyledLine], page_number: int, title: str | None = None, subtitle: str | None = None) -> str:
    commands: list[str] = []
    y = TOP

    if title is not None:
        commands.append(f"BT /F3 26 Tf 1 0 0 1 {LEFT} {TOP} Tm ({escape_pdf_text(title)}) Tj ET")
        if subtitle:
            commands.append(f"BT /F1 14 Tf 1 0 0 1 {LEFT} {TOP - 34} Tm ({escape_pdf_text(subtitle)}) Tj ET")
        commands.append(f"BT /F1 11 Tf 1 0 0 1 {LEFT} {TOP - 82} Tm ({escape_pdf_text('A guided book for programmers new to Puff')}) Tj ET")
        commands.append(f"BT /F1 11 Tf 1 0 0 1 {LEFT} {TOP - 102} Tm ({escape_pdf_text('Generated from the current repository surface')}) Tj ET")
        commands.append(f"BT /F1 10 Tf 1 0 0 1 {LEFT} {FOOTER_Y} Tm ({escape_pdf_text(str(page_number))}) Tj ET")
        return "\n".join(commands)

    for line in lines:
        if line.kind == "spacer":
            y -= SECTION_GAP
            continue

        height = line_height_for(line.kind, line.size)
        x = LEFT + line.indent
        commands.append(f"BT /{line.font} {line.size} Tf 1 0 0 1 {x} {y} Tm ({escape_pdf_text(line.text)}) Tj ET")
        y -= height
        if line.kind in {"chapter", "section"}:
            y -= SECTION_GAP

    commands.append(f"BT /F1 10 Tf 1 0 0 1 {PAGE_WIDTH / 2 - 8:.0f} {FOOTER_Y} Tm ({escape_pdf_text(str(page_number))}) Tj ET")
    return "\n".join(commands)


def make_pdf(page_streams: list[str]) -> bytes:
    objects: list[bytes] = []

    def add_object(data: str | bytes) -> int:
        payload = data.encode("latin-1", "replace") if isinstance(data, str) else data
        objects.append(payload)
        return len(objects)

    f_body = add_object("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>")
    f_code = add_object("<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>")
    f_bold = add_object("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>")

    content_ids: list[int] = []
    page_ids: list[int] = []
    for stream_text in page_streams:
        stream = stream_text.encode("latin-1", "replace")
        content_ids.append(
            add_object(b"<< /Length " + str(len(stream)).encode("ascii") + b" >>\nstream\n" + stream + b"\nendstream")
        )
        page_ids.append(0)

    pages_id = add_object("<< /Type /Pages /Kids [] /Count 0 >>")
    for index, content_id in enumerate(content_ids):
        page_ids[index] = add_object(
            f"<< /Type /Page /Parent {pages_id} 0 R /MediaBox [0 0 {PAGE_WIDTH} {PAGE_HEIGHT}] "
            f"/Resources << /Font << /F1 {f_body} 0 R /F2 {f_code} 0 R /F3 {f_bold} 0 R >> >> "
            f"/Contents {content_id} 0 R >>"
        )

    kids = " ".join(f"{page_id} 0 R" for page_id in page_ids)
    objects[pages_id - 1] = f"<< /Type /Pages /Kids [{kids}] /Count {len(page_ids)} >>".encode("latin-1")
    catalog_id = add_object(f"<< /Type /Catalog /Pages {pages_id} 0 R >>")

    out = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = [0]
    for index, obj in enumerate(objects, start=1):
        offsets.append(len(out))
        out.extend(f"{index} 0 obj\n".encode("ascii"))
        out.extend(obj)
        out.extend(b"\nendobj\n")

    xref_offset = len(out)
    out.extend(f"xref\n0 {len(objects) + 1}\n".encode("ascii"))
    out.extend(b"0000000000 65535 f \n")
    for offset in offsets[1:]:
        out.extend(f"{offset:010d} 00000 n \n".encode("ascii"))
    out.extend(
        f"trailer\n<< /Size {len(objects) + 1} /Root {catalog_id} 0 R >>\nstartxref\n{xref_offset}\n%%EOF\n".encode(
            "ascii"
        )
    )
    return bytes(out)


def main() -> None:
    markdown = SOURCE.read_text(encoding="utf-8")
    title, subtitle, headings, body_lines = parse_markdown(markdown)

    # First paginate body after reserving one title page and estimated TOC pages.
    temp_toc_pages = 1
    body_pages, heading_map = paginate(body_lines, start_page=2 + temp_toc_pages)
    for heading in headings:
        heading.page = heading_map.get(heading.title, 0)

    toc_lines = build_toc_lines(headings)
    toc_pages, _ = paginate(toc_lines, start_page=2)

    # Recompute body with actual TOC length.
    body_start = 2 + len(toc_pages)
    body_pages, heading_map = paginate(body_lines, start_page=body_start)
    for heading in headings:
        heading.page = heading_map.get(heading.title, 0)
    toc_lines = build_toc_lines(headings)
    toc_pages, _ = paginate(toc_lines, start_page=2)

    streams: list[str] = [render_page([], 1, title=title, subtitle=subtitle)]
    for index, page in enumerate(toc_pages, start=2):
        streams.append(render_page(page, index))
    for index, page in enumerate(body_pages, start=2 + len(toc_pages)):
        streams.append(render_page(page, index))

    OUTPUT.write_bytes(make_pdf(streams))
    print(f"wrote {OUTPUT}")


if __name__ == "__main__":
    main()
