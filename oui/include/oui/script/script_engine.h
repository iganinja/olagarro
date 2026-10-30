#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace olagarro::oui::script
{

namespace detail
{
class Runtime;
struct Handle;
} // namespace detail

// What went wrong in a script: its message and, where the engine knows it, the place, as "file:line".
class ScriptError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

// The script ending itself, as oui_run's oui.exit does: nothing went wrong, so there is nothing to tell, and no catch
// or finally of the script's runs on the way out. A function defined from C++ throws it to end the script there.
class ScriptStop : public ScriptError
{
  public:
    ScriptStop() : ScriptError{"the script ended itself"} {}
};

// A JavaScript value held from C++. Copies share the value, which lives while any copy does.
class Value
{
  public:
    // undefined
    Value();

    [[nodiscard]] bool is_undefined() const;
    [[nodiscard]] bool is_null() const;
    [[nodiscard]] bool is_bool() const;
    [[nodiscard]] bool is_number() const;
    [[nodiscard]] bool is_string() const;
    [[nodiscard]] bool is_object() const;
    [[nodiscard]] bool is_array() const;
    [[nodiscard]] bool is_function() const;
    [[nodiscard]] bool is_regexp() const;
    [[nodiscard]] bool is_float_array() const;

    // As JavaScript converts them: String(value), Boolean(value), Number(value).
    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] bool to_bool() const;
    [[nodiscard]] double to_number() const;

    // A property; undefined when it has none.
    [[nodiscard]] Value get(std::string_view property) const;
    // A Float32Array's numbers, copied; a ScriptError for anything else. Pixels go back and forth this way, whole,
    // rather than a number at a time.
    [[nodiscard]] std::vector<float> to_floats() const;
    // An array's length and its elements.
    [[nodiscard]] std::size_t length() const;
    [[nodiscard]] Value at(std::size_t index) const;
    // An object's own enumerable property names, in their order.
    [[nodiscard]] std::vector<std::string> keys() const;

    // Calls a function with `arguments`. Throws ScriptError when it throws.
    Value call(const std::vector<Value>& arguments = {}) const;

  private:
    friend class ScriptEngine;
    friend class detail::Runtime;
    explicit Value(std::shared_ptr<detail::Handle> handle);

    std::shared_ptr<detail::Handle> m_handle;
};

// Runs JavaScript in QuickJS, with what C++ gives scripts to reach. Scripts run in strict mode, on one thread at a
// time (the one that made the engine or another it is handed to), and each run or call has a time limit, so a script
// that loops forever is stopped with an error instead of hanging the program.
class ScriptEngine
{
  public:
    // A C++ function as scripts call it. Throwing ScriptError, or any std::exception, makes the call throw a TypeError
    // in the script, with its message; throwing ScriptStop ends the script there.
    using Function = std::function<Value(const std::vector<Value>& arguments)>;

    ScriptEngine();
    ~ScriptEngine();
    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;

    // Runs `source`; `name` names it in error messages. Throws ScriptError.
    void run(std::string_view source, std::string_view name);
    // Runs `source` as a module, which can `import` others: `path` is its file, and a relative import resolves
    // from the folder it is in, as `import {x} from "./tools.js"`. Imported modules are read from disk. Throws
    // ScriptError when it, or anything it imports, fails while it runs.
    void run_module(std::string_view source, const std::filesystem::path& path);
    // Makes `function` callable as `path`, such as "oui.languages.register", making the objects on the way.
    void define(std::string_view path, Function function);
    // What console.log prints goes here; nowhere unless set.
    void set_print(std::function<void(std::string_view text)> print);
    // How long a run or a call may take; 5 seconds unless set.
    void set_time_limit(std::chrono::milliseconds limit);
    // A promise rejected with nothing to catch it, as when an async function throws after an await, comes
    // here with its error, instead of going unseen; nowhere unless set.
    void set_unhandled_rejection(std::function<void(std::string_view error)> report);

    // A promise C++ settles later, such as when a process ends: a script awaits `promise`, and calling
    // `resolve` or `reject` with a value settles it.
    struct Promise
    {
        Value promise;
        Value resolve;
        Value reject;
    };
    [[nodiscard]] Promise promise();

    [[nodiscard]] Value string(std::string_view text);
    [[nodiscard]] Value number(double value);
    [[nodiscard]] Value boolean(bool value);
    [[nodiscard]] Value undefined();
    [[nodiscard]] Value null();
    [[nodiscard]] Value array(const std::vector<Value>& elements);
    // A Float32Array holding a copy of `count` numbers.
    [[nodiscard]] Value float_array(const float* values, std::size_t count);

  private:
    std::shared_ptr<detail::Runtime> m_runtime;
};

} // namespace olagarro::oui::script
