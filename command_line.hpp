
#include <map>
#include <set>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <memory>
#include <functional>
#include <regex>
#include <stdexcept>
#include <sstream>
#include <type_traits>

namespace command_parser {

    template <class T>
    T convert(const std::string& param);

    template <>
    int convert(const std::string& param) {
        size_t* idx = nullptr;
        int val = 0;
        try {
            val = std::stoi(param, idx);
        } catch (...) {
            throw std::runtime_error(param + " is not integer");
        }
        if (idx != nullptr) { 
            throw std::runtime_error(param + " is not integer");
        }
        return val;
    }

    template <>
    long convert(const std::string& param) {
        size_t* idx = nullptr;
        long val = 0;
        try {
            val = std::stol(param, idx);
        } catch (...) {
            throw std::runtime_error(param + " is not integer");
        }
        if (idx != nullptr) { 
            throw std::runtime_error(param + " is not integer");
        }
        return val;
    }

    template <>
    double convert(const std::string& param) {
        size_t* idx = nullptr;
        double val = 0.0;
        try {
            val = std::stod(param, idx);
        } catch (...) {
            throw std::runtime_error(param + " is not floating point number");
        }
        if (idx != nullptr) { 
            throw std::runtime_error(param + " is not floating point number");
        }
        return val;
    }

    template <>
    std::string convert(const std::string& param) {
        return param;
    }

    template <class T>
    class oneof {
    public:
        template <class ... Args>
        explicit oneof(Args ... args) : candidates_{args...} { }

        std::vector<T>& candidates() { return candidates_; }
    private:
        std::vector<T> candidates_;
    };

    template <class T>
    class range {
    public:
        range(T min, T max) : min_(min), max_(max) { };

        bool operator()(const std::string& param) const {
            T val = convert<T>(param);
            if (min_ <= val && val <= max_) {
                return true;
            }
            std::stringstream range;
            range << "[" << min_ << ", " << max_ << "]";
            throw std::runtime_error(param + " is out of range. range is " + range.str());
        }
        const T& min() const {
            return min_;
        }
        const T& max() const {
            return max_;
        }
    private:
        T min_;
        T max_;
    };

    namespace detail {

        class null_validator {
        public:
            null_validator() {}
            bool operator() (const std::string& str) const {
                return true;
            }
        };
        static null_validator _null_validator_;

        template <class T>
        std::string to_str(T val) {
            return std::to_string(val);
        }

        template <>
        std::string to_str(std::string val) {
            return val;
        }

        // long name style is  "--long_name"
        // short name style is "-short_name"
        class Option {
            using Validator = std::function<bool(const std::string&)>;
        public:
            Option(std::string lname, char sname, std::string message, Validator v = _null_validator_) : 
                lname_(lname),
                sname_(1, sname),
                message_(message),
                validator(v)
            {
            }
            bool has_value() const {
                return has_value_;
            }
            void set_validator(Validator v) {
                validator = v;
            }

            bool use() const {
                return is_use_;
            }
            bool use(bool b) {
                is_use_ = b;
                return use();
            }
            const std::string& value() const {
                if(has_value_ == false) {
                    throw std::logic_error("Don't has a value.");
                }
                return value_;
            }
            const std::string& value(std::string value) {
                if(has_value_ == false) {
                    throw std::logic_error("Don't has a value.");
                }
                value_ = value;
                use(true);
                return value_;
            }
            const std::string& long_name() const {
                return lname_;
            }
            char short_name() const {
                return sname_.at(0);
            }
            const std::string& message() const {
                return message_;
            }
            bool validate() const {
                try {
                    return validator(value_);
                } catch (std::runtime_error& e) {
                    std::string mes = "\"--" + lname_ + "(-" + sname_ + ")\" validation failed. ";
                    mes += e.what();
                    throw std::runtime_error(mes);
                }
            }

        protected:
            std::string lname_;
            std::string sname_;
            std::string message_;
            Validator validator;
            bool has_value_ = false;
            std::string value_ = "";
            bool is_use_ = false;
        };

