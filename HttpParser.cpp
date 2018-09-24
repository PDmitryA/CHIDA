//
// Created by pda on 23.09.18.
//
#include <sstream>
#include <iostream>
#include "HttpParser.h"

HttpParser::HttpParser(const std::string& text) {
    unsigned long start_of_path = text.find(' ', 0);
    unsigned long start_of_query = text.find('?', start_of_path + 1);
    unsigned long end_of_query = text.find(' ', start_of_path + 1);
    if (start_of_query == std::string::npos) {
        start_of_query = end_of_query;
    }

    method = text.substr(0, start_of_path);
    path = text.substr(start_of_path + 1, start_of_query - start_of_path - 1);
    urldecode();
    query = text.substr(start_of_query, end_of_query - start_of_query);
}

std::string HttpParser::get_method() const {
    return this->method;
}

std::string HttpParser::get_path() const {
    return this->path;
}

std::string HttpParser::get_query() const {
    return this->query;
}

std::string
HttpParser::create_response(const std::string &serverName, const std::string &httpVersion, const std::string &status,
                            const std::string &connection, const std::string &content,
                            const std::string &content_type, const std::string &content_length) {
    std::string response;
    response += httpVersion;
    response += " ";
    response += status;
    response += "\r\n";
    response += "Date: ";

    static char buf[100];
    time_t now = time(nullptr);
    struct tm tm = *gmtime(&now);
    strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    response += buf;
    response += "\r\n";

    response += "Connection: ";
    response += connection;
    response += "\r\n";
    response += "Server: ";
    response += serverName;
    response += "\r\n";
    response += "Content-Length: ";
    response += content_length;
    response += "\r\n";
    if (content_type.length()) {
        response += "Content-Type: ";
        response += content_type;
        response += "\r\n";
    }
    response += "\r\n";
    response += content;

    return response;
}

std::string HttpParser::urldecode() {
    char a, b;
    std::string out;
    unsigned long len = path.length();

    for(int i = 0; i < len;) {
        if ((path[i] == '%') &&
            ((a = path[i+1]) && (b = path[i+2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a')
                a -= 'a'-'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a'-'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            out += (char)(16*a+b);
            i+=3;
        } else if (path[i] == '+') {
            out += ' ';
            i++;
        } else {
            out += path[i];
            i++;
        }
    }
    swap(path, out);
    return out;
}
