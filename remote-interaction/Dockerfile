FROM ubuntu:18.04

ENV TERM linux

run echo "travis_fold:start:DIapt\033[33;1minteractions Dockerfile pkgs\033[0m" && \
    apt-get update && apt-get install -y python3 python3-pip  bash curl && \
    echo "\ntravis_fold:end:DIapt\r"

COPY requirements.txt /requirements.txt
run echo "travis_fold:start:DIpip\033[33;1minteractions Dockerfile pip\033[0m" && \
    pip3 install -r requirements.txt && \
    echo "\ntravis_fold:end:DIpip\r"

COPY exploit.sh /exploit.sh

COPY check.py /check.py



