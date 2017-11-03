import cgi
import markdown
import collections
import StringIO
import sys
from HTMLParser import HTMLParser

class PaperWriter(object):
    def __init__(self):
        self.out = StringIO.StringIO()
        self.out.write('<html>\n')
        self.out.write('<head>\n')

        with open('style.html') as f:
            self.style_html = f.read()
            
        self.toc = '<h2>Contents</h2>\n'
        self.body = ''

    def write(self, ln):
        self.out.write(ln)
            
    def finish(self):
        # write the table of contents 
        self.out.write(self.toc + '\n')
        self.out.write(self.body + '\n')    
        self.out.write('</body>\n</html>')
        print self.out.getvalue()

def normal_markdown(line):
    return markdown.markdown(line).replace('<code>',
        '<code class="language-cpp">')
    
writer = PaperWriter()
state = ''
sections = []
ul_depth = 0
in_table = False
header = collections.defaultdict(list)

normal_lines = []
def flush():
    global normal_lines
    if normal_lines:
        writer.body += normal_markdown('\n'.join(normal_lines))
        normal_lines = []

with open(sys.argv[1]) as f:
    for line in f.readlines():
        if not line:
            continue
        elif line.startswith('<pre '):
            state = 'pre'
        elif line.startswith('</pre>'):
            # write the title
            writer.write('<title>{}</title>\n'.format(header['Title'][0]))
            writer.write(writer.style_html)
            writer.write('</head>\n')
            writer.write('<body>\n')
            
            # address field
            writer.write('<address align=right>\n')
            writer.write('Document number: {} <br />\n'.format(header['Shortname'][0]))
            writer.write('Date: 2017-10-24 <br />\n')
            writer.write('Audience: {} <br />\n'.format(header['Audience'][0]))
            writer.write('Reply-To: ')
            for editor in header['Editor']:
                name, email = editor.split(',')
                writer.write('{} &lt;{}><br />\n'.format(name, email.strip()))
            writer.write('</address>\n')
            writer.write('<hr/>')
            writer.write('<h1 align=center>{}</h1>\n'.format(header['Title'][0]))            
        
            state = 'md'
        elif state == 'pre':
            key, val = line.split(':', 1)
            header[key].append(val)
        elif state == 'md':
            if line.startswith('#'):
                flush()
                # this is a header row
                h, title = line.split(' ', 1)
                if len(sections) < len(h):
                    writer.toc += '<ol>'
                    sections.append(1)
                else:
                    writer.toc += '</li>'
                    writer.toc += '</ol></li>' * (len(sections) - len(h))
                    sections = sections[:len(h)]
                    sections[-1] += 1

                section_str = '.'.join(map(str, sections))
                writer.toc += '<li><a href="#toc_{0}">{1}</a>'.format(section_str, title)
                writer.body += '<a name="toc_{0}"></a><h{2}>{0}. {1}</h{2}>\n'.format(
                    section_str, title, len(h)+1)
                """
            elif line.strip().startswith('- '):
                cur_depth = line.index('-')
                if ul_depth < cur_depth:
                    writer.body += '<ul>'
                    ul_depth += 1
                elif ul_depth > cur_depth:
                    writer.body += '</ul>' * (ul_depth - cur_depth)
                    ul_depth = cur_depth

                writer.body += '<li>{}</li>'.format(normal_markdown(line.strip()[1:].strip()))
                pass    
                """
            elif line.strip() == '```':
                flush()
                state = 'codeblock'
                style = 'style="background:transparent;border:0px"' if in_table else ''
                writer.body += '<pre {}><code class="language-cpp">'.format(style)
            elif line.strip() == '<table>':
                flush()
                in_table = True
                writer.body += '<table style="width: 100%">\n'
            elif line.strip() == '</table>':
                in_table = False
                writer.body += '</table>\n'
            elif line.strip():
                normal_lines.append(line)
                """
                if ul_depth > 0:
                    writer.body += '</ul>' * ul_depth
                    ul_depth = 0
                writer.body += '<p>' + normal_markdown(line)
                """
        elif state == 'codeblock':
            if line.strip() == '```':
                state = 'md'
                writer.body += '</code></pre>\n'
            else:
                writer.body += cgi.escape(line)

flush()
writer.toc += '</li></ol>' * len(sections)

writer.finish()
