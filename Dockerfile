FROM ubuntu:16.04
MAINTAINER Dmitry Poponkin
RUN apt-get update
RUN apt-get -y install g++

ADD . .

RUN g++ main.cpp declare.h HttpParser.cpp HttpParser.h MimeType.h MimeType.cpp Server.cpp Server.h aoReadFile.cpp List/ListNode.cpp List/ConcurrentList.cpp parse/parse_config.c -lrt -lpthread -std=c++1y -o highload

EXPOSE 80

CMD ./highload