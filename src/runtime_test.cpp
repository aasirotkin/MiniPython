#include "runtime.h"
#include "test_runner_p.h"

#include <functional>

using namespace std;

namespace runtime {

namespace {
class Logger : public Object {
public:
    static int instance_count;

    explicit Logger(int value_ = 0)
        : id_(value_)  //
    {
        ++instance_count;
    }

    Logger(const Logger& rhs)
        : id_(rhs.id_)  //
    {
        ++instance_count;
    }

    Logger(Logger&& rhs) noexcept
        : id_(rhs.id_)  //
    {
        ++instance_count;
    }

    Logger& operator=(const Logger& /*rhs*/) = default;
    Logger& operator=(Logger&& /*rhs*/) = default;

    [[nodiscard]] int GetId() const {
        return id_;
    }

    ~Logger()  // NOLINT(hicpp-use-override,modernize-use-override)
    {
        --instance_count;
    }

    void Print(ostream& os, [[maybe_unused]] Context& context) override {
        os << id_;
    }

private:
    int id_;
};

int Logger::instance_count = 0;

void TestNumber() {
    Number num(127);

    DummyContext context;

    num.Print(context.output, context);
    ASSERT_EQUAL(context.output.str(), "127"s);
    ASSERT_EQUAL(num.GetValue(), 127);
}

void TestString() {
    String word("hello!"s);

    DummyContext context;
    word.Print(context.output, context);
    ASSERT_EQUAL(context.output.str(), "hello!"s);
    ASSERT_EQUAL(word.GetValue(), "hello!"s);
}

struct TestMethodBody : Executable {
    using Fn = std::function<ObjectHolder(Closure& closure, Context& context)>;
    Fn body;

    explicit TestMethodBody(Fn body)
        : body(std::move(body)) {
    }

