"""Generate small geometric OpenType fixtures using only the Python standard library."""
from pathlib import Path
import struct


def u16(*values):
    return struct.pack(">" + "H" * len(values), *values)


def i16(*values):
    return struct.pack(">" + "h" * len(values), *values)


def u32(*values):
    return struct.pack(">" + "I" * len(values), *values)


def checksum(data):
    padded = data + bytes((-len(data)) % 4)
    return sum(struct.unpack(">" + "I" * (len(padded) // 4), padded)) & 0xFFFFFFFF


def outline(points):
    if not points:
        return b""
    xs, ys = zip(*points)
    deltas_x = [xs[0]] + [b - a for a, b in zip(xs, xs[1:])]
    deltas_y = [ys[0]] + [b - a for a, b in zip(ys, ys[1:])]
    return i16(1, min(xs), min(ys), max(xs), max(ys)) + u16(len(points) - 1, 0) + bytes([1] * len(points)) + i16(*deltas_x) + i16(*deltas_y)


def names(family, style):
    entries = [(1, family), (2, style), (3, family + " " + style), (4, family + " " + style), (6, family.replace(" ", "") + "-" + style), (16, family), (17, style)]
    records = b""
    strings = b""
    for name_id, text in entries:
        encoded = text.encode("utf-16-be")
        records += u16(3, 1, 0x409, name_id, len(encoded), len(strings))
        strings += encoded
    return u16(0, len(entries), 6 + 12 * len(entries)) + records + strings


def packed_deltas(values):
    return bytes([0x40 | (len(values) - 1)]) + i16(*values)


def make_font(style="Regular", family="Nori Text Test", weight=400, italic=False, oblique=False, axes=(), overlap=False):
    points = [
        [(40, 0), (340, 0), (340, 600), (40, 600)],
        [],
        [(50, 0), (450, 0), (450, 700), (50, 700)],
        [(50, 0), (400, 0), (400, 500), (50, 500)],
        [(100, 0), (350, 0), (350, 650), (100, 650)],
        [(100, 0), (250, 0), (250, 300), (100, 300)],
    ]
    if overlap:
        points[2] = [(50, 0), (950, 0), (950, 700), (50, 700)]
        points[3] = [(-250, 0), (450, 0), (450, 500), (-250, 500)]
    bold_shift = 80 if weight == 700 else 0
    italic_shift = 120 if italic or oblique else 0
    outlines = []
    for shape in points:
        shifted = [(x + (bold_shift if index in (1, 2) else 0) + (italic_shift if index in (2, 3) else 0), y) for index, (x, y) in enumerate(shape)]
        outlines.append(outline(shifted))
    locations = [0]
    glyf = b""
    for data in outlines:
        glyf += data + bytes((-len(data)) % 4)
        locations.append(len(glyf))
    head = bytearray(54)
    struct.pack_into(">IIIIHH", head, 0, 0x10000, 0x10000, 0, 0x5F0F3CF5, 3, 1000)
    struct.pack_into(">hhhhHHhhh", head, 36, 0, -200, 700, 800, (1 if weight == 700 else 0) | (2 if italic or oblique else 0), 8, 2, 1, 0)
    hhea = bytearray(36)
    struct.pack_into(">IhhhHhhhhhh", hhea, 0, 0x10000, 800, -200, 0, 700, 0, 0, 700, 1, 0, 0)
    struct.pack_into(">H", hhea, 34, len(points))
    maxp = u32(0x10000) + u16(len(points), 4, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0)
    os2 = bytearray(96)
    struct.pack_into(">HhHH", os2, 0, 4, 600, weight, 5)
    struct.pack_into(">hhhhhhhhhhh", os2, 10, 650, 600, 0, 75, 650, 600, 0, 350, 50, 300, 0)
    os2[58:62] = b"NORI"
    selection = (32 if weight == 700 else 0) | (1 if italic else 0) | (512 if oblique else 0)
    struct.pack_into(">HHHhhhHH", os2, 62, selection or 64, 32, 0xFFFD, 800, -200, 0, 800, 200)
    struct.pack_into(">hhHHH", os2, 86, 500, 700, 0, 32, 1)
    mapping = [(32, 1), (65, 2), (66, 3), (67, 4), (0xFFFD, 5), (0xFFFF, 0)]
    count = len(mapping)
    power = 1 << (count.bit_length() - 1)
    cmap4 = u16(4, 16 + 8 * count, 0, count * 2, power * 2, power.bit_length() - 1, count * 2 - power * 2)
    cmap4 += u16(*(char for char, _ in mapping)) + u16(0)
    cmap4 += u16(*(char for char, _ in mapping))
    cmap4 += u16(*((glyph - char) & 0xFFFF for char, glyph in mapping)) + bytes(count * 2)
    tables = {
        "head": bytes(head), "hhea": bytes(hhea), "maxp": maxp, "OS/2": bytes(os2),
        "hmtx": b"".join(u16(250 if index == 1 else 600 + bold_shift) + i16(min(x for x, _ in shape) if shape else 0) for index, shape in enumerate(points)),
        "loca": u32(*locations), "glyf": glyf, "name": names(family, style),
        "cmap": u16(0, 1, 3, 1) + u32(12) + cmap4, "post": u32(0x30000) + bytes(28),
    }
    if axes:
        fvar = u16(1, 0, 16, 2, len(axes), 20, 0, 4 + len(axes) * 4)
        for index, (tag, minimum, default, maximum) in enumerate(axes):
            fvar += tag.encode("ascii") + struct.pack(">iiiHH", int(minimum * 65536), int(default * 65536), int(maximum * 65536), 0, 256 + index)
        tables["fvar"] = fvar
        records = []
        for shape in points:
            if not shape:
                records.append(b"")
                continue
            headers = b""
            deltas = b""
            tuple_count = 0
            for index, (tag, _, _, _) in enumerate(axes):
                if tag not in ("wght", "ital", "slnt", "opsz"):
                    continue
                if tag == "wght":
                    dx = [0, 80, 80, 0, 0, 80, 0, 0]
                elif tag == "opsz":
                    dx = [0, 40, 40, 0, 0, 0, 0, 0]
                else:
                    dx = [0, 0, 120, 120, 0, 0, 0, 0]
                encoded = packed_deltas(dx) + packed_deltas([0] * 8)
                peak = [0] * len(axes)
                peak[index] = -16384 if tag == "slnt" else 16384
                headers += u16(len(encoded), 0x8000) + i16(*peak)
                deltas += encoded
                tuple_count += 1
            record = u16(tuple_count, 4 + len(headers)) + headers + deltas
            records.append(record + bytes((-len(record)) % 4))
        offsets = [0]
        for record in records:
            offsets.append(offsets[-1] + len(record))
        data_offset = 20 + 4 * len(offsets)
        tables["gvar"] = u16(1, 0, len(axes), 0) + u32(data_offset) + u16(len(points), 1) + u32(data_offset) + u32(*offsets) + b"".join(records)
    return tables


def sfnt(tables, base=0):
    tags = sorted(tables)
    power = 1 << (len(tags).bit_length() - 1)
    directory = (b"OTTO" if "CFF " in tables else u32(0x10000)) + u16(len(tags), power * 16, power.bit_length() - 1, len(tags) * 16 - power * 16)
    data = b""
    for tag in tags:
        body = tables[tag]
        offset = 12 + len(tags) * 16 + len(data)
        directory += tag.encode("ascii") + u32(checksum(body), base + offset, len(body))
        data += body + bytes((-len(body)) % 4)
    result = bytearray(directory + data)
    if base == 0:
        head_record = tags.index("head")
        offset = struct.unpack_from(">I", result, 12 + head_record * 16 + 8)[0]
        struct.pack_into(">I", result, offset + 8, (0xB1B0AFBA - checksum(result)) & 0xFFFFFFFF)
    return bytes(result)


def collection(fonts):
    cursor = 12 + len(fonts) * 4
    offsets = []
    parts = []
    for font in fonts:
        offsets.append(cursor)
        data = sfnt(font, cursor)
        parts.append(data)
        cursor += len(data)
    return b"ttcf" + u32(0x10000, len(fonts), *offsets) + b"".join(parts)


def cff_index(entries):
    if not entries:
        return u16(0)
    offsets = [1]
    for entry in entries:
        offsets.append(offsets[-1] + len(entry))
    return u16(len(entries)) + b"\x02" + u16(*offsets) + b"".join(entries)


def cff_number(number):
    return b"\x1c" + i16(number)


def make_cff(bold=False):
    tables = make_font("Bold" if bold else "Regular", weight=700 if bold else 400)
    shift = 80 if bold else 0
    shapes = [
        [(40, 0), (340 + shift, 0), (340 + shift, 600), (40, 600)],
        [],
        [(50, 0), (450 + shift, 0), (450 + shift, 700), (50, 700)],
        [(50, 0), (400 + shift, 0), (400 + shift, 500), (50, 500)],
        [(100, 0), (350 + shift, 0), (350 + shift, 650), (100, 650)],
        [(100, 0), (250 + shift, 0), (250 + shift, 300), (100, 300)],
    ]
    charstrings = []
    for index, shape in enumerate(shapes):
        commands = cff_number(250 if index == 1 else 600 + shift)
        if shape:
            commands += cff_number(shape[0][0]) + cff_number(shape[0][1]) + b"\x15"
            for start, end in zip(shape, shape[1:] + shape[:1]):
                commands += cff_number(end[0] - start[0]) + cff_number(end[1] - start[1]) + b"\x05"
        charstrings.append(commands + b"\x0e")
    prefix = b"\x01\x00\x04\x04" + cff_index([b"NoriTextTestCFF"])
    strings = cff_index([b"replacement"]) + cff_index([])
    charset = b"\x00" + u16(1, 34, 35, 36, 391)
    empty_top = cff_index([b"\x1d" + u32(0) + b"\x0f\x1d" + u32(0) + b"\x11"])
    charset_offset = len(prefix) + len(empty_top) + len(strings)
    top = cff_index([b"\x1d" + u32(charset_offset) + b"\x0f\x1d" + u32(charset_offset + len(charset)) + b"\x11"])
    tables["CFF "] = prefix + top + strings + charset + cff_index(charstrings)
    tables["maxp"] = u32(0x5000) + u16(len(shapes))
    del tables["glyf"], tables["loca"]
    return tables


destination = Path(__file__).parent
regular = make_font()
bold = make_font("Bold", weight=700)
italic = make_font("Italic", italic=True)
bold_italic = make_font("BoldItalic", weight=700, italic=True)
other = make_font("BoldItalic", family="Nori Other Test", weight=700, italic=True)
oblique = make_font("Oblique", oblique=True)
(destination / "Text-Regular.ttf").write_bytes(sfnt(regular))
(destination / "Text-Bold.ttf").write_bytes(sfnt(bold))
(destination / "Text-Italic.ttf").write_bytes(sfnt(italic))
(destination / "Text-BoldItalic.ttf").write_bytes(sfnt(bold_italic))
(destination / "Text-Styles.ttc").write_bytes(collection([regular, bold, italic, bold_italic]))
(destination / "Text-Partial.ttc").write_bytes(collection([regular, bold, oblique, other]))
variable_axes = (("wght", 400, 400, 700), ("ital", 0, 0, 1), ("opsz", 8, 12, 72))
(destination / "Text-Variable.ttf").write_bytes(sfnt(make_font(axes=variable_axes)))
(destination / "Text-Mixed.ttc").write_bytes(collection([bold, make_font(axes=variable_axes)]))
(destination / "Text-Slant.ttf").write_bytes(sfnt(make_font(axes=(("slnt", -12, 0, 0),))))
(destination / "Text-Limited.ttf").write_bytes(sfnt(make_font(axes=(("wght", 400, 400, 600),))))
(destination / "Text-Regular.otf").write_bytes(sfnt(make_cff()))
(destination / "Text-Bold.otf").write_bytes(sfnt(make_cff(True)))
(destination / "Text-Styles.otc").write_bytes(collection([make_cff(), make_cff(True)]))
(destination / "Text-Overlap.ttf").write_bytes(sfnt(make_font(family="Nori Overlap Test", overlap=True)))
