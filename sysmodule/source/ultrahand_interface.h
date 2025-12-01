#include <string>

class UltraHandInterface {
public:
    UltraHandInterface() = delete;
    
    static void writeNotification(const std::string& message, int fontSize = 28, int priority = 1);
    static void drawAttention();

};