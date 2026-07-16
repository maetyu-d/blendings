#include "ScSheetFormula.h"

namespace
{
    class Parser
    {
    public:
        Parser(juce::String expressionToParse, const ScoreFormula::Variables& variablesToUse)
            : text(expressionToParse.toStdString()), variables(variablesToUse) {}

        double parse()
        {
            pos = 0;
            auto value = expression();
            skip();
            if (pos != text.size())
                fail("Unexpected input");
            return value;
        }

        juce::String getError() const { return error; }

    private:
        double expression()
        {
            auto value = term();
            for (;;)
            {
                skip();
                if (match('+')) value += term();
                else if (match('-')) value -= term();
                else return value;
            }
        }

        double term()
        {
            auto value = power();
            for (;;)
            {
                skip();
                if (match('*')) value *= power();
                else if (match('/'))
                {
                    const auto denom = power();
                    value = std::abs(denom) < 1.0e-12 ? 0.0 : value / denom;
                }
                else if (match('%'))
                {
                    const auto denom = power();
                    value = std::abs(denom) < 1.0e-12 ? 0.0 : std::fmod(value, denom);
                }
                else return value;
            }
        }

        double power()
        {
            auto value = unary();
            skip();
            if (match('^'))
                value = std::pow(value, power());
            return value;
        }

        double unary()
        {
            skip();
            if (match('+')) return unary();
            if (match('-')) return -unary();
            return primary();
        }

        double primary()
        {
            skip();
            if (match('('))
            {
                auto value = expression();
                if (! match(')')) fail("Expected ')'");
                return value;
            }

            if (std::isdigit(peek()) || peek() == '.')
                return number();

            if (std::isalpha(peek()) || peek() == '_')
                return identifier();

            fail("Expected value");
            return 0.0;
        }

        double number()
        {
            const auto start = pos;
            while (pos < text.size() && (std::isdigit(text[pos]) || text[pos] == '.'))
                ++pos;
            return juce::String(text.substr(start, pos - start)).getDoubleValue();
        }

        double identifier()
        {
            const auto start = pos;
            while (pos < text.size() && (std::isalnum(text[pos]) || text[pos] == '_'))
                ++pos;
            const auto name = text.substr(start, pos - start);
            skip();
            if (match('('))
            {
                std::vector<double> args;
                skip();
                if (! match(')'))
                {
                    for (;;)
                    {
                        args.push_back(expression());
                        skip();
                        if (match(')')) break;
                        if (! match(',')) fail("Expected ','");
                    }
                }
                return call(name, args);
            }

            if (name == "pi") return juce::MathConstants<double>::pi;
            if (name == "tau") return juce::MathConstants<double>::twoPi;
            if (name == "phi") return 1.6180339887498948;
            if (auto it = variables.find(name); it != variables.end())
                return it->second;
            return 0.0;
        }

        double call(const std::string& name, const std::vector<double>& args)
        {
            auto a = [&args] (size_t i, double fallback = 0.0) { return i < args.size() ? args[i] : fallback; };
            if (name == "sin") return std::sin(a(0));
            if (name == "cos") return std::cos(a(0));
            if (name == "tan") return std::tan(a(0));
            if (name == "abs") return std::abs(a(0));
            if (name == "floor") return std::floor(a(0));
            if (name == "ceil") return std::ceil(a(0));
            if (name == "round") return std::round(a(0));
            if (name == "sqrt") return std::sqrt(juce::jmax(0.0, a(0)));
            if (name == "pow") return std::pow(a(0), a(1, 1.0));
            if (name == "min") return juce::jmin(a(0), a(1));
            if (name == "max") return juce::jmax(a(0), a(1));
            if (name == "clamp") return juce::jlimit(a(1), a(2, 1.0), a(0));
            if (name == "wrap")
            {
                const auto value = std::fmod(a(0), 1.0);
                return value < 0.0 ? value + 1.0 : value;
            }
            if (name == "noise")
            {
                const auto value = std::sin((a(0) + 1.0) * 12.9898 + (a(1) + 1.0) * 78.233 + (a(2) + 1.0) * 37.719) * 43758.5453123;
                return (value - std::floor(value)) * 2.0 - 1.0;
            }
            fail("Unknown function: " + juce::String(name));
            return 0.0;
        }

        void skip()
        {
            while (pos < text.size() && std::isspace(text[pos]))
                ++pos;
        }

        char peek() const { return pos < text.size() ? text[pos] : '\0'; }

        bool match(char c)
        {
            skip();
            if (peek() != c)
                return false;
            ++pos;
            return true;
        }

        void fail(const juce::String& message)
        {
            if (error.isEmpty())
                error = message;
            throw std::runtime_error(message.toStdString());
        }

        std::string text;
        const ScoreFormula::Variables& variables;
        size_t pos = 0;
        juce::String error;
    };
}

juce::Result ScoreFormula::validate(const juce::String& expression)
{
    if (expression.trim().isEmpty())
        return juce::Result::ok();

    Variables vars;
    for (auto name : { "step", "steps", "beat", "bar", "phase", "row", "section", "phrase", "macro", "accent", "density", "amp", "root", "transpose" })
        vars.emplace(name, 1.0);

    Parser parser(expression, vars);
    try { (void) parser.parse(); }
    catch (...) { return juce::Result::fail(parser.getError()); }
    return juce::Result::ok();
}

double ScoreFormula::evaluate(const juce::String& expression, const Variables& variables, double fallback, juce::String* error)
{
    if (expression.trim().isEmpty())
        return fallback;

    Parser parser(expression, variables);
    try { return parser.parse(); }
    catch (...)
    {
        if (error != nullptr)
            *error = parser.getError();
        return fallback;
    }
}
