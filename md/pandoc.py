#!/usr/bin/env python
import panflute as pf
import base64
import os
import sys
import html
import math
import re
import subprocess
# import pygraphviz
import hashlib

def h1hr(elem, doc):
    """
    Add a bottom border to all the <h1>s
    """
    if not isinstance(elem, pf.Header):
        return None

    if elem.level != 1:
        return None

    elem.attributes['style'] = 'border-bottom:1px solid #cccccc'
    return elem

def bq(elem, doc):
    """
    Add a ::: bq div to make a <blockquote>
    """
    if not isinstance(elem, pf.Div):
        return None

    if elem.classes == ['bq']:
        return pf.BlockQuote(*elem.content)

    if 'std' in elem.classes:
        return pf.Div(pf.BlockQuote(*elem.content), classes=elem.classes)

def sha1(x):
    return hashlib.sha1(x.encode(sys.getfilesystemencoding())).hexdigest()

MD_DIR = os.path.dirname(__file__)

def graphviz(elem, doc):
    if isinstance(elem, pf.CodeBlock) and 'graphviz' in elem.classes:
        code = elem.text
        G = pygraphviz.AGraph(string=code)
        G.layout(prog='dot')

        filename = sha1(code)
        filetype = {'html': 'png', 'latex': 'pdf'}.get(doc.format, 'png')
        caption = elem.attributes.get('caption', '')
        imagedir = f'{MD_DIR}/graphviz-images'
        src = f'{imagedir}/{filename}.{filetype}'
        if not os.path.isfile(src):
            try:
                os.mkdir(imagedir)
                sys.stderr.write(f'Created directory {imagedir}\n')
            except OSError:
                pass
            G.draw(src)
            sys.stderr.write(f'Created image {src}\n')
        return pf.Para(pf.Image(pf.Str(caption), url=src, title=caption))

MERMAID_CLI_VERSION = '11.16.0'
MERMAID_CACHE_VERSION = (
    f'mermaid-cli:{MERMAID_CLI_VERSION};themes:default,dark;transparent;v1'
)

def render_mermaid(code, digest, variant, theme):
    imagedir = f'{MD_DIR}/mermaid-images'
    os.makedirs(imagedir, exist_ok=True)
    path = f'{imagedir}/{digest}.{variant}.svg'

    if not os.path.isfile(path):
        result = subprocess.run([
            'mmdc',
            '--quiet',
            '--input', '-',
            '--output', '-',
            '--outputFormat', 'svg',
            '--theme', theme,
            '--backgroundColor', 'transparent',
            '--svgId', f'mermaid-{digest}-{variant}',
        ], input=code.encode('utf-8'), stdout=subprocess.PIPE,
           stderr=subprocess.PIPE)

        svg = result.stdout.lstrip()
        if result.returncode != 0 or not svg.startswith(b'<svg'):
            details = result.stderr.decode('utf-8', errors='replace').strip()
            if not details:
                details = result.stdout.decode('utf-8', errors='replace').strip()
            raise RuntimeError(
                f'Mermaid CLI failed while rendering the {variant} diagram:\n'
                f'{details}'
            )

        tmp = f'{path}.tmp.{os.getpid()}'
        with open(tmp, 'wb') as f:
            f.write(svg)
        os.replace(tmp, path)
        sys.stderr.write(f'Created image {path}\n')

    with open(path, 'rb') as f:
        return f.read()

def svg_data_uri(svg):
    return 'data:image/svg+xml;base64,' + base64.b64encode(svg).decode('ascii')

def svg_dimensions(svg):
    match = re.search(
        rb'\bviewBox="[-+\d.]+\s+[-+\d.]+\s+([\d.]+)\s+([\d.]+)"',
        svg[:1024]
    )
    if match:
        return tuple(math.ceil(float(value)) for value in match.groups())
    return (800, 600)

def mermaid(elem, doc):
    if isinstance(elem, pf.CodeBlock) and 'mermaid' in elem.classes:
        doc.has_mermaid = True
        code = elem.text
        digest = sha1(f'{MERMAID_CACHE_VERSION}\0{code}')
        light = render_mermaid(code, digest, 'light', 'default')
        dark = render_mermaid(code, digest, 'dark', 'dark')
        width, height = svg_dimensions(light)

        caption = elem.attributes.get('caption')
        alt = elem.attributes.get('alt') or caption or 'Mermaid diagram'
        picture = (
            '<picture class="mermaid-diagram">\n'
            f'<source media="screen and (prefers-color-scheme: dark)" '
            f'srcset="{svg_data_uri(dark)}">\n'
            f'<img src="{svg_data_uri(light)}" '
            f'alt="{html.escape(alt, quote=True)}" '
            f'width="{width}" height="{height}">\n'
            '</picture>'
        )
        if caption:
            picture = (
                f'<figure class="mermaid-figure">\n{picture}\n'
                f'<figcaption>{html.escape(caption)}</figcaption>\n</figure>'
            )
        return pf.RawBlock(picture, format='html')

def op(elem, doc):
    if isinstance(elem, pf.Code) and 'op' in elem.classes:
        return pf.RawInline(f'<code><span class="op">{elem.text}</span></code>')

def std(elem, doc):
    # only applies to HackMD docs
    if not doc.get_metadata('hackmd', False):
        return

    # only wrap code blocks (which were already put into a RawBlock)
    if isinstance(elem, pf.RawBlock) and elem.text.startswith('<div class="sourceCode"'):
        # if it's already wrapped, don't need to wrap again
        p = elem.parent
        while True:
            if isinstance(p, pf.BlockQuote):
                return
            if p is None:
                break
            p = p.parent

        return pf.Div(pf.BlockQuote(elem), classes=["std"])


def prepare(doc):
    doc.has_mermaid = False

def add_header_include(doc, text):
    include = pf.MetaBlocks(pf.RawBlock(text, format='html'))
    existing = doc.metadata.content.get('header-includes')
    if existing is None:
        doc.metadata['header-includes'] = pf.MetaList(include)
    elif isinstance(existing, pf.MetaList):
        existing.append(include)
    else:
        doc.metadata['header-includes'] = pf.MetaList(existing, include)

def finalize(doc):
    # Turn the static table of contents into a floating, scrollspy
    # sidebar on wide viewports.
    with open(f'{MD_DIR}/floating-toc.html') as f:
        add_header_include(doc, f.read())

    if doc.has_mermaid:
        add_header_include(doc, """<style>
        .mermaid-diagram {
          display: block;
          margin: 1.5em auto;
          text-align: center;
        }
        .mermaid-diagram img {
          display: block;
          max-width: 100%;
          height: auto;
          margin: auto;
        }
        .mermaid-figure {
          margin: 1.5em auto;
          text-align: center;
        }
        .mermaid-figure .mermaid-diagram {
          margin: 0 auto;
        }
        </style>""")


if __name__ == '__main__':
    pf.run_filters([h1hr, bq, std, mermaid, op], prepare=prepare, finalize=finalize)