    ObjectHolder Execute(Closure& closure, Context& context) override {
        if (body) {
            return body(closure, context);
        }
        return {};
    }
};

void TestMethodInvocation() {
    DummyContext context;
    Closure base_closure;
    auto base_method_1 = [&base_closure, &context](Closure& closure, Context& ctx) {
        ASSERT_EQUAL(&context, &ctx);
        base_closure = closure;
        return ObjectHolder::Own(Number{123});
    };
    auto base_method_2 = [&base_closure, &context](Closure& closure, Context& ctx) {
        ASSERT_EQUAL(&context, &ctx);
        base_closure = closure;
        return ObjectHolder::Own(Number{456});
    };
    vector<Method> base_methods;
    base_methods.push_back(
        {"test"s, {"arg1"s, "arg2"s}, make_unique<TestMethodBody>(base_method_1)});
    base_methods.push_back({"test_2"s, {"arg1"s}, make_unique<TestMethodBody>(base_method_2)});
    Class base_class{"Base"s, std::move(base_methods), nullptr};
    ClassInstance base_inst{base_class};
    base_inst.Fields()["base_field"s] = ObjectHolder::Own(String{"hello"s});
    ASSERT(base_inst.HasMethod("test"s, 2U));
    auto res = base_inst.Call(
        "test"s, {ObjectHolder::Own(Number{1}), ObjectHolder::Own(String{"abc"s})}, context);
    ASSERT(Equal(res, ObjectHolder::Own(Number{123}), context));
    ASSERT_EQUAL(base_closure.size(), 3U);
    ASSERT_EQUAL(base_closure.count("self"s), 1U);
    ASSERT_EQUAL(base_closure.at("self"s).Get(), &base_inst);
    ASSERT_EQUAL(base_closure.count("self"s), 1U);
    ASSERT_EQUAL(base_closure.count("arg1"s), 1U);
    ASSERT(Equal(base_closure.at("arg1"s), ObjectHolder::Own(Number{1}), context));
    ASSERT_EQUAL(base_closure.count("arg2"s), 1U);
    ASSERT(Equal(base_closure.at("arg2"s), ObjectHolder::Own(String{"abc"s}), context));
    ASSERT_EQUAL(base_closure.count("base_field"s), 0U);

    Closure child_closure;
    auto child_method_1 = [&child_closure, &context](Closure& closure, Context& ctx) {
        ASSERT_EQUAL(&context, &ctx);
        child_closure = closure;
        return ObjectHolder::Own(String("child"s));
    };
    vector<Method> child_methods;
    child_methods.push_back(
        {"test"s, {"arg1_child"s, "arg2_child"s}, make_unique<TestMethodBody>(child_method_1)});
    Class child_class{"Child"s, std::move(child_methods), &base_class};
    ClassInstance child_inst{child_class};
    ASSERT(child_inst.HasMethod("test"s, 2U));
    base_closure.clear();
    res = child_inst.Call(
        "test"s, {ObjectHolder::Own(String{"value1"s}), ObjectHolder::Own(String{"value2"s})},
        context);
    ASSERT(Equal(res, ObjectHolder::Own(String{"child"s}), context));
    ASSERT(base_closure.empty());
    ASSERT_EQUAL(child_closure.size(), 3U);
    ASSERT_EQUAL(child_closure.count("self"s), 1U);
    ASSERT_EQUAL(child_closure.at("self"s).Get(), &child_inst);
    ASSERT_EQUAL(child_closure.count("arg1_child"s), 1U);
    ASSERT(Equal(child_closure.at("arg1_child"s), (ObjectHolder::Own(String{"value1"s})), context));
    ASSERT_EQUAL(child_closure.count("arg2_child"s), 1U);
    ASSERT(Equal(child_closure.at("arg2_child"s), (ObjectHolder::Own(String{"value2"s})), context));

    ASSERT(child_inst.HasMethod("test_2"s, 1U));
    child_closure.clear();
    res = child_inst.Call("test_2"s, {ObjectHolder::Own(String{":)"s})}, context);
    ASSERT(Equal(res, ObjectHolder::Own(Number{456}), context));
    ASSERT_EQUAL(base_closure.size(), 2U);
    ASSERT_EQUAL(base_closure.count("self"s), 1U);
    ASSERT_EQUAL(base_closure.at("self"s).Get(), &child_inst);
    ASSERT_EQUAL(base_closure.count("arg1"s), 1U);
    ASSERT(Equal(base_closure.at("arg1"s), (ObjectHolder::Own(String{":)"s})), context));

    ASSERT(!child_inst.HasMethod("test"s, 1U));
    ASSERT_THROWS(child_inst.Call("test"s, {ObjectHolder::None()}, context), runtime_error);
}

void TestNonowning() {
    ASSERT_EQUAL(Logger::instance_count, 0);
    Logger logger(784);
    {
        auto oh = ObjectHolder::Share(logger);
        ASSERT(oh);
    }
    ASSERT_EQUAL(Logger::instance_count, 1);

    auto oh = ObjectHolder::Share(logger);
    ASSERT(oh);
    ASSERT(oh.Get() == &logger);

    DummyContext context;
    oh->Print(context.output, context);

    ASSERT_EQUAL(context.output.str(), "784"sv);
}

void TestOwning() {
    ASSERT_EQUAL(Logger::instance_count, 0);
    {
        auto oh = ObjectHolder::Own(Logger());
        ASSERT(oh);
        ASSERT_EQUAL(Logger::instance_count, 1);
    }
    ASSERT_EQUAL(Logger::instance_count, 0);

    auto oh = ObjectHolder::Own(Logger(312));
    ASSERT(oh);
    ASSERT_EQUAL(Logger::instance_count, 1);

    DummyContext context;
    oh->Print(context.output, context);

    ASSERT_EQUAL(context.output.str(), "312"sv);
}

void TestMove() {
    {
        ASSERT_EQUAL(Logger::instance_count, 0);
        Logger logger;

        auto one = ObjectHolder::Share(logger);
        ObjectHolder two = std::move(one);

        ASSERT_EQUAL(Logger::instance_count, 1);
        ASSERT(two.Get() == &logger);
    }
    {
        ASSERT_EQUAL(Logger::instance_count, 0);
        auto one = ObjectHolder::Own(Logger());
        ASSERT_EQUAL(Logger::instance_count, 1);
        Object* stored = one.Get();
        ObjectHolder two = std::move(one);
        ASSERT_EQUAL(Logger::instance_count, 1);

        ASSERT(two.Get() == stored);
        ASSERT(!one);  // NOLINT
    }
}

void TestNullptr() {
    ObjectHolder oh;
    ASSERT(!oh);
    ASSERT(!oh.Get());
}

void TestIsTrue() {
    {
        ObjectHolder oh_false = ObjectHolder::Own(Number(0));
        ObjectHolder oh_true = ObjectHolder::Own(Number(10));
        ASSERT(!IsTrue(oh_false));
        ASSERT(IsTrue(oh_true));
    }
    {
        ObjectHolder oh_false = ObjectHolder::Own(Bool(false));
        ObjectHolder oh_true = ObjectHolder::Own(Bool(true));
        ASSERT(!IsTrue(oh_false));
        ASSERT(IsTrue(oh_true));
    }
    {
        ObjectHolder oh_false = ObjectHolder::None();
        ASSERT(!IsTrue(oh_false));
    }
    {
        ObjectHolder oh_false;
        ASSERT(!IsTrue(oh_false));
    }
    {
        ObjectHolder oh_false = ObjectHolder::Own(String(""s));
        ObjectHolder oh_true = ObjectHolder::Own(String("Yup"s));
        ASSERT(!IsTrue(oh_false));
        ASSERT(IsTrue(oh_true));
    }
    {
        ObjectHolder oh_false = ObjectHolder::Own(ValueObject(false));
        ASSERT(!IsTrue(oh_false));
    }
    {
        ObjectHolder oh_true = ObjectHolder::Own(ValueObject(true));
        ASSERT(!IsTrue(oh_true));
    }
    {
        ObjectHolder oh_false = ObjectHolder::Own(ValueObject(0));
        ASSERT(!IsTrue(oh_false));
    }
    {
        ObjectHolder oh_true = ObjectHolder::Own(Class("Base"s, {}, nullptr));
        ASSERT(!IsTrue(oh_true));
    }
    {
        Class cls("Base"s, {}, nullptr);
        ObjectHolder oh_true = ObjectHolder::Own(ClassInstance(cls));
        ASSERT(!IsTrue(oh_true));
    }
}

void TestEqualSimple() {
    DummyContext ctx;
    {
        ObjectHolder oh_number_1 = ObjectHolder::Own(Number(1));
        ObjectHolder oh_number_2 = ObjectHolder::Own(Number(1));
        ASSERT(Equal(oh_number_1, oh_number_2, ctx));
    }
    {
        ObjectHolder oh_number_1 = ObjectHolder::Own(Number(1));
        ObjectHolder oh_number_2 = ObjectHolder::Own(Number(2));
        ASSERT(!Equal(oh_number_1, oh_number_2, ctx));
    }
    {
        ObjectHolder oh_str_1 = ObjectHolder::Own(String(""));
        ObjectHolder oh_str_2 = ObjectHolder::Own(String(""));
        ASSERT(Equal(oh_str_1, oh_str_2, ctx));
    }
    {
        ObjectHolder oh_str_1 = ObjectHolder::Own(String("Yuppy"));
        ObjectHolder oh_str_2 = ObjectHolder::Own(String("Yuppy"));
        ASSERT(Equal(oh_str_1, oh_str_2, ctx));
    }
    {
        ObjectHolder oh_str_1 = ObjectHolder::Own(String("Yuppy"));
        ObjectHolder oh_str_2 = ObjectHolder::Own(String("Crappy"));
        ASSERT(!Equal(oh_str_1, oh_str_2, ctx));
    }
    {
        ObjectHolder oh_bl_1 = ObjectHolder::Own(Bool(true));
        ObjectHolder oh_bl_2 = ObjectHolder::Own(Bool(true));
        ASSERT(Equal(oh_bl_1, oh_bl_2, ctx));
    }
    {
        ObjectHolder oh_bl_1 = ObjectHolder::Own(Bool(true));
        ObjectHolder oh_bl_2 = ObjectHolder::Own(Bool(false));
        ASSERT(!Equal(oh_bl_1, oh_bl_2, ctx));
    }
    {
        ObjectHolder oh_nm_1 = ObjectHolder::Own(Number(3));
        ObjectHolder oh_bl_1 = ObjectHolder::Own(Bool(true));
        ASSERT_THROWS(Equal(oh_nm_1, oh_bl_1, ctx), runtime_error);
    }
    {
        ObjectHolder oh_none_1 = ObjectHolder::None();
        ObjectHolder oh_none_2 = ObjectHolder::None();
        ASSERT(Equal(oh_none_1, oh_none_2, ctx));
    }
    {
        ObjectHolder oh_none_1 = ObjectHolder::None();
        ObjectHolder oh_nm_1 = ObjectHolder::Own(Number(3));
        ASSERT_THROWS(Equal(oh_none_1, oh_nm_1, ctx), runtime_error);
    }
    {
        vector<Method> methods;
        ObjectHolder oh_true = ObjectHolder::Own(Bool{ true });
        auto base_method_true = [&oh_true](Closure&, Context&) {
            return oh_true;
        };
        methods.push_back({ "__eq__"s, { "other"s }, make_unique<TestMethodBody>(base_method_true) });
        Class cls("Base"s, std::move(methods), nullptr);
        ClassInstance ci(cls);
        ObjectHolder oh_ci = ObjectHolder::Share(ci);
        ObjectHolder oh_nm_1 = ObjectHolder::Own(Number(3));
        ASSERT(Equal(oh_ci, oh_nm_1, ctx));
    }
    {
        vector<Method> methods;
        ObjectHolder oh_false = ObjectHolder::Own(Bool{ false });
        auto base_method_false = [&oh_false](Closure&, Context&) {
            return oh_false;
        };
        methods.push_back({ "__eq__"s, { "other"s }, make_unique<TestMethodBody>(base_method_false) });
        Class cls("Base"s, std::move(methods), nullptr);
        ClassInstance ci(cls);
        ObjectHolder oh_ci = ObjectHolder::Share(ci);
        ObjectHolder oh_nm_1 = ObjectHolder::Own(Bool(true));
        ASSERT(!Equal(oh_ci, oh_nm_1, ctx));
    }
}

void TestEqualLessGreater() {
    DummyContext ctx;
    {
        ObjectHolder oh_number_1 = ObjectHolder::Own(Number(1));
        ObjectHolder oh_number_2 = ObjectHolder::Own(Number(15));
        ASSERT(Equal(oh_number_1, oh_number_1, ctx));
        ASSERT(NotEqual(oh_number_1, oh_number_2, ctx));
        ASSERT(Less(oh_number_1, oh_number_2, ctx));
        ASSERT(LessOrEqual(oh_number_1, oh_number_2, ctx));
        ASSERT(LessOrEqual(oh_number_2, oh_number_2, ctx));
        ASSERT(Greater(oh_number_2, oh_number_1, ctx));
        ASSERT(GreaterOrEqual(oh_number_2, oh_number_1, ctx));
        ASSERT(GreaterOrEqual(oh_number_2, oh_number_2, ctx));
    }
    {
        ObjectHolder oh_str_1 = ObjectHolder::Own(String("a"));
        ObjectHolder oh_str_2 = ObjectHolder::Own(String("b"));
        ASSERT(Equal(oh_str_1, oh_str_1, ctx));
        ASSERT(NotEqual(oh_str_1, oh_str_2, ctx));
        ASSERT(Less(oh_str_1, oh_str_2, ctx));
        ASSERT(LessOrEqual(oh_str_1, oh_str_2, ctx));
        ASSERT(LessOrEqual(oh_str_2, oh_str_2, ctx));
        ASSERT(Greater(oh_str_2, oh_str_1, ctx));
        ASSERT(GreaterOrEqual(oh_str_2, oh_str_1, ctx));
        ASSERT(GreaterOrEqual(oh_str_2, oh_str_2, ctx));
    }
    {
        ObjectHolder oh_bl_1 = ObjectHolder::Own(Bool(false)); // 0
        ObjectHolder oh_bl_2 = ObjectHolder::Own(Bool(true)); // 1
        ASSERT(Equal(oh_bl_1, oh_bl_1, ctx));
        ASSERT(NotEqual(oh_bl_1, oh_bl_2, ctx));
        ASSERT(Less(oh_bl_1, oh_bl_2, ctx));
        ASSERT(LessOrEqual(oh_bl_1, oh_bl_2, ctx));
        ASSERT(LessOrEqual(oh_bl_2, oh_bl_2, ctx));
        ASSERT(Greater(oh_bl_2, oh_bl_1, ctx));
        ASSERT(GreaterOrEqual(oh_bl_2, oh_bl_1, ctx));
        ASSERT(GreaterOrEqual(oh_bl_2, oh_bl_2, ctx));
    }
    {
        ObjectHolder oh_none_1 = ObjectHolder::None();
        ObjectHolder oh_none_2 = ObjectHolder::None();
        ASSERT_DOESNT_THROW(Equal(oh_none_1, oh_none_1, ctx), runtime_error);
        ASSERT_DOESNT_THROW(NotEqual(oh_none_1, oh_none_1, ctx), runtime_error);
        ASSERT_THROWS(Less(oh_none_1, oh_none_1, ctx), runtime_error);
        ASSERT_THROWS(LessOrEqual(oh_none_1, oh_none_1, ctx), runtime_error);
        ASSERT_THROWS(Greater(oh_none_1, oh_none_1, ctx), runtime_error);
        ASSERT_THROWS(GreaterOrEqual(oh_none_1, oh_none_1, ctx), runtime_error);
    }
    {
        ObjectHolder oh_none_1 = ObjectHolder::None();
        ObjectHolder oh_nm_1 = ObjectHolder::Own(Number(3));
        ASSERT_THROWS(Equal(oh_none_1, oh_nm_1, ctx), runtime_error);
        ASSERT_THROWS(NotEqual(oh_none_1, oh_nm_1, ctx), runtime_error);
        ASSERT_THROWS(Less(oh_none_1, oh_nm_1, ctx), runtime_error);
        ASSERT_THROWS(LessOrEqual(oh_none_1, oh_nm_1, ctx), runtime_error);
        ASSERT_THROWS(LessOrEqual(oh_none_1, oh_nm_1, ctx), runtime_error);
        ASSERT_THROWS(Greater(oh_none_1, oh_nm_1, ctx), runtime_error);
        ASSERT_THROWS(GreaterOrEqual(oh_none_1, oh_nm_1, ctx), runtime_error);
        ASSERT_THROWS(GreaterOrEqual(oh_none_1, oh_nm_1, ctx), runtime_error);
    }

    ObjectHolder oh_true = ObjectHolder::Own(Bool{ true });
    auto base_method_true = [&oh_true](Closure&, Context&) {
        return oh_true;
    };
    ObjectHolder oh_false = ObjectHolder::Own(Bool{ false });
    auto base_method_false = [&oh_false](Closure&, Context&) {
        return oh_false;
    };
    {
        vector<Method> methods;
        methods.push_back({ "__lt__"s, { "other"s }, make_unique<TestMethodBody>(base_method_true) });
        methods.push_back({ "__eq__"s, { "other"s }, make_unique<TestMethodBody>(base_method_false) });
        Class cls("Base"s, std::move(methods), nullptr);
        ClassInstance ci(cls);
        ObjectHolder oh_ci = ObjectHolder::Share(ci);
        ObjectHolder oh_nm_1 = ObjectHolder::Own(Number(3));
        ASSERT(!Equal(oh_ci, oh_nm_1, ctx));
        ASSERT(NotEqual(oh_ci, oh_nm_1, ctx));
        ASSERT(Less(oh_ci, oh_nm_1, ctx));
        ASSERT(LessOrEqual(oh_ci, oh_nm_1, ctx));
        ASSERT(!Greater(oh_ci, oh_nm_1, ctx));
        ASSERT(!GreaterOrEqual(oh_ci, oh_nm_1, ctx));
    }
    {
        vector<Method> methods;
        methods.push_back({ "__lt__"s, { "other"s }, make_unique<TestMethodBody>(base_method_false) });
        methods.push_back({ "__eq__"s, { "other"s }, make_unique<TestMethodBody>(base_method_true) });
        Class cls("Base"s, std::move(methods), nullptr);
        ClassInstance ci(cls);
        ObjectHolder oh_ci = ObjectHolder::Share(ci);
        ObjectHolder oh_nm_1 = ObjectHolder::Own(Number(3));
        ASSERT(Equal(oh_ci, oh_nm_1, ctx));
        ASSERT(!NotEqual(oh_ci, oh_nm_1, ctx));
        ASSERT(!Less(oh_ci, oh_nm_1, ctx));
        ASSERT(LessOrEqual(oh_ci, oh_nm_1, ctx));
        ASSERT(!Greater(oh_ci, oh_nm_1, ctx));
        ASSERT(GreaterOrEqual(oh_ci, oh_nm_1, ctx));
    }
    {
        vector<Method> methods;
        methods.push_back({ "__lt__"s, { "other"s }, make_unique<TestMethodBody>(base_method_false) });
        methods.push_back({ "__eq__"s, { "other"s }, make_unique<TestMethodBody>(base_method_false) });
        Class cls("Base"s, std::move(methods), nullptr);
        ClassInstance ci(cls);
        ObjectHolder oh_ci = ObjectHolder::Share(ci);
        ObjectHolder oh_nm_1 = ObjectHolder::Own(Number(3));
        ASSERT(!Equal(oh_ci, oh_nm_1, ctx));
        ASSERT(NotEqual(oh_ci, oh_nm_1, ctx));
        ASSERT(!Less(oh_ci, oh_nm_1, ctx));
        ASSERT(!LessOrEqual(oh_ci, oh_nm_1, ctx));
        ASSERT(Greater(oh_ci, oh_nm_1, ctx));
        ASSERT(GreaterOrEqual(oh_ci, oh_nm_1, ctx));
    }
    {
        vector<Method> methods;
        Class cls("Base"s, std::move(methods), nullptr);
        ClassInstance ci(cls);
        ObjectHolder oh_ci = ObjectHolder::Share(ci);
        ObjectHolder oh_nm_1 = ObjectHolder::Own(Number(3));
        ASSERT_THROWS(!Equal(oh_ci, oh_nm_1, ctx), runtime_error);
        ASSERT_THROWS(NotEqual(oh_ci, oh_nm_1, ctx), runtime_error);
        ASSERT_THROWS(!Less(oh_ci, oh_nm_1, ctx), runtime_error);
        ASSERT_THROWS(!LessOrEqual(oh_ci, oh_nm_1, ctx), runtime_error);
        ASSERT_THROWS(Greater(oh_ci, oh_nm_1, ctx), runtime_error);
        ASSERT_THROWS(GreaterOrEqual(oh_ci, oh_nm_1, ctx), runtime_error);
    }
}

// ----------------------------------------------------------------------------

class BoolExecutable : public Executable {
public:
    explicit BoolExecutable(bool value)
        : value_(Bool{ value }) {
    }

