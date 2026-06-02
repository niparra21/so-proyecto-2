from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import inch
from reportlab.platypus import (
    SimpleDocTemplate,
    Paragraph,
    Spacer,
    Preformatted,
    PageBreak,
)


SOURCE = "docs/documentacion.md"
OUTPUT = "docs/documentacion.pdf"


def parse_markdown(path):
    with open(path, "r", encoding="utf-8") as source:
        lines = source.readlines()

    blocks = []
    paragraph = []
    code = []
    in_code = False

    def flush_paragraph():
        if paragraph:
            blocks.append(("p", " ".join(line.strip() for line in paragraph)))
            paragraph.clear()

    for raw_line in lines:
        line = raw_line.rstrip("\n")

        if line.startswith("```"):
            if in_code:
                blocks.append(("code", "\n".join(code)))
                code.clear()
                in_code = False
            else:
                flush_paragraph()
                in_code = True
            continue

        if in_code:
            code.append(line)
            continue

        if not line.strip():
            flush_paragraph()
            continue

        if line.startswith("# "):
            flush_paragraph()
            blocks.append(("title", line[2:].strip()))
            continue

        if line.startswith("## "):
            flush_paragraph()
            blocks.append(("h2", line[3:].strip()))
            continue

        if line.startswith("- "):
            flush_paragraph()
            blocks.append(("bullet", line[2:].strip()))
            continue

        paragraph.append(line)

    flush_paragraph()
    return blocks


def build_pdf():
    styles = getSampleStyleSheet()
    styles.add(
        ParagraphStyle(
            name="ProjectTitle",
            parent=styles["Title"],
            fontSize=20,
            leading=24,
            spaceAfter=24,
        )
    )
    styles.add(
        ParagraphStyle(
            name="SectionHeading",
            parent=styles["Heading2"],
            fontSize=14,
            leading=18,
            spaceBefore=14,
            spaceAfter=8,
        )
    )
    styles.add(
        ParagraphStyle(
            name="Body",
            parent=styles["BodyText"],
            fontSize=10.5,
            leading=14,
            spaceAfter=8,
        )
    )
    styles.add(
        ParagraphStyle(
            name="BulletBody",
            parent=styles["BodyText"],
            fontSize=10.5,
            leading=14,
            leftIndent=18,
            firstLineIndent=-10,
            spaceAfter=5,
        )
    )

    doc = SimpleDocTemplate(
        OUTPUT,
        pagesize=letter,
        rightMargin=0.75 * inch,
        leftMargin=0.75 * inch,
        topMargin=0.75 * inch,
        bottomMargin=0.75 * inch,
        title="Proyecto 2 JSON Index",
    )

    story = []
    first_section = True
    for kind, text in parse_markdown(SOURCE):
        escaped = (
            text.replace("&", "&amp;")
            .replace("<", "&lt;")
            .replace(">", "&gt;")
        )
        if kind == "title":
            story.append(Paragraph(escaped, styles["ProjectTitle"]))
        elif kind == "h2":
            if not first_section and text == "Introduccion":
                story.append(PageBreak())
            first_section = False
            story.append(Paragraph(escaped, styles["SectionHeading"]))
        elif kind == "bullet":
            story.append(Paragraph("- " + escaped, styles["BulletBody"]))
        elif kind == "code":
            story.append(Preformatted(text, styles["Code"]))
            story.append(Spacer(1, 8))
        else:
            story.append(Paragraph(escaped, styles["Body"]))

    doc.build(story)


if __name__ == "__main__":
    build_pdf()
