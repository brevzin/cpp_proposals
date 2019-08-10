import json
import yaml

macros = yaml.load(open('macros.yaml'), Loader=yaml.Loader)

def refs_to_dict(filename):
    data = yaml.load(open(filename), Loader=yaml.Loader)
    return {elem['id']: elem for elem in data['references']}

refs = refs_to_dict('../md/wg21/data/index.yaml')
refs.update(refs_to_dict('../md/wg21_fmt.yaml'))

reduced_refs = {}
for top in ('language', 'library', 'attributes'):
    for elem in macros[top]:
        for value in elem['values']:
            if value.get('papers'):
                for paper in value['papers'].split():
                    reduced_refs[paper] = refs[paper]['title']

print(json.dumps(reduced_refs))
