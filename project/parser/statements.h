#ifndef STATEMENTS_H
#define STATEMENTS_H

#include <vector>
#include <string>
#include <stdexcept>
#include <variant>

// Forward declarations
struct Expr;
struct Token;
enum class TokenType;
class Parser;

// Statement type enum
enum class StatementType {
    Select,
    Insert  // For future use
};

// Select statement structure
struct SelectStmt {
    std::vector<Expr*> columns;
    std::string table;
    Expr* where = nullptr;
    std::vector<Expr*> order_by;  // Optional
    std::vector<Expr*> group_by;  // Optional
};

// Statement abstraction class using std::variant
class Statement {
private:
    std::variant<SelectStmt> data;

public:
    // Constructor for Select statement
    Statement(SelectStmt stmt) : data(stmt) {}

    // Copy constructor (default is fine with std::variant)
    Statement(const Statement& other) = default;

    // Assignment operator (default is fine with std::variant)
    Statement& operator=(const Statement& other) = default;

    // Destructor (default is fine with std::variant)
    ~Statement() = default;

    // Get statement type
    StatementType get_type() const {
        if (std::holds_alternative<SelectStmt>(data)) {
            return StatementType::Select;
        }
        throw std::runtime_error("Unknown statement type");
    }

    // Access SELECT statement
    SelectStmt& as_select() {
        if (!std::holds_alternative<SelectStmt>(data)) {
            throw std::runtime_error("Statement is not a SELECT statement");
        }
        return std::get<SelectStmt>(data);
    }

    const SelectStmt& as_select() const {
        if (!std::holds_alternative<SelectStmt>(data)) {
            throw std::runtime_error("Statement is not a SELECT statement");
        }
        return std::get<SelectStmt>(data);
    }
};

// Function to parse SELECT statement (returns SelectStmt, will be wrapped in Statement)
SelectStmt parse_select(Parser& parser);

#endif // STATEMENTS_H
