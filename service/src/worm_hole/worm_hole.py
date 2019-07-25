#!/usr/bin/env python3

import argparse
import collections
import logging
import os
import socketserver

import ipdb

import chaoslib

HTTPReq = collections.namedtuple('HTTPReq', ['method', 'uri', 'version', 'headers', 'body'])

CADR_ADDR = 0o401
OUR_ADDR = 0o406
SERVICE_NAME = b"HTTP"

def parse_http_req(f):
    """
    Return an HTTPReq if request was valid, otherwise return None if request was invalid
    """
    header_line = f.readline().rstrip()
    header = header_line.split(b' ')
    if len(header) != 3:
        return None

    method = header[0]
    uri = header[1]
    version = header[2]

    # TODO: verify that the URI can be URI decoded
    headers = collections.OrderedDict()

    while True:
        line = f.readline().rstrip()
        if not line:
            break

        header_parts = line.split(b':', maxsplit=1)
        if len(header_parts) != 2:
            return None

        name = header_parts[0]
        value = header_parts[1].strip()

        lower_name = name.decode('utf-8').lower()

        headers[lower_name] = (name, value)

    if not headers:
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
            return None

        if length <= 0:
            return None

        to_read = length
        done = False
        while not done:
            data = f.read(to_read)
            if not data:
                done = True
            else:
                body += data
                to_read -= len(data)
                if to_read <= 0:
                    done = True

    return HTTPReq(method, uri, version, headers, body)

def http_response(http_code, reason, body = b""):
    return b"WHAT"

# High level of what we want to do:

# Create an http server to listen on a specific port
# Whenever you get a new connection, verify that the request is valid
# (do some sanity checking so that we don't have to do it in lisp.
# Then, for each valid request, make a chaos connection to 0401 on the "HTTP" service
# And send back the HTTP response

class WormHoleTCPHandler(socketserver.StreamRequestHandler):

    def handle(self):
        req = parse_http_req(self.rfile)
        if not req:
            self.request.sendall(http_response(400, b"Your request did not survive into the worm hole."))
            return

        # request was good, send it to the chaoslib and see what they say
        status_line = req.method + b" " + req.uri + b" " + req.version + chaoslib.cadr_return

        headers = b""
        for lower_name, (name, value) in req.headers.items():
            headers += name + b": " + value + chaoslib.cadr_return

        body = req.body

        connection = chaoslib.ChaosStream(OUR_ADDR, CADR_ADDR, SERVICE_NAME)

        chaos_http_req = status_line + headers + chaoslib.cadr_return + body

        print(repr(chaos_http_req))

        connection.writeall(chaos_http_req)

        response = chaoslib.replace_cadr_return_with_newlines(connection.readall())
        print(repr(response))

        connection.close()

        self.request.sendall(response)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog="worm_hole")
    parser.add_argument("--debug", action="store_true", help="Enable debugging AS OF NOW DOES NOTHING")
    parser.add_argument("--processes", type=int, default=2, help="Number of processes to use")
    parser.add_argument("--port", type=int, default=7159, help="Port to listen on [default: 7159]")
    parser.add_argument("--host", default='127.0.0.1', help="Host to listen on [default: 127.0.0.1]")
    parser.add_argument("--version", action="version", version="%(prog)s v0.0.1")
    logging.basicConfig()
    args = parser.parse_args()
#    with socketserver.TCPServer((args.host, args.port), WormHoleTCPHandler) as server:    
    with socketserver.ForkingTCPServer((args.host, args.port), WormHoleTCPHandler) as server:
        server.serve_forever()
