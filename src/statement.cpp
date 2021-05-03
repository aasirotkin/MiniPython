#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;

namespace {
    const string ADD_METHOD = "__add__"s;
    const string INIT_METHOD = "__init__"s;
    const string STR_METHOD = "__str__"s;
}  // namespace

// ----------------------------------------------------------------------------

VariableValue::VariableValue(const std::string& var_name)
    : dotted_ids_{ var_name } {
}

VariableValue::VariableValue(std::vector<std::string> dotted_ids)
    : dotted_ids_(std::move(dotted_ids)) {
}

ObjectHolder VariableValue::Execute(Closure& closure, Context& /*context*/) {
    Closure* ptr_closure = &closure;
    size_t i = 1;
    for (; i < dotted_ids_.size(); ++i) {
        const std::string& id = dotted_ids_.at(i - 1);
        if (ptr_closure->count(id)) {
            if (auto* p = ptr_closure->at(id).TryAs<runtime::ClassInstance>()) {
                ptr_closure = &p->Fields();
                continue;
            }
        }
        break;
    }
    if (ptr_closure->count(dotted_ids_.back()) == 0) {
        std::stringstream ss;
        ss << "Closure doesn't have variable with name: "s << dotted_ids_.back();
        throw std::runtime_error(ss.str());
    }
    return ptr_closure->at(dotted_ids_.back());
}

// ----------------------------------------------------------------------------

Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
    : var_(std::move(var))
    , rv_(std::move(rv)) {
}

ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
    closure[var_] = rv_->Execute(closure, context);
    return closure.at(var_);
}

// ----------------------------------------------------------------------------

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name, std::unique_ptr<Statement> rv)
    : object_(std::move(object))
    , field_name_(std::move(field_name))
    , rv_(std::move(rv)) {
}

ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
    ObjectHolder oh = object_.Execute(closure, context);
    if (auto* p = oh.TryAs<runtime::ClassInstance>()) {
        p->Fields()[field_name_] = rv_->Execute(closure, context);
        return p->Fields().at(field_name_);
    }
    return ObjectHolder::None();
}

// ----------------------------------------------------------------------------

NewInstance::NewInstance(const runtime::Class& class_type)
    : ci_(class_type) {
}

NewInstance::NewInstance(const runtime::Class& class_type, std::vector<std::unique_ptr<Statement>> args)
    : ci_(class_type)
    , args_(std::move(args)) {
}

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
    if (ci_.HasMethod(INIT_METHOD, args_.size())) {
        std::vector<ObjectHolder> args_values;
        for (const auto& args_i : args_) {
            args_values.push_back(args_i->Execute(closure, context));
        }
        ci_.Call(INIT_METHOD, args_values, context);
    }
    ObjectHolder oh = ObjectHolder::Share(ci_);
    return oh;
}

// ----------------------------------------------------------------------------

Print::Print(unique_ptr<Statement> argument) {
    args_.push_back(std::move(argument));
}

Print::Print(vector<unique_ptr<Statement>> args)
    : args_(std::move(args)) {
}

unique_ptr<Print> Print::Variable(const std::string& name) {
    VariableValue value(name);
    std::vector<std::unique_ptr<Statement>> args;
    args.push_back(std::make_unique<VariableValue>(std::move(value)));
    return std::make_unique<Print>(std::move(args));
}

ObjectHolder Print::Execute(Closure& closure, Context& context) {
    bool first = true;
    for (const auto& unuque_ptr_statement : args_) {
        if (!first) {
            context.GetOutputStream() << ' ';
        }
        first = false;
        Statement* st = unuque_ptr_statement.get();
        ObjectHolder oh = st->Execute(closure, context);
        if (oh) {
            runtime::Object* ob = oh.Get();
            ob->Print(context.GetOutputStream(), context);
        }
        else {
            static const char* none_str = "None";
            context.GetOutputStream() << none_str;
        }
    }
    context.GetOutputStream() << '\n';
    return {};
}

