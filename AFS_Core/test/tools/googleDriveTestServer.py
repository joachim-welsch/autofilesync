#!/usr/bin/env python3
"""A stand-in for Google's login and Drive API for the tests of AutoFileSync.

Keeps the files in memory, listens on the loopback interface and prints the
chosen port as its first line. It answers what GoogleAuth.cpp and
GoogleDrive.cpp send: the consent page (which redirects at once, as a user who
agrees would), the token endpoint, and of the Drive API v3 the calls files.list,
files.create, files.update, files.get with alt=media and resumable uploads.

The paths below /__test are for the tests alone: GET /__test/files lists every
file with its path and content, PUT /__test/files/<path> lays a file into the
drive and creates its folders.

Usage: googleDriveTestServer.py [--page-size N]
"""
import argparse
import base64
import hashlib
import json
import re
import sys
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

FOLDER = "application/vnd.google-apps.folder"
CLIENT_ID = "test-client"
CLIENT_SECRET = "test-secret"
REFRESH_TOKEN = "refresh-ok"
QUERY = re.compile(r"^'((?:[^'\\]|\\.)*)' in parents and trashed = false(?: and name = '((?:[^'\\]|\\.)*)')?$")


def unquoted(text):
    return re.sub(r"\\(.)", r"\1", text)


