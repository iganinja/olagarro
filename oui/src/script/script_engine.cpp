#include <oui/script/script_engine.h>

#include <oui/file_system.h>

#include <quickjs.h>

#include <cstring>
#include <limits>
#include <memory>
#include <string>

namespace olagarro::oui::script
{

namespace detail
{

// The engine's state, shared by every value it handed out: it lives until the engine and all of them go.
class Runtime : public std::enable_shared_from_this<Runtime>
{
  public:
    Runtime() : runtime{JS_NewRuntime()}, context{JS_NewContext(runtime)}
    {
        JS_SetContextOpaque(context, this);
        // A rejection is told only if it is still not handled once the work in hand is done, as a later catch
        // can still handle it.
        JS_SetHostPromiseRejectionTracker(
            runtime,
            [](JSContext* context, JSValueConst promise, JSValueConst reason, bool is_handled, void* opaque)
            {
                auto& self{*static_cast<Runtime*>(opaque)};
                if(is_handled)
                {
                    for(auto rejection{self.rejections.begin()}; rejection != self.rejections.end(); ++ rejection)
                    {
                        if(JS_IsSameValue(context, rejection->first, promise))
                        {
                            JS_FreeValue(context, rejection->first);
                            self.rejections.erase(rejection);
                            return;
                        }
                    }
                    return;
                }
                auto text{self.text_of(reason)};
                if(JS_IsError(reason))
                {
                    const auto stack{JS_GetPropertyStr(context, reason, "stack")};
                    if(JS_IsString(stack))
                    {
                        text += "\n" + self.text_of(stack);
                    }
                    JS_FreeValue(context, stack);
                }
                self.rejections.emplace_back(JS_DupValue(context, promise), std::move(text));
            },
            this);
        JS_SetInterruptHandler(
            runtime, [](JSRuntime*, void* opaque) -> int
            { return std::chrono::steady_clock::now() > static_cast<Runtime*>(opaque)->deadline ? 1 : 0; }, this);
        // Imports read their module from disk; QuickJS's own normaliser resolves "./" and "../" from the
        // importing module's folder.
        JS_SetModuleLoaderFunc(runtime, nullptr, load_module, nullptr);
    }

    // Reads and compiles the module at `path`, or throws in the script that imported it.
    static JSModuleDef* load_module(JSContext* context, const char* path, void*)
    {
        const auto source{read_file(path_from_utf8(path))};
        if(!source)
        {
            JS_ThrowReferenceError(context, "cannot read module '%s'", path);
            return nullptr;
        }
        const auto compiled{
            JS_Eval(context, source->c_str(), source->size(), path, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY)};
        if(JS_IsException(compiled))
        {
            return nullptr;
        }
        auto* const module{static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(compiled))};
        JS_FreeValue(context, compiled);
        return module;
    }
    ~Runtime()
    {
        for(auto& rejection : rejections)
        {
            JS_FreeValue(context, rejection.first);
        }
        JS_FreeContext(context);
        JS_FreeRuntime(runtime);
    }
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    [[nodiscard]] Value wrap(JSValue value) { return Value{std::make_shared<Handle>(shared_from_this(), value)}; }
    [[nodiscard]] static JSValueConst raw(const Value& value);

    // The exception the engine has pending, as a ScriptError; thrown as a ScriptStop when the script ended itself.
    [[nodiscard]] ScriptError pending_error()
    {
        const auto exception{JS_GetException(context)};
        // Running out of time is uncatchable too, which is why the runtime remembers it was a stop.
        if(stopped && JS_IsUncatchableError(exception))
        {
            // Out, and over: an uncatchable error after this one is not a stop unless a stop says so again.
            stopped = false;
            JS_FreeValue(context, exception);
            throw ScriptStop{};
        }
        auto text{text_of(exception)};
        if(JS_IsError(exception))
        {
            const auto stack{JS_GetPropertyStr(context, exception, "stack")};
            if(JS_IsString(stack))
            {
                if(const auto where{text_of(stack)}; !where.empty())
                {
                    text += "\n" + where;
                }
            }
            JS_FreeValue(context, stack);
        }
        JS_FreeValue(context, exception);
        while(!text.empty() && text.back() == '\n')
        {
            text.pop_back();
        }
        return ScriptError{text};
    }

