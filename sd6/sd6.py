import json
import sys
import yaml

import jinja2

data = yaml.load(open('macros.yaml'), Loader=yaml.CLoader)
refs = json.load(open('reduced.json'))

loader = jinja2.FileSystemLoader(searchpath='./')
env = jinja2.Environment(loader=loader)
template = env.get_template('sd6.tmpl')

def refactor(macros):
    # turn something like
    # {'name': '__cpp_lib_byte', 'values': [{'value': 201603, 'papers': 'P0298R3'
    # into
    # {'name': '__cpp_lib_byte', 'value': {201603: ['P0298R3']}} 
    for macro in macros:
        try:
            macro['value'] = {
                v['value']: {
                    'papers': v['papers'].split() if v.get('papers') else '',
                    'feature': v.get('feature', ''),
                    'removed': v.get('removed', False)
                } for v in macro.pop('values')}
        except KeyError as e:
            print(f'{e} thrown for {macro}', file=sys.stderr)
            raise
    return sorted(macros, key=lambda m: m['name'])

print(template.render(
    lang_macros=refactor(data['language']),
    lib_macros=refactor(data['library']),
    attr_macros=refactor(data['attributes']),
    refs=refs))