// ----------------------------------------------------------------------------

ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
    ObjectHolder oh = GetArg().get()->Execute(closure, context);
    if (auto* p = oh.TryAs<runtime::ClassInstance>()) {
        if (p->HasMethod(STR_METHOD, 0)) {
            oh = p->Call(STR_METHOD, {}, context);
        }
    }

    std::string value("None"s);
    if (oh) {
        std::stringstream ss;
        static runtime::DummyContext empty;
        oh.Get()->Print(ss, empty);
        value = ss.str();
    }

    runtime::String str(value);
    return ObjectHolder::Own(std::move(str));
}

// ----------------------------------------------------------------------------

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
    std::vector<std::unique_ptr<Statement>> args)
    : object_(std::move(object))
    , method_(std::move(method))
    , args_(std::move(args)) {
}

ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
    ObjectHolder oh = object_->Execute(closure, context);
    if (auto* p = oh.TryAs<runtime::ClassInstance>()) {
        if (p->HasMethod(method_, args_.size())) {
            std::vector<ObjectHolder> args_values;
            for (const auto& args_i : args_) {
                args_values.push_back(args_i->Execute(closure, context));
            }
            return p->Call(method_, args_values, context);
        }
    }
    return {};
}

// ----------------------------------------------------------------------------

ObjectHolder Add::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_oh = GetLhs()->Execute(closure, context);
    ObjectHolder rhs_oh = GetRhs()->Execute(closure, context);

    if (lhs_oh.IsType<runtime::Number>() && rhs_oh.IsType<runtime::Number>()) {
        runtime::Number value(
            lhs_oh.TryAs<runtime::Number>()->GetValue() +
            rhs_oh.TryAs<runtime::Number>()->GetValue());
        return ObjectHolder::Own(std::move(value));
    }

    if (lhs_oh.IsType<runtime::String>() && rhs_oh.IsType<runtime::String>()) {
        runtime::String value(
            lhs_oh.TryAs<runtime::String>()->GetValue() +
            rhs_oh.TryAs<runtime::String>()->GetValue());
        return ObjectHolder::Own(std::move(value));
    }

    if (lhs_oh.IsType<runtime::ClassInstance>()) {
        auto* p_lhs = lhs_oh.TryAs<runtime::ClassInstance>();
        if (p_lhs->HasMethod(ADD_METHOD, 1)) {
            std::vector<ObjectHolder> args({ rhs_oh });
            ObjectHolder value = p_lhs->Call(ADD_METHOD, args, context);
            return value;
        }
    }

    throw std::runtime_error("Couldn't add this objects :("s);

    return {};
}

// ----------------------------------------------------------------------------

ObjectHolder Sub::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_oh = GetLhs()->Execute(closure, context);
    ObjectHolder rhs_oh = GetRhs()->Execute(closure, context);

    if (lhs_oh.IsType<runtime::Number>() && rhs_oh.IsType<runtime::Number>()) {
        runtime::Number value(
            lhs_oh.TryAs<runtime::Number>()->GetValue() -
            rhs_oh.TryAs<runtime::Number>()->GetValue());
        return ObjectHolder::Own(std::move(value));
    }

    throw std::runtime_error("Couldn't sub this objects :("s);

    return {};
}

// ----------------------------------------------------------------------------

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_oh = GetLhs()->Execute(closure, context);
    ObjectHolder rhs_oh = GetRhs()->Execute(closure, context);

    if (lhs_oh.IsType<runtime::Number>() && rhs_oh.IsType<runtime::Number>()) {
        runtime::Number value(
            lhs_oh.TryAs<runtime::Number>()->GetValue() *
            rhs_oh.TryAs<runtime::Number>()->GetValue());
        return ObjectHolder::Own(std::move(value));
    }

    throw std::runtime_error("Couldn't muld this objects :("s);

    return {};
}

// ----------------------------------------------------------------------------

ObjectHolder Div::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_oh = GetLhs()->Execute(closure, context);
    ObjectHolder rhs_oh = GetRhs()->Execute(closure, context);

    if (lhs_oh.IsType<runtime::Number>() && rhs_oh.IsType<runtime::Number>()) {
        auto divider = rhs_oh.TryAs<runtime::Number>()->GetValue();
        if (divider != 0) {
            runtime::Number value(
                lhs_oh.TryAs<runtime::Number>()->GetValue() /
                divider);
            return ObjectHolder::Own(std::move(value));
        }
    }

    throw std::runtime_error("Couldn't div this objects :("s);

    return {};
}