    [[nodiscard]] std::string text_of(JSValueConst value) const
    {
        std::size_t length{};
        const auto* const text{JS_ToCStringLen(context, &length, value)};
        if(text == nullptr)
        {
            JS_FreeValue(context, JS_GetException(context));
            return {};
        }
        std::string result{text, length};
        JS_FreeCString(context, text);
        return result;
    }

    // Runs `work` against the clock, then whatever promises it settled; the clock starts with the outermost.
    template <typename Work> auto timed(Work&& work)
    {
        const auto outermost{depth == 0};
        if(outermost)
        {
            deadline = std::chrono::steady_clock::now() + limit;
        }
        ++ depth;
        struct Leave
        {
            Runtime& runtime;
            bool outermost;
            ~Leave()
            {
                -- runtime.depth;
                if(outermost)
                {
                    runtime.deadline = std::chrono::steady_clock::time_point::max();
                }
            }
        } leave{*this, outermost};
        auto result{work()};
        if(outermost)
        {
            run_jobs();
            tell_rejections();
        }
        return result;
    }

    // The rejections nothing handled by the end of the work.
    void tell_rejections()
    {
        auto rejected{std::move(rejections)};
        rejections.clear();
        for(auto& [promise, text] : rejected)
        {
            JS_FreeValue(context, promise);
            if(unhandled_rejection)
            {
                unhandled_rejection(text);
            }
        }
    }

    void run_jobs()
    {
        JSContext* job_context{};
        while(true)
        {
            const auto status{JS_ExecutePendingJob(runtime, &job_context)};
            if(status == 0)
            {
                return;
            }
            if(status < 0)
            {
                throw pending_error();
            }
        }
    }

