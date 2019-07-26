FROM ubuntu:18.04

RUN apt-get update && apt-get install -y python3 python3-pip bash xvfb supervisor python3-venv && \
	pip3 install wheel

RUN mkdir -p /opt/chaos /opt/usim /opt/worm_hole /tree
RUN useradd cadr

RUN python3 -m venv /opt/worm_hole/venv
COPY src/worm_hole/requirements.txt /opt/worm_hole/requirements.txt

RUN /opt/worm_hole/venv/bin/pip3 install -r /opt/worm_hole/requirements.txt

COPY src/worm_hole/chaoslib.py src/worm_hole/worm_hole.py src/worm_hole/static /opt/worm_hole/

COPY deploy/chaos/chaosd deploy/chaos/FILE deploy/chaos/MINI deploy/chaos/server /opt/chaos/

COPY deploy/usim /opt/usim/
RUN touch /opt/usim/disk.img && chown cadr:cadr /opt/usim/disk.img

COPY deploy/supervisord.conf /etc/supervisord.conf

expose 31337
CMD ["/usr/bin/supervisord"]
