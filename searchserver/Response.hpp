#pragma once
#include <string>

std::string make_response(int status,
                          const std::string& body,
                          const std::string& content_type = "text/plain",
                          const std::string& status_text = "");
