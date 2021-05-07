#include "runtime.h"

#include <algorithm>
#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

namespace runtime_string_consts {

static const std::string SELF("self"s);
static const std::string TRUE("True"s);
static const std::string FALSE("False"s);
static const std::string CLASS("Class"s);
static const std::string STR("__str__"s);
static const std::string EQ("__eq__"s);
static const std::string LT("__lt__"s);

} // namespace runtime_string_consts

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
    return false;
}

// ----------------------------------------------------------------------------

String operator+(const String& lhs, const String& rhs)
{
    return String(lhs.GetValue() + rhs.GetValue());
}

Number operator+(const Number& lhs, const Number& rhs)
{
    return Number(lhs.GetValue() + rhs.GetValue());
}

Number operator-(const Number& lhs, const Number& rhs)
{
    return Number(lhs.GetValue() - rhs.GetValue());
}

Number operator*(const Number& lhs, const Number& rhs)
{
    return Number(lhs.GetValue() * rhs.GetValue());
}

Number operator/(const Number& lhs, const Number& rhs)
{
    if (rhs.GetValue() == 0) {
        throw runtime_error("Divider equals to zero"s);
    }
    return Number(lhs.GetValue() / rhs.GetValue());
}

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------

void Bool::Print(std::ostream& os, Context&) {
    os << (GetValue() ? runtime_string_consts::TRUE : runtime_string_consts::FALSE);
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

const Method* Class::GetMethod(const std::string& name, size_t args_count) const {
    const Method* ptr_method = GetMethod(name);
    return (ptr_method && (ptr_method->formal_params.size() == args_count)) ? ptr_method : nullptr;
}

[[nodiscard]] const std::string& Class::GetName() const {
    return name_;
}

void Class::Print(ostream& os, Context&) {
    os << runtime_string_consts::CLASS << ' ' << name_;
}

// ----------------------------------------------------------------------------

ClassInstance::ClassInstance(const Class& cls)
    : cls_(cls) {
}

void ClassInstance::Print(std::ostream& os, Context& context) {
    if (const auto* ptr_method = TryMethod(runtime_string_consts::STR, 0)) {
        ObjectHolder oh = Call(ptr_method, {}, context);
        oh.Get()->Print(os, context);
    }
    else {
        os << this;
    }
}

ObjectHolder ClassInstance::Call(
    const std::string& method, const std::vector<ObjectHolder>& actual_args, Context& context) {
    const auto* ptr_method = GetMethod(method, actual_args.size());
    return Call(ptr_method, actual_args, context);
}

ObjectHolder ClassInstance::Call(const Method* method, const std::vector<ObjectHolder>& actual_args, Context& context) {
    Closure local_closure = CreateLocalClosure(method->formal_params, actual_args);
    return method->body.get()->Execute(local_closure, context);
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    return TryMethod(method, argument_count) != nullptr;
}

const Method* ClassInstance::GetMethod(const std::string& method, size_t argument_count) const {
    if (const auto* ptr_method = cls_.GetMethod(method, argument_count)) {
        return ptr_method;
    }
    std::stringstream ss;
    ss << "Unknown method name: "sv << method;
    throw std::runtime_error(ss.str());
}

const Method* ClassInstance::TryMethod(const std::string& method, size_t argument_count) const {
    return cls_.GetMethod(method, argument_count);
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
    closure.emplace(runtime_string_consts::SELF, ObjectHolder::Share(*this));
    for (size_t i = 0; i < formal_params.size(); ++i) {
        closure.emplace(formal_params.at(i), actual_args.at(i));
    }
    return closure;
}

// ----------------------------------------------------------------------------

template <typename Compare>
bool MakeComparison(
    const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context,
    const std::string& func_name, Compare cmp) {
    if (lhs && rhs) {
        if (auto* ptr_cls = lhs.TryAs<ClassInstance>()) {
            return IsTrue(ptr_cls->Call(func_name, { rhs }, context));
        }
#define VALUED_OUTPUT(type) \
    if (lhs.IsType<type>() && rhs.IsType<type>()) return cmp(lhs.TryAs<type>()->GetValue(), rhs.TryAs<type>()->GetValue());

        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Bool);

#undef VALUED_OUTPUT
    }
    throw std::runtime_error("Cannot compare objects for "s + func_name);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (!lhs && !rhs) {
        return true;
    }
    return MakeComparison(lhs, rhs, context, runtime_string_consts::EQ, std::equal_to{});
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return MakeComparison(lhs, rhs, context, runtime_string_consts::LT, std::less{});
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs, context);
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Greater(lhs, rhs, context);
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context);
}

// ----------------------------------------------------------------------------

}  // namespace runtime
