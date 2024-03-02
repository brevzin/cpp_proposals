import jinja2
import sys

print(jinja2.Template(open(sys.argv[1]).read()).render())
