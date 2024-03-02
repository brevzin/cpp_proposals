import jinja2

print(jinja2.Template(open("boost-lambda2.tpl.md").read()).render())
