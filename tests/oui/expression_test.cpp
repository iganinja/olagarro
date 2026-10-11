#include <oui/application.h>
#include <oui/calculator_view.h>
#include <oui/scripted_backend.h>
#include <oui/expression.h>

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <string_view>

using namespace olagarro;

namespace
{

using oui::AngleUnit;
using oui::Calculator;

// What the expression comes to as a calculator shows it, or "error: " and why, with where.
[[nodiscard]] std::string shown(Calculator& calculator, std::string_view expression)
{
    const auto result{calculator.evaluate(expression)};
    if(result.error)
    {
        return "error at " + std::to_string(result.error->position) + ": " + result.error->message;
    }
    return result.value ? oui::format_number(*result.value) : "nothing";
}

[[nodiscard]] std::string shown(std::string_view expression)
{
    Calculator calculator;
    return shown(calculator, expression);
}

[[nodiscard]] bool fails(std::string_view expression)
{
    Calculator calculator;
    return calculator.evaluate(expression).error.has_value();
}

} // namespace

TEST(OuiExpressionTests, WorksOutWhatOneWrites)
{
    EXPECT_EQ(shown("(800*0.16) - (0.32 * 500)"), "-32");
    EXPECT_EQ(shown("1 + 2 * 3"), "7");
    EXPECT_EQ(shown("(1 + 2) * 3"), "9");
    EXPECT_EQ(shown("10 / 4"), "2.5");
    EXPECT_EQ(shown("7 mod 3"), "1");
    EXPECT_EQ(shown("  2*  3 "), "6");
}

TEST(OuiExpressionTests, ShowsWhatFloatingPointWouldGetWrong)
{
    EXPECT_EQ(shown("0.1 + 0.2"), "0.3");
    EXPECT_EQ(shown("1/3"), "0.333333333333333");
    EXPECT_EQ(shown("2/3"), "0.666666666666667");
    EXPECT_EQ(shown("1 - 0.9"), "0.1");
}

TEST(OuiExpressionTests, ReadsNumbersAsTheyAreWritten)
{
    EXPECT_EQ(shown(".5 + 0.5"), "1");
    EXPECT_EQ(shown("1e3"), "1000");
    EXPECT_EQ(shown("2.5E-2"), "0.025");
    EXPECT_EQ(shown("1_000_000"), "1000000");
    EXPECT_EQ(shown("0x1F + 0b101 + 0o17"), "51");
}

TEST(OuiExpressionTests, PowersAndSignsGoAsInMathematics)
{
    EXPECT_EQ(shown("2^10"), "1024");
    EXPECT_EQ(shown("2**3"), "8");
    EXPECT_EQ(shown("-2^2"), "-4");
    EXPECT_EQ(shown("2^3^2"), "512");
    EXPECT_EQ(shown("2^-1"), "0.5");
    EXPECT_EQ(shown("--3"), "3");
    EXPECT_EQ(shown("3 - -2"), "5");
    EXPECT_EQ(shown("5!"), "120");
    EXPECT_EQ(shown("3!^2"), "36");
    EXPECT_EQ(shown("√16 + √(9)"), "7");
    EXPECT_EQ(shown("6 × 7 ÷ 2"), "21");
}

TEST(OuiExpressionTests, APercentageIsAHundredthAndAShareOfWhatItIsAddedTo)
{
    EXPECT_EQ(shown("800 * 16%"), "128");
    EXPECT_EQ(shown("800 + 16%"), "928");
    EXPECT_EQ(shown("800 - 25%"), "600");
    EXPECT_EQ(shown("50%"), "0.5");
    EXPECT_EQ(shown("200 + (10+10)%"), "240");
}

TEST(OuiExpressionTests, ANameOrABracketNextToAValueMultipliesIt)
{
    EXPECT_EQ(shown("3(4+1)"), "15");
    EXPECT_EQ(shown("(1+2)(3+4)"), "21");
    EXPECT_EQ(shown("2pi"), shown("2*pi"));
    EXPECT_EQ(shown("2 sqrt(9)"), "6");
    // Two numbers side by side are a slip, not a product.
    EXPECT_TRUE(fails("2 3"));
}

