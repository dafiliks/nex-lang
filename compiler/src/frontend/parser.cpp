// parser.cpp
// Copyright (C) David Filiks

#include <cstdlib>
#include <iostream>
#include <memory>
#include "tokenizer.hpp"
#include "parser.hpp"
#include "ast.hpp"

Parser::Parser(const std::vector<Token>& tokens): m_tokens(tokens) {}

TermExpr Parser::parse_term()
{
    if (peek().get_type() == TokenType::int_lit)
    {
        IntExpr int_expr{};
        int_expr.m_value = std::stoi(consume().get_value());

        return TermExpr{int_expr};
    }
    else if (peek().get_type() == TokenType::identifier)
    {
        if (peek(1).get_type() == TokenType::sqo_bracket)
        {
            ArrayExpr arr_expr{};
            arr_expr.m_ident = consume().get_value();

            consume();
            arr_expr.m_index = std::make_shared<Expr>(parse_expr());
            try_consume_expect(TokenType::sqc_bracket);
            return TermExpr{arr_expr};
        }

        IdentExpr ident_expr{};
        ident_expr.m_ident = consume().get_value();

        return TermExpr{ident_expr};
    }
    else if (peek().get_type() == TokenType::in)
    {
        InExpr in_expr{};

        consume();
        return TermExpr{in_expr};
    }

    parser_error("Expected term");
}

Expr Parser::parse_expr(int min_prec)
{
    Expr lhs{parse_term()};

    while (true)
    {
        if (!is_bin_op(peek().get_type()) || is_eof(peek().get_type()))
        {
            break;
        }

        BinOpExpr bin_op_expr{};
        int prec{1};

        auto match{bin_op_prec.find(peek().get_type())};
        if (match != bin_op_prec.end())
        {
            prec = match->second;
        }

        if (prec < min_prec)
        {
            break;
        }

        std::string op{consume().get_value()};

        Expr rhs{parse_expr(prec + 1)};

        bin_op_expr.m_op = op;
        bin_op_expr.m_lhs = std::make_shared<Expr>(lhs);
        bin_op_expr.m_rhs = std::make_shared<Expr>(rhs);

        lhs = Expr{bin_op_expr};
    }

    return lhs;
}

Scope Parser::parse_scope()
{
    Scope scope{};

    try_consume_expect(TokenType::o_bracket);

    do
    {
        if (is_eof(peek().get_type()))
        {
            parser_error("No matching bracket found");
        }
        scope.m_body.push_back(parse_stmt());
    }
    while (peek().get_type() != TokenType::c_bracket);

    consume();

    return scope;
}