// ----------------------------------------------------------------------------

ObjectHolder Compound::Execute(Closure& closure, Context& context) {
    for (auto& op : operations_) {
        op->Execute(closure, context);
    }
    return ObjectHolder::None();
}

// ----------------------------------------------------------------------------

ObjectHolder Or::Execute(Closure& closure, Context& context) {
    static const ObjectHolder local_true = ObjectHolder::Own(runtime::Bool(true));
    static const ObjectHolder local_false = ObjectHolder::Own(runtime::Bool(false));

    ObjectHolder lhs_oh = GetLhs()->Execute(closure, context);

    if (!runtime::IsTrue(lhs_oh)) {
        ObjectHolder rhs_oh = GetRhs()->Execute(closure, context);
        if (!runtime::IsTrue(rhs_oh)) {
            return local_false;
        }
    }
    return local_true;
}

// ----------------------------------------------------------------------------

ObjectHolder And::Execute(Closure& closure, Context& context) {
    static const ObjectHolder local_true = ObjectHolder::Own(runtime::Bool(true));
    static const ObjectHolder local_false = ObjectHolder::Own(runtime::Bool(false));

    ObjectHolder lhs_oh = GetLhs()->Execute(closure, context);

    if (runtime::IsTrue(lhs_oh)) {
        ObjectHolder rhs_oh = GetRhs()->Execute(closure, context);
        if (runtime::IsTrue(rhs_oh)) {
            return local_true;
        }
    }
    return local_false;
}

// ----------------------------------------------------------------------------

ObjectHolder Not::Execute(Closure& closure, Context& context) {
    static const ObjectHolder local_true = ObjectHolder::Own(runtime::Bool(true));
    static const ObjectHolder local_false = ObjectHolder::Own(runtime::Bool(false));

    ObjectHolder oh = GetArg()->Execute(closure, context);

    return (runtime::IsTrue(oh)) ? local_false : local_true;
}

// ----------------------------------------------------------------------------

Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs))
    , cmp_(std::move(cmp)) {
}

ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
    static const ObjectHolder local_true = ObjectHolder::Own(runtime::Bool(true));
    static const ObjectHolder local_false = ObjectHolder::Own(runtime::Bool(false));

    ObjectHolder lhs_oh = GetLhs()->Execute(closure, context);
    ObjectHolder rhs_oh = GetRhs()->Execute(closure, context);

    if (cmp_(lhs_oh, rhs_oh, context)) {
        return local_true;
    }

    return local_false;
}

// ----------------------------------------------------------------------------

ObjectHolder Return::Execute(Closure& closure, Context& context) {
    ObjectHolder oh = statement_->Execute(closure, context);
    throw ReturnException(std::move(oh));
    return {};
}

// ----------------------------------------------------------------------------

MethodBody::MethodBody(std::unique_ptr<Statement>&& body)
    : body_(std::move(body)) {
}

ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
    try {
        body_->Execute(closure, context);
    }
    catch (ReturnException& ret) {
        return ret.GetResult();
    }
    return ObjectHolder::None();
}

// ----------------------------------------------------------------------------

ClassDefinition::ClassDefinition(ObjectHolder cls)
    : cls_(std::move(cls)) {
}

ObjectHolder ClassDefinition::Execute(Closure& closure, Context& /*context*/) {
    runtime::Class* ptr_cls = cls_.TryAs<runtime::Class>();
    closure.emplace(ptr_cls->GetName(), cls_);
    return {};
}

// ----------------------------------------------------------------------------

IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
    std::unique_ptr<Statement> else_body)
    : condition_(std::move(condition))
    , if_body_(std::move(if_body))
    , else_body_(std::move(else_body)) {
}

ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
    ObjectHolder oh_condition = condition_->Execute(closure, context);
    if (runtime::IsTrue(oh_condition)) {
        return if_body_->Execute(closure, context);
    }
    else {
        if (else_body_) {
            return else_body_->Execute(closure, context);
        }
    }
    return {};
}

}  // namespace ast