    JSRuntime* runtime;
    JSContext* context;
    std::vector<ScriptEngine::Function> functions;
    std::function<void(std::string_view text)> print;
    std::function<void(std::string_view error)> unhandled_rejection;
    std::vector<std::pair<JSValue, std::string>> rejections;
    std::chrono::milliseconds limit{5000};
    std::chrono::steady_clock::time_point deadline{std::chrono::steady_clock::time_point::max()};
    int depth{};
    // A function defined from C++ threw ScriptStop: the script is ending itself, and does not start again.
    bool stopped{};
};

struct Handle
{
    Handle(std::shared_ptr<Runtime> runtime, JSValue value) : runtime{std::move(runtime)}, value{value} {}
    ~Handle() { JS_FreeValue(runtime->context, value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    std::shared_ptr<Runtime> runtime;
    JSValue value;
};

JSValueConst Runtime::raw(const Value& value)
{
    return value.m_handle == nullptr ? JS_UNDEFINED : value.m_handle->value;
}

} // namespace detail

namespace
{

// How scripts reach a defined function: its index in the engine's table is the function's magic number.
JSValue call_function(JSContext* context, JSValueConst, int count, JSValueConst* values, int magic, JSValueConst*)
{
    auto& runtime{*static_cast<detail::Runtime*>(JS_GetContextOpaque(context))};
    std::vector<Value> arguments;
    for(int index{}; index < count; ++ index)
    {
        arguments.push_back(runtime.wrap(JS_DupValue(context, values[index])));
    }
    try
    {
        const auto result{runtime.functions.at(static_cast<std::size_t>(magic))(arguments)};
        return JS_DupValue(context, detail::Runtime::raw(result));
    }
    catch(const ScriptStop&)
    {
        // An error no catch takes, all the way out, where pending_error makes it a ScriptStop again.
        runtime.stopped = true;
        const auto error{JS_NewError(context)};
        JS_SetUncatchableError(context, error);
        return JS_Throw(context, error);
    }
    catch(const std::exception& failure)
    {
        return JS_ThrowTypeError(context, "%s", failure.what());
    }
}

} // namespace

Value::Value() = default;
Value::Value(std::shared_ptr<detail::Handle> handle) : m_handle{std::move(handle)}
{
}

bool Value::is_undefined() const
{
    return m_handle == nullptr || JS_IsUndefined(m_handle->value);
}
bool Value::is_null() const
{
    return m_handle != nullptr && JS_IsNull(m_handle->value);
}
bool Value::is_bool() const
{
    return m_handle != nullptr && JS_IsBool(m_handle->value);
}
bool Value::is_number() const
{
    return m_handle != nullptr && JS_IsNumber(m_handle->value);
}
bool Value::is_string() const
{
    return m_handle != nullptr && JS_IsString(m_handle->value);
}
bool Value::is_object() const
{
    return m_handle != nullptr && JS_IsObject(m_handle->value);
}
bool Value::is_array() const
{
    return m_handle != nullptr && JS_IsArray(m_handle->value);
}
bool Value::is_function() const
{
    return m_handle != nullptr && JS_IsFunction(m_handle->runtime->context, m_handle->value);
}
bool Value::is_regexp() const
{
    return m_handle != nullptr && JS_IsRegExp(m_handle->value);
}

std::string Value::to_string() const
{
    return m_handle == nullptr ? "undefined" : m_handle->runtime->text_of(m_handle->value);
}

bool Value::to_bool() const
{
    return m_handle != nullptr && JS_ToBool(m_handle->runtime->context, m_handle->value) == 1;
}

double Value::to_number() const
{
    double result{std::numeric_limits<double>::quiet_NaN()};
    if(m_handle != nullptr && JS_ToFloat64(m_handle->runtime->context, &result, m_handle->value) != 0)
    {
        JS_FreeValue(m_handle->runtime->context, JS_GetException(m_handle->runtime->context));
    }
    return result;
}

Value Value::get(std::string_view property) const
{
    if(!is_object())
    {
        return {};
    }
    const std::string name{property};
    auto& runtime{*m_handle->runtime};
    const auto value{JS_GetPropertyStr(runtime.context, m_handle->value, name.c_str())};
    if(JS_IsException(value))
    {
        throw runtime.pending_error();
    }
    return runtime.wrap(value);
}

bool Value::is_float_array() const
{
    return m_handle != nullptr && JS_GetTypedArrayType(m_handle->value) == JS_TYPED_ARRAY_FLOAT32;
}

std::vector<float> Value::to_floats() const
{
    if(!is_float_array())
    {
        throw ScriptError{"a Float32Array is wanted"};
    }
    auto* const context{m_handle->runtime->context};
    std::size_t offset{};
    std::size_t length{};
    std::size_t element_size{};
    const auto buffer{JS_GetTypedArrayBuffer(context, m_handle->value, &offset, &length, &element_size)};
    if(JS_IsException(buffer))
    {
        throw m_handle->runtime->pending_error();
    }
    std::size_t size{};
    const auto* const bytes{JS_GetArrayBuffer(context, &size, buffer)};
    std::vector<float> values(length / sizeof(float));
    if(bytes != nullptr && offset + length <= size)
    {
        std::memcpy(values.data(), bytes + offset, values.size() * sizeof(float));
    }
    JS_FreeValue(context, buffer);
    return values;
}

std::size_t Value::length() const
{
    if(!is_object())
    {
        return 0;
    }
    std::int64_t length{};
    if(JS_GetLength(m_handle->runtime->context, m_handle->value, &length) != 0)
    {
        throw m_handle->runtime->pending_error();
    }
    return static_cast<std::size_t>(length);
}

Value Value::at(std::size_t index) const
{
    if(!is_object())
    {
        return {};
    }
    auto& runtime{*m_handle->runtime};
    const auto value{JS_GetPropertyUint32(runtime.context, m_handle->value, static_cast<std::uint32_t>(index))};
    if(JS_IsException(value))
    {
        throw runtime.pending_error();
    }
    return runtime.wrap(value);
}

std::vector<std::string> Value::keys() const
{
    std::vector<std::string> result;
    if(!is_object())
    {
        return result;
    }
    auto* const context{m_handle->runtime->context};
    JSPropertyEnum* properties{};
    std::uint32_t count{};
    if(JS_GetOwnPropertyNames(context, &properties, &count, m_handle->value, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) !=
       0)
    {
        throw m_handle->runtime->pending_error();
    }
    for(std::uint32_t index{}; index < count; ++ index)
    {
        const auto* const name{JS_AtomToCString(context, properties[index].atom)};
        result.emplace_back(name);
        JS_FreeCString(context, name);
    }
    JS_FreePropertyEnum(context, properties, count);
    return result;
}

Value Value::call(const std::vector<Value>& arguments) const
{
    if(!is_function())
    {
        throw ScriptError{"TypeError: not a function"};
    }
    auto& runtime{*m_handle->runtime};
    std::vector<JSValueConst> values;
    for(const auto& argument : arguments)
    {
        values.push_back(detail::Runtime::raw(argument));
    }
    return runtime.timed(
        [&]
        {
            const auto result{JS_Call(runtime.context, m_handle->value, JS_UNDEFINED, static_cast<int>(values.size()),
                                      values.data())};
            if(JS_IsException(result))
            {
                throw runtime.pending_error();
            }
            return runtime.wrap(result);
        });
}

// api(everywhere): console.log(...values: any[]): void
// Writes the values on a line, apart by spaces: to the terminal in oui_run, to the output tab in OUI Code.
ScriptEngine::ScriptEngine() : m_runtime{std::make_shared<detail::Runtime>()}
{
    define("console.log",
           [runtime{m_runtime.get()}](const std::vector<Value>& arguments)
           {
               std::string line;
               for(const auto& argument : arguments)
               {
                   line += (line.empty() ? "" : " ") + argument.to_string();
               }
               if(runtime->print)
               {
                   runtime->print(line);
               }
               return Value{};
           });
}

ScriptEngine::~ScriptEngine() = default;

void ScriptEngine::run(std::string_view source, std::string_view name)
{
    const std::string text{source};
    const std::string file{name};
    auto& runtime{*m_runtime};
    runtime.timed(
        [&]
        {
            const auto result{JS_Eval(runtime.context, text.c_str(), text.size(), file.c_str(),
                                      JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_STRICT)};
            if(JS_IsException(result))
            {
                throw runtime.pending_error();
            }
            JS_FreeValue(runtime.context, result);
            return 0;
        });
}

void ScriptEngine::run_module(std::string_view source, const std::filesystem::path& path)
{
    const std::string text{source};
    // With '/' between folders, the only separator QuickJS resolves relative imports by.
    const auto file{utf8_of(path)};
    auto& runtime{*m_runtime};
    runtime.timed(
        [&]
        {
            const auto result{JS_Eval(runtime.context, text.c_str(), text.size(), file.c_str(), JS_EVAL_TYPE_MODULE)};
            if(JS_IsException(result))
            {
                throw runtime.pending_error();
            }
            // A module runs as a promise, settled once its top level has run, awaits included. Failed already,
            // it is this run that failed; still pending, what it waits on settles it later.
            if(JS_PromiseState(runtime.context, result) == JS_PROMISE_REJECTED)
            {
                const auto reason{JS_PromiseResult(runtime.context, result)};
                // Told here, so not again as a rejection nobody handled.
                for(auto rejection{runtime.rejections.begin()}; rejection != runtime.rejections.end();)
                {
                    if(JS_IsSameValue(runtime.context, rejection->first, result))
                    {
                        JS_FreeValue(runtime.context, rejection->first);
                        rejection = runtime.rejections.erase(rejection);
                    }
                    else
                    {
                        ++ rejection;
                    }
                }
                JS_FreeValue(runtime.context, result);
                JS_Throw(runtime.context, reason);
                throw runtime.pending_error();
            }
            JS_FreeValue(runtime.context, result);
            return 0;
        });
}

void ScriptEngine::define(std::string_view path, Function function)
{
    auto& runtime{*m_runtime};
    auto* const context{runtime.context};
    auto holder{JS_GetGlobalObject(context)};
    std::string_view rest{path};
    while(true)
    {
        const auto dot{rest.find('.')};
        const std::string name{rest.substr(0, dot)};
        if(dot == std::string_view::npos)
        {
            runtime.functions.push_back(std::move(function));
            const auto magic{static_cast<int>(runtime.functions.size() - 1)};
            JS_SetPropertyStr(context, holder, name.c_str(),
                              JS_NewCFunctionData(context, call_function, 0, magic, 0, nullptr));
            JS_FreeValue(context, holder);
            return;
        }
        auto next{JS_GetPropertyStr(context, holder, name.c_str())};
        if(!JS_IsObject(next))
        {
            JS_FreeValue(context, next);
            next = JS_NewObject(context);
            JS_SetPropertyStr(context, holder, name.c_str(), JS_DupValue(context, next));
        }
        JS_FreeValue(context, holder);
        holder = next;
        rest.remove_prefix(dot + 1);
    }
}

void ScriptEngine::set_print(std::function<void(std::string_view text)> print)
{
    m_runtime->print = std::move(print);
}

void ScriptEngine::set_time_limit(std::chrono::milliseconds limit)
{
    m_runtime->limit = limit;
}

void ScriptEngine::set_unhandled_rejection(std::function<void(std::string_view error)> report)
{
    m_runtime->unhandled_rejection = std::move(report);
}

ScriptEngine::Promise ScriptEngine::promise()
{
    JSValue settle[2];
    const auto promise{JS_NewPromiseCapability(m_runtime->context, settle)};
    if(JS_IsException(promise))
    {
        throw m_runtime->pending_error();
    }
    return {m_runtime->wrap(promise), m_runtime->wrap(settle[0]), m_runtime->wrap(settle[1])};
}

Value ScriptEngine::string(std::string_view text)
{
    return m_runtime->wrap(JS_NewStringLen(m_runtime->context, text.data(), text.size()));
}

Value ScriptEngine::number(double value)
{
    return m_runtime->wrap(JS_NewFloat64(m_runtime->context, value));
}

Value ScriptEngine::boolean(bool value)
{
    return m_runtime->wrap(JS_NewBool(m_runtime->context, value));
}

Value ScriptEngine::undefined()
{
    return {};
}

Value ScriptEngine::null()
{
    return m_runtime->wrap(JS_NULL);
}

Value ScriptEngine::float_array(const float* values, std::size_t count)
{
    auto* const context{m_runtime->context};
    auto buffer{JS_NewArrayBufferCopy(context, reinterpret_cast<const std::uint8_t*>(values), count * sizeof(float))};
    const auto array{JS_NewTypedArray(context, 1, &buffer, JS_TYPED_ARRAY_FLOAT32)};
    JS_FreeValue(context, buffer);
    return m_runtime->wrap(array);
}

Value ScriptEngine::array(const std::vector<Value>& elements)
{
    auto* const context{m_runtime->context};
    const auto result{JS_NewArray(context)};
    for(std::size_t index{}; index < elements.size(); ++ index)
    {
        JS_SetPropertyUint32(context, result, static_cast<std::uint32_t>(index),
                             JS_DupValue(context, detail::Runtime::raw(elements[index])));
    }
    return m_runtime->wrap(result);
}

} // namespace olagarro::oui::script
