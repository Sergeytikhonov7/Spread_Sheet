#include "common.h"
#include "test_runner_p.h"
#include "FormulaAST.h"
#include "formula.h"

inline std::ostream& operator<<(std::ostream& output, Position pos) {
    return output << "(" << pos.row << ", " << pos.col << ")";
}

inline Position operator "" _pos(const char* str, std::size_t) {
    return Position::FromString(str);
}

inline std::ostream& operator<<(std::ostream& output, Size size) {
    return output << "(" << size.rows << ", " << size.cols << ")";
}

inline std::ostream& operator<<(std::ostream& output, const CellInterface::Value& value) {
    std::visit(
            [&](const auto& x) {
                output << x;
            },
            value);
    return output;
}

namespace {

    void TestEmpty() {
        auto sheet = CreateSheet();
        ASSERT_EQUAL(sheet->GetPrintableSize(), (Size{0, 0}));
    }

    void TestInvalidPosition() {
        auto sheet = CreateSheet();
        try {
            sheet->SetCell(Position{-1, 0}, "");
        } catch (const InvalidPositionException&) {
        }
        try {
            sheet->GetCell(Position{0, -2});
        } catch (const InvalidPositionException&) {
        }
        try {
            sheet->ClearCell(Position{Position::MAX_ROWS, 0});
        } catch (const InvalidPositionException&) {
        }
    }

    void TestSetCellPlainText() {
        auto sheet = CreateSheet();

        auto checkCell = [&](Position pos, std::string text) {
            sheet->SetCell(pos, text);
            CellInterface* cell = sheet->GetCell(pos);
            ASSERT(cell != nullptr);
            ASSERT_EQUAL(cell->GetText(), text);
            ASSERT_EQUAL(std::get<std::string>(cell->GetValue()), text);
        };

        checkCell("A1"_pos, "Hello");
        checkCell("A1"_pos, "World");
        checkCell("B2"_pos, "Purr");
        checkCell("A3"_pos, "Meow");

        const SheetInterface& constSheet = *sheet;
        ASSERT_EQUAL(constSheet.GetCell("B2"_pos)->GetText(), "Purr");

        sheet->SetCell("A3"_pos, "'=escaped");
        CellInterface* cell = sheet->GetCell("A3"_pos);
        ASSERT_EQUAL(cell->GetText(), "'=escaped");
        ASSERT_EQUAL(std::get<std::string>(cell->GetValue()), "=escaped");
    }

    void TestClearCell() {
        auto sheet = CreateSheet();

        sheet->SetCell("C2"_pos, "Me gusta");
        sheet->ClearCell("C2"_pos);
        ASSERT(sheet->GetCell("C2"_pos) == nullptr);
        ASSERT_EQUAL(sheet->GetPrintableSize(), (Size{0, 0}));

        sheet->ClearCell("A1"_pos);
        sheet->ClearCell("J10"_pos);
    }

    void TestPrint() {
        auto sheet = CreateSheet();
        sheet->SetCell("A2"_pos, "meow");
        sheet->SetCell("B2"_pos, "=1+2");
        sheet->SetCell("A1"_pos, "=1/0");

        ASSERT_EQUAL(sheet->GetPrintableSize(), (Size{2, 2}));

        std::ostringstream texts;
        sheet->PrintTexts(texts);
        ASSERT_EQUAL(texts.str(), "=1/0\t\nmeow\t=1+2\n");

        std::ostringstream values;
        sheet->PrintValues(values);
        ASSERT_EQUAL(values.str(), "#DIV/0!\t\nmeow\t3\n");

        sheet->ClearCell("B2"_pos);
        ASSERT_EQUAL(sheet->GetPrintableSize(), (Size{2, 1}));
    }

    void TestErrorValue() {
        auto sheet = CreateSheet();
        sheet->SetCell("E2"_pos, "A1");
        sheet->SetCell("E4"_pos, "=E2");
        ASSERT_EQUAL(sheet->GetCell("E4"_pos)->GetValue(),
                     CellInterface::Value(FormulaError::Category::Value))
        sheet->SetCell("E2"_pos, "3D");
        ASSERT_EQUAL(sheet->GetCell("E4"_pos)->GetValue(),
                     CellInterface::Value(FormulaError::Category::Value))
    }

    void TestEmptyCellTreatedAsZero() {
        auto sheet = CreateSheet();
        sheet->SetCell("A1"_pos, "=B2");
        ASSERT_EQUAL(sheet->GetCell("A1"_pos)->GetValue(), CellInterface::Value(0))
    }

    void TestFormulaInvalidPosition() {
        auto sheet = CreateSheet();
        auto try_formula = [&](const std::string& formula) {
            try {
                sheet->SetCell("A1"_pos, formula);
                ASSERT(false)
            } catch (const FormulaException&) {
                // we expect this one
            }
        };
        try_formula("=X0");
        try_formula("=ABCD1");
        try_formula("=A123456");
        try_formula("=ABCDEFGHIJKLMNOPQRS1234567890");
        try_formula("=XFD16385");
        try_formula("=XFE16384");
        try_formula("=R2D2");
    }

