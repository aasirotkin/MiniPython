#pragma once

#include <deque>
#include <iosfwd>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <utility>

namespace parse {

namespace token_type {

struct Number {        // Лексема «число»
    int value;
};

struct Id {            // Лексема «идентификатор»
    std::string value;
};

struct Char {          // Лексема «символ»
    char value;
};

struct String {        // Лексема «строковая константа»
    std::string value;
};

struct IndentCounter { // Лексема сохраняющая число лексем Indent или Dedent
    bool is_dedent;
    size_t value;
};

struct Class {};       // Лексема «class»
struct Return {};      // Лексема «return»
struct If {};          // Лексема «if»
struct Else {};        // Лексема «else»
struct Def {};         // Лексема «def»
struct Newline {};     // Лексема «конец строки»
struct Print {};       // Лексема «print»
struct Indent {};      // Лексема «увеличение отступа», соответствует двум пробелам
struct Dedent {};      // Лексема «уменьшение отступа»
struct Savedent {};    // Лексема «сохранение отступа»
struct Eof {};         // Лексема «конец файла»
struct And {};         // Лексема «and»
struct Or {};          // Лексема «or»
struct Not {};         // Лексема «not»
struct Eq {};          // Лексема «==»
struct NotEq {};       // Лексема «!=»
struct LessOrEq {};    // Лексема «<=»
struct GreaterOrEq {}; // Лексема «>=»
struct None {};        // Лексема «None»
struct True {};        // Лексема «True»
struct False {};       // Лексема «False»

}  // namespace token_type

// ----------------------------------------------------------------------------

using TokenBase
    = std::variant<token_type::Number, token_type::Id, token_type::Char, token_type::String,
                   token_type::Class, token_type::Return, token_type::If, token_type::Else,
                   token_type::Def, token_type::Newline, token_type::Print, token_type::Indent,
                   token_type::IndentCounter, token_type::Dedent, token_type::Savedent,
                   token_type::And, token_type::Or, token_type::Not, token_type::Eq, token_type::NotEq,
                   token_type::LessOrEq, token_type::GreaterOrEq, token_type::None, token_type::True,
                   token_type::False, token_type::Eof>;

// ----------------------------------------------------------------------------

struct Token : TokenBase {
    using TokenBase::TokenBase;

    template <typename T>
    [[nodiscard]] bool Is() const {
        return std::holds_alternative<T>(*this);
    }

    template <typename T>
    [[nodiscard]] const T& As() const {
        return std::get<T>(*this);
    }

