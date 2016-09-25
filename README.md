# cmdpsr
simple command line parser


## sample

```
#include <string>
#include "command_line.hpp"

#include <regex>

class ip_address_varidator {
public:
    bool operator()(const std::string& param) {
        std::regex re("\\d{1,3}(\\.\\d{1,3}){3}");
        if (std::regex_match(param, re)) {
            return true;
        }
        return false;
    }
};

int main(int argc, char const * argv[])
{
    using namespace command_parser;

    rule r { "help", 'h' };
    r.add_option<std::string>("pattern", 'p', "Support selectable candidates", "disable", oneof<std::string>("disable", "enable"));
    r.add_option<int>("range", 'r', "Support value of range.", 1, range<int>(1, 100));
    r.add_option<std::string>("ip", 'i', "you can original validation", "0.0.0.0", ip_address_varidator());

    r.add_parameter("src", "required parameter", 256);
    r.add_parameter("dst", "required parameter with original Validator", 15, ip_address_varidator());
    r.add_parameter<int>("int", "required parameter with range", range<int>(1, 100));

    r.parse(argc, argv);
    if ( r.is_option_use("ip") )
        std::cout << r.get_option_value<std::string>("ip") << std::endl;
    if ( r.is_option_use("pattern") ) 
        std::cout << r.get_option_value<std::string>("pattern") << std::endl;

    std::cout << r.get_param_value<std::string>("src") << std::endl;;
    std::cout << r.get_param_value<std::string>("dst") << std::endl;;

    return 0;
}
```
