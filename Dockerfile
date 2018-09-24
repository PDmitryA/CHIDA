FROM ubuntu:16.04
MAINTAINER Dmitry Poponkin
RUN apt-get update
RUN apt-get -y install g++
RUN apt-get -y install git

RUN git clone https://github.com/init/http-test-suite.git

ADD ./ ./http-test-suite
WORKDIR ./http-test-suite
RUN g++ main.cpp declare.h HttpParser.cpp HttpParser.h MimeType.h MimeType.cpp -lrt -std=c++1y -o highload

EXPOSE 80

CMD ./highload 80