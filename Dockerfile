FROM ubuntu:22.04

# copy ./build-weak/wch to /
COPY build-weak/wch /
ENV PORT=7777

# [optional] use tsinghua mirror
COPY ./source.list.tuna /etc/apt/sources.list
RUN echo 'Acquire::https::Verify-Peer "false";' > /etc/apt/apt.conf.d/10ssl-verify.conf

# update and install python3
RUN apt-get update && apt-get install -y python3

# expose the port
EXPOSE 7777
STOPSIGNAL SIGINT

# start a shell for debugging
# ENTRYPOINT ["/bin/bash"]
ENTRYPOINT ["/wch"]
