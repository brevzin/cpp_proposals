import argparse
import collections
import json
import os
import sys
import time

import markdown
from markdown.extensions import Extension
#from markdown.extensions.codehilite import CodeHiliteExtension
from markdown.inlinepatterns import (
    LinkPattern, LINK_RE, BacktickPattern, BACKTICK_RE,
    ReferencePattern, REFERENCE_RE, SHORT_REF_RE)
from markdown.treeprocessors import Treeprocessor
from markdown.postprocessors import Postprocessor
from markdown.preprocessors import Preprocessor
from markdown.util import etree, string_type, isBlockLevel, AtomicString
import lxml.etree

from codehilite import CodeHiliteExtension
from toc import TocExtension

def local_path(filename):
    return os.path.join(sys.path[0], filename)

def load_wg21():
    wg21 = json.load(open(local_path('index.json')))
    for key in sorted(wg21, reverse=True):
        if key.startswith('P') and key[:5] not in wg21:
            wg21[key[:5]] = wg21[key]
    return wg21
    
class LinkSaver(LinkPattern):
    def __init__(self, wg21, *args, **kwargs):
        super(LinkSaver, self).__init__(*args, **kwargs)
        self.wg21 = wg21
        
    def handleMatch(self, m):
        el = etree.Element('a')
        el.text = m.group(2)
        title = m.group(13)
        href = m.group(9)
        self.markdown.saved_links.add(href)
        
        if href:
            if href[0] == '<':
                href = href[1:-1]
            el.set('href', self.sanitize_url(self.unescape(href.strip())))
        else:
            el.set('href', '')
        
        if title:
            title = self.unescape(title)
            el.set("title", title)
        elif 'wg21.link' in href:
            el.set("title", self.wg21[href.split('/')[-1].upper()]['title'])
        return el

class RefSaver(ReferencePattern):
    def makeTag(self, href, title, text):
        return ReferencePattern.makeTag(self,
            href, title.split('||')[0], text)
        
class RefProcessor(Treeprocessor):
    def __init__(self, md, wg21):
        super(RefProcessor, self).__init__(md)
        self.wg21 = wg21

    def run(self, doc):
        wg21_links = []
        other_links = []
        for link in self.markdown.saved_links:
            if 'wg21.link' in link:
                wg21_links.append(link.split('/')[-1].upper())
        wg21_links.sort()

        if wg21_links or self.markdown.references:
            refs = etree.SubElement(doc, 'h1')
            refs.text = 'References'
            ul = etree.SubElement(doc, 'ul')
            
        for link in wg21_links:
            info = self.wg21[link]
            li = etree.SubElement(ul, 'li')
            a = etree.SubElement(li, 'a')
            a.attrib["href"] = info['long_link']
            a.text = '[{}]'.format(link)

            desc = etree.SubElement(li, 'span')
            desc.attrib['style'] = "margin-left: 5px;"
            if info['type'] == 'paper':
                desc.text = u'"{}" by {}, {}'.format(
                    info['title'],
                    info['author'],
                    info['date'])
            elif info['type'] == 'issue':
                if 'last_modified' in info:
                    date = info['last_modified']
                else:
                    date = info['date']
            
                desc.text = u'"{}" by {}, {}'.format(
                    info['title'],
                    info['submitter'],
                    date)
                        
        for ref in sorted(self.markdown.references):
            href, title = self.markdown.references[ref]
            li = etree.SubElement(ul, 'li')
            a = etree.SubElement(li, 'a')
            a.attrib["href"] = href
            a.text = '[{}]'.format(ref)
            
            desc = etree.SubElement(li, 'span')
            desc.attrib['style'] = "margin-left: 5px;"
            parts = title.split('||')
            if len(parts) == 1:
                if title[0] == '[' and title[-1] == ']':
                    desc.text = u'Current Working Draft'.format(title)
                else:
                    desc.text = title
            else:
                desc.text = u'"{}" by {}, {}'.format(*parts)

class TableCodeBlockProcessor(Preprocessor):
    def run(self, lines):
        new_lines = []
        to_markup = []
        target = new_lines
        
        for line in lines:
            if line.startswith('<th') or line.startswith('<td'):
                new_lines.append(line)
                target = to_markup
            elif line.startswith('</th') or line.startswith('</td'):
                target = new_lines
                new_lines.extend(
                    self.markdown.convert(u'\n'.join(to_markup))
                        .replace('<pre class="codehilite"',
                            '<pre style="background:transparent;border:0px"')
                        .split('\n'))
                to_markup = []
                new_lines.append(line)
            else:
                target.append(line)
        return new_lines

