#!/usr/bin/env python3

import sys
import time

import requests

def main():

    host = sys.argv[1]
    port = int(sys.argv[2])

    base_uri = f"http://{host}:{port}"

    # Test home page
    result = requests.get(base_uri)
    required_strings = ['Wanted Criminals', 'Terran Criminal Tracker', 'Capt. James T. Kirk', 'Commander Benjamin Sisko', "Overthrowing the Empire."]
    for s in required_strings:
        if not s in result.text:
            print(f"Error getting homepage", file=sys.stderr)
            sys.exit(-1)

    # test list criminals
    time.sleep(2)
    result = requests.get(f"{base_uri}/criminals")
    required_strings = ['Wanted Criminals', 'Terran Criminal Tracker', 'Capt. James T. Kirk', 'Commander Benjamin Sisko', "Overthrowing the Empire."]
    for s in required_strings:
        if not s in result.text:
            print(f"Error getting criminals", file=sys.stderr)
            sys.exit(-1)

    # add a criminal
    time.sleep(2)
    result = requests.get(f"{base_uri}/add-criminal?id=ADAM&name=aname&pic=apicture&crime=the_crime&reward=901249242", allow_redirects=False)
    if result.status_code != 302:
        print(f"Invalid status code for add-criminal.", file=sys.stderr)
        sys.exit(-1)

    time.sleep(2)
    result = requests.get(f"{base_uri}/criminals")
    required_strings = ['ADAM', 'aname', 'apicture', 'the_crime', "901249242"]
    for s in required_strings:
        if not s in result.text:
            print(f"Error adding criminal", file=sys.stderr)
            sys.exit(-1)

    # remove a criminal
    time.sleep(2)
    result = requests.post(f"{base_uri}/remove-criminal?id=ADAM", allow_redirects=False)
    if result.status_code != 302:
        print(f"Invalid status code for remove criminal", file=sys.stderr)
        sys.exit(-1)

    time.sleep(2)        
    result = requests.get(f"{base_uri}/criminals")
    required_strings = ['ADAM', 'aname', 'apicture', 'the_crime', "901249242"]
    for s in required_strings:
        if s in result.text:
            print(f"Error removing criminal", file=sys.stderr)
            sys.exit(-1)
        
    # /debug functionality
    time.sleep(2)
    result = requests.get(f"{base_uri}/debug?foo=bar")
    required_strings = ['foo', 'bar', 'Some system info', 'GET', 'user: CADR']
    for s in required_strings:
        if not s in result.text:
            print(f"Error with debug", file=sys.stderr)
            sys.exit(-1)


    sys.exit(0)


if __name__ == '__main__':
    main()
    

