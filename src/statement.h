#pragma once

#include "runtime.h"

#include <functional>

namespace ast {

using Statement = runtime::Executable;

// ----------------------------------------------------------------------------

// ���������, ������������ �������� ���� T,
// ������������ ��� ������ ��� �������� ��������
template <typename T>
class ValueStatement : public Statement {
public:
    explicit ValueStatement(T v)
        : value_(std::move(v)) {
    }

    runtime::ObjectHolder Execute(runtime::Closure& /*closure*/,
        runtime::Context& /*context*/) override {
        return runtime::ObjectHolder::Share(value_);
    }

private:
    T value_;
};

using NumericConst = ValueStatement<runtime::Number>;
using StringConst = ValueStatement<runtime::String>;
using BoolConst = ValueStatement<runtime::Bool>;

// ----------------------------------------------------------------------------

// ��������� �������� ���������� ���� ������� ������� ����� �������� id1.id2.id3
class VariableValue : public Statement {
public:
    explicit VariableValue(const std::string& var_name);
    explicit VariableValue(std::vector<std::string> dotted_ids);

    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    std::vector<std::string> dotted_ids_;
};

// ----------------------------------------------------------------------------

// ����������� ����������, ��� ������� ������ � ��������� var, �������� ��������� rv
class Assignment : public Statement {
public:
    Assignment(std::string var, std::unique_ptr<Statement> rv);

    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

public:
    std::string var_;
    std::unique_ptr<Statement> rv_;
};

// ----------------------------------------------------------------------------

// ����������� ���� object.field_name �������� ��������� rv
class FieldAssignment : public Statement {
public:
    FieldAssignment(VariableValue object, std::string field_name, std::unique_ptr<Statement> rv);

    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    VariableValue object_;
    std::string field_name_;
    std::unique_ptr<Statement> rv_;
};

// ----------------------------------------------------------------------------

// ������ ����� ��������� ������ class_, ��������� ��� ������������ ����� ���������� args
class NewInstance : public Statement {
public:
    explicit NewInstance(const runtime::Class& class_type);
    NewInstance(const runtime::Class& class_type, std::vector<std::unique_ptr<Statement>> args);

    // ���������� ������, ���������� �������� ���� ClassInstance
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    runtime::ClassInstance ci_;
    std::vector<std::unique_ptr<Statement>> args_{};
};

// ----------------------------------------------------------------------------

// �������� None
class None : public Statement {
public:
    runtime::ObjectHolder Execute([[maybe_unused]] runtime::Closure& closure,
        [[maybe_unused]] runtime::Context& context) override {
        return {};
    }
};

// ----------------------------------------------------------------------------

// ������� print
class Print : public Statement {
public:
    // �������������� ������� print ��� ������ �������� ��������� argument
    explicit Print(std::unique_ptr<Statement> argument);
    // �������������� ������� print ��� ������ ������ �������� args
    explicit Print(std::vector<std::unique_ptr<Statement>> args);

    // �������������� ������� print ��� ������ �������� ���������� name
    static std::unique_ptr<Print> Variable(const std::string& name);

    // �� ����� ���������� ������� print ����� ������ �������������� � �����, ������������ ��
    // context.GetOutputStream()
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    std::vector<std::unique_ptr<Statement>> args_;
};

// ----------------------------------------------------------------------------

// �������� ����� object.method �� ������� ���������� args
class MethodCall : public Statement {
public:
    MethodCall(std::unique_ptr<Statement> object, std::string method,
        std::vector<std::unique_ptr<Statement>> args);

    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    std::unique_ptr<Statement> object_;
    std::string method_;
    std::vector<std::unique_ptr<Statement>> args_;
};

// ----------------------------------------------------------------------------

// ������� ����� ��� ������� ��������
class UnaryOperation : public Statement {
public:
    explicit UnaryOperation(std::unique_ptr<Statement> argument)
        : arg_(std::move(argument)) {
    }

    const std::unique_ptr<Statement>& GetArg() const {
        return arg_;
    }

private:
    std::unique_ptr<Statement> arg_;
};

// ----------------------------------------------------------------------------

// �������� str, ������������ ��������� �������� ������ ���������
class Stringify : public UnaryOperation {
public:
    using UnaryOperation::UnaryOperation;
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
};

// ----------------------------------------------------------------------------

// ������������ ����� �������� �������� � ����������� lhs � rhs
class BinaryOperation : public Statement {
public:
    BinaryOperation(std::unique_ptr<Statement> lhs, std::unique_ptr<Statement> rhs)
        : lhs_(std::move(lhs))
        , rhs_(std::move(rhs)) {
    }

    const std::unique_ptr<Statement>& GetLhs() const {
        return lhs_;
    }

    const std::unique_ptr<Statement>& GetRhs() const {
        return rhs_;
    }

private:
    std::unique_ptr<Statement> lhs_;
    std::unique_ptr<Statement> rhs_;
};

// ----------------------------------------------------------------------------

// ���������� ��������� �������� + ��� ����������� lhs � rhs
class Add : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;

    // �������������� ��������:
    //  ����� + �����
    //  ������ + ������
    //  ������1 + ������2, ���� � ������1 - ���������������� ����� � ������� _add__(rhs)
    // � ��������� ������ ��� ���������� ������������� runtime_error
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
};

// ----------------------------------------------------------------------------

