#!/usr/bin/env python
import panflute as pf
import os
import sys
import html
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

def mermaid(elem, doc):
    if isinstance(elem, pf.CodeBlock) and 'mermaid' in elem.classes:
        # Let mermaid render the diagram natively in the browser. The
        # script itself is injected into header-includes in finalize(),
        # only if the document actually has any diagrams.
        doc.has_mermaid = True
        block = f'<pre class="mermaid">\n{html.escape(elem.text)}\n</pre>'
        caption = elem.attributes.get('caption')
        if caption:
            block = (f'<figure>\n{block}\n'
                     f'<figcaption>{html.escape(caption)}</figcaption>\n</figure>')
        return pf.RawBlock(block, format='html')

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
    if doc.format == 'html':
        # Turn the static table of contents into a floating, scrollspy
        # sidebar on wide viewports.
        with open(f'{MD_DIR}/floating-toc.html') as f:
            add_header_include(doc, f.read())

    if doc.has_mermaid:
        add_header_include(doc, """<script type="module">
        import mermaid from 'https://cdn.jsdelivr.net/npm/mermaid@11/dist/mermaid.esm.min.mjs';
        mermaid.initialize({ startOnLoad: true });
        </script>""")


if __name__ == '__main__':
    pf.run_filters([h1hr, bq, std, mermaid, op], prepare=prepare, finalize=finalize)