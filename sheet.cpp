#include "sheet.h"

#include <algorithm>
#include <iostream>
#include <optional>

using namespace std::literals;

Sheet::~Sheet() {}

void Sheet::SetCell(Position pos, std::string text) {
    IsValidPosition(pos);
    InsertRows(pos);
    auto& cell = cells_[pos.row][pos.col];
    if (!cell) {
        cell = std::make_unique<Cell>(*this);
    }
    cell->Set(text);
    ResizePrintableArea(pos);
}

void Sheet::ResizePrintableArea(const Position& pos) {
    if (pos.row + 1 > printable_size_.rows) {
        printable_size_.rows = pos.row + 1;
    }
    if (pos.col + 1 > printable_size_.cols) {
        printable_size_.cols = pos.col + 1;
    }
}

void Sheet::IsValidPosition(const Position& pos) const {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position");
    }
}

const CellInterface* Sheet::GetCell(Position pos) const {
    IsValidPosition(pos);
    if (pos.row >= int(cells_.size()) || pos.col >= int(cells_[pos.row].size())) {
        return nullptr;
    }
    return cells_[pos.row][pos.col].get();
}

CellInterface* Sheet::GetCell(Position pos) {
    return const_cast<CellInterface*>(static_cast<const Sheet&>(*this).GetCell(pos));
}

void Sheet::ClearCell(Position pos) {
    IsValidPosition(pos);
    if (IsCellInTable(pos)) {
        if (auto& cell = cells_[pos.row][pos.col]) {
            cell->Clear();
            if (!cell->IsReferenced()) {
                cell.reset();
            }
        }
    }
    if (pos.row + 1 == printable_size_.rows || pos.col + 1 == printable_size_.cols) {
        printable_size_ = CalculatePrintableSize();
    }
}

Size Sheet::GetPrintableSize() const {
    return printable_size_;
}

void Sheet::PrintValues(std::ostream& output) const {
    for (int row = 0; row < printable_size_.rows; ++row) {
        for (int col = 0; col < printable_size_.cols; ++col) {
            if (col > 0) {
                output << '\t';
            }
            if (col < int(cells_[row].size())) {
                if (const auto& cell = cells_[row][col]) {
                    auto out = cell->GetValue();
                    if (std::holds_alternative<std::string>(out)) {
                        output << std::get<std::string>(out);
                    }
                    if (std::holds_alternative<double>(out)) {
                        output << std::get<double>(out);
                    }
                    if (std::holds_alternative<FormulaError>(out)) {
                        output << std::get<FormulaError>(out);
                    }
                }
            }
        }
        output << '\n';
    }
}

void Sheet::PrintTexts(std::ostream& output) const {
    for (int row = 0; row < printable_size_.rows; ++row) {
        for (int col = 0; col < printable_size_.cols; ++col) {
            if (col > 0) {
                output << '\t';
            }
            if (col < int(cells_[row].size())) {
                if (const auto& cell = cells_[row][col]) {
                    output << cell->GetText();
                }
            }
        }
        output << '\n';
    }
}

void Sheet::InsertRows(const Position pos) {
    if (pos.row + 1 > int(cells_.size())) {
        cells_.resize(pos.row + 1);
        cells_[pos.row].resize(pos.col + 1);
    } else {
        if (pos.col + 1 > int(cells_[pos.row].size())) {
            for (int row = 0; row < pos.row + 1; ++row) {
                cells_[pos.row].resize(pos.col + 1);
            }
        }
    }
}

Size Sheet::CalculatePrintableSize() {
    Size size;
    for (size_t row = 0; row < cells_.size(); ++row) {
        for (int col = cells_[row].size() - 1; col >= 0; --col) {
            if (const auto& cell = cells_[row][col]) {
                if (!cell->GetText().empty()) {
                    size.rows = std::max(size.rows, int(row + 1));
                    size.cols = std::max(size.cols, int(col + 1));
                    break;
                }
            }
        }
    }
    return size;
}

bool Sheet::IsCellInTable(const Position pos) {
    if (cells_.empty()) {
        return false;
    }
    return int(cells_.size()) > pos.row && int(cells_[pos.row].size()) > pos.col;
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}