TEST(OuiExpressionTests, KnowsTheUsualFunctionsAndConstants)
{
    EXPECT_EQ(shown("sqrt(2)^2"), "2");
    EXPECT_EQ(shown("abs(-3) + round(2.5) + floor(2.7) + ceil(2.1)"), "11");
    EXPECT_EQ(shown("round(3.14159, 2)"), "3.14");
    EXPECT_EQ(shown("ln(e)"), "1");
    EXPECT_EQ(shown("log(1000)"), "3");
    EXPECT_EQ(shown("log(8, 2)"), "3");
    EXPECT_EQ(shown("log2(1024)"), "10");
    EXPECT_EQ(shown("min(4, 2, 8) + max(1, 9)"), "11");
    EXPECT_EQ(shown("cos(pi)"), "-1");
    EXPECT_EQ(shown("π"), shown("pi"));
    EXPECT_EQ(shown("hypot(3, 4)"), "5");
    EXPECT_EQ(shown("gcd(12, 18) + lcm(4, 6)"), "18");
}

TEST(OuiExpressionTests, AnglesInDegreesGiveTheirExactValues)
{
    Calculator calculator;
    calculator.set_angle_unit(AngleUnit::degrees);
    EXPECT_EQ(shown(calculator, "sin(30)"), "0.5");
    EXPECT_EQ(shown(calculator, "cos(90)"), "0");
    EXPECT_EQ(shown(calculator, "sin(180)"), "0");
    EXPECT_EQ(shown(calculator, "tan(45)"), "1");
    EXPECT_EQ(shown(calculator, "asin(1)"), "90");
    EXPECT_NE(shown(calculator, "tan(90)").find("error"), std::string::npos);
}

TEST(OuiExpressionTests, KeepsNamesAndTheLastValue)
{
    Calculator calculator;
    EXPECT_EQ(shown(calculator, "price = 800"), "800");
    EXPECT_EQ(calculator.variables().at("price"), 800);
    EXPECT_EQ(shown(calculator, "price * 16%"), "128");
    EXPECT_EQ(shown(calculator, "ans + 1"), "129");
    // A preview keeps nothing.
    EXPECT_TRUE(calculator.preview("other = 1").value);
    EXPECT_EQ(calculator.variables().count("other"), 0u);
    EXPECT_EQ(shown(calculator, "ans"), "129");
}

TEST(OuiExpressionTests, SaysWhatIsWrongAndWhere)
{
    EXPECT_EQ(shown("1 +"), "error at 3: Something is missing at the end");
    EXPECT_EQ(shown("(1 + 2"), "error at 6: A ) is missing");
    EXPECT_EQ(shown("1 + 2)"), "error at 5: Unexpected )");
    EXPECT_EQ(shown("2 * banana"), "error at 4: Unknown name banana");
    EXPECT_EQ(shown("1 / 0"), "error at 2: Division by zero");
    EXPECT_EQ(shown("sqrt(-1)"), "error at 0: sqrt(-1) is not a real number");
    EXPECT_EQ(shown("sqrt(1, 2)"), "error at 0: sqrt takes 1 value, not 2");
    EXPECT_EQ(shown("sqrt"), "error at 0: sqrt is a function: write sqrt(...)");
    EXPECT_EQ(shown("2.5!"), "error at 3: ! takes a whole number, 0 or more");
    EXPECT_EQ(shown("10^400"), "error at 2: The result is too large");
    EXPECT_EQ(shown("1 $ 2"), "error at 2: Unexpected $");
    EXPECT_EQ(shown("pi = 3"), "error at 0: pi is a constant");
    EXPECT_EQ(shown("1.2.3"), "error at 3: Unexpected .3");
}

TEST(OuiExpressionTests, NothingWrittenIsNothingToWorkOut)
{
    Calculator calculator;
    const auto result{calculator.evaluate("   ")};
    EXPECT_FALSE(result.value);
    EXPECT_FALSE(result.error);
}

