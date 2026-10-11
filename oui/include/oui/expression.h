#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace olagarro::oui
{

// Arithmetic as one writes it on paper or types it into a search box, worked out as a calculator does:
//
//     (800*0.16) - (0.32 * 500)       2^10       sqrt(2)/2       3(4+1)       x = 5 → 2x + 1
//
// Numbers: 12, 0.5, .5, 1e-3, 1_000, 0x1F, 0b101, 0o17. Operators, from the loosest: + and -; *, /, × and ÷, and
// mod; - and + before a value, and √; ^ and ** for a power, from the right, so -2^2 is -4 and 2^3^2 is 2^9; ! for a
// factorial and % for a percentage after a value. A value next to a name or a bracket multiplies it: 2pi, 3(4+1),
// (1+2)(3+4); two numbers next to each other are a mistake, not a product.
//
// A percentage is a hundredth, 16% is 0.16, so 800 * 16% is 128; added to or taken from a value it is that share of
// it, as a shop's sums are: 800 + 16% is 928, and 800 - 25% is 600.
//
// "name = expression" keeps the value under the name, for later expressions; ans is the last value worked out.

enum class AngleUnit
{
    radians,
    degrees,
};

// Why an expression has no value, and the byte of it where that was seen.
struct ExpressionError
{
    std::string message;
    std::size_t position{};
};

struct ExpressionResult
{
    // The value; none when there is an error, or nothing to work out.
    std::optional<double> value;
    // The name the value was kept under, for "name = expression".
    std::optional<std::string> assigned;
    std::optional<ExpressionError> error;
};

class Calculator
{
  public:
    // Works out the expression; when it has a value, keeps it as ans, and under its name for an assignment.
    ExpressionResult evaluate(std::string_view expression);
    // The same, keeping nothing: what the expression comes to as it is being written.
    [[nodiscard]] ExpressionResult preview(std::string_view expression) const;

    // A word that is not to be a name, as a command of the prompt the calculator is in: "exit = 1" is a mistake.
    void reserve(std::string word);

    void set_angle_unit(AngleUnit unit);
    [[nodiscard]] AngleUnit angle_unit() const;
    [[nodiscard]] const std::map<std::string, double>& variables() const;
    [[nodiscard]] std::optional<double> answer() const;

  private:
    std::map<std::string, double> m_variables;
    std::optional<double> m_answer;
    std::set<std::string> m_reserved;
    AngleUnit m_angle_unit{AngleUnit::radians};
};

// A value as a calculator shows it: to 15 significant digits, so 0.1 + 0.2 shows as 0.3, without zeros at the end,
// and with an exponent, as 1.5e20, when it is very large or very small.
[[nodiscard]] std::string format_number(double value);
// A whole value in base 2, 8 or 16, with its prefix, as 0x1F; none for a value that is not whole, or too large to be
// exact (beyond 2^53). A negative one keeps its sign: -0x1F.
[[nodiscard]] std::optional<std::string> format_in_base(double value, int base);
// The functions the calculator knows, as each is written with what it takes, and its constants.
[[nodiscard]] std::vector<std::string> calculator_functions();
[[nodiscard]] std::vector<std::string> calculator_constants();

} // namespace olagarro::oui