    ObjectHolder Execute(Closure&, Context&) override {
        return ObjectHolder::Share(value_);
    }
private:
    Bool value_;
};

void TestComparisonAmrulla() {
    {
        Class cls{ "class", {}, nullptr };
        ClassInstance instance{ cls };
        DummyContext context;
        ObjectHolder lhs = ObjectHolder::Share(instance);
        ObjectHolder rhs = ObjectHolder::Share(instance);
        ASSERT_THROWS(Equal(lhs, rhs, context), std::runtime_error);
        ASSERT_THROWS(NotEqual(lhs, rhs, context), std::runtime_error);
        ASSERT_THROWS(Less(lhs, rhs, context), std::runtime_error);
        ASSERT_THROWS(LessOrEqual(lhs, rhs, context), std::runtime_error);
        ASSERT_THROWS(Greater(lhs, rhs, context), std::runtime_error);
        ASSERT_THROWS(GreaterOrEqual(lhs, rhs, context), std::runtime_error);
    }

    {
        std::vector<std::string> formal_params = { "rhs"s };
        Method eq{ "__eq__"s, formal_params, std::unique_ptr<Executable>(new BoolExecutable(true)) };
        Method lt{ "__lt__"s, formal_params, std::unique_ptr<Executable>(new BoolExecutable(false)) };
        vector<Method> methods;
        methods.push_back(std::move(eq));
        methods.push_back(std::move(lt));

        Class cls{ "class", std::move(methods), nullptr };
        ClassInstance instance{ cls };
        DummyContext context;
        ObjectHolder lhs = ObjectHolder::Share(instance);
        ObjectHolder rhs = ObjectHolder::Share(instance);
        ASSERT_EQUAL(Equal(lhs, rhs, context), true);
        ASSERT_EQUAL(NotEqual(lhs, rhs, context), false);
        ASSERT_EQUAL(Less(lhs, rhs, context), false);
        ASSERT_EQUAL(Greater(lhs, rhs, context), false);
        ASSERT_EQUAL(LessOrEqual(lhs, rhs, context), true);
        ASSERT_EQUAL(GreaterOrEqual(lhs, rhs, context), true);
    }

    {
        std::vector<std::string> formal_params = { "rhs"s };
        Method eq{ "__eq__"s, formal_params, std::unique_ptr<Executable>(new BoolExecutable(false)) };
        Method lt{ "__lt__"s, formal_params, std::unique_ptr<Executable>(new BoolExecutable(true)) };
        vector<Method> methods;
        methods.push_back(std::move(eq));
        methods.push_back(std::move(lt));

        Class cls{ "class", std::move(methods), nullptr };
        ClassInstance instance{ cls };
        DummyContext context;
        ObjectHolder lhs = ObjectHolder::Share(instance);
        ObjectHolder rhs = ObjectHolder::Share(instance);
        ASSERT_EQUAL(Equal(lhs, rhs, context), false);
        ASSERT_EQUAL(NotEqual(lhs, rhs, context), true);
        ASSERT_EQUAL(Less(lhs, rhs, context), true);
        ASSERT_EQUAL(Greater(lhs, rhs, context), false);
        ASSERT_EQUAL(LessOrEqual(lhs, rhs, context), true);
        ASSERT_EQUAL(GreaterOrEqual(lhs, rhs, context), false);
    }

    {
        std::vector<std::string> formal_params = { "rhs"s };
        Method eq{ "__eq__"s, formal_params, std::unique_ptr<Executable>(new BoolExecutable(false)) };
        Method lt{ "__lt__"s, formal_params, std::unique_ptr<Executable>(new BoolExecutable(false)) };
        vector<Method> methods;
        methods.push_back(std::move(eq));
        methods.push_back(std::move(lt));

        Class cls{ "class", std::move(methods), nullptr };
        ClassInstance instance{ cls };
        DummyContext context;
        ObjectHolder lhs = ObjectHolder::Share(instance);
        ObjectHolder rhs = ObjectHolder::Share(instance);
        ASSERT_EQUAL(Equal(lhs, rhs, context), false);
        ASSERT_EQUAL(NotEqual(lhs, rhs, context), true);
        ASSERT_EQUAL(Less(lhs, rhs, context), false);
        ASSERT_EQUAL(Greater(lhs, rhs, context), true);
        ASSERT_EQUAL(LessOrEqual(lhs, rhs, context), false);
        ASSERT_EQUAL(GreaterOrEqual(lhs, rhs, context), true);
    }
}

// ----------------------------------------------------------------------------

}  // namespace

void RunObjectsTests(TestRunner& tr) {
    RUN_TEST(tr, runtime::TestNumber);
    RUN_TEST(tr, runtime::TestString);
    RUN_TEST(tr, runtime::TestMethodInvocation);
}

void RunObjectHolderTests(TestRunner& tr) {
    RUN_TEST(tr, runtime::TestNonowning);
    RUN_TEST(tr, runtime::TestOwning);
    RUN_TEST(tr, runtime::TestMove);
    RUN_TEST(tr, runtime::TestNullptr);
}

void RunComparisonTests(TestRunner& tr) {
    RUN_TEST(tr, runtime::TestComparisonAmrulla);
    RUN_TEST(tr, runtime::TestIsTrue);
    RUN_TEST(tr, runtime::TestEqualSimple);
    RUN_TEST(tr, runtime::TestEqualLessGreater);
}

}  // namespace runtime
