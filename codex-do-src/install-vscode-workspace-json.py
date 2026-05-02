import json
import os
import sys
import tempfile


def load_json(path):
    if not os.path.exists(path):
        return {}
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def render_json(data):
    return json.dumps(data, indent=4) + "\n"


def update_file(path, data, check_only):
    new_text = render_json(data)
    try:
        with open(path, "r", encoding="utf-8") as f:
            old_text = f.read()
    except FileNotFoundError:
        old_text = None

    if old_text == new_text:
        print("ok: " + path)
        return False

    if check_only:
        print("needs update: " + path, file=sys.stderr)
        return True

    directory = os.path.dirname(path)
    fd, tmp_path = tempfile.mkstemp(prefix=".tmp-", suffix=".json", dir=directory)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write(new_text)
        os.replace(tmp_path, path)
    except Exception:
        try:
            os.unlink(tmp_path)
        except FileNotFoundError:
            pass
        raise

    print("updated: " + path)
    return False


def main():
    if len(sys.argv) != 5:
        print(
            "usage: install-vscode-workspace-json.py "
            "SETTINGS_PATH EXTENSIONS_PATH ENDPOINT_SCRIPT CHECK_ONLY",
            file=sys.stderr,
        )
        return 2

    settings_path, extensions_path, endpoint_script, check_arg = sys.argv[1:5]
    check_only = check_arg == "true"

    try:
        settings = load_json(settings_path)
        extensions = load_json(extensions_path)
    except json.JSONDecodeError as e:
        print(f"{e.filename}: invalid JSON: {e}", file=sys.stderr)
        return 1

    if not isinstance(settings, dict):
        print(settings_path + ": expected a JSON object", file=sys.stderr)
        return 1
    if not isinstance(extensions, dict):
        print(extensions_path + ": expected a JSON object", file=sys.stderr)
        return 1

    rest_api = settings.get("rest.api")
    if not isinstance(rest_api, dict):
        rest_api = {}

    rest_api["autoStart"] = True
    rest_api["openInBrowser"] = False
    rest_api["port"] = 17817

    endpoints = rest_api.get("endpoints")
    if not isinstance(endpoints, dict):
        endpoints = {}

    unpersisted_endpoint = endpoints.get("unpersisted-documents")
    if not isinstance(unpersisted_endpoint, dict):
        unpersisted_endpoint = {}

    unpersisted_endpoint["script"] = endpoint_script
    endpoints["unpersisted-documents"] = unpersisted_endpoint
    rest_api["endpoints"] = endpoints
    settings["rest.api"] = rest_api

    recommendations = extensions.get("recommendations")
    if not isinstance(recommendations, list):
        recommendations = []
    if "mkloubert.vs-rest-api" not in recommendations:
        recommendations.append("mkloubert.vs-rest-api")
    extensions["recommendations"] = recommendations

    failed = False
    failed = update_file(settings_path, settings, check_only) or failed
    failed = update_file(extensions_path, extensions, check_only) or failed

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
