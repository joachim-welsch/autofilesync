#!/usr/bin/env python3
"""A stand-in for an S3 service for the tests of AutoFileSync.

Keeps buckets and objects in memory, listens on the loopback interface and
prints the chosen port as its first line. It answers the calls S3.cpp makes,
with path-style addresses: list buckets, HEAD bucket, ListObjectsV2, HEAD, GET,
PUT, DELETE and copy of an object, and multipart uploads.

Every request must carry a valid AWS Signature Version 4. The server works the
signature out itself and, when botocore is installed, a second time with
botocore, an implementation that shares no code with AutoFileSync or with this
file; a request both do not agree on is refused. GET /__test/botocore says
whether that second check is active.

The paths below /__test need no signature and are for the tests alone:
GET /__test/files lists every object, PUT /__test/files/<bucket>/<key> lays one
down.

Usage: s3TestServer.py [--page-size N]
"""
import argparse
import hashlib
import hmac
import json
import sys
import urllib.parse
from email.utils import formatdate
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from xml.sax.saxutils import escape

try:
    from botocore.auth import S3SigV4Auth
    from botocore.awsrequest import AWSRequest
    from botocore.credentials import Credentials
    HAVE_BOTOCORE = True
except ImportError:
    HAVE_BOTOCORE = False

ACCESS_KEY = "AKIATESTTESTTEST"
SECRET_KEY = "test-secret-key/with+signs"
REGION = "eu-test-1"
BUCKETS = ("sicherung", "zweiter")
LAST_MODIFIED = "2099-01-01T00:00:00.000Z"


def quote(text):
    return urllib.parse.quote(text, safe="-_.~")


def sign(key, text):
    return hmac.new(key, text.encode("utf-8"), hashlib.sha256).digest()