class CppBacktickPattern(BacktickPattern):
    def handleMatch(self, m):
        if m.group(4):
            text = m.group(4).strip()
            el = etree.Element('code')
            if text.startswith('#!'):
                el.text = AtomicString(text[2:].strip())
            else:
                el.attrib["class"] = "language-cpp"
                el.text = AtomicString(text.strip())
            return el
        else:
            return super(CppBacktickPattern, self).handleMatch(m)

class CppBacktickExtension(Extension):
    def extendMarkdown(self, md, md_globals):
        md.inlinePatterns[u'backtick'] = CppBacktickPattern(BACKTICK_RE)
            
class RefExtension(Extension):
    def extendMarkdown(self, md, md_globals):
        md.saved_links = set()
        wg21 = load_wg21()
        md.inlinePatterns[u'link'] = LinkSaver(wg21, LINK_RE, md)
        md.inlinePatterns[u'reference'] = RefSaver(REFERENCE_RE, md)
        md.inlinePatterns[u'short_reference'] = RefSaver(SHORT_REF_RE, md)
        md.treeprocessors.add("refs", RefProcessor(md, wg21), '<toc')

class TableCodeBlockExtension(Extension):
    def extendMarkdown(self, md, md_globals):
        new_md = markdown.Markdown(extensions=[ CodeHiliteExtension(use_pygments=False),
                    CppBacktickExtension()
                    ])
        new_md.references = md.references
        new_md.inlinePatterns[u'reference'] = RefSaver(REFERENCE_RE, new_md)
        new_md.inlinePatterns[u'short_reference'] = RefSaver(SHORT_REF_RE, new_md)
          
        md.preprocessors.add("tbl_codeblock",
            TableCodeBlockProcessor(new_md),
            '_begin')
        
def main(argv=None):
    parser = argparse.ArgumentParser(
        description='Markdown formatter for papers')
    parser.add_argument('-i', '--input', dest='input', type=argparse.FileType('r'))
    parser.add_argument('-o', '--output', dest='output', type=argparse.FileType('w'),
        default='-')
    parser.add_argument('--references', action='store_true')
    
    args = parser.parse_args(argv)
    
    write = args.output.write
    
    extensions = [CodeHiliteExtension(use_pygments=False),
        TocExtension(baselevel=2, anchorlink=False, title=None, marker=''),
        'markdown.extensions.meta',
        CppBacktickExtension(),
        TableCodeBlockExtension()]
    noref_md = markdown.Markdown(extensions=extensions)
    if args.references:
        extensions.append(RefExtension())
    
    class OrderedMarkdown(markdown.Markdown):
        def registerExtensions(self, *args, **kwargs):
            self.references = collections.OrderedDict()
            markdown.Markdown.registerExtensions(self, *args, **kwargs)
    
    md = OrderedMarkdown(extensions=extensions)
    in_md = args.input.read().decode('utf-8')
    html = md.convert(in_md)
    write('<html>\n')
    write('<head>\n')
    write('<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">\n')

    def get_title(input):
        title = noref_md.convert(input)
        title_root = lxml.etree.fromstring(title)
        lxml.etree.strip_tags(title_root, '*')
        return title_root.text
    
    write('<title>{}</title>\n'.format(
        get_title(md.Meta['title'][0])))

    def add_style(filename):
        suffix = filename.rsplit('.', 1)[1]
        fmt = {'css': '<style type="text/css">{}</style>\n',
            'js': '<script type="text/javascript">{}</script>\n'
        }[suffix]
            
        write(fmt.format(open(local_path(filename)).read()))

    add_style('general.css')
    add_style('prism_default.css')
    add_style('prism.js')
    
    #write('<style type="text/css">{}</style>\n'.format(
    #    open(local_path('general.css')).read()))    
    #write('<style type="text/css">{}</style>\n'.format(
    #    open(local_path('prism.css')).read().replace('fdf6e3', 'f8f8f8')))
    #write('<script type="text/javascript">{}</script>\n'.format(
    #    open(local_path('prism.js')).read()))
        
    write('\n</head>\n')
    write('<body>\n')
    write('<address align=right>\n')    
    write('Document Number: {} <br />\n'.format(md.Meta['document-number'][0]))
    write('Date: {} <br />\n'.format(time.strftime('%Y-%m-%d', time.localtime())))
    write('Audience: {} <br />\n'.format(md.Meta['audience'][0]))
    write('Reply-To: {} <br />\n'.format('<br />'.join(md.Meta['authors'])))
    write('</address>\n')
    write('<hr /><h1 align=center>{}</h1>\n'.format(
        noref_md.convert(md.Meta['title'][0])))
    write('<h2>Contents</h2>\n')
    write('{}\n{}\n</html>'.format(md.toc, html.encode('utf-8')))

if __name__ == '__main__':
    main()
