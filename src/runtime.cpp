#include "runtime.h"

#include <algorithm>
#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

// ----------------------------------------------------------------------------

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

// ----------------------------------------------------------------------------
// Проверяет, содержится ли в object значение, приводимое к True
// Для 0, False, None, и пустых строк возвращается false, в остальных случаях - true
bool IsTrue(const ObjectHolder& object) {
    if (!object) {
        return false;
    }
#define IS_CASTED_AND_NOT_EQUAL(type, value) \
    if (auto p = object.TryAs<type>()) return p->GetValue() != value;

    IS_CASTED_AND_NOT_EQUAL(String, ""s);
    IS_CASTED_AND_NOT_EQUAL(Number, 0);
    IS_CASTED_AND_NOT_EQUAL(Bool, false);

#undef IS_CASTED_AND_NOT_EQUAL
    return true;
}

// ----------------------------------------------------------------------------

void Bool::Print(std::ostream& os, Context& context) {
    (void)context;
    os << (GetValue() ? "True"sv : "False"sv);
}

// ----------------------------------------------------------------------------

Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
    : name_(std::move(name))
    , methods_(std::move(methods))
    , parent_(std::move(parent)) {
}

const Method* Class::GetMethod(const std::string& name) const {
    auto it = std::find_if(methods_.begin(), methods_.end(), [&name](const auto& value) { return value.name == name; });
    return (it == methods_.end()) ? ( (parent_) ? parent_->GetMethod(name) : nullptr ) : &(*it);
}

[[nodiscard]] const std::string& Class::GetName() const {
    return name_;
}

void Class::Print(ostream& os, Context& context) {
    (void)context;
    os << "Class "sv << name_;
}

// ----------------------------------------------------------------------------

ClassInstance::ClassInstance(const Class& cls)
    : cls_(cls) {
}

void ClassInstance::Print(std::ostream& os, Context& context) {
    if (HasMethod("__str__"s, 0)) {
        const auto* ptr_method = GetMethod("__str__"s, 0);
        Closure local_closure = CreateLocalClosure({}, {});
        ObjectHolder oh = ptr_method->body.get()->Execute(local_closure, context);
        os << oh.TryAs<String>()->GetValue();
    }
    else {
        os << this;
    }
}

ObjectHolder ClassInstance::Call(
    const std::string& method, const std::vector<ObjectHolder>& actual_args, Context& context) {
    const auto* ptr_method = GetMethod(method, actual_args.size());
    Closure local_closure = CreateLocalClosure(ptr_method->formal_params, actual_args);
    return ptr_method->body.get()->Execute(local_closure, context);
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    const auto* ptr_method = cls_.GetMethod(method);
    return (ptr_method) ? (ptr_method->formal_params.size() == argument_count) : false;
}

const Method* ClassInstance::GetMethod(const std::string& method, size_t argument_count) const {
    if (!HasMethod(method, argument_count)) {
        std::stringstream ss;
        ss << "Unknown method name: "sv << method;
        throw std::runtime_error(ss.str());
    }
    return cls_.GetMethod(method);
}

Closure& ClassInstance::Fields() {
    return closure_;
}

const Closure& ClassInstance::Fields() const {
    return closure_;
}

Closure ClassInstance::CreateLocalClosure(
    const std::vector<std::string>& formal_params,
    const std::vector<ObjectHolder>& actual_args) {
    assert(formal_params.size() == actual_args.size());
    Closure closure;
    closure.emplace("self"s, ObjectHolder::Share(*this));
    for (size_t i = 0; i < formal_params.size(); ++i) {
        closure.emplace(formal_params.at(i), actual_args.at(i));
    }
    return closure;
}

// ----------------------------------------------------------------------------

Closure CreateCompareClosure(const ObjectHolder& lhs, const ObjectHolder& rhs, const std::string arg_name) {
    Closure closure;
    closure.emplace("self"s, lhs);
    closure.emplace(arg_name, rhs);
    return closure;
}

template <typename Compare>
bool MakeComparison(
    const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context,
    const std::string& func_name, Compare cmp) {
    if (const auto* ptr_cls = lhs.TryAs<ClassInstance>()) {
        if (const auto* prt_eq = ptr_cls->GetMethod(func_name, 1U)) {
            Closure local_closure = CreateCompareClosure(lhs, rhs, prt_eq->formal_params.front());
            return dynamic_cast<Bool*>(prt_eq->body.get()->Execute(local_closure, context).Get())->GetValue();
        }
    }
#define VALUED_OUTPUT(type) \
    if (lhs.IsType<type>() && rhs.IsType<type>()) return cmp(lhs.TryAs<type>()->GetValue(), rhs.TryAs<type>()->GetValue());

    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Bool);

#undef VALUED_OUTPUT
    throw std::runtime_error("Cannot compare objects for equality"s);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return MakeComparison(lhs, rhs, context, "__eq__"s, std::equal_to{});
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return MakeComparison(lhs, rhs, context, "__lt__"s, std::less{});
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs, context);
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs, context) && !Less(lhs, rhs, context);
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return Equal(lhs, rhs, context) || Less(lhs, rhs, context);
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return Equal(lhs, rhs, context) || !Less(lhs, rhs, context);
}

// ----------------------------------------------------------------------------

}  // namespace runtime
