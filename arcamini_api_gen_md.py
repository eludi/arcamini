import json

def param_md(param):
    name = param["name"]
    typ = param.get("type", "any")
    default = param.get("defaultValue")
    desc = param.get("description", "")
    line = f"- {{{typ}}} {name}"
    if default is not None:
        line += f" (default: {default})"
    if desc:
        line += f" - {desc}"
    return line

def func_md(func, is_callback=False):
    name = func["function"]
    params = func.get("parameters", [])
    ret = func.get("returnType")
    desc = func.get("description", "")
    out = [f"### {'callback ' if is_callback else ''}function {name}"]
    if desc:
        out.append(desc)
    if params:
        out.append("#### Parameters:")
        for p in params:
            out.append(param_md(p))
    if ret and ret != "null":
        out.append("\n#### Returns:\n- {" + ret + "}")
    return "\n".join(out)

def module_md(mod):
    out = [f"## module {mod['module']}\n"]
    if "description" in mod:
        out.append(mod["description"])
    for func in mod.get("functions", []):
        out.append(func_md(func))
        out.append("")
    for cb in mod.get("callbacks", []):
        out.append(func_md(cb, is_callback=True))
        out.append("")
    return "\n".join(out)

def main():
    with open("arcamini_api.json", "r") as f:
        api = json.load(f)
    out = ["# arcamini API\n"]
    for mod in api:
        out.append(module_md(mod))
    with open("arcamini_api.md", "w") as f:
        f.write("\n".join(out))

if __name__ == "__main__":
    main()