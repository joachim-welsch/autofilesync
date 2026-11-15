#!/usr/bin/env python3
"""A stand-in for Dropbox's login and the files part of the Dropbox API v2
for the tests of AutoFileSync.

Keeps the files in memory, listens on the loopback interface and prints the
chosen port as its first line. It answers what GoogleAuth.cpp sends for a
Dropbox account and what Dropbox.cpp sends: the consent page (which redirects
at once, to one of the registered ports only), the token endpoint, the RPC
calls below /2/files and the content calls below /content/2/files. Like
Dropbox it ignores the case of a path but keeps the case of a name, and it
refuses a Dropbox-API-Arg header that is not plain ASCII.

The paths below /__test are for the tests alone: GET /__test/files lists every
file with its path and content, PUT /__test/files/<path> lays a file into the
drive and creates its folders.

Usage: dropboxTestServer.py [--page-size N]
"""
import argparse
import base64
import hashlib
import json
import sys
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

APP_KEY = "test-app-key"
REFRESH_TOKEN = "refresh-ok"
REDIRECTS = ["http://localhost:%d" % port for port in (53682, 53683, 53684)]
EPOCH = "2026-01-01T00:00:00Z"


class Conflict(Exception):
    pass


class Drive:
    def __init__(self):
        self.items = {"": {"name": "", "folder": True, "content": b"", "modified": EPOCH}}
        self.sessions = {}
        self.cursors = {}
        self.access_tokens = set()
        self.challenges = {}
        self.counter = 0

    def next(self, prefix):
        self.counter += 1
        return "%s%d" % (prefix, self.counter)

    def children(self, key):
        prefix = key + "/"
        return sorted(k for k in self.items if k.startswith(prefix) and "/" not in k[len(prefix):])

    def put(self, path, content, modified=EPOCH, folder=False):
        key = path.lower()
        parent = key.rsplit("/", 1)[0]
        if parent not in self.items or not self.items[parent]["folder"]:
            raise Conflict("path/not_found/")
        self.items[key] = {"name": path.rsplit("/", 1)[-1], "folder": folder, "content": content, "modified": modified}
        return key

    def put_with_parents(self, path, content):
        parts = path.split("/")[1:]
        for depth in range(1, len(parts)):
            folder = "/" + "/".join(parts[:depth])
            if folder.lower() not in self.items:
                self.put(folder, b"", folder=True)
        self.put(path, content)

    def metadata(self, key):
        item = self.items[key]
        if item["folder"]:
            return {".tag": "folder", "name": item["name"], "path_lower": key}
        return {".tag": "file", "name": item["name"], "path_lower": key, "size": len(item["content"]),
                "client_modified": item["modified"], "server_modified": "2099-01-01T00:00:00Z"}


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    @property
    def drive(self):
        return self.server.drive

    def reply(self, status, body=b"", headers=()):
        self.send_response(status)
        for name, value in headers:
            self.send_header(name, value)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def reply_json(self, value, status=200):
        self.reply(status, json.dumps(value).encode("utf-8"), [("Content-Type", "application/json")])

    def conflict(self, summary):
        self.reply_json({"error_summary": summary, "error": {".tag": summary.split("/")[0]}}, 409)

    def body(self):
        return self.rfile.read(int(self.headers.get("Content-Length", 0)))

    def authorised(self):
        given = self.headers.get("Authorization", "")
        return given.startswith("Bearer ") and given[7:] in self.drive.access_tokens

    def header_argument(self):
        raw = self.headers.get("Dropbox-API-Arg", "{}")
        if any(ord(char) > 127 for char in raw):
            return None
        return json.loads(raw)

    def consent(self, query):
        field = lambda name: query.get(name, [""])[0]
        if field("client_id") != APP_KEY or field("token_access_type") != "offline":
            return self.reply(400, b"wrong app or token type")
        if field("redirect_uri") not in REDIRECTS:
            return self.reply(400, b"redirect_uri is not registered")
        code = self.drive.next("code")
        self.drive.challenges[code] = field("code_challenge")
        self.reply(302, headers=[("Location", field("redirect_uri") + "?" + urllib.parse.urlencode({"code": code, "state": field("state")}))])

    def token(self):
        form = urllib.parse.parse_qs(self.body().decode("utf-8"))
        field = lambda name: form.get(name, [""])[0]
        refused = lambda: self.reply_json({"error": "invalid_grant", "error_description": "refresh token is invalid or revoked"}, 400)
        if field("client_id") != APP_KEY:
            return self.reply_json({"error": "invalid_client", "error_description": "unknown app"}, 401)
        answer = {"expires_in": 14400, "token_type": "bearer"}
        if field("grant_type") == "authorization_code":
            challenge = self.drive.challenges.pop(field("code"), None)
            digest = hashlib.sha256(field("code_verifier").encode("ascii")).digest()
            if challenge is None or base64.urlsafe_b64encode(digest).rstrip(b"=").decode("ascii") != challenge:
                return refused()
            answer["refresh_token"] = REFRESH_TOKEN
        elif field("grant_type") != "refresh_token" or field("refresh_token") != REFRESH_TOKEN:
            return refused()
        answer["access_token"] = self.drive.next("access")
        self.drive.access_tokens.add(answer["access_token"])
        self.reply_json(answer)

    def page(self, keys, start):
        size = self.server.page_size or 2000
        answer = {"entries": [self.drive.metadata(key) for key in keys[start:start + size]], "has_more": start + size < len(keys)}
        answer["cursor"] = self.drive.next("cursor")
        self.drive.cursors[answer["cursor"]] = (keys, start + size)
        return answer

    def rpc(self, call, argument):
        drive = self.drive
        key = argument.get("path", "").lower()
        if call == "list_folder/continue":
            return self.page(*drive.cursors.pop(argument["cursor"]))
        if call == "create_folder_v2":
            if key in drive.items:
                raise Conflict("path/conflict/folder/" if drive.items[key]["folder"] else "path/conflict/file/")
            return {"metadata": drive.metadata(drive.put(argument["path"], b"", folder=True))}
        if call == "move_v2":
            old, new = argument["from_path"].lower(), argument["to_path"].lower()
            if old not in drive.items:
                raise Conflict("from_lookup/not_found/")
            if new in drive.items:
                raise Conflict("to/conflict/file/")
            if new.rsplit("/", 1)[0] not in drive.items:
                raise Conflict("to/not_found/")
            for known in [k for k in drive.items if k == old or k.startswith(old + "/")]:
                drive.items[new + known[len(old):]] = drive.items.pop(known)
            drive.items[new]["name"] = argument["to_path"].rsplit("/", 1)[-1]
            return {"metadata": drive.metadata(new)}
        if key not in drive.items or (key == "" and call != "list_folder"):
            raise Conflict("path/not_found/" if call != "delete_v2" else "path_lookup/not_found/")
        if call == "get_metadata":
            return drive.metadata(key)
        if call == "list_folder":
            return self.page(drive.children(key), 0)
        if call == "delete_v2":
            metadata = drive.metadata(key)
            for known in [k for k in drive.items if k == key or k.startswith(key + "/")]:
                del drive.items[known]
            return {"metadata": metadata}
        raise Conflict("unknown/")

    def content(self, call, argument, data):
        drive = self.drive
        if call == "upload":
            return drive.metadata(drive.put(argument["path"], data, argument.get("client_modified", EPOCH)))
        if call == "upload_session/start":
            session = drive.next("session")
            drive.sessions[session] = data
            return {"session_id": session}
        cursor = argument["cursor"]
        if cursor["session_id"] not in drive.sessions or cursor["offset"] != len(drive.sessions[cursor["session_id"]]):
            raise Conflict("lookup_failed/incorrect_offset/")
        if call == "upload_session/append_v2":
            drive.sessions[cursor["session_id"]] += data
            return None
        commit = argument["commit"]
        whole = drive.sessions.pop(cursor["session_id"]) + data
        return drive.metadata(drive.put(commit["path"], whole, commit.get("client_modified", EPOCH)))

    def do_GET(self):
        url = urllib.parse.urlsplit(self.path)
        if url.path == "/oauth2/authorize":
            return self.consent(urllib.parse.parse_qs(url.query))
        if url.path == "/__test/files":
            return self.reply_json([{"path": key[1:].rsplit("/", 1)[0] + "/" + item["name"] if "/" in key[1:] else item["name"],
                                     "folder": item["folder"], "modifiedTime": item["modified"],
                                     "content": item["content"].decode("utf-8", "replace")} for key, item in self.drive.items.items() if key])
        if url.path == "/content/2/files/download":
            argument = self.header_argument()
            if not self.authorised():
                return self.reply_json({"error_summary": "invalid_access_token/"}, 401)
            if argument is None:
                return self.reply(400, b"Dropbox-API-Arg is not ASCII")
            item = self.drive.items.get(argument.get("path", "").lower())
            if item is None or item["folder"]:
                return self.conflict("path/not_found/")
            return self.reply(200, item["content"])
        self.reply(404)

    def do_POST(self):
        path = urllib.parse.urlsplit(self.path).path
        data = self.body()
        if path == "/oauth2/token":
            self.rfile = __import__("io").BytesIO(data)
            self.headers.replace_header("Content-Length", str(len(data)))
            return self.token()
        if not self.authorised():
            return self.reply_json({"error_summary": "invalid_access_token/", "error": {".tag": "invalid_access_token"}}, 401)
        try:
            if path.startswith("/2/files/"):
                return self.reply_json(self.rpc(path[len("/2/files/"):], json.loads(data or b"{}")))
            if path.startswith("/content/2/files/"):
                argument = self.header_argument()
                if argument is None:
                    return self.reply(400, b"Dropbox-API-Arg is not ASCII")
                return self.reply_json(self.content(path[len("/content/2/files/"):], argument, data))
        except Conflict as conflict:
            return self.conflict(str(conflict))
        self.reply(404)

    def do_PUT(self):
        path = urllib.parse.unquote(urllib.parse.urlsplit(self.path).path)
        data = self.body()
        if path.startswith("/__test/files/"):
            self.drive.put_with_parents("/" + path[len("/__test/files/"):], data)
            return self.reply(204)
        self.reply(404)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--page-size", type=int, default=0)
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    server.drive = Drive()
    server.page_size = args.page_size
    print(server.server_address[1], flush=True)
    server.serve_forever()


if __name__ == "__main__":
    sys.exit(main())