class Drive:
    def __init__(self):
        self.files = {}
        self.counter = 0
        self.sessions = {}
        self.access_tokens = set()
        self.challenges = {}

    def new_id(self):
        self.counter += 1
        return "id%d" % self.counter

    def create(self, name, parent, mime="application/octet-stream", content=b"", modified="2026-01-01T00:00:00.000Z"):
        file = {"id": self.new_id(), "name": name, "mimeType": mime, "parents": [parent], "trashed": False,
                "modifiedTime": modified, "content": content}
        self.files[file["id"]] = file
        return file

    def children(self, parent, name=None):
        return [f for f in self.files.values()
                if parent in f["parents"] and not f["trashed"] and (name is None or f["name"] == name)]

    def path_of(self, file):
        parent = file["parents"][0]
        return file["name"] if parent == "root" else self.path_of(self.files[parent]) + "/" + file["name"]

    def public(self, file):
        shown = {key: file[key] for key in ("id", "name", "mimeType", "modifiedTime")}
        if file["mimeType"] != FOLDER:
            shown["size"] = str(len(file["content"]))
        return shown

    def put(self, path, content):
        parent = "root"
        parts = [part for part in path.split("/") if part]
        for name in parts[:-1]:
            found = [f for f in self.children(parent, name) if f["mimeType"] == FOLDER]
            parent = (found[0] if found else self.create(name, parent, FOLDER))["id"]
        return self.create(parts[-1], parent, content=content)


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
        self.reply(status, json.dumps(value).encode("utf-8"), [("Content-Type", "application/json; charset=utf-8")])

    def body(self):
        return self.rfile.read(int(self.headers.get("Content-Length", 0)))

    def parts(self):
        url = urllib.parse.urlsplit(self.path)
        return url.path, urllib.parse.parse_qs(url.query)

    def authorised(self):
        given = self.headers.get("Authorization", "")
        if given.startswith("Bearer ") and given[7:] in self.drive.access_tokens:
            return True
        self.reply_json({"error": {"code": 401, "message": "Invalid Credentials"}}, 401)
        return False

    def consent(self, query):
        if query.get("client_id", [""])[0] != CLIENT_ID or query.get("scope", [""])[0] != "https://www.googleapis.com/auth/drive.file":
            return self.reply(400, b"wrong client or scope")
        code = "code%d" % (len(self.drive.challenges) + 1)
        self.drive.challenges[code] = query.get("code_challenge", [""])[0]
        target = query["redirect_uri"][0] + "?" + urllib.parse.urlencode({"code": code, "state": query.get("state", [""])[0]})
        self.reply(302, headers=[("Location", target)])

    def token(self):
        form = urllib.parse.parse_qs(self.body().decode("utf-8"))
        field = lambda name: form.get(name, [""])[0]
        refused = lambda: self.reply_json({"error": "invalid_grant", "error_description": "Token has been expired or revoked."}, 400)
        if field("client_id") != CLIENT_ID or field("client_secret") != CLIENT_SECRET:
            return self.reply_json({"error": "invalid_client", "error_description": "Unauthorized"}, 401)
        if field("grant_type") == "authorization_code":
            challenge = self.drive.challenges.pop(field("code"), None)
            digest = hashlib.sha256(field("code_verifier").encode("ascii")).digest()
            if challenge is None or base64.urlsafe_b64encode(digest).rstrip(b"=").decode("ascii") != challenge:
                return refused()
        elif field("grant_type") != "refresh_token" or field("refresh_token") != REFRESH_TOKEN:
            return refused()
        access = "access%d" % (len(self.drive.access_tokens) + 1)
        self.drive.access_tokens.add(access)
        self.reply_json({"access_token": access, "expires_in": 3600, "refresh_token": REFRESH_TOKEN, "token_type": "Bearer"})

    def list_files(self, query):
        match = QUERY.match(query.get("q", [""])[0])
        if not match:
            return self.reply_json({"error": {"code": 400, "message": "Invalid Value"}}, 400)
        name = None if match.group(2) is None else unquoted(match.group(2))
        found = sorted(self.drive.children(unquoted(match.group(1)), name), key=lambda f: f["id"])
        size = self.server.page_size or int(query.get("pageSize", ["100"])[0])
        start = int(query.get("pageToken", ["0"])[0])
        answer = {"files": [self.drive.public(f) for f in found[start:start + size]]}
        if start + size < len(found):
            answer["nextPageToken"] = str(start + size)
        self.reply_json(answer)

    def start_upload(self, file_id):
        metadata = json.loads(self.body() or b"{}")
        session = "s%d" % (len(self.drive.sessions) + 1)
        self.drive.sessions[session] = (file_id, metadata)
        self.reply(200, headers=[("Location", "http://127.0.0.1:%d/upload/session/%s" % (self.server.server_address[1], session))])

    def finish_upload(self, session):
        file_id, metadata = self.drive.sessions.pop(session)
        content = self.body()
        if file_id is None:
            file = self.drive.create(metadata["name"], metadata["parents"][0], content=content)
        else:
            file = self.drive.files[file_id]
            file["content"] = content
        file["modifiedTime"] = metadata.get("modifiedTime", file["modifiedTime"])
        self.reply_json(self.drive.public(file))

    def do_GET(self):
        path, query = self.parts()
        if path == "/auth":
            return self.consent(query)
        if path == "/__test/files":
            return self.reply_json([{"path": self.drive.path_of(f), "trashed": f["trashed"], "folder": f["mimeType"] == FOLDER,
                                     "modifiedTime": f["modifiedTime"], "content": f["content"].decode("utf-8", "replace")}
                                    for f in self.drive.files.values()])
        if not self.authorised():
            return
        if path == "/drive/v3/files":
            return self.list_files(query)
        file = self.drive.files.get(path.rsplit("/", 1)[-1])
        if file is None or file["trashed"]:
            return self.reply_json({"error": {"code": 404, "message": "File not found"}}, 404)
        if query.get("alt") == ["media"]:
            return self.reply(200, file["content"])
        self.reply_json(self.drive.public(file))

    def do_POST(self):
        path, query = self.parts()
        if path == "/token":
            return self.token()
        if not self.authorised():
            return self.body() and None
        if path == "/upload/drive/v3/files":
            return self.start_upload(None)
        metadata = json.loads(self.body())
        file = self.drive.create(metadata["name"], metadata["parents"][0], metadata.get("mimeType", "application/octet-stream"))
        self.reply_json(self.drive.public(file))

    def do_PATCH(self):
        path, query = self.parts()
        if not self.authorised():
            return self.body() and None
        file_id = path.rsplit("/", 1)[-1]
        if file_id not in self.drive.files:
            self.body()
            return self.reply_json({"error": {"code": 404, "message": "File not found"}}, 404)
        if path.startswith("/upload/"):
            return self.start_upload(file_id)
        file = self.drive.files[file_id]
        metadata = json.loads(self.body() or b"{}")
        for key in ("name", "trashed", "modifiedTime"):
            if key in metadata:
                file[key] = metadata[key]
        for parent in query.get("removeParents", []):
            file["parents"].remove(parent)
        file["parents"] += query.get("addParents", [])
        self.reply_json(self.drive.public(file))

    def do_PUT(self):
        path, query = self.parts()
        if path.startswith("/__test/files/"):
            self.drive.put(urllib.parse.unquote(path[len("/__test/files/"):]), self.body())
            return self.reply(204)
        if path.startswith("/upload/session/"):
            return self.finish_upload(path.rsplit("/", 1)[-1])
        self.body()
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
