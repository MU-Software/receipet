"""A label packed with Hangul, to see which strokes survive on real paper.

Hangul is the hard case: a syllable like 뷁 puts a dozen strokes in one em, so
it fails at a size where Latin still reads fine.  The card steps the point size
down, labelling each block with its own pixel height, then packs whatever is
left with body text - so the printout says both "how small can it go" and "does
a full label come out clean".
"""

from PIL import Image as PILImage
from PIL import ImageDraw, ImageFont
from receipet import Image

FONT = "/System/Library/Fonts/AppleSDGothicNeo.ttc"  # always present on macOS

# Many-stroke syllables first - these are what break.
SAMPLES = [
    "뷁뷀쒫쮕훯릫쫡꿿밟닭",
    "한글 인쇄 품질 시험 0123",
    "서울特別市 江南區 테헤란로",
    "받침 겹받침 앉 없 핥 읊 값",
    "다람쥐 헌 쳇바퀴에 타고파",
]
BODY = (
    "국민의 모든 자유와 권리는 국가안전보장·질서유지 또는 공공복리를 위하여 필요한 경우에 한하여 "
    "법률로써 제한할 수 있으며, 제한하는 경우에도 자유와 권리의 본질적인 내용을 침해할 수 없다. "
)


def _font(px):
    return ImageFont.truetype(FONT, px, index=0)


def _wrap(text, font, width):
    """Greedy wrap by character - Hangul has no spaces to rely on."""
    lines, line = [], ""
    for ch in text:
        if font.getlength(line + ch) > width:
            lines.append(line)
            line = ch
        else:
            line += ch
    if line:
        lines.append(line)
    return lines


def build(w, h, title):
    """A PIL 1-bit image w x h, filled top to bottom."""
    im = PILImage.new("1", (w, h), 1)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, w - 1, h - 1], outline=0, width=2)

    pad, y = 5, 6
    inner = w - 2 * pad

    head = _font(max(11, min(20, h // 24)))
    d.text((pad, y), f"{title} {w}x{h}", font=head, fill=0)
    y += head.size + 4
    d.line([(pad, y), (w - pad - 1, y)], fill=0, width=1)
    y += 4

    # Size ladder: one line each, largest first, skipping any that cannot fit.
    for i, px in enumerate(p for p in (44, 36, 30, 24, 20, 17, 14, 12, 10, 8) if p <= h // 6):
        f = _font(px)
        if y + px + 3 > h - pad:
            break
        text = f"{px}px {SAMPLES[i % len(SAMPLES)]}"
        d.text((pad, y), _wrap(text, f, inner)[0], font=f, fill=0)
        y += px + 2

    # Pack the rest with body text so the whole label carries ink.
    px = max(9, min(16, h // 30))
    f = _font(px)
    reps = -(-h // max(1, px)) // 2 + 2
    for line in _wrap(BODY * reps, f, inner):
        if y + px + 1 > h - pad:
            break
        d.text((pad, y), line, font=f, fill=0)
        y += px + 1

    return im


def card(w, h, title):
    """The same thing as a receipet Image, ready for any encoder."""
    return Image.from_pil(build(w, h, title))