    template <typename T>
    [[nodiscard]] const T* TryAs() const {
        return std::get_if<T>(this);
    }
};

// ----------------------------------------------------------------------------

namespace lexer_consts {

static const size_t INDENT_STEP = 2;

static const std::unordered_set<char> SPECIAL_SIGNS = {
    ':', '(', ')', ',', '.', '+', '-', '*', '/', '!', '>', '<', '='
};

static const std::unordered_map<std::string, Token> OPERATION_SIGNS = {
    { std::string("=="), token_type::Eq{}          },
    { std::string("!="), token_type::NotEq{}       },
    { std::string("<="), token_type::LessOrEq{}    },
    { std::string(">="), token_type::GreaterOrEq{} }
};

static const std::unordered_map<std::string, Token> SPECIAL_WORDS = {
    { std::string("class"),  token_type::Class{}   },
    { std::string("return"), token_type::Return{}  },
    { std::string("if"),     token_type::If{}      },
    { std::string("else"),   token_type::Else{}    },
    { std::string("def"),    token_type::Def{}     },
    { std::string("print"),  token_type::Print{}   },
    { std::string("and"),    token_type::And{}     },
    { std::string("or"),     token_type::Or{}      },
    { std::string("not"),    token_type::Not{}     },
    { std::string("None"),   token_type::None{}    },
    { std::string("True"),   token_type::True{}    },
    { std::string("False"),  token_type::False{}   }
};

} // namespace lexer_consts

// ----------------------------------------------------------------------------

bool operator==(const Token& lhs, const Token& rhs);
bool operator!=(const Token& lhs, const Token& rhs);

std::ostream& operator<<(std::ostream& os, const Token& rhs);

// ----------------------------------------------------------------------------

class LexerError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// ----------------------------------------------------------------------------

namespace detail_lexer {

inline char GetChar(std::istream& input) {
    return static_cast<char>(input.get());
}

inline char PeekChar(std::istream& input) {
    return static_cast<char>(input.peek());
}

inline bool IsAlpha(char c) {
    return std::isalpha(c) || c == '_';
}

inline bool IsDigit(char c) {
    return std::isdigit(c);
}

inline bool IsEndOfLine(char c) {
    return c == '\n';
}

inline bool IsNotEndOfLine(char c) {
    return c != '\n';
}

inline bool IsJail(char c) {
    return c == '#';
}

inline bool IsSpecialSign(char c) {
    return lexer_consts::SPECIAL_SIGNS.count(c) != 0;
}

inline bool IsQuote(char c) {
    return c == '\'' || c == '\"';
}

inline bool IsSpace(char c) {
    return c == ' ';
}

inline bool IsNextEndOfLine(std::istream& input) {
    if (!input) {
        return true;
    }
    char c = PeekChar(input);
    if (IsEndOfLine(c)) {
        return true;
    }
    return false;
}

inline bool IsEof(std::istream& input) {
    if (!input) {
        return true;
    }
    char c = GetChar(input);
    if (!input) {
        return true;
    }
    input.putback(c);
    return false;
}

template <typename Comparator>
inline void SkipWordIf(std::istream& input, Comparator cmp) {
    char c;
    while (input && (c = GetChar(input))) {
        if (!cmp(c)) {
            break;
        }
    }
    if (input) {
        input.putback(c);
    }
}

inline void SkipSpaces(std::istream& input) {
    SkipWordIf(input, IsSpace);
}

inline void SkipTillTheEndOfLine(std::istream& input) {
    SkipWordIf(input, IsNotEndOfLine);
}

template <typename Comparator>
inline std::string ReadWordIf(std::istream& input, Comparator cmp) {
    char c;
    std::string output;
    while (input && (c = GetChar(input))) {
        if (!cmp(c)) {
            break;
        }
        output.push_back(c);
    }
    if (input) {
        input.putback(c);
    }
    return output;
}

inline std::string ReadSpaces(std::istream& input) {
    return ReadWordIf(input, IsSpace);
}

// ----------------------------------------------------------------------------

class LexerTokenCreator {
public:
    Token NextToken(std::istream& input);

private:
    Token SkipEmptyLinesAndCreateIndent(std::istream& input);

    Token CreateIndent(size_t local_indent_counter);

    Token CreateNewLine(std::istream& input);

    Token CreateEof(std::istream& input);

    Token CreateNumber(std::istream& input);

    Token CreateString(std::istream& input);

    Token CreateSpecialSignOrChar(std::istream& input);

    Token CreateSpecialWordOrId(std::istream& input);

private:
    bool is_new_line_ = true;
    bool is_end_of_file_ = false;
    size_t indent_counter_ = 0;
};

// ----------------------------------------------------------------------------

void PushIndentsInPlace(std::deque<Token>& tokens, const token_type::IndentCounter& indent_cnt);

void CheckTokensBackInPlace(std::deque<Token>& tokens);

// ----------------------------------------------------------------------------

inline std::deque<Token> SplitIntoTokens(std::istream& input);

} // namespace detail_lexer

// ----------------------------------------------------------------------------

class Lexer {
public:
    explicit Lexer(std::istream& input);

    // Возвращает ссылку на текущий токен или token_type::Eof, если поток токенов закончился
    [[nodiscard]] const Token& CurrentToken() const;

    // Возвращает следующий токен, либо token_type::Eof, если поток токенов закончился
    Token NextToken();

    // Если текущий токен имеет тип T, метод возвращает ссылку на него.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T>
    const T& Expect() const {
        if (!CurrentToken().Is<T>()) {
            using namespace std::literals;
            throw LexerError("Expect token type error"s);
        }
        return CurrentToken().As<T>();
    }

    // Метод проверяет, что текущий токен имеет тип T, а сам токен содержит значение value.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T, typename U>
    void Expect(const U& value) const {
        const auto& token = Expect<T>();
        if (token.value != value) {
            using namespace std::literals;
            throw LexerError("Expect token value error"s);
        }
    }

    // Если следующий токен имеет тип T, метод возвращает ссылку на него.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T>
    const T& ExpectNext() {
        current_index_++;
        return Expect<T>();
    }

    // Метод проверяет, что следующий токен имеет тип T, а сам токен содержит значение value.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T, typename U>
    void ExpectNext(const U& value) {
        current_index_++;
        return Expect<T>(value);
    }

private:
    std::deque<Token> tokens_;
    size_t current_index_ = 0;
};

}  // namespace parse
