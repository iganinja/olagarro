#include <oui/expression.h>

#include <oui/translation.h>
#include <oui/utf8.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <numeric>
#include <utility>

namespace olagarro::oui
{

namespace
{

constexpr double pi{3.14159265358979323846};

// What stops the working out: why, and where.
struct Stop
{
    std::string message;
    std::size_t position;
};

enum class TokenKind
{
    number,
    name,
    symbol,
    end,
};

struct Token
{
    TokenKind kind;
    // As written: "+", "**", "×", "sqrt", "0x1F".
    std::string text;
    std::size_t position;
    double number{};
};

[[nodiscard]] bool is_digit(char character)
{
    return character >= '0' && character <= '9';
}

// The operators written with characters beyond ASCII, as they are in UTF-8.
constexpr std::string_view times{"\xc3\x97"};
constexpr std::string_view divided{"\xc3\xb7"};
constexpr std::string_view root{"\xe2\x88\x9a"};

[[nodiscard]] bool starts_symbol(std::string_view text)
{
    return text.substr(0, times.size()) == times || text.substr(0, divided.size()) == divided ||
           text.substr(0, root.size()) == root;
}

[[nodiscard]] bool is_name_character(std::string_view rest, bool first)
{
    const auto character{static_cast<unsigned char>(rest.front())};
    if(character >= 0x80)
    {
        return !starts_symbol(rest);
    }
    return character == '_' || (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
           (!first && is_digit(static_cast<char>(character)));
}

[[nodiscard]] std::vector<Token> tokens_of(std::string_view text)
{
    std::vector<Token> tokens;
    std::size_t index{};
    while(index < text.size())
    {
        const auto character{text[index]};
        if(character == ' ' || character == '\t')
        {
            ++ index;
            continue;
        }
        const auto start{index};
        const auto rest{text.substr(index)};
        if(is_digit(character) || (character == '.' && index + 1 < text.size() && is_digit(text[index + 1])))
        {
            std::string digits;
            double value{};
            const auto base{rest.size() > 2 && character == '0' ? (rest[1] == 'x' || rest[1] == 'X'   ? 16
                                                                   : rest[1] == 'b' || rest[1] == 'B' ? 2
                                                                   : rest[1] == 'o' || rest[1] == 'O' ? 8
                                                                                                      : 10)
                                                                : 10};
            if(base != 10)
            {
                index += 2;
                while(index < text.size() &&
                      (std::isxdigit(static_cast<unsigned char>(text[index])) != 0 || text[index] == '_'))
                {
                    if(text[index] != '_')
                    {
                        digits += text[index];
                    }
                    ++ index;
                }
                char* end{};
                value = static_cast<double>(std::strtoull(digits.c_str(), &end, base));
                if(digits.empty() || *end != '\0')
                {
                    throw Stop{format_text(translate("Expression", "Not a number in base {0}: {1}"),
                                           {std::to_string(base), text.substr(start, index - start)}),
                               start};
                }
            }
            else
            {
                const auto take_digits{[&]
                                       {
                                           while(index < text.size() && (is_digit(text[index]) || text[index] == '_'))
                                           {
                                               if(text[index] != '_')
                                               {
                                                   digits += text[index];
                                               }
                                               ++ index;
                                           }
                                       }};
                take_digits();
                if(index < text.size() && text[index] == '.' && index + 1 < text.size() && is_digit(text[index + 1]))
                {
                    digits += text[index++];
                    take_digits();
                }
                else if(index < text.size() && text[index] == '.' &&
                        (index + 1 >= text.size() || !is_digit(text[index + 1])) && !digits.empty())
                {
                    // "2." is 2.
                    ++ index;
                }
                // An exponent only where digits follow: 2e is 2 times e.
                if(index < text.size() && (text[index] == 'e' || text[index] == 'E'))
                {
                    auto after{index + 1};
                    if(after < text.size() && (text[after] == '+' || text[after] == '-'))
                    {
                        ++ after;
                    }
                    if(after < text.size() && is_digit(text[after]))
                    {
                        digits += 'e';
                        digits.append(text.substr(index + 1, after - index - 1));
                        index = after;
                        take_digits();
                    }
                }
                value = std::strtod(digits.c_str(), nullptr);
            }
            tokens.push_back({TokenKind::number, std::string{text.substr(start, index - start)}, start, value});
            continue;
        }
        if(is_name_character(rest, true))
        {
            while(index < text.size() && is_name_character(text.substr(index), false))
            {
                // A character beyond ASCII is taken whole.
                static_cast<void>(decode_utf8(text, index));
            }
            tokens.push_back({TokenKind::name, std::string{text.substr(start, index - start)}, start});
            continue;
        }
        for(const auto symbol :
            {std::string_view{"**"}, times, divided, root, std::string_view{"+"}, std::string_view{"-"},
             std::string_view{"*"}, std::string_view{"/"}, std::string_view{"^"}, std::string_view{"!"},
             std::string_view{"%"}, std::string_view{"("}, std::string_view{")"}, std::string_view{","},
             std::string_view{"="}})
        {
            if(rest.substr(0, symbol.size()) == symbol)
            {
                tokens.push_back({TokenKind::symbol, std::string{symbol}, start});
                index += symbol.size();
                break;
            }
        }
        if(index == start)
        {
            auto next{index};
            static_cast<void>(decode_utf8(text, next));
            throw Stop{format_text(translate("Expression", "Unexpected {0}"), {text.substr(start, next - start)}),
                       start};
        }
    }
    tokens.push_back({TokenKind::end, {}, text.size()});
    return tokens;
}

// A value as it is worked out, and whether it is a percentage, which adding to or taking from a value makes a share of
// it.
struct Value
{
    double number;
    bool percent{false};
};

struct Function
{
    std::string_view name;
    std::size_t least;
    // How many it takes at most; 0 for any number.
    std::size_t most;
    std::function<double(const std::vector<double>&, AngleUnit)> work;
    // How it is written, for a list of them.
    std::string_view usage;
};

// A whole number, for gcd and lcm.
[[nodiscard]] bool is_whole(double value)
{
    return std::isfinite(value) && std::floor(value) == value;
}

// The sine, cosine and tangent of an angle in degrees, exact where the angle is a whole number of right angles; NaN for
// a tangent that has no value there.
[[nodiscard]] double trigonometry(double angle, AngleUnit unit, int which)
{
    if(unit == AngleUnit::degrees)
    {
        auto turn{std::fmod(angle, 360.0)};
        if(turn < 0)
        {
            turn += 360.0;
        }
        if(std::fmod(turn, 90.0) == 0.0)
        {
            const auto quarter{static_cast<int>(turn / 90.0)};
            const double sines[]{0, 1, 0, -1};
            const double cosines[]{1, 0, -1, 0};
            const auto sine{sines[quarter]};
            const auto cosine{cosines[quarter]};
            return which == 0 ? sine : which == 1 ? cosine : cosine == 0 ? std::nan("") : sine / cosine;
        }
        angle = angle * pi / 180.0;
    }
    return which == 0 ? std::sin(angle) : which == 1 ? std::cos(angle) : std::tan(angle);
}

[[nodiscard]] double to_angle(double radians, AngleUnit unit)
{
    return unit == AngleUnit::degrees ? radians * 180.0 / pi : radians;
}

[[nodiscard]] const std::vector<Function>& functions()
{
    using Arguments = const std::vector<double>&;
    static const std::vector<Function> all{
        {"sqrt", 1, 1, [](Arguments a, AngleUnit) { return std::sqrt(a[0]); }, "sqrt(x)"},
        {"cbrt", 1, 1, [](Arguments a, AngleUnit) { return std::cbrt(a[0]); }, "cbrt(x)"},
        {"abs", 1, 1, [](Arguments a, AngleUnit) { return std::fabs(a[0]); }, "abs(x)"},
        {"round", 1, 2,
         [](Arguments a, AngleUnit)
         {
             const auto scale{a.size() > 1 ? std::pow(10.0, std::round(a[1])) : 1.0};
             return std::round(a[0] * scale) / scale;
         },
         "round(x, digits)"},
        {"floor", 1, 1, [](Arguments a, AngleUnit) { return std::floor(a[0]); }, "floor(x)"},
        {"ceil", 1, 1, [](Arguments a, AngleUnit) { return std::ceil(a[0]); }, "ceil(x)"},
        {"trunc", 1, 1, [](Arguments a, AngleUnit) { return std::trunc(a[0]); }, "trunc(x)"},
        {"sign", 1, 1, [](Arguments a, AngleUnit) { return a[0] > 0   ? 1.0
                                                           : a[0] < 0 ? -1.0
                                                                      : 0.0; }, "sign(x)"},
        {"sin", 1, 1, [](Arguments a, AngleUnit unit) { return trigonometry(a[0], unit, 0); }, "sin(x)"},
        {"cos", 1, 1, [](Arguments a, AngleUnit unit) { return trigonometry(a[0], unit, 1); }, "cos(x)"},
        {"tan", 1, 1, [](Arguments a, AngleUnit unit) { return trigonometry(a[0], unit, 2); }, "tan(x)"},
        {"asin", 1, 1, [](Arguments a, AngleUnit unit) { return to_angle(std::asin(a[0]), unit); }, "asin(x)"},
        {"acos", 1, 1, [](Arguments a, AngleUnit unit) { return to_angle(std::acos(a[0]), unit); }, "acos(x)"},
        {"atan", 1, 1, [](Arguments a, AngleUnit unit) { return to_angle(std::atan(a[0]), unit); }, "atan(x)"},
        {"atan2", 2, 2, [](Arguments a, AngleUnit unit) { return to_angle(std::atan2(a[0], a[1]), unit); },
         "atan2(y, x)"},
        {"sinh", 1, 1, [](Arguments a, AngleUnit) { return std::sinh(a[0]); }, "sinh(x)"},
        {"cosh", 1, 1, [](Arguments a, AngleUnit) { return std::cosh(a[0]); }, "cosh(x)"},
        {"tanh", 1, 1, [](Arguments a, AngleUnit) { return std::tanh(a[0]); }, "tanh(x)"},
        {"ln", 1, 1, [](Arguments a, AngleUnit) { return a[0] < 0 ? std::nan("") : std::log(a[0]); }, "ln(x)"},
        {"log", 1, 2,
         [](Arguments a, AngleUnit)
         {
             if(a[0] < 0 || (a.size() > 1 && a[1] <= 0))
             {
                 return std::nan("");
             }
             return a.size() > 1 ? std::log(a[0]) / std::log(a[1]) : std::log10(a[0]);
         },
         "log(x, base)"},
        {"log2", 1, 1, [](Arguments a, AngleUnit) { return a[0] < 0 ? std::nan("") : std::log2(a[0]); }, "log2(x)"},
        {"log10", 1, 1, [](Arguments a, AngleUnit) { return a[0] < 0 ? std::nan("") : std::log10(a[0]); }, "log10(x)"},
        {"exp", 1, 1, [](Arguments a, AngleUnit) { return std::exp(a[0]); }, "exp(x)"},
        {"min", 1, 0, [](Arguments a, AngleUnit) { return *std::min_element(a.begin(), a.end()); }, "min(a, b, ...)"},
        {"max", 1, 0, [](Arguments a, AngleUnit) { return *std::max_element(a.begin(), a.end()); }, "max(a, b, ...)"},
        {"hypot", 2, 0,
         [](Arguments a, AngleUnit)
         {
             double sum{};
             for(const auto value : a)
             {
                 sum += value * value;
             }
             return std::sqrt(sum);
         },
         "hypot(a, b, ...)"},
        {"gcd", 2, 2,
         [](Arguments a, AngleUnit)
         {
             return is_whole(a[0]) && is_whole(a[1])
                        ? static_cast<double>(std::gcd(static_cast<long long>(a[0]), static_cast<long long>(a[1])))
                        : std::nan("");
         },
         "gcd(a, b)"},
        {"lcm", 2, 2,
         [](Arguments a, AngleUnit)
         {
             return is_whole(a[0]) && is_whole(a[1])
                        ? static_cast<double>(std::lcm(static_cast<long long>(a[0]), static_cast<long long>(a[1])))
                        : std::nan("");
         },
         "lcm(a, b)"},
    };
    return all;
}

[[nodiscard]] const Function* function_named(std::string_view name)
{
    for(const auto& function : functions())
    {
        if(function.name == name)
        {
            return &function;
        }
    }
    return nullptr;
}

[[nodiscard]] std::optional<double> constant_named(std::string_view name)
{
    if(name == "pi" || name == "\xcf\x80")
    {
        return pi;
    }
    if(name == "e")
    {
        return std::exp(1.0);
    }
    if(name == "tau")
    {
        return 2 * pi;
    }
    return std::nullopt;
}

// Works out the tokens of an expression, from the loosest operators to the tightest.
class Evaluator
{
  public:
    Evaluator(std::vector<Token> tokens, const std::map<std::string, double>& variables, std::optional<double> answer,
              AngleUnit unit, const std::set<std::string>& reserved)
        : m_tokens{std::move(tokens)}, m_variables{variables}, m_answer{answer}, m_unit{unit}, m_reserved{reserved}
    {
    }

    // The value, and the name to keep it under for an assignment.
    [[nodiscard]] std::pair<double, std::optional<std::string>> statement()
    {
        std::optional<std::string> assigned;
        if(peek().kind == TokenKind::name && m_tokens[m_index + 1].text == "=")
        {
            const auto& name{peek()};
            if(constant_named(name.text))
            {
                throw Stop{format_text(translate("Expression", "{0} is a constant"), {name.text}), name.position};
            }
            if(function_named(name.text) != nullptr)
            {
                throw Stop{format_text(translate("Expression", "{0} is a function"), {name.text}), name.position};
            }
            if(name.text == "ans" || name.text == "mod" || m_reserved.count(name.text) > 0)
            {
                throw Stop{format_text(translate("Expression", "{0} is a word of the calculator's own"), {name.text}),
                           name.position};
            }
            assigned = name.text;
            m_index += 2;
        }
        const auto value{expression()};
        if(peek().kind != TokenKind::end)
        {
            throw unexpected(peek());
        }
        return {value.number, assigned};
    }

  private:
    [[nodiscard]] const Token& peek() const { return m_tokens[m_index]; }
    [[nodiscard]] bool at(std::string_view symbol) const
    {
        return peek().kind == TokenKind::symbol && peek().text == symbol;
    }

    [[nodiscard]] static Stop unexpected(const Token& token)
    {
        if(token.kind == TokenKind::end)
        {
            return {translate("Expression", "Something is missing at the end"), token.position};
        }
        return {format_text(translate("Expression", "Unexpected {0}"), {token.text}), token.position};
    }

    // A value that came out too large or not a real number, at the operator that made it.
    static double checked(double value, std::size_t position)
    {
        if(std::isnan(value))
        {
            throw Stop{translate("Expression", "Not a real number"), position};
        }
        if(std::isinf(value))
        {
            throw Stop{translate("Expression", "The result is too large"), position};
        }
        return value;
    }

    Value expression()
    {
        auto left{term()};
        while(at("+") || at("-"))
        {
            const auto& op{peek()};
            ++ m_index;
            const auto right{term()};
            // A percentage added or taken away is that share of the value.
            const auto amount{right.percent ? left.number * right.number : right.number};
            left = {checked(op.text == "+" ? left.number + amount : left.number - amount, op.position)};
        }
        return left;
    }

    Value term()
    {
        auto left{unary()};
        while(true)
        {
            const auto& op{peek()};
            const auto multiply{at("*") || at(times)};
            const auto divide{at("/") || at(divided)};
            const auto modulo{op.kind == TokenKind::name && op.text == "mod"};
            // A name or a bracket right after a value multiplies it: 2pi, 3(4+1).
            const auto implicit{!multiply && !divide && !modulo && (op.kind == TokenKind::name || at("("))};
            if(!multiply && !divide && !modulo && !implicit)
            {
                return left;
            }
            if(!implicit)
            {
                ++ m_index;
            }
            const auto right{unary()};
            if((divide || modulo) && right.number == 0)
            {
                throw Stop{translate("Expression", "Division by zero"), op.position};
            }
            const auto value{divide   ? left.number / right.number
                             : modulo ? std::fmod(left.number, right.number)
                                      : left.number * right.number};
            left = {checked(value, op.position)};
        }
    }

    Value unary()
    {
        if(at("-") || at("+"))
        {
            const auto negative{at("-")};
            ++ m_index;
            auto value{unary()};
            value.number = negative ? -value.number : value.number;
            return value;
        }
        if(at(root))
        {
            const auto position{peek().position};
            ++ m_index;
            const auto value{unary()};
            if(value.number < 0)
            {
                throw Stop{translate("Expression", "Not a real number"), position};
            }
            return {std::sqrt(value.number)};
        }
        return power();
    }

    Value power()
    {
        auto base{postfix()};
        if(at("^") || at("**"))
        {
            const auto position{peek().position};
            ++ m_index;
            const auto exponent{unary()};
            return {checked(std::pow(base.number, exponent.number), position)};
        }
        return base;
    }

    Value postfix()
    {
        auto value{primary()};
        while(at("!") || at("%"))
        {
            const auto& op{peek()};
            ++ m_index;
            if(op.text == "%")
            {
                value = {value.number / 100.0, true};
                continue;
            }
            if(!is_whole(value.number) || value.number < 0)
            {
                throw Stop{translate("Expression", "! takes a whole number, 0 or more"), op.position};
            }
            value = {checked(std::tgamma(value.number + 1), op.position)};
        }
        return value;
    }

    Value primary()
    {
        const auto& token{peek()};
        if(token.kind == TokenKind::number)
        {
            ++ m_index;
            return {token.number};
        }
        if(at("("))
        {
            ++ m_index;
            const auto inside{expression()};
            if(!at(")"))
            {
                if(peek().kind == TokenKind::end)
                {
                    throw Stop{translate("Expression", "A ) is missing"), peek().position};
                }
                throw unexpected(peek());
            }
            ++ m_index;
            return inside;
        }
        if(token.kind == TokenKind::name)
        {
            ++ m_index;
            if(const auto* const function{function_named(token.text)})
            {
                return {call(*function, token)};
            }
            if(const auto constant{constant_named(token.text)})
            {
                return {*constant};
            }
            if(token.text == "ans")
            {
                if(!m_answer)
                {
                    throw Stop{translate("Expression", "There is no answer yet"), token.position};
                }
                return {*m_answer};
            }
            if(const auto variable{m_variables.find(token.text)}; variable != m_variables.end())
            {
                return {variable->second};
            }
            throw Stop{format_text(translate("Expression", "Unknown name {0}"), {token.text}), token.position};
        }
        throw unexpected(token);
    }

    double call(const Function& function, const Token& name)
    {
        if(!at("("))
        {
            throw Stop{format_text(translate("Expression", "{0} is a function: write {0}(...)"), {name.text}),
                       name.position};
        }
        ++ m_index;
        std::vector<double> arguments;
        if(!at(")"))
        {
            arguments.push_back(expression().number);
            while(at(","))
            {
                ++ m_index;
                arguments.push_back(expression().number);
            }
        }
        if(!at(")"))
        {
            if(peek().kind == TokenKind::end)
            {
                throw Stop{translate("Expression", "A ) is missing"), peek().position};
            }
            throw unexpected(peek());
        }
        ++ m_index;
        const auto count{arguments.size()};
        if(count < function.least || (function.most != 0 && count > function.most))
        {
            const auto taken{
                function.least == function.most ? std::to_string(function.least)
                : function.most == 0
                    ? format_text(translate("Expression", "at least {0}"), {std::to_string(function.least)})
                    : format_text(translate("Expression", "{0} or {1}"),
                                  {std::to_string(function.least), std::to_string(function.most)})};
            const auto pattern{function.least == 1 && function.most == 1
                                   ? translate("Expression", "{0} takes {1} value, not {2}")
                                   : translate("Expression", "{0} takes {1} values, not {2}")};
            throw Stop{format_text(pattern, {name.text, taken, std::to_string(count)}), name.position};
        }
        const auto value{function.work(arguments, m_unit)};
        if(std::isnan(value))
        {
            std::string written;
            for(const auto argument : arguments)
            {
                written += (written.empty() ? "" : ", ") + format_number(argument);
            }
            const auto pattern{function.name == "tan" ? translate("Expression", "{0}({1}) is not defined")
                                                      : translate("Expression", "{0}({1}) is not a real number")};
            throw Stop{format_text(pattern, {name.text, written}), name.position};
        }
        return checked(value, name.position);
    }

    std::vector<Token> m_tokens;
    std::size_t m_index{};
    const std::map<std::string, double>& m_variables;
    std::optional<double> m_answer;
    AngleUnit m_unit;
    const std::set<std::string>& m_reserved;
};

} // namespace

ExpressionResult Calculator::preview(std::string_view expression) const
{
    ExpressionResult result;
    try
    {
        auto tokens{tokens_of(expression)};
        if(tokens.size() == 1)
        {
            return result;
        }
        Evaluator evaluator{std::move(tokens), m_variables, m_answer, m_angle_unit, m_reserved};
        auto [value, assigned]{evaluator.statement()};
        result.value = value == 0 ? 0.0 : value;
        result.assigned = std::move(assigned);
    }
    catch(const Stop& stop)
    {
        result.error = ExpressionError{stop.message, stop.position};
    }
    return result;
}

ExpressionResult Calculator::evaluate(std::string_view expression)
{
    auto result{preview(expression)};
    if(result.value)
    {
        m_answer = result.value;
        if(result.assigned)
        {
            m_variables[*result.assigned] = *result.value;
        }
    }
    return result;
}

void Calculator::reserve(std::string word)
{
    m_reserved.insert(std::move(word));
}

void Calculator::set_angle_unit(AngleUnit unit)
{
    m_angle_unit = unit;
}

AngleUnit Calculator::angle_unit() const
{
    return m_angle_unit;
}

const std::map<std::string, double>& Calculator::variables() const
{
    return m_variables;
}

std::optional<double> Calculator::answer() const
{
    return m_answer;
}

std::string format_number(double value)
{
    if(value == 0)
    {
        return "0";
    }
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.15g", value);
    std::string text{buffer};
    // 1.5e+20 as 1.5e20, and 2.5e-07 as 2.5e-7.
    if(const auto exponent{text.find('e')}; exponent != std::string::npos)
    {
        auto mantissa{text.substr(0, exponent)};
        auto power{text.substr(exponent + 1)};
        const auto negative{!power.empty() && power.front() == '-'};
        power.erase(0, power.find_first_not_of("+-"));
        power.erase(0, std::min(power.find_first_not_of('0'), power.size() - 1));
        text = mantissa + "e" + (negative ? "-" : "") + power;
    }
    return text;
}

std::optional<std::string> format_in_base(double value, int base)
{
    if(!is_whole(value) || std::fabs(value) > 9007199254740992.0)
    {
        return std::nullopt;
    }
    auto whole{static_cast<unsigned long long>(std::fabs(value))};
    std::string digits;
    do
    {
        digits.insert(digits.begin(), "0123456789ABCDEF"[whole % static_cast<unsigned>(base)]);
        whole /= static_cast<unsigned>(base);
    } while(whole != 0);
    const auto* const prefix{base == 16 ? "0x" : base == 8 ? "0o" : base == 2 ? "0b" : ""};
    return (value < 0 ? "-" : "") + std::string{prefix} + digits;
}

std::vector<std::string> calculator_functions()
{
    std::vector<std::string> usages;
    for(const auto& function : functions())
    {
        usages.emplace_back(function.usage);
    }
    return usages;
}

std::vector<std::string> calculator_constants()
{
    return {"pi", "\xcf\x80", "e", "tau", "ans"};
}

} // namespace olagarro::oui