Stmt Parser::parse_stmt()
{
    if (peek().get_type() == TokenType::var)
    {
        VariableStmt var_stmt{};

        consume();
        var_stmt.m_name = peek().get_value();
        try_consume_expect(TokenType::identifier);

        if (peek().get_type() == TokenType::semi)
        {
            consume();
        }
        else
        {
            try_consume_expect(TokenType::equals);
            var_stmt.m_expr = std::make_shared<Expr>(parse_expr());
            try_consume_expect(TokenType::semi);
        }

        return Stmt{var_stmt};
    }
    else if (peek().get_type() == TokenType::arr)
    {
        ArrayStmt arr_stmt{};

        consume();
        arr_stmt.m_name = peek().get_value();
        try_consume_expect(TokenType::identifier);

        if (peek().get_type() == TokenType::semi)
        {
            consume();
            arr_stmt.m_arr_size = 30000;
            return Stmt{arr_stmt};
        }

        try_consume_expect(TokenType::sqo_bracket);

        if (peek().get_type() == TokenType::sqc_bracket)
        {
            consume();
            arr_stmt.m_arr_size = 30000;
            try_consume_expect(TokenType::semi);
            return Stmt{arr_stmt};
        }

        arr_stmt.m_arr_size = std::atoi(peek().get_value().c_str());

        try_consume_expect(TokenType::int_lit);
        try_consume_expect(TokenType::sqc_bracket);
        try_consume_expect(TokenType::semi);

        return Stmt{arr_stmt};
    }
    else if (peek().get_type() == TokenType::identifier)
    {
        IdentStmt ident_stmt{};
        ident_stmt.m_dest = consume().get_value();

        if (peek().get_type() == TokenType::sqo_bracket)
        {
            consume();
            ident_stmt.m_index = std::make_shared<Expr>(parse_expr());
            try_consume_expect(TokenType::sqc_bracket);
            try_consume_expect(TokenType::equals);
            ident_stmt.m_expr = std::make_shared<Expr>(parse_expr());
            try_consume_expect(TokenType::semi);
            return Stmt{ident_stmt};
        }

        try_consume_expect(TokenType::equals);
        ident_stmt.m_expr = std::make_shared<Expr>(parse_expr());
        try_consume_expect(TokenType::semi);

        return Stmt{ident_stmt};
    }
    else if (peek().get_type() == TokenType::ex)
    {
        ExitStmt ex_stmt{};

        consume();
        ex_stmt.m_expr = std::make_shared<Expr>(parse_expr());
        try_consume_expect(TokenType::semi);

        return Stmt{ex_stmt};
    }
    else if (peek().get_type() == TokenType::set)
    {
        LabelStmt label_stmt{};

        consume();
        label_stmt.m_name = peek().get_value();
        try_consume_expect(TokenType::identifier);
        try_consume_expect(TokenType::semi);

        return Stmt{label_stmt};
    }
    else if (peek().get_type() == TokenType::go)
    {
        GoStmt go_stmt{};

        consume();
        go_stmt.m_dest = peek().get_value();
        try_consume_expect(TokenType::identifier);
        try_consume_expect(TokenType::semi);

        return Stmt{go_stmt};
    }
    else if (peek().get_type() == TokenType::out)
    {
        OutStmt out_stmt{};

        consume();
        out_stmt.m_output = peek().get_value();
        try_consume_expect(TokenType::identifier);
        try_consume_expect(TokenType::semi);

        return Stmt{out_stmt};
    }
    else if (peek().get_type() == TokenType::ifz)
    {
        IfzStmt ifz_stmt{};

        consume();
        ifz_stmt.m_cond = std::make_shared<Expr>(parse_expr());
        ifz_stmt.m_body = std::make_shared<Scope>(parse_scope());

        if (peek().get_type() == TokenType::el)
        {
            ElStmt else_stmt{};

            consume();
            else_stmt.m_body = std::make_shared<Scope>(parse_scope());
            ifz_stmt.m_el = std::make_shared<Stmt>(else_stmt);
        }

        return Stmt{ifz_stmt};
    }

    parser_error("Expected statement");
}

void Parser::parse_program()
{
    while (!is_eof(peek().get_type()))
    {
        m_program.m_body.push_back(parse_stmt());
    }
}

Token Parser::peek(const std::size_t offset) const
{
    if (m_index + offset < m_tokens.size())
    {
        return m_tokens.at(m_index + offset);
    }
    else
    {
        parser_error("Peek offset out of range");
    }
}

Token Parser::consume(const std::size_t distance)
{
    if (m_index + distance < m_tokens.size())
    {
        m_index += distance;
        return m_tokens.at(m_index - distance);
    }
    else
    {
        parser_error("Consume distance out of range");
    }
}

Token Parser::try_consume_expect(const TokenType type)
{
    if (peek().get_type() == type)
    {
        return consume();
    }
    parser_error("Expected " + to_string(type));
}

void Parser::parser_error(const std::string& err_message) const
{
    std::cout << "Nex: parser error: " << err_message << "\n";
    exit(EXIT_FAILURE);
}

std::vector<Token> Parser::get_tokens() const
{
    return m_tokens;
}

Program Parser::get_program() const
{
    return m_program;
}

bool is_eof(const TokenType type)
{
    if (type == TokenType::eof)
    {
        return true;
    }
    return false;
}

bool is_stmt(const TokenType type)
{
    if (type == TokenType::var ||
        type == TokenType::ex ||
        type == TokenType::set ||
        type == TokenType::go)
    {
        return true;
    }
    return false;
}

bool is_bin_op(const TokenType type)
{
    if (type == TokenType::add ||
        type == TokenType::neg ||
        type == TokenType::multiply ||
        type == TokenType::divide)
    {
        return true;
    }
    return false;
}
