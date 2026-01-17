#include "statements.cpp"

int main() {
    std::string sql =
        "SELECT price * discount / 100 "
        "FROM products "
        "WHERE price >= 100 AND discount < 20;";

    Parser parser(sql);
    Statement stmt = parse_statement(parser);

    // Access SELECT statement through Statement abstraction
    if (stmt.get_type() == StatementType::Select) {
        const SelectStmt& select_stmt = stmt.as_select();
        std::cout << "Parsed SELECT on table: " << select_stmt.table << "\n";
    }
    
    // Example with ORDER BY and GROUP BY
    std::string sql2 =
        "SELECT name, price "
        "FROM products "
        "WHERE price >= 100 "
        "ORDER BY price "
        "GROUP BY category;";
    
    Parser parser2(sql2);
    Statement stmt2 = parse_statement(parser2);
    
    if (stmt2.get_type() == StatementType::Select) {
        const SelectStmt& select_stmt2 = stmt2.as_select();
        std::cout << "Parsed SELECT on table: " << select_stmt2.table << "\n";
        std::cout << "ORDER BY columns: " << select_stmt2.order_by.size() << "\n";
        std::cout << "GROUP BY columns: " << select_stmt2.group_by.size() << "\n";
    }
    
    return 0;
}
