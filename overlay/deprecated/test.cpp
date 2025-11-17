#include <string>
#include <iostream>

int main() {
    std::string str = "+1155462797689090377:2253595715205424570|Papa+1155510029677435490:2253595715205424570|Paul+1155510030001430007:2253595715205424570|Camille";

    std::string_view sv = str;
        size_t pos = 0;

        while (pos < sv.size()) {
            // Find account separator
            size_t next_plus = sv.find('+', pos);
            std::string_view account_fragment;
            if (next_plus == std::string_view::npos) {
                account_fragment = sv.substr(pos); // last fragment
                pos = sv.size();
            } else {
                account_fragment = sv.substr(pos, next_plus - pos);
                pos = next_plus + 1;
            }

            // Separate UID and nickname
            size_t pipe_pos = account_fragment.find('|');
            if (pipe_pos != std::string_view::npos) {
                std::string_view uid_part  = account_fragment.substr(0, pipe_pos);
                std::string_view nickname  = account_fragment.substr(pipe_pos + 1);

                std::cout << std::string(uid_part);
                std::cout << std::string(nickname);    
                std::cout << std::endl;            
            }
        }
}