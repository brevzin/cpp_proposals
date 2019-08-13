import json
import sys
import yaml

import jinja2

data = yaml.load(open('macros.yaml'), Loader=yaml.CLoader)
refs = json.load(open('reduced.json'))

loader = jinja2.FileSystemLoader(searchpath='./')
env = jinja2.Environment(loader=loader)
template = env.get_template('sd6.tmpl')

print(template.render(
    lang_macros=data['language'],
    lib_macros=data['library'],
    attr_macros=data['attributes'],
    refs=refs))
