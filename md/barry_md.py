r'''
Markdown formatter for papers.

in: argv[0]
out: stdout
'''
from __future__ import print_function
import cgi
import markdown
import collections
import StringIO
import sys
import argparse

def cmd_parser():
    parser = argparse.ArgumentParser(
            description="Markdown formatter for papers."
            )
    parser.add_argument(
            'input',
            help='input file name',
            default='-',
            )
    parser.add_argument(
            'output',
            help='output file name',
            default='-',
            )
    parser.add_argument(
            '--style,-s',
            help='style file (html)',
            default='style.html',
            dest='style',
            )
    return parser

def open_or_stdout(fname):
    return sys.stdout if fname == '-' else open(fname, 'wb')

def parse_args(argv=None):
    parser = cmd_parser()
    args = parser.parse_args(argv)
    if args.input == '-':
        args.input = sys.stdin
    else:
        args.input = open(args.input, 'r')

    if args.style == '-':
        args.style = StringIO.StringIO()
    else:
        args.style = open(args.style, 'r')

    return args


class PaperWriter(object):
    def __init__(self, out):
        self._out = out
        self._out.write('<html>\n')
        self._out.write('<head>\n')

        self.toc = '<h2>Contents</h2>\n'
        self.body = ''

    def write(self, ln):
        self._out.write(ln)

    def finish(self):
        # write the table of contents
        self._out.write(self.toc + '\n')
        self._out.write(self.body.encode('utf-8') + '\n')
        self._out.write('</body>\n</html>')
        return self._out

def normal_markdown(line):
    md = markdown.markdown(line)
    md = md.replace('<code>', '<code class="language-cpp">')
    # now, remove all the <li><p> stuff
    lines = md.splitlines()
    for i in range(1, len(lines)):
        if (lines[i-1] == '<li>' and
                lines[i].startswith('<p>') and
                lines[i].endswith('</p>')):
            lines[i] = lines[i][3:-4]
    return '\n'.join(lines)


def process(in_file, out_file, style_file):
    writer = PaperWriter(out_file)
    state = ''
    sections = []
    in_table = False
    header = collections.defaultdict(list)

    normal_lines = []
    def flush():
        if normal_lines:
            writer.body += normal_markdown('\n'.join(normal_lines))
            del normal_lines[:]

    for line in in_file.readlines():
        line = line.decode('utf-8')

        if not line:
            continue
        elif line.startswith('<pre '):
            state = 'pre'
        elif line.startswith('</pre>'):
            # write the title
            writer.write('<title>{}</title>\n'.format(header['Title'][0]))
            writer.write(style_file.read())
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
                ed_row = '{} &lt;{}><br />\n'.format(
                    name.encode('utf-8'),
                    email.encode('utf-8').strip())
                writer.write(ed_row)
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
        elif state == 'codeblock':
            if line.strip() == '```':
                state = 'md'
                writer.body += '</code></pre>\n'
            else:
                writer.body += cgi.escape(line)

    flush()
    writer.toc += '</li></ol>' * len(sections)

    return writer.finish()

def main(argv=None):
    args = parse_args(argv)
    io = process(args.input, StringIO.StringIO(), args.style)
    # only open after the file has been processed, to avoid touching it on
    # failure. Doing that would break make.
    out_file = open_or_stdout(args.output)
    print(io.getvalue(), file=out_file)

if __name__ == '__main__':
    main()