    void TestCellReferences() {
        auto sheet = CreateSheet();
        sheet->SetCell("A1"_pos, "1");
        sheet->SetCell("A2"_pos, "=A1");
        sheet->SetCell("B2"_pos, "=A1");
        ASSERT(sheet->GetCell("A1"_pos)->GetReferencedCells().empty())
        ASSERT_EQUAL(sheet->GetCell("A2"_pos)->GetReferencedCells(),
                     std::vector{"A1"_pos})
        ASSERT_EQUAL(sheet->GetCell("B2"_pos)->GetReferencedCells(),
                     std::vector{"A1"_pos})
        // Ссылка на пустую ячейку
        sheet->SetCell("B2"_pos, "=B1");
        ASSERT(sheet->GetCell("B1"_pos)->GetReferencedCells().empty())
        ASSERT_EQUAL(sheet->GetCell("B2"_pos)->GetReferencedCells(),
                     std::vector{"B1"_pos})
        sheet->SetCell("A2"_pos, "");
        ASSERT(sheet->GetCell("A1"_pos)->GetReferencedCells().empty())
        ASSERT(sheet->GetCell("A2"_pos)->GetReferencedCells().empty())
        // Ссылка на ячейку за пределами таблицы
        sheet->SetCell("B1"_pos, "=C3");
        ASSERT_EQUAL(sheet->GetCell("B1"_pos)->GetReferencedCells(),
                     std::vector{"C3"_pos})
    }

    void TestCellCircularReferences() {
        auto sheet = CreateSheet();
        sheet->SetCell("E2"_pos, "=E4");
        sheet->SetCell("E4"_pos, "=X9");
        sheet->SetCell("X9"_pos, "=M6");
        sheet->SetCell("M6"_pos, "Ready");
        bool caught = false;
        try {
            sheet->SetCell("M6"_pos, "=E2");
        } catch (const CircularDependencyException&) {
            caught = true;
        }
        ASSERT(caught)
        ASSERT_EQUAL(sheet->GetCell("M6"_pos)->GetText(), "Ready")
    }

    void TestFormulaReferences() {
        auto sheet = CreateSheet();
        auto evaluate = [&](std::string expr) {
            return std::get<double>(ParseFormula(std::move(expr))->Evaluate(*sheet));
        };
        sheet->SetCell("A1"_pos, "1");
        ASSERT_EQUAL(evaluate("A1"), 1)
        sheet->SetCell("A2"_pos, "2");
        ASSERT_EQUAL(evaluate("A1+A2"), 3)
        // Тест на нули:
        sheet->SetCell("B3"_pos, "");
        ASSERT_EQUAL(evaluate("A1+B3"), 1)  // Ячейка с пустым текстом
        ASSERT_EQUAL(evaluate("A1+B1"), 1)  // Пустая ячейка
        ASSERT_EQUAL(evaluate("A1+E4"), 1)  // Ячейка за пределами таблицы
    }

    void TestFormulaArithmetic() {
        auto sheet = CreateSheet();
        auto evaluate = [&](std::string expr) {
            return std::get<double>(ParseFormula(std::move(expr))->Evaluate(*sheet));
        };
        ASSERT_EQUAL(evaluate("1"), 1)
        ASSERT_EQUAL(evaluate("42"), 42)
        ASSERT_EQUAL(evaluate("2 + 2"), 4)
        ASSERT_EQUAL(evaluate("2 + 2*2"), 6)
        ASSERT_EQUAL(evaluate("4/2 + 6/3"), 4)
        ASSERT_EQUAL(evaluate("(2+3)*4 + (3-4)*5"), 15)
        ASSERT_EQUAL(evaluate("(12+13) * (14+(13-24/(1+1))*55-46)"), 575)
    }

    void TestFormulaReferencedCells() {
        ASSERT(ParseFormula("1")->GetReferencedCells().empty())
        auto a1 = ParseFormula("A1");
        ASSERT_EQUAL(a1->GetReferencedCells(), (std::vector{"A1"_pos}))
        auto b2c3 = ParseFormula("B2+C3");
        ASSERT_EQUAL(b2c3->GetReferencedCells(), (std::vector{"B2"_pos, "C3"_pos}))
        auto tricky = ParseFormula("A1 + A2 + A1 + A3 + A1 + A2 + A1");
        ASSERT_EQUAL(tricky->GetExpression(), "A1+A2+A1+A3+A1+A2+A1")
        ASSERT_EQUAL(tricky->GetReferencedCells(),
                     (std::vector{"A1"_pos, "A2"_pos, "A3"_pos}))
    }

}  // namespace

int main() {
    TestRunner tr;
    RUN_TEST(tr, TestEmpty);
    RUN_TEST(tr, TestInvalidPosition);
    RUN_TEST(tr, TestSetCellPlainText);
    RUN_TEST(tr, TestClearCell);
    RUN_TEST(tr, TestPrint);
    RUN_TEST(tr, TestErrorValue);
    RUN_TEST(tr, TestEmptyCellTreatedAsZero);
    RUN_TEST(tr, TestFormulaInvalidPosition);
    RUN_TEST(tr, TestCellReferences);
    RUN_TEST(tr, TestCellCircularReferences);
    RUN_TEST(tr, TestFormulaReferences);
    RUN_TEST(tr, TestFormulaArithmetic);
    RUN_TEST(tr, TestFormulaReferencedCells);
    return 0;
}
  