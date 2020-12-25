#!/usr/bin/env python
import panflute as pf
import os
import sys
# sys.path.append('/home/brevzin/.local/lib/python3.6/site-packages')
import pygraphviz
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

def graphviz(elem, doc):
    def sha1(x):
        return hashlib.sha1(x.encode(sys.getfilesystemencoding())).hexdigest()

    if isinstance(elem, pf.CodeBlock) and 'graphviz' in elem.classes:
        code = elem.text
        caption = "caption"
        G = pygraphviz.AGraph(string=code)
        G.layout()

        filename = sha1(code)
        filetype = {'html': 'png', 'latex': 'pdf'}.get(doc.format, 'png')
        alt = pf.Str(caption)
        imagedir = 'graphviz-images'
        src = f'{imagedir}/{filename}.{filetype}'
        if not os.path.isfile(src):
            try:
                os.mkdir(imagedir)
                sys.stderr.write(f'Created directory {imagedir}\n')
            except OSError:
                pass
            G.draw(src, prog='dot')
            sys.stderr.write(f'Created image {src}\n')
        return pf.Para(pf.Image(alt, url=src, title=''))

if __name__ == '__main__':
    pf.run_filters([h1hr, bq, graphviz])
