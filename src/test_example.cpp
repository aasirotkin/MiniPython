#include "lexer.h"
#include "parse.h"
#include "runtime.h"
#include "statement.h"
#include "test_runner_p.h"

using namespace std;

namespace parse {

void RunOpenLexerTests(TestRunner& tr);

void TestParseProgram(TestRunner& tr);

} // namespace parse

namespace ast {

void RunUnitTests(TestRunner& tr);

} // namespace ast

namespace runtime {

void RunObjectHolderTests(TestRunner& tr);
void RunObjectsTests(TestRunner& tr);
void RunComparisonTests(TestRunner& tr);

} // namespace runtime

namespace tests {

void RunMythonProgram(istream& input, ostream& output) {
    parse::Lexer lexer(input);
    auto program = ParseProgram(lexer);

    runtime::SimpleContext context{output};
    runtime::Closure closure;
    program->Execute(closure, context);
}

void TestSimplePrints() {
    istringstream input(R"(
print 57
print 10, 24, -8
print 'hello'
print "world"
print True, False
print
print None
)");

    ostringstream output;
    RunMythonProgram(input, output);

    ASSERT_EQUAL(output.str(), "57\n10 24 -8\nhello\nworld\nTrue False\n\nNone\n");
}

void TestAssignments() {
    istringstream input(R"(
x = 57
print x
x = 'C++ black belt'
print x
y = False
x = y
print x
x = None
print x, y
)");

    ostringstream output;
    RunMythonProgram(input, output);

    ASSERT_EQUAL(output.str(), "57\nC++ black belt\nFalse\nNone False\n");
}

void TestArithmetics() {
    istringstream input("print 1+2+3+4+5, 1*2*3*4*5, 1-2-3-4-5, 36/4/3, 2*5+10/2");

    ostringstream output;
    RunMythonProgram(input, output);

    ASSERT_EQUAL(output.str(), "15 120 -13 3 15\n");
}

void TestVariablesArePointers() {
    istringstream input(R"(
class Counter:
  def __init__():
    self.value = 0

  def add():
    self.value = self.value + 1

class Dummy:
  def do_add(counter):
    counter.add()

x = Counter()
y = x

x.add()
y.add()

print x.value

d = Dummy()
d.do_add(x)

print y.value
)");

    ostringstream output;
    RunMythonProgram(input, output);

    ASSERT_EQUAL(output.str(), "2\n3\n");
}

void TestComparison() {
    istringstream input(R"(
class Point:
  def __init__(px, py):
    self.px = px
    self.py = py

  def __eq__(other):
    px_bool = (self.px == other.px)
    py_bool = (self.py == other.py)
    return px_bool and py_bool

  def __lt__(other):
    pxy_self = self.px * self.py
    pxy_other = other.px * other.py
    return pxy_self < pxy_other

  def TestOr(value):
    return self.px == value or self.py == value

  def TestAnd(value):
    return self.px == value and self.py == value

  def TestNot(value):
    return not (self.px == value) and not (self.py == value)

class Point2(Point):
  def __init__(px, py):
    self.px = px
    self.py = py

class Point3(Point2):
  def __init__(px, py):
    self.px = px
    self.py = py

p1 = Point(1, 1)
p2 = Point2(2, 2)
p3 = Point3(2, 2)

p4 = None
p5 = None

print (p1 == p2), (p1 != p2), (p2 == p3), (p2 != p3)

print (p1 < p2), (p1 >= p2), (p2 <= p3), (p3 > p1), (p4 == p5)

p5 = Point(1, 2)

print p5.TestOr(0), p5.TestOr(1), p5.TestAnd(1), p5.TestAnd(2), p5.TestNot(6)
)");

    ostringstream output;
    RunMythonProgram(input, output);

    ASSERT_EQUAL(output.str(), "False True True False\nTrue False True True True\nFalse True False False True\n");
}

void TestAll() {
    TestRunner tr;
    parse::RunOpenLexerTests(tr);
    runtime::RunObjectHolderTests(tr);
    runtime::RunObjectsTests(tr);
    runtime::RunComparisonTests(tr);
    ast::RunUnitTests(tr);
    parse::TestParseProgram(tr);

    RUN_TEST(tr, TestSimplePrints);
    RUN_TEST(tr, TestAssignments);
    RUN_TEST(tr, TestArithmetics);
    RUN_TEST(tr, TestVariablesArePointers);
    RUN_TEST(tr, TestComparison);
}

} // namespace tests