        // long name style is  "--long_name=value"
        // short name style is "-short_name value"
        class ValueOption : public Option {
            using Validator = std::function<bool(const std::string&)>;
        public:
            ValueOption(std::string lname, char sname, std::string message, std::string def_val, Validator v = _null_validator_) : 
                Option(lname, sname, message)
            {
                has_value_ = true;
                value_ = def_val;
                set_validator(v);
            }

            template <class T>
            ValueOption(std::string lname, char sname, std::string message, std::string def_val, range<T> r) : 
                Option(lname, sname, message)
            {
                has_value_ = true;
                value_ = def_val;
                set_validator(r);
                message_ += " : range is [" + to_str(r.min()) + ", " + to_str(r.max()) + "]";
            }
        };

        class WithCandidateValueOption : public ValueOption {
        public:
            template <class T>
                WithCandidateValueOption(std::string lname, char sname, std::string message, std::string def_val, std::vector<T> candidates) : 
                    ValueOption(lname, sname, message, def_val)
            {
                to_str_candidates(candidates);
                set_validator(std::bind(&WithCandidateValueOption::validate_candidates, std::ref(*this), std::placeholders::_1));

                message_ += " : Available pattern {";
                bool first=true;
                for(const auto& c : candidates_) {
                    if( first ) {
                        message_ += c;
                        first = false;
                    } else {
                        message_ += ", " + c;
                    }
                }
                message_ += "}";
            }

            bool validate_candidates(const std::string& param) const {
                if ( candidates_.count(param) == 0 ) { 
                    throw std::runtime_error("--" + long_name() + "(-" + short_name() + ") cannot specify the \"" + param + "\"");
                }

                return true;
            }

        private:
            template <class T>
            void to_str_candidates(std::vector<T> cand) {
                for(auto v : cand) {
                    candidates_.insert(detail::to_str(v));
                }
            }

            std::set<std::string> candidates_;
        };

        class OptionsInfo{
        public:
            OptionsInfo() {}

            void add(std::unique_ptr<Option> op) {
                if(is_exist(op->short_name())) {
                    throw std::logic_error("duplicated option");
                }
                if(is_exist(op->long_name())) {
                    throw std::logic_error("duplicated option");
                }
                if(op->long_name().length() > max_lname_length) {
                    max_lname_length = op->long_name().length();
                }
                short_long_map.insert(std::make_pair(op->short_name(), op->long_name()));
                options_.insert(std::make_pair(op->long_name(), std::move(op)));
            }
            int get_max_length() const {
                return max_lname_length;
            }
            bool is_exist(const std::string& long_name) const {
                return options_.count(long_name) == 1;
            }
            bool is_exist(char short_name) const {
                return short_long_map.count(short_name) == 1;
            }
            void set(const std::string& long_name) {
                options_[long_name]->use(true);
            }
            void set(const std::string& long_name, const std::string& value) {
                set(long_name);
                options_[long_name]->value(value);
            }
            bool is_use(const std::string& long_name) const {
                if(is_exist(long_name) == false) {
                    throw std::logic_error("Undefined parameter.");
                }
                return options_.at(long_name)->use();
            }
            bool has_value(const std::string& long_name) const {
                if(is_exist(long_name) == false) {
                    throw std::runtime_error("Parameter invalid");
                }
                return options_.at(long_name)->has_value();

            }
            bool has_value(char short_name) const {
                if(is_exist(short_name) == false) {
                    throw std::runtime_error("Parameter invalid");
                }

                return options_.at(short_long_map.at(short_name))->has_value();
            }

            template <class T>
            T get_value(const std::string& long_name) const {
                if(is_exist(long_name) == false) {
                    throw std::logic_error(long_name + "is not defined.");
                }
                return convert<T>(options_.at(long_name)->value());
            }

            const std::string& to_long_name(char short_name) const {
                if(is_exist(short_name) == false) {
                    throw std::runtime_error("Option name invalid");
                }

                return short_long_map.at(short_name);
            }

            auto begin() { return options_.begin(); }
            const auto begin() const { return options_.begin(); }

            auto end() { return options_.end(); }
            const auto end() const { return options_.end(); }

            bool empty() const {
                return options_.empty();
            }
        private:
            int max_lname_length = 0;
            std::map<char, std::string> short_long_map;
            std::map<std::string, std::unique_ptr<Option>> options_;
        };