// ���������� ��������� ��������� ���������� lhs � rhs
class Sub : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;

    // �������������� ���������:
    //  ����� - �����
    // ���� lhs � rhs - �� �����, ������������� ���������� runtime_error
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
};

// ----------------------------------------------------------------------------

// ���������� ��������� ��������� ���������� lhs � rhs
class Mult : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;

    // �������������� ���������:
    //  ����� * �����
    // ���� lhs � rhs - �� �����, ������������� ���������� runtime_error
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
};

// ----------------------------------------------------------------------------

// ���������� ��������� ������� lhs � rhs
class Div : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;

    // �������������� �������:
    //  ����� / �����
    // ���� lhs � rhs - �� �����, ������������� ���������� runtime_error
    // ���� rhs ����� 0, ������������� ���������� runtime_error
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
};

// ----------------------------------------------------------------------------

// ��������� ���������� (��������: ���� ������, ���������� ����� if, ���� else)
class Compound : public Statement {
public:
    // ������������ Compound �� ���������� ���������� ���� unique_ptr<Statement>
    template <typename... Args>
    explicit Compound(Args&&... args) {
        if constexpr ((sizeof...(args)) != 0) {
            FillOperations(std::forward<Args>(args)...);
        }
    }

    // ��������� ��������� ���������� � ����� ��������� ����������
    void AddStatement(std::unique_ptr<Statement> stmt) {
        operations_.push_back(std::move(stmt));
    }

    // ��������������� ��������� ����������� ����������. ���������� None
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    // ��������� ��������� ���������� � ����� ��������� ����������
    template <typename... Args>
    void FillOperations(std::unique_ptr<Statement>&& stmt, Args&&... args) {
        operations_.push_back(std::move(stmt));
        if constexpr (sizeof...(args) != 0) {
            FillOperations(std::forward<Args>(args)...);
        }
    }

private:
    std::vector<std::unique_ptr<Statement>> operations_;
};

// ----------------------------------------------------------------------------

// ���������� ��������� ���������� ���������� �������� or ��� lhs � rhs
class Or : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;
    // �������� ��������� rhs �����������, ������ ���� �������� lhs
    // ����� ���������� � Bool ����� False
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
};

// ----------------------------------------------------------------------------

// ���������� ��������� ���������� ���������� �������� and ��� lhs � rhs
class And : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;
    // �������� ��������� rhs �����������, ������ ���� �������� lhs
    // ����� ���������� � Bool ����� True
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
};

// ----------------------------------------------------------------------------

// ���������� ��������� ���������� ���������� �������� not ��� ������������ ���������� ��������
class Not : public UnaryOperation {
public:
    using UnaryOperation::UnaryOperation;
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
};

// ----------------------------------------------------------------------------

// �������� ���������
class Comparison : public BinaryOperation {
public:
    // Comparator ����� �������, ����������� ��������� �������� ����������
    using Comparator = std::function<bool(const runtime::ObjectHolder&,
        const runtime::ObjectHolder&, runtime::Context&)>;

    Comparison(Comparator cmp, std::unique_ptr<Statement> lhs, std::unique_ptr<Statement> rhs);

    // ��������� �������� ��������� lhs � rhs � ���������� ��������� ������ comparator,
    // ���������� � ���� runtime::Bool
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    Comparator cmp_;
};

// ----------------------------------------------------------------------------

class ReturnException : std::runtime_error {
public:
    explicit ReturnException(runtime::ObjectHolder&& result)
        : std::runtime_error("result")
        , result_(std::move(result)) {
    }

    runtime::ObjectHolder GetResult() const {
        return result_;
    }

private:
    runtime::ObjectHolder result_{};
};

// ----------------------------------------------------------------------------

// ��������� ���������� return � ���������� statement
class Return : public Statement {
public:
    explicit Return(std::unique_ptr<Statement> statement)
        : statement_(std::move(statement)) {
    }

    // ������������� ���������� �������� ������. ����� ���������� ��������� ���������� statement,
    // ����������� � �����������
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    std::unique_ptr<Statement> statement_;
};

// ----------------------------------------------------------------------------

// ���� ������. ��� �������, �������� ��������� ����������
class MethodBody : public Statement {
public:
    explicit MethodBody(std::unique_ptr<Statement>&& body);

    // ��������� ����������, ���������� � �������� body.
    // ���� ������ body ���� ��������� ���������� return, ���������� ��������� return
    // � ��������� ������ ���������� None
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    std::unique_ptr<Statement> body_;
};

// ----------------------------------------------------------------------------

// ��������� �����
class ClassDefinition : public Statement {
public:
    // �������������, ��� ObjectHolder �������� ������ ���� runtime::Class
    explicit ClassDefinition(runtime::ObjectHolder cls);

    // ������ ������ closure ����� ������, ����������� � ������ ������ � ���������, ���������� �
    // �����������
    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    runtime::ObjectHolder cls_;
};

// ----------------------------------------------------------------------------

// ���������� if <condition> <if_body> else <else_body>
class IfElse : public Statement {
public:
    // �������� else_body ����� ���� ����� nullptr
    IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
        std::unique_ptr<Statement> else_body);

    runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

private:
    std::unique_ptr<Statement> condition_;
    std::unique_ptr<Statement> if_body_;
    std::unique_ptr<Statement> else_body_;
};

// ----------------------------------------------------------------------------

}  // namespace ast
