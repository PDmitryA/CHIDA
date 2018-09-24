FROM ubuntu:16.04
MAINTAINER Dmitry Poponkin
RUN apt-get update
RUN apt-get -y install g++

ADD ./ ./

RUN g++ main.cpp declare.h HttpParser.cpp HttpParser.h MimeType.h MimeType.cpp -lrt -std=c++1y -o highload

EXPOSE 80

CMD ./highload 80