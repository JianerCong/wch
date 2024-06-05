FROM ubuntu:latest

# copy ./build-weak/wch to /
COPY build-weak/wch /

ENV PORT=7777

# expose the port
EXPOSE $PORT

# start a shell for debugging
# ENTRYPOINT ["/bin/bash"]
ENTRYPOINT ["/wch"]
