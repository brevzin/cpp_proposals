#!/usr/bin/env python3
import panflute as pf

def code_cpp(elem, doc):
    """
    add cpp to the classes of empty code blocks so you can just write
    `operator==` instead of `operator==`{.cpp}
    """
    if not isinstance(elem, pf.Code):
        return None

    if not elem.classes:
        elem.classes.append('cpp')
    return elem

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

def self_link(elem, doc):
    """
    Add a self-link to each header
    """
    if not isinstance(elem, pf.Header):
        return None

    link = pf.Link(url='#{}'.format(elem.identifier), classes=['self-link'])
    elem.content.insert(0, link)
    return elem

def cpp2language(elem, doc):
    """
    Change all the cpp to language-cpp for prism
    """
    if not isinstance(elem, (pf.Code, pf.CodeBlock)):
        return None

    elem.classes = ['language-cpp' if c == 'cpp' else c for c in elem.classes]
    return elem

if __name__ == '__main__':
    pf.run_filters([code_cpp, h1hr, self_link])
