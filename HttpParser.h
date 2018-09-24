//
// Created by pda on 23.09.18.
//

#ifndef HIGHLOAD_HTTPPARSER_H
#define HIGHLOAD_HTTPPARSER_H
#include <string>



class HttpParser {
private:
    std::string method, path, query;
    std::string urldecode();
public:
    HttpParser(const std::string& text);

    std::string get_method() const;

    std::string get_path() const;

    std::string get_query() const;

    static std::string create_response(
            const std::string& serverName,
            const std::string& httpVersion,
            const std::string& status,
            const std::string& connection,
            const std::string& content = "",
            const std::string& content_type = "",
            const std::string &content_length = "0"
            );
};


#endif //HIGHLOAD_HTTPPARSER_H
