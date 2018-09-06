import argparse
import json
import os
import sys
import time

import markdown
from markdown.extensions import Extension
#from markdown.extensions.codehilite import CodeHiliteExtension
from markdown.inlinepatterns import LinkPattern, LINK_RE, BacktickPattern, BACKTICK_RE
from markdown.treeprocessors import Treeprocessor
from markdown.postprocessors import Postprocessor
from markdown.preprocessors import Preprocessor
from markdown.util import etree, string_type, isBlockLevel, AtomicString

from codehilite import CodeHiliteExtension
from toc import TocExtension

def local_path(filename):
    return os.path.join(sys.path[0], filename)

class LinkSaver(LinkPattern):
    def __init__(self, *args, **kwargs):
        super(LinkSaver, self).__init__(*args, **kwargs)
        self.markdown.saved_links = set()
        self.wg21 = json.load(open(local_path('index.json')))
        for key in sorted(self.wg21, reverse=True):
            if key.startswith('P') and key[:5] not in self.wg21:
                self.wg21[key[:5]] = self.wg21[key]
        
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

class RefProcessor(Treeprocessor):
    def __init__(self, md):
        super(RefProcessor, self).__init__(md)

        self.wg21 = json.load(open(local_path('index.json')))
        for key in sorted(self.wg21, reverse=True):
            if key.startswith('P') and key[:5] not in self.wg21:
                self.wg21[key[:5]] = self.wg21[key]

    def run(self, doc):
        wg21_links = []
        other_links = []
        for link in self.markdown.saved_links:
            if 'wg21.link' in link:
                wg21_links.append(link.split('/')[-1].upper())
        wg21_links.sort()

        if wg21_links:
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
                        .replace('<pre class="codehilite">',
                            '<pre style="background:transparent;border:0px">')
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
        md.inlinePatterns[u'link'] = LinkSaver(LINK_RE, md)
        md.treeprocessors.add("refs", RefProcessor(md), '<toc')

class TableCodeBlockExtension(Extension):
    def extendMarkdown(self, md, md_globals):
        md.preprocessors.add("tbl_codeblock",
            TableCodeBlockProcessor(
                markdown.Markdown(extensions=[CodeHiliteExtension(use_pygments=False),
                    CppBacktickExtension()
                    ])
            ),
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
    if args.references:
        extensions.append(RefExtension())
     
    md = markdown.Markdown(extensions=extensions)
    in_md = args.input.read().decode('utf-8')
    html = md.convert(in_md)
    write('<html>\n')
    write('<head>\n')
    write('<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">\n')
    write('<title>{}</title>\n'.format(md.Meta['title'][0]))

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
    write('<hr /><h1 align=center>{}</h1>\n'.format(md.Meta['title'][0]))
    write('<h2>Contents</h2>\n')
    write('{}\n{}\n</html>'.format(md.toc, html.encode('utf-8')))

if __name__ == '__main__':
    main()
