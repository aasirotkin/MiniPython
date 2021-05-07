#pragma once

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace runtime {

// ----------------------------------------------------------------------------

// Контекст исполнения инструкций Mython
class Context {
public:
    // Возвращает поток вывода для команд print
    virtual std::ostream& GetOutputStream() = 0;

protected:
    ~Context() = default;
};

// ----------------------------------------------------------------------------

// Базовый класс для всех объектов языка Mython
class Object {
public:
    virtual ~Object() = default;
    // выводит в os своё представление в виде строки
    virtual void Print(std::ostream& os, Context& context) = 0;
};

// ----------------------------------------------------------------------------

// Специальный класс-обёртка, предназначенный для хранения объекта в Mython-программе
class ObjectHolder {
public:
    // Создаёт пустое значение
    ObjectHolder() = default;

    // Возвращает ObjectHolder, владеющий объектом типа T
    // Тип T - конкретный класс-наследник Object.
    // object копируется или перемещается в кучу
    template <typename T>
    [[nodiscard]] static ObjectHolder Own(T&& object) {
        return ObjectHolder(std::make_shared<T>(std::forward<T>(object)));
    }

    // Создаёт ObjectHolder, не владеющий объектом (аналог слабой ссылки)
    [[nodiscard]] static ObjectHolder Share(Object& object);
    // Создаёт пустой ObjectHolder, соответствующий значению None
    [[nodiscard]] static ObjectHolder None();

    // Возвращает ссылку на Object внутри ObjectHolder.
    // ObjectHolder должен быть непустым
    Object& operator*() const;

    Object* operator->() const;

    [[nodiscard]] Object* Get() const;

    // Возвращает указатель на объект типа T либо nullptr, если внутри ObjectHolder не хранится
    // объект данного типа
    template <typename T>
    [[nodiscard]] T* TryAs() const {
        return dynamic_cast<T*>(this->Get());
    }

    // Возвращает true если указатель на объект типа T равен nullptr
    template <typename T>
    bool IsType() const {
        return TryAs<T>() != nullptr;
    }

    // Возвращает true, если ObjectHolder не пуст
    explicit operator bool() const;

private:
    explicit ObjectHolder(std::shared_ptr<Object> data);
    void AssertIsValid() const;

    std::shared_ptr<Object> data_;
};

// ----------------------------------------------------------------------------

// Объект-значение, хранящий значение типа T
template <typename T>
class ValueObject : public Object {
public:
    ValueObject(T v)  // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
        : value_(v) {
    }

    void Print(std::ostream& os, [[maybe_unused]] Context& context) override {
        os << value_;
    }

    [[nodiscard]] const T& GetValue() const {
        return value_;
    }

private:
    T value_;
};

template <typename T>
bool operator== (const ValueObject<T>& lhs, const ValueObject<T>& rhs) {
    return lhs.GetValue() == rhs.GetValue();
}

// ----------------------------------------------------------------------------

// Таблица символов, связывающая имя объекта с его значением
using Closure = std::unordered_map<std::string, ObjectHolder>;

// ----------------------------------------------------------------------------

// Проверяет, содержится ли в object значение, приводимое к True
// Для 0, False, None, и пустых строк возвращается false, в остальных случаях - false
bool IsTrue(const ObjectHolder& object);

// ----------------------------------------------------------------------------

// Интерфейс для выполнения действий над объектами Mython
class Executable {
public:
    virtual ~Executable() = default;
    // Выполняет действие над объектами внутри closure, используя context
    // Возвращает результирующее значение либо None
    virtual ObjectHolder Execute(Closure& closure, Context& context) = 0;
};

// ----------------------------------------------------------------------------

// Строковое значение
using String = ValueObject<std::string>;

String operator+ (const String& lhs, const String& rhs);

// Числовое значение
using Number = ValueObject<int>;

Number operator+ (const Number& lhs, const Number& rhs);
Number operator- (const Number& lhs, const Number& rhs);
Number operator* (const Number& lhs, const Number& rhs);
Number operator/ (const Number& lhs, const Number& rhs);

// Логическое значение
class Bool : public ValueObject<bool> {
public:
    using ValueObject<bool>::ValueObject;

    void Print(std::ostream& os, Context& context) override;
};

// ----------------------------------------------------------------------------

// Метод класса
struct Method {
    // Имя метода
    std::string name;
    // Имена формальных параметров метода
    std::vector<std::string> formal_params;
    // Тело метода
    std::unique_ptr<Executable> body;
};

// ----------------------------------------------------------------------------

// Класс
class Class : public Object {
public:
    // Создаёт класс с именем name и набором методов methods, унаследованный от класса parent
    // Если parent равен nullptr, то создаётся базовый класс
    explicit Class(std::string name, std::vector<Method> methods, const Class* parent);

    // Возвращает указатель на метод name или nullptr, если метод с таким именем отсутствует
    [[nodiscard]] const Method* GetMethod(const std::string& name) const;

    // Возвращает указатель на метод name или nullptr, если метод с таким именем с таким числом параметров отсутствует
    [[nodiscard]] const Method* GetMethod(const std::string& name, size_t args_count) const;

    // Возвращает имя класса
    [[nodiscard]] const std::string& GetName() const;

    // Выводит в os строку "Class <имя класса>", например "Class cat"
    void Print(std::ostream& os, Context& context) override;

private:
    std::string name_;
    std::vector<Method> methods_;
    const Class* parent_;
};

// ----------------------------------------------------------------------------

// Экземпляр класса
class ClassInstance : public Object {
public:
    explicit ClassInstance(const Class& cls);

    void Print(std::ostream& os, Context& context) override;

    /*
     * Вызывает у объекта метод method, передавая ему actual_args параметров.
     * Параметр context задаёт контекст для выполнения метода.
     * Если ни сам класс, ни его родители не содержат метод method, метод выбрасывает исключение
     * runtime_error
     */
    ObjectHolder Call(const std::string& method, const std::vector<ObjectHolder>& actual_args,
                      Context& context);

    /*
     * Вызывает у объекта метод method, передавая ему actual_args параметров.
     * Параметр context задаёт контекст для выполнения метода.
     */
    ObjectHolder Call(const Method* method, const std::vector<ObjectHolder>& actual_args,
                      Context& context);

    // Возвращает true, если объект имеет метод method, принимающий argument_count параметров
    [[nodiscard]] bool HasMethod(const std::string& method, size_t argument_count) const;

    /*
     * Возвращает указатель на Method.
     * Если ни сам класс, ни его родители не содержат метод method, с заданным числом параметров
     * метод выбрасывает исключение runtime_error
     */
    const Method* GetMethod(const std::string& method, size_t argument_count) const;

    /*
     * Возвращает указатель на Method.
     * Если ни сам класс, ни его родители не содержат метод method, с заданным числом параметров
     * метод возвращает nullptr
     */
    const Method* TryMethod(const std::string& method, size_t argument_count) const;

    // Возвращает ссылку на Closure, содержащий поля объекта
    [[nodiscard]] Closure& Fields();
    // Возвращает константную ссылку на Closure, содержащую поля объекта
    [[nodiscard]] const Closure& Fields() const;

private:
    /*
    * Возвращает таблицу символов с именами из formal_params и значениями из actual_args.
    * Размер formal_params должен быть равен размеру actual_args
    */
    Closure CreateLocalClosure(
        const std::vector<std::string>& formal_params,
        const std::vector<ObjectHolder>& actual_args);

private:
    const Class& cls_;
    Closure closure_;
};

// ----------------------------------------------------------------------------

/*
 * Возвращает true, если lhs и rhs содержат одинаковые числа, строки или значения типа Bool.
 * Если lhs - объект с методом __eq__, функция возвращает результат вызова lhs.__eq__(rhs),
 * приведённый к типу Bool. Если lhs и rhs имеют значение None, функция возвращает true.
 * В остальных случаях функция выбрасывает исключение runtime_error.
 *
 * Параметр context задаёт контекст для выполнения метода __eq__
 */
bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);
/*
 * Если lhs и rhs - числа, строки или значения bool, функция возвращает результат их сравнения
 * оператором <.
 * Если lhs - объект с методом __lt__, возвращает результат вызова lhs.__lt__(rhs),
 * приведённый к типу bool. В остальных случаях функция выбрасывает исключение runtime_error.
 *
 * Параметр context задаёт контекст для выполнения метода __lt__
 */
bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);
// Возвращает значение, противоположное Equal(lhs, rhs, context)
bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);
// Возвращает значение lhs>rhs, используя функции Equal и Less
bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);
// Возвращает значение lhs<=rhs, используя функции Equal и Less
bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);
// Возвращает значение, противоположное Less(lhs, rhs, context)
bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);

// ----------------------------------------------------------------------------

// Контекст-заглушка, применяется в тестах.
// В этом контексте весь вывод перенаправляется в строковый поток вывода output
struct DummyContext : Context {
    std::ostream& GetOutputStream() override {
        return output;
    }

    std::string GetStr() {
        return output.str();
    }

    std::ostringstream output;
};

// Простой контекст, в нём вывод происходит в поток output, переданный в конструктор
class SimpleContext : public Context {
public:
    explicit SimpleContext(std::ostream& output)
        : output_(output) {
    }

    std::ostream& GetOutputStream() override {
        return output_;
    }

private:
    std::ostream& output_;
};

// ----------------------------------------------------------------------------

}  // namespace runtime
