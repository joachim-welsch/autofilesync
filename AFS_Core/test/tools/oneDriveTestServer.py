#!/usr/bin/env python3
"""A stand-in for Microsoft's login and the OneDrive part of Microsoft Graph
for the tests of AutoFileSync.

Keeps the files in memory, listens on the loopback interface and prints the
chosen port as its first line. It answers what GoogleAuth.cpp sends for a
Microsoft account and what OneDrive.cpp sends: the consent page (which
redirects at once), the token endpoint (which hands out a new refresh token
with every access token, as Microsoft does), and below /me/drive/root the
item, children, content, createUploadSession, PATCH and DELETE calls with
path addressing.

The paths below /__test are for the tests alone: GET /__test/files lists every
file with its path and content, PUT /__test/files/<path> lays a file into the
drive and creates its folders.

Usage: oneDriveTestServer.py [--page-size N]
"""
import argparse
import base64
import hashlib
import json
import sys
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

CLIENT_ID = "test-client"
ROOT = "/me/drive/root"
FACETS = ("children", "content", "createUploadSession")


class Drive:
    def __init__(self):
        self.items = {"": {"folder": True, "content": b"", "modified": "2026-01-01T00:00:00Z"}}
        self.sessions = {}
        self.downloads = {}
        self.access_tokens = set()
        self.refresh_tokens = {"refresh-ok"}
        self.challenges = {}
        self.counter = 0

    def next(self, prefix):
        self.counter += 1
        return "%s%d" % (prefix, self.counter)

    def children(self, path):
        prefix = path + "/"
        return sorted(p for p in self.items if p.startswith(prefix) and "/" not in p[len(prefix):])

    def make_parents(self, path):
        parts = path.split("/")[1:-1]
        for depth in range(1, len(parts) + 1):
            self.items.setdefault("/" + "/".join(parts[:depth]), {"folder": True, "content": b"", "modified": "2026-01-01T00:00:00Z"})

    def remove(self, path):
        for known in [p for p in self.items if p == path or p.startswith(path + "/")]:
            del self.items[known]

    def rename(self, old, new):
        for known in [p for p in self.items if p == old or p.startswith(old + "/")]:
            self.items[new + known[len(old):]] = self.items.pop(known)


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    @property
    def drive(self):
        return self.server.drive

    def base(self):
        return "http://127.0.0.1:%d" % self.server.server_address[1]

    def reply(self, status, body=b"", headers=()):
        self.send_response(status)
        for name, value in headers:
            self.send_header(name, value)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def reply_json(self, value, status=200):
        self.reply(status, json.dumps(value).encode("utf-8"), [("Content-Type", "application/json; charset=utf-8")])

    def error(self, status, code):
        self.reply_json({"error": {"code": code, "message": code}}, status)

    def body(self):
        return self.rfile.read(int(self.headers.get("Content-Length", 0)))

    def parts(self):
        url = urllib.parse.urlsplit(self.path)
        return urllib.parse.unquote(url.path), urllib.parse.parse_qs(url.query)

    def authorised(self):
        given = self.headers.get("Authorization", "")
        if given.startswith("Bearer ") and given[7:] in self.drive.access_tokens:
            return True
        self.body()
        self.error(401, "InvalidAuthenticationToken")
        return False

    def address(self, path):
        """The item path and the facet of a Graph address below /me/drive/root, or None."""
        if not path.startswith(ROOT):
            return None
        rest = path[len(ROOT):]
        if rest == "":
            return "", ""
        if rest.startswith("/") and rest[1:] in FACETS:
            return "", rest[1:]
        if not rest.startswith(":/"):
            return None
        item, _, facet = rest[1:].partition(":/")
        if facet == "" and item.endswith(":"):
            item = item[:-1]
        return item.rstrip("/"), facet

    def public(self, path, with_download=False):
        item = self.drive.items[path]
        shown = {"name": path.rsplit("/", 1)[-1] if path else "root", "lastModifiedDateTime": "2099-01-01T00:00:00Z",
                 "fileSystemInfo": {"lastModifiedDateTime": item["modified"]}}
        if item["folder"]:
            shown["folder"] = {"childCount": len(self.drive.children(path))}
        else:
            shown["size"] = len(item["content"])
            shown["file"] = {}
            if with_download:
                ticket = self.drive.next("d")
                self.drive.downloads[ticket] = path
                shown["@microsoft.graph.downloadUrl"] = self.base() + "/download/" + ticket
        return shown

    def consent(self, query):
        scope = query.get("scope", [""])[0].split()
        if query.get("client_id", [""])[0] != CLIENT_ID or "Files.ReadWrite" not in scope or "offline_access" not in scope:
            return self.reply(400, b"wrong client or scope")
        if not query.get("redirect_uri", [""])[0].startswith("http://localhost:"):
            return self.reply(400, b"redirect not registered")
        code = self.drive.next("code")
        self.drive.challenges[code] = query.get("code_challenge", [""])[0]
        target = query["redirect_uri"][0] + "?" + urllib.parse.urlencode({"code": code, "state": query.get("state", [""])[0]})
        self.reply(302, headers=[("Location", target)])

    def token(self):
        form = urllib.parse.parse_qs(self.body().decode("utf-8"))
        field = lambda name: form.get(name, [""])[0]
        refused = lambda: self.reply_json({"error": "invalid_grant", "error_description": "AADSTS70000: The refresh token has expired."}, 400)
        if field("client_id") != CLIENT_ID or "client_secret" in form:
            return self.reply_json({"error": "invalid_client", "error_description": "AADSTS700025: Client is public."}, 401)
        if field("grant_type") == "authorization_code":
            challenge = self.drive.challenges.pop(field("code"), None)
            digest = hashlib.sha256(field("code_verifier").encode("ascii")).digest()
            if challenge is None or base64.urlsafe_b64encode(digest).rstrip(b"=").decode("ascii") != challenge:
                return refused()
        elif field("grant_type") != "refresh_token" or field("refresh_token") not in self.drive.refresh_tokens:
            return refused()
        access = self.drive.next("access")
        refresh = self.drive.next("refresh")
        self.drive.access_tokens.add(access)
        self.drive.refresh_tokens.add(refresh)
        self.reply_json({"access_token": access, "expires_in": 3600, "refresh_token": refresh, "token_type": "Bearer"})

    def list_children(self, path, query):
        names = self.drive.children(path)
        size = self.server.page_size or 200
        start = int(query.get("$skiptoken", ["0"])[0])
        answer = {"value": [self.public(p) for p in names[start:start + size]]}
        if start + size < len(names):
            answer["@odata.nextLink"] = "%s/me/drive/root%s/children?$skiptoken=%d" % (
                self.base(), (":" + urllib.parse.quote(path) + ":") if path else "", start + size)
        self.reply_json(answer)

    def do_GET(self):
        path, query = self.parts()
        if path == "/authorize":
            return self.consent(query)
        if path == "/__test/files":
            return self.reply_json([{"path": p[1:], "folder": i["folder"], "modifiedTime": i["modified"],
                                     "content": i["content"].decode("utf-8", "replace")} for p, i in self.drive.items.items() if p])
        if path.startswith("/download/"):
            item = self.drive.items.get(self.drive.downloads.get(path.rsplit("/", 1)[-1]))
            return self.reply(200, item["content"]) if item else self.reply(404)
        if not self.authorised():
            return
        where = self.address(path)
        if where is None or where[0] not in self.drive.items:
            return self.error(404, "itemNotFound")
        if where[1] == "children":
            return self.list_children(where[0], query)
        self.reply_json(self.public(where[0], "$select" not in query))

    def do_POST(self):
        path, query = self.parts()
        if path == "/token":
            return self.token()
        if not self.authorised():
            return
        where = self.address(path)
        body = json.loads(self.body() or b"{}")
        if where is None:
            return self.error(400, "invalidRequest")
        if where[1] == "createUploadSession":
            if where[0].rsplit("/", 1)[0] not in self.drive.items:
                return self.error(404, "itemNotFound")
            session = self.drive.next("s")
            self.drive.sessions[session] = {"path": where[0], "data": b"", "modified": body["item"]["fileSystemInfo"]["lastModifiedDateTime"]}
            return self.reply_json({"uploadUrl": self.base() + "/upload/" + session})
        if where[1] == "children" and "folder" in body:
            if where[0] not in self.drive.items:
                return self.error(404, "itemNotFound")
            new = where[0] + "/" + body["name"]
            if new in self.drive.items:
                return self.error(409, "nameAlreadyExists")
            self.drive.items[new] = {"folder": True, "content": b"", "modified": "2026-01-01T00:00:00Z"}
            return self.reply_json(self.public(new), 201)
        self.error(400, "invalidRequest")

    def do_PUT(self):
        path, query = self.parts()
        if path.startswith("/__test/files/"):
            item = "/" + path[len("/__test/files/"):]
            self.drive.make_parents(item)
            self.drive.items[item] = {"folder": False, "content": self.body(), "modified": "2026-01-01T00:00:00Z"}
            return self.reply(204)
        if path.startswith("/upload/"):
            return self.upload_chunk(path.rsplit("/", 1)[-1])
        if not self.authorised():
            return
        where = self.address(path)
        data = self.body()
        if where is None or where[1] != "content" or where[0].rsplit("/", 1)[0] not in self.drive.items:
            return self.error(404, "itemNotFound")
        self.drive.items[where[0]] = {"folder": False, "content": data, "modified": "2099-01-01T00:00:00Z"}
        self.reply_json(self.public(where[0]), 201)

    def upload_chunk(self, name):
        session = self.drive.sessions.get(name)
        data = self.body()
        if session is None:
            return self.error(404, "itemNotFound")
        span, total = self.headers.get("Content-Range", "bytes 0-0/0")[6:].split("/")
        first, last = (int(n) for n in span.split("-"))
        if first != len(session["data"]) or last - first + 1 != len(data):
            return self.error(416, "invalidRange")
        session["data"] += data
        if len(session["data"]) < int(total):
            return self.reply_json({"nextExpectedRanges": ["%d-" % len(session["data"])]}, 202)
        del self.drive.sessions[name]
        self.drive.items[session["path"]] = {"folder": False, "content": session["data"], "modified": session["modified"]}
        self.reply_json(self.public(session["path"]), 201)

    def do_PATCH(self):
        path, query = self.parts()
        if not self.authorised():
            return
        where = self.address(path)
        body = json.loads(self.body() or b"{}")
        if where is None or where[0] not in self.drive.items or where[0] == "":
            return self.error(404, "itemNotFound")
        item = where[0]
        if "fileSystemInfo" in body:
            self.drive.items[item]["modified"] = body["fileSystemInfo"]["lastModifiedDateTime"]
        parent = body.get("parentReference", {}).get("path")
        new_parent = item.rsplit("/", 1)[0] if parent is None else parent[len("/drive/root:"):]
        new = new_parent + "/" + body.get("name", item.rsplit("/", 1)[-1])
        if new != item:
            if new in self.drive.items:
                return self.error(409, "nameAlreadyExists")
            if new_parent not in self.drive.items:
                return self.error(404, "itemNotFound")
            self.drive.rename(item, new)
        self.reply_json(self.public(new))

    def do_DELETE(self):
        path, query = self.parts()
        if not self.authorised():
            return
        where = self.address(path)
        if where is None or where[0] not in self.drive.items or where[0] == "":
            return self.error(404, "itemNotFound")
        self.drive.remove(where[0])
        self.reply(204)


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