class Store:
    def __init__(self):
        self.objects = {bucket: {} for bucket in BUCKETS}
        self.uploads = {}
        self.counter = 0


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    @property
    def store(self):
        return self.server.store

    def reply(self, status, body=b"", headers=()):
        self.send_response(status)
        for name, value in headers:
            self.send_header(name, value)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def reply_xml(self, xml, status=200):
        self.reply(status, ('<?xml version="1.0" encoding="UTF-8"?>' + xml).encode("utf-8"), [("Content-Type", "application/xml")])

    def error(self, status, code, message):
        self.reply_xml("<Error><Code>%s</Code><Message>%s</Message></Error>" % (code, escape(message)), status)

    def body(self):
        return self.rfile.read(int(self.headers.get("Content-Length", 0)))

    def own_signature(self, credential, signed_headers):
        url = urllib.parse.urlsplit(self.path)
        pairs = urllib.parse.parse_qsl(url.query, keep_blank_values=True)
        query = "&".join("%s=%s" % (quote(k), quote(v)) for k, v in sorted(pairs))
        headers = "".join("%s:%s\n" % (name, " ".join(self.headers.get(name, "").split())) for name in signed_headers)
        canonical = "\n".join([self.command, url.path, query, headers, ";".join(signed_headers), self.headers.get("x-amz-content-sha256", "")])
        date, region, service, terminator = credential.split("/")[1:]
        scope = "/".join([date, region, service, terminator])
        to_sign = "\n".join(["AWS4-HMAC-SHA256", self.headers.get("x-amz-date", ""), scope, hashlib.sha256(canonical.encode("utf-8")).hexdigest()])
        key = sign(sign(sign(sign(("AWS4" + SECRET_KEY).encode("utf-8"), date), region), service), terminator)
        return hmac.new(key, to_sign.encode("utf-8"), hashlib.sha256).hexdigest()

    def botocore_signature(self, signed_headers):
        url = "http://%s%s" % (self.headers.get("Host"), self.path)
        request = AWSRequest(method=self.command, url=url, headers={name: self.headers.get(name) for name in signed_headers if name != "host"})
        request.context["timestamp"] = self.headers.get("x-amz-date", "")
        auth = S3SigV4Auth(Credentials(ACCESS_KEY, SECRET_KEY), "s3", REGION)
        return auth.signature(auth.string_to_sign(request, auth.canonical_request(request)), request)

    def signed(self):
        fields = dict(part.strip().split("=", 1) for part in self.headers.get("Authorization", "").replace("AWS4-HMAC-SHA256 ", "").split(",") if "=" in part)
        credential = fields.get("Credential", "")
        signed_headers = fields.get("SignedHeaders", "").split(";")
        refuse = lambda why: self.error(403, "SignatureDoesNotMatch", why) or False
        if credential.split("/")[0] != ACCESS_KEY or credential.count("/") != 4:
            self.body()
            return self.error(403, "InvalidAccessKeyId", "The access key is not known.") or False
        if credential.split("/")[2] != REGION:
            self.body()
            return self.error(400, "AuthorizationHeaderMalformed", "The region is wrong; expecting %s." % REGION) or False
        if any(name.lower().startswith("x-amz-") and name.lower() not in signed_headers for name in self.headers.keys()):
            self.body()
            return refuse("An x-amz header is not signed.")
        if fields.get("Signature") != self.own_signature(credential, signed_headers):
            self.body()
            return refuse("The signature does not match.")
        if HAVE_BOTOCORE and fields.get("Signature") != self.botocore_signature(signed_headers):
            self.body()
            return refuse("botocore computes another signature.")
        return True

    def place(self):
        path = urllib.parse.unquote(urllib.parse.urlsplit(self.path).path)
        bucket, _, key = path.lstrip("/").partition("/")
        return bucket, key

    def query(self):
        return dict(urllib.parse.parse_qsl(urllib.parse.urlsplit(self.path).query, keep_blank_values=True))

    def bucket_or_error(self, bucket):
        if bucket in self.store.objects:
            return self.store.objects[bucket]
        self.error(404, "NoSuchBucket", "The bucket does not exist.")
        return None

    def object_headers(self, item):
        headers = [("Last-Modified", formatdate(4070908800, usegmt=True)), ("ETag", '"%s"' % hashlib.md5(item["data"]).hexdigest())]
        return headers + [("x-amz-meta-" + name, value) for name, value in item["meta"].items()]

    def meta(self):
        return {name[len("x-amz-meta-"):].lower(): value for name, value in self.headers.items() if name.lower().startswith("x-amz-meta-")}

    def list_objects(self, objects, query):
        prefix = query.get("prefix", "")
        size = min(int(query.get("max-keys", "1000")), self.server.page_size or 1000)
        names = {}
        for key in sorted(objects):
            if key.startswith(prefix):
                rest = key[len(prefix):]
                names.setdefault(prefix + rest.split("/")[0] + "/" if "/" in rest else key, "/" in rest)
        listed = sorted(names)
        start = int(query.get("continuation-token", "0"))
        xml = "<ListBucketResult>"
        for name in listed[start:start + size]:
            if names[name]:
                xml += "<CommonPrefixes><Prefix>%s</Prefix></CommonPrefixes>" % escape(name)
            else:
                xml += "<Contents><Key>%s</Key><Size>%d</Size><LastModified>%s</LastModified></Contents>" % (escape(name), len(objects[name]["data"]), LAST_MODIFIED)
        more = start + size < len(listed)
        xml += "<IsTruncated>%s</IsTruncated>" % ("true" if more else "false")
        if more:
            xml += "<NextContinuationToken>%d</NextContinuationToken>" % (start + size)
        self.reply_xml(xml + "</ListBucketResult>")

    def do_GET(self):
        if self.path == "/__test/botocore":
            return self.reply(200, b"yes" if HAVE_BOTOCORE else b"no")
        if self.path == "/__test/files":
            return self.reply(200, json.dumps([{"path": bucket + "/" + key, "content": item["data"].decode("utf-8", "replace"), "meta": item["meta"],
                                                "folder": False, "modifiedTime": item["meta"].get("mtime", "")}
                                               for bucket, objects in self.store.objects.items() for key, item in objects.items()]).encode("utf-8"))
        if not self.signed():
            return
        bucket, key = self.place()
        if bucket == "":
            return self.reply_xml("<ListAllMyBucketsResult><Buckets>%s</Buckets></ListAllMyBucketsResult>" % "".join("<Bucket><Name>%s</Name></Bucket>" % name for name in BUCKETS))
        objects = self.bucket_or_error(bucket)
        if objects is None:
            return
        if key == "":
            return self.list_objects(objects, self.query())
        if key not in objects:
            return self.error(404, "NoSuchKey", "The specified key does not exist.")
        self.reply(200, objects[key]["data"], self.object_headers(objects[key]))

    def do_HEAD(self):
        if not self.signed():
            return
        bucket, key = self.place()
        if bucket not in self.store.objects or (key and key not in self.store.objects[bucket]):
            return self.reply(404)
        if key == "":
            return self.reply(200)
        item = self.store.objects[bucket][key]
        self.send_response(200)
        for name, value in self.object_headers(item):
            self.send_header(name, value)
        self.send_header("Content-Length", str(len(item["data"])))
        self.end_headers()

    def do_PUT(self):
        if self.path.startswith("/__test/files/"):
            bucket, _, key = urllib.parse.unquote(self.path[len("/__test/files/"):]).partition("/")
            self.store.objects[bucket][key] = {"data": self.body(), "meta": {}}
            return self.reply(204)
        if not self.signed():
            return
        data = self.body()
        bucket, key = self.place()
        query = self.query()
        objects = self.bucket_or_error(bucket)
        if objects is None:
            return
        if "uploadId" in query:
            upload = self.store.uploads.get(query["uploadId"])
            if upload is None:
                return self.error(404, "NoSuchUpload", "The upload does not exist.")
            upload["parts"][int(query["partNumber"])] = data
            return self.reply(200, headers=[("ETag", '"%s"' % hashlib.md5(data).hexdigest())])
        source = self.headers.get("x-amz-copy-source")
        if source is not None:
            source_bucket, _, source_key = urllib.parse.unquote(source).lstrip("/").partition("/")
            if source_key not in self.store.objects.get(source_bucket, {}):
                return self.error(404, "NoSuchKey", "The source does not exist.")
            item = self.store.objects[source_bucket][source_key]
            objects[key] = {"data": item["data"], "meta": dict(item["meta"])}
            return self.reply_xml("<CopyObjectResult><LastModified>%s</LastModified></CopyObjectResult>" % LAST_MODIFIED)
        objects[key] = {"data": data, "meta": self.meta()}
        self.reply(200, headers=[("ETag", '"%s"' % hashlib.md5(data).hexdigest())])

    def do_POST(self):
        if not self.signed():
            return
        data = self.body()
        bucket, key = self.place()
        query = self.query()
        objects = self.bucket_or_error(bucket)
        if objects is None:
            return
        if "uploads" in query:
            self.store.counter += 1
            upload_id = "upload%d" % self.store.counter
            self.store.uploads[upload_id] = {"parts": {}, "meta": self.meta()}
            return self.reply_xml("<InitiateMultipartUploadResult><UploadId>%s</UploadId></InitiateMultipartUploadResult>" % upload_id)
        upload = self.store.uploads.pop(query.get("uploadId", ""), None)
        if upload is None:
            return self.error(404, "NoSuchUpload", "The upload does not exist.")
        named = data.count(b"<PartNumber>")
        if named != len(upload["parts"]) or any(hashlib.md5(part).hexdigest().encode() not in data for part in upload["parts"].values()):
            return self.error(400, "InvalidPart", "The parts named do not match the parts sent.")
        objects[key] = {"data": b"".join(upload["parts"][number] for number in sorted(upload["parts"])), "meta": upload["meta"]}
        self.reply_xml("<CompleteMultipartUploadResult><Key>%s</Key></CompleteMultipartUploadResult>" % escape(key))

    def do_DELETE(self):
        if not self.signed():
            return
        bucket, key = self.place()
        query = self.query()
        if "uploadId" in query:
            self.store.uploads.pop(query["uploadId"], None)
            return self.reply(204)
        objects = self.bucket_or_error(bucket)
        if objects is None:
            return
        objects.pop(key, None)
        self.reply(204)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--page-size", type=int, default=0)
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    server.store = Store()
    server.page_size = args.page_size
    print(server.server_address[1], flush=True)
    server.serve_forever()


if __name__ == "__main__":
    sys.exit(main())