TEST(OuiExpressionTests, FormatsLargeSmallAndOtherBases)
{
    EXPECT_EQ(oui::format_number(1.5e20), "1.5e20");
    EXPECT_EQ(oui::format_number(-2.5e-7), "-2.5e-7");
    EXPECT_EQ(oui::format_number(-0.0), "0");
    EXPECT_EQ(oui::format_number(123456789012345), "123456789012345");
    EXPECT_EQ(oui::format_in_base(255, 16), std::optional<std::string>{"0xFF"});
    EXPECT_EQ(oui::format_in_base(5, 2), std::optional<std::string>{"0b101"});
    EXPECT_EQ(oui::format_in_base(-15, 8), std::optional<std::string>{"-0o17"});
    EXPECT_FALSE(oui::format_in_base(2.5, 16));
    EXPECT_FALSE(oui::format_in_base(std::pow(2.0, 60), 16));
}

TEST(OuiExpressionTests, ANameKeptIsUsedInTheExpressionsThatFollow)
{
    Calculator calculator;
    EXPECT_EQ(shown(calculator, "interes = 800*0.03"), "24");
    EXPECT_EQ(shown(calculator, "interes + 800"), "824");
    // Letters beyond ASCII are letters of a name too.
    EXPECT_EQ(shown(calculator, "interés = interes * 2"), "48");
    EXPECT_EQ(shown(calculator, "interés + 2interes"), "96");
    // Kept again, it changes.
    EXPECT_EQ(shown(calculator, "interes = 1"), "1");
    EXPECT_EQ(shown(calculator, "interes + 800"), "801");
}

TEST(OuiCalculatorHelpTests, ListsTheKeysOfTheProgramThatShowsIt)
{
    // Alone, the calculator's own keys; a program adds its own, as oui_calc's Ctrl+Q.
    const auto alone{oui::calculator_help()};
    EXPECT_NE(alone.find("| F1 |"), std::string::npos);
    EXPECT_EQ(alone.find("Ctrl+Q"), std::string::npos);
    const auto in_a_program{oui::calculator_help({{"Ctrl+Q", "Quit"}})};
    EXPECT_NE(in_a_program.find("| Ctrl+Q | Quit |"), std::string::npos);
    // In the table of keys, after F1.
    EXPECT_GT(in_a_program.find("| Ctrl+Q |"), in_a_program.find("| F1 |"));
}

TEST(OuiCalculatorViewTests, WhatWasWrittenComesBackWithItsLogAndItsValues)
{
    oui::Application application{oui::scripted_backend({})};
    oui::CalculatorView first{application};
    for(const auto* const line : {"interes = 800*0.03", "2 * banana", "deg", "sin(30)"})
    {
        first.prompt().set_text(line);
        first.submit();
    }
    // What went wrong never went into the log, so it is not kept.
    EXPECT_EQ(first.entries(), (std::vector<std::string>{"interes = 800*0.03", "deg", "sin(30)"}));

    oui::CalculatorView again{application};
    again.replay(first.entries());
    EXPECT_EQ(again.log(), first.log());
    again.prompt().set_text("interes + 1");
    again.submit();
    EXPECT_NE(again.log().find("= 25"), std::string_view::npos);

    again.prompt().set_text("clear");
    again.submit();
    EXPECT_TRUE(again.entries().empty());
}

TEST(OuiExpressionTests, AWordReservedForTheCalculatorIsNoName)
{
    Calculator calculator;
    calculator.reserve("exit");
    EXPECT_EQ(shown(calculator, "exit = 3"), "error at 0: exit is a word of the calculator's own");
    EXPECT_EQ(shown(calculator, "exits = 3"), "3");
}

TEST(OuiCalculatorViewTests, ExitAsksToCloseAndItsCommandsAreNoNames)
{
    oui::Application application{oui::scripted_backend({})};
    oui::CalculatorView view{application};
    auto asked{0};
    auto connection{view.on_exit([&asked] { ++ asked; })};
    for(const auto* const word : {"exit", "deg", "rad", "clear", "help"})
    {
        view.prompt().set_text(std::string{word} + " = 1");
        view.submit();
        EXPECT_TRUE(view.entries().empty()) << word << " is not a name to keep";
    }
    view.prompt().set_text("exit");
    view.submit();
    EXPECT_EQ(asked, 1);
}