        class Parameter {
            using Validator = std::function<bool(const std::string&)>;
        public:
            Parameter(int order, std::string name, std::string message, Validator v = _null_validator_) :
                order_(order),
                name_(name),
                message_(message),
                validator(v)
            {
            }
            int get_order() const {
                return order_;
            }
            void set_validator(Validator v) {
                validator = v;
            }

            // TODO
            void set(const std::string& value) {
                value_ = value;
            }

            const std::string& value() const {
                return value_;
            }
            const std::string& value(const std::string& value) {
                value_ = value;
                return value_;
            }

            bool is_use() const {
                return value_.length() != 0;
            }

            const std::string& name() const {
                return name_;
            }
            const std::string& message() const {
                return message_;
            }
            bool validate() const {
                if (value_ == "") {
                    std::stringstream mes;
                    mes << "The " << order_ << "(" + name_ + ")" << " argument is not specified.";
                    throw std::runtime_error(mes.str());
                }
                try {
                    return validator(value_);
                } catch (std::runtime_error& e) {
                    std::string mes = "\"" + name_ + "\" validation failed. ";
                    mes += e.what();
                    throw std::runtime_error(mes);
                }
            }

        protected:
            int order_;
            std::string name_;
            std::string message_;
            Validator validator;
            std::string value_ = "";
        };

        template <class T>
        class RangeParameter : public Parameter {
        public:
            RangeParameter(int order, std::string name, std::string message, range<T> r) :
                Parameter(order, name, message),
                r_(r)
            {
                set_validator(std::bind(&RangeParameter::validate_range, std::ref(*this), std::placeholders::_1));

                message_ += " : range is [" + to_str(r.min()) + ", " + to_str(r.max()) + "]";
            }

            bool validate_range(const std::string& param) const {
                return r_(param);
            }

        private:

            range<T> r_;
        };
        class StringParameter : public Parameter {
            using Validator = std::function<bool(const std::string&)>;
        public:
            StringParameter(int order, std::string name, std::string message, int max_length, Validator v = detail::_null_validator_) :
                Parameter(order, name, message),
                max_length_(max_length),
                str_validator_(v)
            {
                set_validator(
                        [this](const std::string& param) -> bool {
                            if (this->max_length_ < param.length()) {
                                throw std::runtime_error("Over-length error. Max length of \"" + this->name() +  + "\" is " + to_str(max_length_) + ".");
                            }
                            return this->str_validator_(param);
                        }
                        );

            }

        private:
            int max_length_;
            Validator str_validator_;
        };

        class ParametersInfo{
        public:
            ParametersInfo() {}

            void add(std::unique_ptr<Parameter> op) {
                if(order_map_.count(op->name()) != 0) {
                    throw std::logic_error("duplicated option");
                }
                order_map_.insert(std::make_pair(op->name(), order_index_));
                params_.insert(std::make_pair(order_index_, std::move(op)));
                order_index_++;
            }

            void set(const std::string value) {
                for (auto& i : params_) {
                    if( i.second->is_use() == false) {
                        i.second->set(value);
                        return;
                    }
                }
                throw std::runtime_error("Parameter invalid");
            }

            size_t size() const {
                return params_.size();
            }

            bool is_exist(const std::string& name) const {
                return order_map_.count(name) == 1;
            }

            template <class T>
            T get_value(const std::string& param) const {
                if(is_exist(param) == false) {
                    throw std::logic_error(param + "is not defined.");
                }
                return convert<T>(params_.at(order_map_.at(param))->value());
            }

            auto begin() { return params_.begin(); }
            const auto begin() const { return params_.begin(); }

            auto end() { return params_.end(); }
            const auto end() const { return params_.end(); }
        private:
            int order_index_ = 0;
            std::map<std::string, int> order_map_;
            std::map<int, std::unique_ptr<Parameter>> params_;
        };


        class parser {
            enum class OptionType {
                NOT_OP = -1,
                LONG = 1,
                LONG_WITH_VAL = 2,
                SHORT = 3,
            };

