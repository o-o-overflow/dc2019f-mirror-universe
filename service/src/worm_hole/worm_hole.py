#!/usr/bin/env python3

import argparse
import collections
import datetime
import logging
import multiprocessing
import os
import socketserver

import ipdb

import chaoslib

l = logging.getLogger("worm-hole")

HTTPReq = collections.namedtuple('HTTPReq', ['method', 'uri', 'version', 'headers', 'body'])

CADR_ADDR = 0o401
OUR_ADDR = 0o406
SERVICE_NAME = b"HTTP"

chaos_lock = multiprocessing.Semaphore(4)

def parse_http_req(f):
    """
    Return an HTTPReq if request was valid, otherwise return None if request was invalid
    """
    header_line = f.readline().rstrip()
    l.debug(f"Read line {header_line}")
    
    header = header_line.split(b' ')
    if len(header) != 3:
        l.warning(f"received invalid start header line {header_line}")
        return None

    method = header[0]
    uri = header[1]
    version = header[2]

    l.info(f"parsed the {method} {uri} {version}")

    # TODO: verify that the URI can be URI decoded
    headers = collections.OrderedDict()

    while True:
        line = f.readline().rstrip()
        if not line:
            break

        header_parts = line.split(b':', maxsplit=1)
        if len(header_parts) != 2:
            l.warning(f"got a bad header {line}, ignoring the whole request.")
            return None

        name = header_parts[0]
        value = header_parts[1].strip()
        l.info(f"got a new header {name}: {value}")

        lower_name = name.decode('utf-8').lower()

        headers[lower_name] = (name, value)

    if not headers:
        l.warning(f"Did not get any headers")
        return None

    # According to the spec, if we don't have a content-length header,
    # then there is no body

    body = b""

    content_length = headers.get('content-length')
    if content_length:
        actual_content_length = content_length[1]
        try:
            length = int(actual_content_length)
        except:
            l.warning(f"invalid content length {actual_content_length}")
            return None

        if length < 0:
            l.warning(f"content length was negative {length}")
            return None

        to_read = length
        done = (length == 0)
        while not done:
            data = f.read(to_read)
            if not data:
                done = True
            else:
                body += data
                to_read -= len(data)
                if to_read <= 0:
                    done = True
    else:
         l.info("no content length, not reading any more.")

    return HTTPReq(method, uri, version, headers, body)

def http_response(http_code, reason, body=b"", content_type=b"text/html"):
    response = b"HTTP/1.1 " + http_code + b" " + reason + b"\r\n"

    if body:
        response += b"Content-Length: " + bytes(str(len(body)), 'utf-8') + b"\r\n\r\n"
        response += body
    else:
        response += b"\r\n"

    return response

# High level of what we want to do:

# Create an http server to listen on a specific port
# Whenever you get a new connection, verify that the request is valid
# (do some sanity checking so that we don't have to do it in lisp.
# Then, for each valid request, make a chaos connection to 0401 on the "HTTP" service
# And send back the HTTP response

class WormHoleTCPHandler(socketserver.StreamRequestHandler):

    def handle(self):
        start = datetime.datetime.now()
        l.info(f"Got new http request")
        req = parse_http_req(self.rfile)
        if not req:
            l.info(f"invalid request, sending a 400")
            self.request.sendall(http_response(b"400", b"Bad request", b"Your request did not survive into the worm hole."))
            return

        # request was good, send it to the chaoslib and see what they say
        status_line = req.method + b" " + req.uri + b" " + req.version + chaoslib.cadr_return

        headers = b""
        for lower_name, (name, value) in req.headers.items():
            headers += name + b": " + value + chaoslib.cadr_return

        body = req.body

        # Check, if it was a GET request with something in the ./static directory, send them that
        if req.method == b"GET":
            if not b'..' in req.uri:
                potential_file = b'static' + req.uri
                if os.path.isfile(potential_file):
                    l.info(f"GET request for {potential_file}, going to serve that from here")
                    with open(potential_file, "rb") as f:
                        self.request.sendall(http_response(b"200", b"OK", f.read()))
                    return
            else:
                l.info(f"tried a LFI exploit {req.uri}, not going to fall for it")

        #self.request.sendall(http_response(b"200", b"OK", b"hello world"))
        with chaos_lock:
            l.info(f"connecting to cadr")
            connection = chaoslib.ChaosStream(OUR_ADDR, CADR_ADDR, SERVICE_NAME)

            chaos_http_req = status_line + headers + chaoslib.cadr_return + body

            l.debug(f"sending {chaos_http_req}")
            connection.writeall(chaos_http_req)

            response = chaoslib.replace_cadr_return_with_newlines(connection.readall())
            l.debug(f"received {response}")
            connection.close()
            
        self.request.sendall(response)
        end = datetime.datetime.now()
        diff = end - start
        l.info(f"responded to request in {diff.total_seconds()} seconds")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog="worm_hole")
    parser.add_argument("--debug", default=False, action="store_true", help="Enable debugging")
    parser.add_argument("--port", type=int, default=7159, help="Port to listen on [default: 7159]")
    parser.add_argument("--host", default='127.0.0.1', help="Host to listen on [default: 127.0.0.1]")
    parser.add_argument("--version", action="version", version="%(prog)s v0.0.1")
    args = parser.parse_args()
    if args.debug:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.INFO)
#    with socketserver.TCPServer((args.host, args.port), WormHoleTCPHandler) as server:    
    with socketserver.ForkingTCPServer((args.host, args.port), WormHoleTCPHandler) as server:
        l.info(f"starting the wormhole {args.host} {args.port}")
        server.serve_forever()
