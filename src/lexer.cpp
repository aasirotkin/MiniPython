#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

using namespace std;

namespace parse {

// ----------------------------------------------------------------------------

bool operator==(const Token& lhs, const Token& rhs) {
    using namespace token_type;

    if (lhs.index() != rhs.index()) {
        return false;
    }
    if (lhs.Is<Char>()) {
        return lhs.As<Char>().value == rhs.As<Char>().value;
    }
    if (lhs.Is<Number>()) {
        return lhs.As<Number>().value == rhs.As<Number>().value;
    }
    if (lhs.Is<String>()) {
        return lhs.As<String>().value == rhs.As<String>().value;
    }
    if (lhs.Is<Id>()) {
        return lhs.As<Id>().value == rhs.As<Id>().value;
    }
    return true;
}

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const Token& rhs) {
    using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Id);
    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

    UNVALUED_OUTPUT(Class);
    UNVALUED_OUTPUT(Return);
    UNVALUED_OUTPUT(If);
    UNVALUED_OUTPUT(Else);
    UNVALUED_OUTPUT(Def);
    UNVALUED_OUTPUT(Newline);
    UNVALUED_OUTPUT(Print);
    UNVALUED_OUTPUT(Indent);
    UNVALUED_OUTPUT(Dedent);
    UNVALUED_OUTPUT(And);
    UNVALUED_OUTPUT(Or);
    UNVALUED_OUTPUT(Not);
    UNVALUED_OUTPUT(Eq);
    UNVALUED_OUTPUT(NotEq);
    UNVALUED_OUTPUT(LessOrEq);
    UNVALUED_OUTPUT(GreaterOrEq);
    UNVALUED_OUTPUT(None);
    UNVALUED_OUTPUT(True);
    UNVALUED_OUTPUT(False);
    UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

    return os << "Unknown token :("sv;
}

// ----------------------------------------------------------------------------

namespace detail_lexer {

Token LexerTokenCreator::NextToken(std::istream& input) {
    if (IsEof(input)) {
        return CreateEof(input);
    }

    char c = PeekChar(input);

    if (is_new_line_) {
        return SkipEmptyLinesAndCreateIndent(input);
    }

    if (IsSpace(c)) {
        SkipSpaces(input);
        return NextToken(input);
    }

    if (IsJail(c)) {
        SkipTillTheEndOfLine(input);
        return NextToken(input);
    }

    if (IsQuote(c)) {
        return CreateString(input);
    }

    if (IsSpecialSign(c)) {
        return CreateSpecialSignOrChar(input);
    }

    if (IsDigit(c)) {
        return CreateNumber(input);
    }

    if (IsEndOfLine(c)) {
        return CreateNewLine(input);
    }

    return CreateSpecialWordOrId(input);
}

Token LexerTokenCreator::SkipEmptyLinesAndCreateIndent(std::istream& input) {
    is_new_line_ = false;
    const std::string indent = ReadSpaces(input);

    if (IsNextEndOfLine(input)) {
        CreateNewLine(input);
        return NextToken(input);
    }

    return CreateIndent(indent.size());
}

Token LexerTokenCreator::CreateIndent(size_t local_indent_counter) {
    int delta = static_cast<int>(local_indent_counter) - static_cast<int>(indent_counter_);
    int step = static_cast<int>(lexer_consts::INDENT_STEP);
    indent_counter_ = local_indent_counter;

    if (delta % lexer_consts::INDENT_STEP != 0) {
        using namespace std::literals;
        std::stringstream ss;
        ss << "Unknown indent: local counter is "sv << local_indent_counter << ", delta is "sv << delta;
        throw LexerError(ss.str());
    }

    if (delta == 0) {
        return token_type::Savedent{};
    }
    else if (delta >= step) {
        return token_type::IndentCounter{ static_cast<size_t>(delta / step) };
    }
    else /*if (delta <= -step)*/ {
        return token_type::DedentCounter{ static_cast<size_t>(-delta / step) };
    }
}

Token LexerTokenCreator::CreateNewLine(std::istream& input) {
    GetChar(input);
    is_new_line_ = true;
    return token_type::Newline{};
}

Token LexerTokenCreator::CreateEof(std::istream& input) {
    GetChar(input);
    return token_type::Eof{};
}

Token LexerTokenCreator::CreateNumber(std::istream& input) {
    return token_type::Number{ std::stoi(ReadWordIf(input, IsDigit)) };
}

Token LexerTokenCreator::CreateString(std::istream& input) {
    char q = GetChar(input);
    std::string output = ReadWordIf(input, [q](char c) { return c != q; });
    q = GetChar(input);
    return token_type::String{ std::move(output) };
}

Token LexerTokenCreator::CreateSpecialSignOrChar(std::istream& input) {
    char c = GetChar(input);
    std::string output{ c };
    if (input) {
        char next_c = GetChar(input);
        if (next_c == '=') {
            output += next_c;
            if (lexer_consts::OPERATION_SIGNS.count(output) != 0) {
                return lexer_consts::OPERATION_SIGNS.at(output);
            }
        }
        input.putback(next_c);
    }
    return token_type::Char{ c };
}

Token LexerTokenCreator::CreateSpecialWordOrId(std::istream& input) {
    std::string output = ReadWordIf(input, [](char c) { return IsAlpha(c) || IsDigit(c); });
    return (lexer_consts::SPECIAL_WORDS.count(output) != 0)
        ? lexer_consts::SPECIAL_WORDS.at(output)
        : token_type::Id{ std::move(output) };
}

std::deque<Token> SplitIntoTokens(std::istream& input) {
    LexerTokenCreator token_creator;
    const size_t& indent_counter = token_creator.IndentCounter();
    std::deque<Token> tokens;
    while (input) {
        Token token = token_creator.NextToken(input);
        if (token == token_type::Savedent{}) {
            continue;
        }
        if (tokens.empty() && token == token_type::Newline{}) {
            continue;
        }
        if (token.Is<token_type::IndentCounter>()) {
            for (size_t i = 0; i < token.As<token_type::IndentCounter>().value; ++i) {
                tokens.push_back(token_type::Indent{});
            }
            continue;
        }
        if (token.Is<token_type::DedentCounter>()) {
            for (size_t i = 0; i < token.As<token_type::DedentCounter>().value; ++i) {
                tokens.push_back(token_type::Dedent{});
            }
            continue;
        }
        if (token == token_type::Eof{} && indent_counter > 0) {
            for (size_t i = 0; i < static_cast<size_t>(indent_counter / lexer_consts::INDENT_STEP); ++i) {
                tokens.push_back(token_type::Dedent{});
            }
        }
        tokens.push_back(std::move(token));
    }
    if (!tokens.empty() && tokens.back() != token_type::Eof{} && tokens.back() != token_type::Newline{}) {
        tokens.push_back(token_type::Newline{});
    }
    if (tokens.empty() || tokens.back() != token_type::Eof{}) {
        tokens.push_back(token_type::Eof{});
    }
    return tokens;
}

} // namespace detail_lexer

// ----------------------------------------------------------------------------

Lexer::Lexer(std::istream& input)
    : tokens_(detail_lexer::SplitIntoTokens(input)) {
}

const Token& Lexer::CurrentToken() const {
    if (current_index_ < tokens_.size()) {
        return tokens_.at(current_index_);
    }
    else {
        static const Token eof = token_type::Eof{};
        return eof;
    }
}

Token Lexer::NextToken() {
    current_index_++;
    return CurrentToken();
}

}  // namespace parse