        public:
            parser(int argc, char const* argv[], detail::OptionsInfo& info) : 
                argc_(argc),
                argv_(argv),
                option_info_(info)
            {
            }

            std::pair<std::string, std::string> next_option() {
                for(;  id_op_ < argc_; id_op_++) {
                    std::string arg = argv_[id_op_];

                    OptionType opt = option_type(arg);
                    if (opt == OptionType::NOT_OP) {
                        continue;
                    }
                    if (opt == OptionType::LONG) {
                        std::string op(arg.substr(2));
                        id_op_+=1;
                        if(option_info_.has_value(op) == true) {
                            throw std::runtime_error("Option \"--" + op + "\" need a value.");
                        }
                        return std::make_pair(std::move(op), "");
                    } else if (opt == OptionType::LONG_WITH_VAL) {
                        std::string op(arg.substr(2, arg.find("=")-2));
                        std::string value(arg.substr(arg.find("=")+1));

                        id_op_+=1;
                        if(option_info_.has_value(op) == true) {
                            return std::make_pair(std::move(op), std::move(value));
                        } else {
                            throw std::runtime_error("Option " + op + " does't need a value.");
                        }
                    } else if (opt == OptionType::SHORT) {
                        std::string op(option_info_.to_long_name(arg.at(1)));

                        if(option_info_.has_value(op)) {
                            if(id_op_+1 < argc_) {
                                std::string value(argv_[id_op_+1]);
                                id_op_+=2;
                                return std::make_pair(std::move(op), std::move(value));
                            } else {
                            throw std::runtime_error("Option \"" + arg + "\" need a value.");
                            }
                        } else {
                            id_op_+=1;
                            return std::make_pair(std::move(op), "");
                        }
                    } else {
                        throw std::runtime_error("command invalid");
                    }
                }
                return std::make_pair("", "");
            }

            std::string next_parameter() {
                for(; id_p_ < argc_; id_p_++) {
                    std::string arg = argv_[id_p_];

                    OptionType opt = option_type(arg);
                    if (opt == OptionType::NOT_OP) {
                        id_p_++;
                        return arg;
                    } else if (opt == OptionType::SHORT) {
                        std::string op(option_info_.to_long_name(arg.at(1)));
                        if(option_info_.has_value(op)) {
                            id_p_++;
                        }
                        continue;
                    }
                }
                return "";
            }
        private:

            OptionType option_type(const std::string& param) const {
                if (std::regex_match(param, lvtype_)) {
                    return OptionType::LONG_WITH_VAL;
                }
                if (std::regex_match(param, ltype_)) {
                    return OptionType::LONG;
                }
                if (std::regex_match(param, stype_)) {
                    return OptionType::SHORT;
                }
                return OptionType::NOT_OP;
            }

            int argc_;
            char const** argv_;
            detail::OptionsInfo& option_info_;

            int id_op_ = 1;
            int id_p_ = 1;

            std::regex lvtype_ {"--[\\w|-]*=[\"]?[\\w]*[\"]?"};
            std::regex ltype_ {"--[\\w]*"};
            std::regex stype_ {"-[\\w]"};
        };
    };  // namespace detail

    class rule {
    public:
        rule(std::string help_long = "help", char help_short = 'h') : 
            help_long(help_long), help_short(help_short) 
        { 
            add_option(help_long, help_short, "display the usage.");
        }

        void add_option(std::string long_name, char short_name, std::string message) {
            add_option_impl<detail::Option>(long_name, short_name, message);
        }
        template <class T>
        void add_option(std::string long_name, char short_name, std::string message,
                        T def_val, std::function<bool(const std::string&)> validator = detail::_null_validator_) {
            std::string def = detail::to_str(def_val);
            add_option_impl<detail::ValueOption>(long_name, short_name, message, def, validator);
        }

        template <class T, class F>
        void add_option(std::string long_name, char short_name, std::string message,
                        T def_val, range<F> r) {
            static_assert(std::is_same<T, F>::value, "missmach between type of value and type of range");
            std::string def = detail::to_str(def_val);
            add_option_impl<detail::ValueOption>(long_name, short_name, message, def, r);
        }

