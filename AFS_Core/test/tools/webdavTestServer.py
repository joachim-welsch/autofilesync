#!/usr/bin/env python3
"""A small WebDAV server for the integration tests of AFS_Core.

Serves one directory over HTTP on the loopback interface and prints the chosen
port as its first line. It speaks just what WebDav.cpp uses: PROPFIND with
depth 0 and 1, GET, PUT, MKCOL, DELETE and MOVE, with Basic authentication.
With --keep-mtime it honours the X-OC-Mtime header the way Nextcloud does;
without it an uploaded file gets the time of the upload, like a plain server.

Usage: webdavTestServer.py ROOT [--keep-mtime] [--user NAME --password SECRET]
"""
import argparse
import base64
import email.utils
import os
import shutil
import sys
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from xml.sax.saxutils import escape


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def local(self, url_path=None):
        path = urllib.parse.unquote(urllib.parse.urlsplit(url_path or self.path).path)
        parts = [part for part in path.split("/") if part not in ("", ".")]
        if ".." in parts:
            return None
        return os.path.join(self.server.root, *parts)

    def reply(self, status, body=b"", headers=()):
        self.send_response(status)
        for name, value in headers:
            self.send_header(name, value)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def body(self):
        return self.rfile.read(int(self.headers.get("Content-Length", 0)))

    def authorised(self):
        expected = self.server.credentials
        if expected is None:
            return True
        given = self.headers.get("Authorization", "")
        if given == "Basic " + base64.b64encode(expected.encode()).decode():
            return True
        self.body()
        self.reply(401, headers=[("WWW-Authenticate", 'Basic realm="test"')])
        return False

    def target(self):
        if not self.authorised():
            return None
        path = self.local()
        if path is None:
            self.body()
            self.reply(403)
        return path

    def properties(self, path, href):
        info = os.stat(path)
        is_dir = os.path.isdir(path)
        if is_dir and not href.endswith("/"):
            href += "/"
        kind = "<D:collection/>" if is_dir else ""
        length = "" if is_dir else "<D:getcontentlength>%d</D:getcontentlength>" % info.st_size
        modified = email.utils.formatdate(int(info.st_mtime), usegmt=True)
        return ("<D:response><D:href>%s</D:href><D:propstat><D:prop><D:resourcetype>%s</D:resourcetype>%s"
                "<D:getlastmodified>%s</D:getlastmodified></D:prop><D:status>HTTP/1.1 200 OK</D:status>"
                "</D:propstat></D:response>") % (escape(href), kind, length, modified)

    def do_PROPFIND(self):
        path = self.target()
        if path is None:
            return
        self.body()
        if not os.path.exists(path):
            return self.reply(404)
        href = urllib.parse.urlsplit(self.path).path
        items = [self.properties(path, href)]
        if os.path.isdir(path) and self.headers.get("Depth", "1") != "0":
            for name in sorted(os.listdir(path)):
                items.append(self.properties(os.path.join(path, name), href.rstrip("/") + "/" + urllib.parse.quote(name)))
        xml = '<?xml version="1.0" encoding="utf-8"?><D:multistatus xmlns:D="DAV:">%s</D:multistatus>' % "".join(items)
        self.reply(207, xml.encode("utf-8"), [("Content-Type", "application/xml; charset=utf-8")])

    def do_GET(self):
        path = self.target()
        if path is None:
            return
        if not os.path.isfile(path):
            return self.reply(404)
        with open(path, "rb") as source:
            data = source.read()
        self.reply(200, data, [("Last-Modified", email.utils.formatdate(int(os.stat(path).st_mtime), usegmt=True))])

    def do_PUT(self):
        path = self.target()
        if path is None:
            return
        data = self.body()
        if not os.path.isdir(os.path.dirname(path)):
            return self.reply(409)
        if os.path.isdir(path):
            return self.reply(405)
        created = not os.path.exists(path)
        with open(path, "wb") as target:
            target.write(data)
        headers = []
        mtime = self.headers.get("X-OC-Mtime")
        if self.server.keep_mtime and mtime is not None:
            os.utime(path, (int(mtime), int(mtime)))
            headers.append(("X-OC-Mtime", "accepted"))
        self.reply(201 if created else 204, headers=headers)

    def do_MKCOL(self):
        path = self.target()
        if path is None:
            return
        self.body()
        if os.path.exists(path):
            return self.reply(405)
        if not os.path.isdir(os.path.dirname(path)):
            return self.reply(409)
        os.mkdir(path)
        self.reply(201)

    def do_DELETE(self):
        path = self.target()
        if path is None:
            return
        self.body()
        if not os.path.exists(path):
            return self.reply(404)
        if os.path.isdir(path):
            shutil.rmtree(path)
        else:
            os.remove(path)
        self.reply(204)

    def do_MOVE(self):
        path = self.target()
        if path is None:
            return
        self.body()
        destination = self.local(self.headers.get("Destination", ""))
        if destination is None or not os.path.exists(path):
            return self.reply(404)
        if os.path.exists(destination) and self.headers.get("Overwrite", "T") == "F":
            return self.reply(412)
        os.replace(path, destination)
        self.reply(201)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("root")
    parser.add_argument("--keep-mtime", action="store_true")
    parser.add_argument("--user")
    parser.add_argument("--password", default="")
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    server.root = os.path.abspath(args.root)
    server.keep_mtime = args.keep_mtime
    server.credentials = None if args.user is None else args.user + ":" + args.password
    print(server.server_address[1], flush=True)
    server.serve_forever()


if __name__ == "__main__":
    sys.exit(main())
