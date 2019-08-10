import json
import yaml

import jinja2

data = yaml.load(open('macros.yaml'), Loader=yaml.Loader)
refs = json.load(open('../md/index.json'))

loader = jinja2.FileSystemLoader(searchpath='./')
env = jinja2.Environment(loader=loader)
template = env.get_template('sd6.tmpl')

print(template.render(
    lang_macros=sorted(data['language'], key=lambda m: m['name']),
    refs=refs))