        template <class T>
        void add_option(std::string long_name, char short_name, std::string message, T def_val, oneof<T> cand) {
            std::string def = detail::to_str(def_val);
            add_option_impl<detail::WithCandidateValueOption>(long_name, short_name, message, def, cand.candidates());
        }

        template <class T>
        void add_parameter(std::string name, std::string message, range<T> r) {
            add_parameter_impl<detail::RangeParameter<T>>(name, message, r);
        }

        void add_parameter(std::string name, std::string message, int max_length = 256) {
            add_parameter_impl<detail::StringParameter>(name, message, max_length);
        }

        void add_parameter(std::string name, std::string message, int max_length, std::function<bool(const std::string&)> validator) {
            add_parameter_impl<detail::StringParameter>(name, message, max_length, validator);
        }

        void parse(int argc, char const* argv[]) try {
            detail::parser p(argc, argv, options_);

            while(true) {
                std::pair<std::string, std::string> op = p.next_option();
                if(op.first == help_long) {
                    usage(argv[0]);
                    exit(0);
                }
                if(op.first == "") {
                    break;
                }
                if(op.second == "") {
                    options_.set(op.first);
                } else {
                    options_.set(op.first, op.second);
                }
            }
            while(true) {
                std::string param = p.next_parameter();
                if( param == "") {
                    break;
                }
                params_.set(param);
            }

            for( auto& op : options_ ) {
                if(!op.second->validate()) {
                    std::string mes = "Option validation failed. \"--" + op.second->long_name() + "(-" + op.second->short_name() + ")\"";
                    throw std::runtime_error(mes);
                }
            }
            for( auto& p : params_ ) {
                if(!p.second->validate()) {
                    std::string mes = "Argument validation failed. \"" + p.second->name() + "\"";
                    throw std::runtime_error(mes);
                }
            }

            return;
        } catch (std::runtime_error& e) {
            std::cout << "Error: " << e.what() << std::endl << std::endl;
            usage(argv[0]);
            exit(1);
        }

        bool is_option_use(const std::string& name) const {
            return options_.is_use(name);
        }
        template <class T>
        T get_option_value(const std::string& option_name) const {
            return options_.get_value<T>(option_name);
        }

        template <class T>
        T get_param_value(const std::string& param_name) const {
            return params_.get_value<T>(param_name);
        }

        void usage(std::string program) const {
            std::string args;
            for (const auto&p : params_) {
                args += "<" + p.second->name() + "> ";
            }
            std::string usage = "Usage: " + program + " ";
            if( !options_.empty() ){
                usage += "[Options ...] ";
            }
            std::cout << usage << args << std::endl;
            if( !options_.empty() ) {
                std::cout << std::endl << "Options:" << std::endl;
                for(const auto& op : options_) {
                    std::cout << "  " << "--" << op.second->long_name();
                    int padding = options_.get_max_length() - op.second->long_name().size();
                    if(op.second->has_value()) {
                        std::cout << "=<value> " << std::setw(padding+2);
                        std::cout << "[-" << op.second->short_name() <<  " <value>]" << "\t" << op.second->message() << std::endl;
                    } else {
                        std::cout << " " << std::setw(padding+10) << "[-" << op.second->short_name() << "]       " << "\t" << op.second->message() << std::endl;
                    }
                }
            }

            if( params_.size() > 0) {
                std::cout << std::endl << "Arguments:" << std::endl;
                for(const auto& p : params_) {
                    std::cout << "  " << p.second->name() << ":\t" << p.second->message() << std::endl;
                }
            }
        }

    private:
        template <class T, class ... Args>
        void add_parameter_impl(Args ... args) {
            int order = params_.size()+1;
            std::unique_ptr<detail::Parameter> p = std::make_unique<T>(order, std::forward<Args>(args)...);
            params_.add(std::move(p));
        }
        template <class T, class ... Args>
        void add_option_impl(Args ... args) {
            std::unique_ptr<detail::Option> op = std::make_unique<T>(std::forward<Args>(args)...);
            options_.add(std::move(op));
        }

        std::string help_long;
        char help_short;
        detail::OptionsInfo options_;
        detail::ParametersInfo params_;
    };



};  // command_parser
