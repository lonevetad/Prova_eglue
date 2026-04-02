#include <iostream>
#include <stdio.h>
#include <stdexcept>
#include <string>
#include <map>
#include <array>

#define MAX_RECURSION_DEPTH 10000

#define ALLOW_ETHEROGENEOUS_COMPARISONS true // if true, then numeric and boolean comparisons are allowed BUT statically evaluated to true/false

namespace ExpressionsEvaluator
{
    /*
    A compile-time character lookup tables seems faster than either a cascading chaing of comparisons in OR or an actual std::set<char> membership check
    */
    inline constexpr std::array<bool, 256> UNALLOWED_CHARS_TERMINAL = []()
    {
        std::array<bool, 256> result{};
        constexpr std::array chars{' ', '\t', '\n', '(', ')', '!', '&', '|', '=', '<', '>', ',', '+', '-', '*', '/', '\'', '\"', '\\', '#', '@', '[', ']', '{', '}', ';', ':', '?', '^', '%', '~', '`'}; // add more if needed
        for (char c : chars)
        {
            result[static_cast<unsigned char>(c)] = true;
        }
        return result;
    }();

    /**
     * Technically, "EXPR"(-ession) is not an operator and "TERMINAL" is a "leaf" node
     */
    enum class Operators
    {
        AND,
        OR,
        EQ,
        NEQ,
        LT,
        LTE,
        GT,
        GTE,
        NOT,
        /*SUM, SUBTRACT, MULTIPLY, DIVIDE,*/
        NEGATE,
        EXPR,
        TERMINAL
    };

    enum class ValueType
    {
        UNKNOWN,
        BOOLEAN,
        NUMBER /* double, actually */
    };

    struct ParserNode
    {
        Operators op{Operators::EXPR};
        ValueType valueType;
        int depth;
        std::string *value; /* only for TERMINAL */
        ParserNode *left;   /* only for non-TERMINAL */
        ParserNode *right;  /* only for non-TERMINAL */

        ParserNode(Operators o, ValueType t, std::string *v) : op(o), valueType(t), depth(0), value(v), left(nullptr), right(nullptr) {}
    };

    struct EvaluationContext
    {
        ValueType resultType;
        union
        {
            bool boolValue;
            double numberValue;
        } result;
    };

    struct ParserContext
    {
        int index;
        std::string &expression;

        ParserContext(std::string &e) : index(0), expression(e) {}

        bool is_white(char c) const
        {
            return c == ' ' || c == '\n' || c == '\t';
        }

        bool is_unallowed(char c) const
        {
            return UNALLOWED_CHARS_TERMINAL[static_cast<unsigned char>(c)];
        }
    };

    // cleanup functions

    static void deleteTree(ParserNode *node)
    {
        if (node)
        {
            delete node->value;
            deleteTree(node->left);
            deleteTree(node->right);
            delete node;
        }
    }

    // parser node functions

    static void alterDepth(ParserNode *node, int delta, bool evenMyself)
    {
        if (evenMyself)
        {
            node->depth += delta;
        }
        if (node->left != nullptr)
        {
            alterDepth(node->left, delta, true);
        }
        if (node->right != nullptr)
        {
            alterDepth(node->right, delta, true);
        }
    }
    static void alterDepth(ParserNode *node, int delta)
    {
        alterDepth(node, delta, true);
    }

    static ParserNode *optimize(ParserNode *node)
    {
        if (node->op == Operators::EXPR && node->left != nullptr)
        {
            ParserNode *subExpr = node->left;
            node->op = subExpr->op;
            node->value = subExpr->value;
            node->right = subExpr->right;
            node->left = subExpr->left;
            delete subExpr;
            alterDepth(node, -1, false);
            return optimize(node); // should I optimize again?
        }
        if (
            // self-inverse operators
            (node->op == Operators::NOT || node->op == Operators::NEGATE) &&
            node->left != nullptr && node->left->op == node->op)
        {
            ParserNode *notNode = node->left;
            ParserNode *deeperNotNode = notNode->left;
            node->op = deeperNotNode->op;
            node->value = deeperNotNode->value;
            node->right = deeperNotNode->right;
            node->left = deeperNotNode->left;
            delete deeperNotNode;
            delete notNode;
            alterDepth(node, -2, false);
            return optimize(node); // should I optimize again?
        }
        if (node->left != nullptr)
        {
            optimize(node->left);
        }
        if (node->right != nullptr)
        {
            optimize(node->right);
        }
        return node;
    }
    static void addTabs(std::ostream &out, int d)
    {
        while (d-- > 0)
        {
            out << "  ";
        }
    }

    static void printTree(std::ostream &out, ParserNode *node)
    {
        if (node == nullptr)
        {
            out << "NULL NODE (should not happen)" << std::endl;
            return;
        }
        switch (node->op)
        {
        case Operators::AND:
            printTree(out, node->left);
            addTabs(out, node->depth);
            out << "&& (depth: " << node->depth << ")" << std::endl;
            printTree(out, node->right);
            break;
        case Operators::OR:
            printTree(out, node->left);
            addTabs(out, node->depth);
            out << "|| (depth: " << node->depth << ")" << std::endl;
            printTree(out, node->right);
            break;
        case Operators::NOT:
            addTabs(out, node->depth);
            out << "! (depth: " << node->depth << ")" << std::endl;
            printTree(out, node->left);
            break;
        case Operators::NEGATE:
            addTabs(out, node->depth);
            out << "- (depth: " << node->depth << ")" << std::endl;
            printTree(out, node->left);
            break;
        case Operators::EQ:
            printTree(out, node->left);
            addTabs(out, node->depth);
            out << "== (depth: " << node->depth << ")" << std::endl;
            printTree(out, node->right);
            break;
        case Operators::NEQ:
            printTree(out, node->left);
            addTabs(out, node->depth);
            out << "!= (depth: " << node->depth << ")" << std::endl;
            printTree(out, node->right);
            break;
        case Operators::LT:
            printTree(out, node->left);
            addTabs(out, node->depth);
            out << "< (depth: " << node->depth << ")" << std::endl;
            printTree(out, node->right);
            break;
        case Operators::GT:
            printTree(out, node->left);
            addTabs(out, node->depth);
            out << "> (depth: " << node->depth << ")" << std::endl;
            printTree(out, node->right);
            break;
        case Operators::LTE:
            printTree(out, node->left);
            addTabs(out, node->depth);
            out << "<= (depth: " << node->depth << ")" << std::endl;
            printTree(out, node->right);
            break;
        case Operators::GTE:
            printTree(out, node->left);
            addTabs(out, node->depth);
            out << ">= (depth: " << node->depth << ")" << std::endl;
            printTree(out, node->right);
            break;
        case Operators::EXPR:
            addTabs(out, node->depth);
            out << "( (depth: " << node->depth << ")" << std::endl;
            printTree(out, node->left);
            addTabs(out, node->depth);
            out << ")" << std::endl;
            break;
        case Operators::TERMINAL:
            addTabs(out, node->depth);
            out << "T: (depth: " << node->depth << ") : " << *node->value << std::endl;
            break;
        }
    }

    // helper functions

    static void checkRecursionDepth(int depth, const std::string &expression)
    {
        if (depth > MAX_RECURSION_DEPTH)
        {
            throw std::runtime_error(
                std::format("Recursion depth ({}) exceeded in: {}", depth, expression));
        }
    }

    static void checkEndOfExpression(ParserContext &ctx, ParserNode *currentNode)
    {
        if ((unsigned)ctx.index >= ctx.expression.length())
        {
            if (currentNode != nullptr)
            {
                deleteTree(currentNode); // free the memory
            }
            throw std::runtime_error(
                std::format("END OF EXPRESSION: {} ; at index {}", ctx.expression, ctx.index));
        }
    }

    static void skipWhiteChars(ParserContext &ctx)
    {
        int len = (int)ctx.expression.length();
        while (ctx.index < len && ctx.is_white(ctx.expression[ctx.index]))
        {
            ctx.index++;
        }
    }

    // PARSING

    static ParserNode *parseOr(ParserContext &ctx, int depth);
    static ParserNode *parseOr(ParserNode *left, ParserContext &ctx, int depth);
    static ParserNode *parseAnd(ParserContext &ctx, int depth);
    static ParserNode *parseAnd(ParserNode *left, ParserContext &ctx, int depth);
    static ParserNode *parseEqual(ParserContext &ctx, int depth);
    static ParserNode *parseEqual(ParserNode *left, ParserContext &ctx, int depth);
    static ParserNode *parseNumericComparison(ParserContext &ctx, int depth);                   // then / equal
    static ParserNode *parseNumericComparison(ParserNode *left, ParserContext &ctx, int depth); // then / equal
    static ParserNode *parseNot(ParserContext &ctx, int depth);
    static ParserNode *parseNegate(ParserContext &ctx, int depth);
    static ParserNode *parseExpr(ParserContext &ctx, int depth);
    static ParserNode *parseTerminal(ParserContext &ctx, int depth);

    static ParserNode *parseOr(ParserContext &ctx, int depth)
    {
        return parseOr(nullptr, ctx, depth);
    }
    static ParserNode *parseOr(ParserNode *left, ParserContext &ctx, int depth)
    {
        checkRecursionDepth(depth, ctx.expression);
        if (!left)
        {
            checkEndOfExpression(ctx, nullptr);
            left = parseAnd(ctx, depth);
            skipWhiteChars(ctx);
            if ((unsigned)ctx.index >= (ctx.expression.length() - 1) || ctx.expression[ctx.index] != '|' || ctx.expression[ctx.index + 1] != '|')
            {
                return left; // no OR to be consumed -> expression fully evaluated
            }
        }
        alterDepth(left, 1); // this is not a simple expression, but an OR: so the "left" is deeper than the current one
        ctx.index += 2;      // consume the OR
        skipWhiteChars(ctx);
        checkEndOfExpression(ctx, left);
        ParserNode *orNode = new ParserNode(Operators::OR, ValueType::BOOLEAN, nullptr);
        orNode->depth = depth;
        orNode->left = left;
        try
        {
            orNode->right = parseOr(ctx, depth + 1); // is there another OR in a chain? anway, we can't lower the expression level
        }
        catch (const std::runtime_error &e)
        {
            deleteTree(orNode);
            throw e;
        }
        return orNode;
    }

    static ParserNode *parseAnd(ParserContext &ctx, int depth)
    {
        return parseAnd(nullptr, ctx, depth);
    }
    static ParserNode *parseAnd(ParserNode *left, ParserContext &ctx, int depth)
    {
        checkRecursionDepth(depth, ctx.expression);
        if (!left)
        {
            checkEndOfExpression(ctx, nullptr);
            left = parseEqual(ctx, depth);
            skipWhiteChars(ctx);
            if ((unsigned)ctx.index >= (ctx.expression.length() - 1) || ctx.expression[ctx.index] != '&' || ctx.expression[ctx.index + 1] != '&')
            {
                return left; // no AND to be consumed -> expression fully evaluated
            }
        }
        alterDepth(left, 1); // this is not a simple expression, but an AND: so the "left" is deeper than the current one
        ctx.index += 2;      // consume the AND
        skipWhiteChars(ctx);
        checkEndOfExpression(ctx, left);
        ParserNode *andNode = new ParserNode(Operators::AND, ValueType::BOOLEAN, nullptr);
        andNode->depth = depth;
        andNode->left = left;
        try
        {
            andNode->right = parseAnd(ctx, depth + 1); // is there another AND in a chain? anway, we can't lower the expression level
        }
        catch (const std::runtime_error &e)
        {
            deleteTree(andNode);
            throw e;
        }
        return andNode;
    }

    static ParserNode *parseEqual(ParserContext &ctx, int depth)
    {
        return parseEqual(nullptr, ctx, depth);
    }
    static ParserNode *parseEqual(ParserNode *left, ParserContext &ctx, int depth)
    {
        checkRecursionDepth(depth, ctx.expression);
        if (!left)
        {
            checkEndOfExpression(ctx, nullptr);
            left = parseNumericComparison(ctx, depth);
            skipWhiteChars(ctx);
            if (
                (unsigned)ctx.index >= (ctx.expression.length() - 1) ||
                (ctx.expression[ctx.index] != '=' && ctx.expression[ctx.index] != '!') || ctx.expression[ctx.index + 1] != '=')
            {
                return left; // no EQUAL to be consumed -> expression fully evaluated
            }
        }
        alterDepth(left, 1); // this is not a simple expression, but an AND: so the "left" is deeper than the current one
        bool isEqual = ctx.expression[ctx.index] == '=';
        ctx.index += 2; // consume the EQUAL
        skipWhiteChars(ctx);
        checkEndOfExpression(ctx, left);
        ParserNode *eqNode = new ParserNode(isEqual ? Operators::EQ : Operators::NEQ, ValueType::BOOLEAN, nullptr);
        eqNode->depth = depth;
        eqNode->left = left;
        try
        {
            eqNode->right = parseEqual(ctx, depth + 1); // associativity of equality -> can chain equalities
        }
        catch (const std::runtime_error &e)
        {
            deleteTree(eqNode);
            throw e;
        }
        return eqNode;
    }

    static ParserNode *parseNumericComparison(ParserContext &ctx, int depth)
    {
        return parseNumericComparison(nullptr, ctx, depth);
    }
    static ParserNode *parseNumericComparison(ParserNode *left, ParserContext &ctx, int depth)
    {
        checkRecursionDepth(depth, ctx.expression);
        checkEndOfExpression(ctx, left);
        int len = ctx.expression.length();
        if (!left)
        {
            left = parseNot(ctx, depth);
            skipWhiteChars(ctx);
            if (
                ctx.index >= (len - 1) ||
                (ctx.expression[ctx.index] != '<' && ctx.expression[ctx.index] != '>'))
            {
                return left; // no comparison to be consumed -> expression fully evaluated
            }
        }
        alterDepth(left, 1); // this is not a simple expression, but an AND: so the "left" is deeper than the current one
        bool isGT = ctx.expression[ctx.index] == '>';
        bool isEqual = false; // hypotesis
        ctx.index++;          // consume the GT/LT
        checkEndOfExpression(ctx, left);
        if (ctx.expression[ctx.index] == '=')
        {
            isEqual = true;
            ctx.index++; // consume the EQUAL
        }
        skipWhiteChars(ctx);
        checkEndOfExpression(ctx, left);
        ParserNode *comparisonNode = new ParserNode(     //
            isEqual ?                                    //
                (isGT ? Operators::GTE : Operators::LTE) //
                    :                                    //
                (isGT ? Operators::GT : Operators::LT),  //
            ValueType::BOOLEAN,                          //
            nullptr                                      //
        );
        comparisonNode->depth = depth;
        comparisonNode->left = left;
        try
        {
            comparisonNode->right = parseNot(ctx, depth); // disequalities are NOT associative in general -> the right term MUST be a simple expression (or sub-expression)
        }
        catch (const std::runtime_error &e)
        {
            deleteTree(comparisonNode);
            throw e;
        }
        return comparisonNode;
    }

    static ParserNode *parseNot(ParserContext &ctx, int depth)
    {
        checkRecursionDepth(depth, ctx.expression);
        checkEndOfExpression(ctx, nullptr);
        if (ctx.expression[ctx.index] == '!')
        {
            ctx.index++; // consume the NOT
            skipWhiteChars(ctx);
            checkEndOfExpression(ctx, nullptr);
            ParserNode *notNode = new ParserNode(Operators::NOT, ValueType::BOOLEAN, nullptr);
            notNode->depth = depth;
            try
            {
                notNode->left = parseNot(ctx, depth + 1); // NOT is right-associative -> can chain
            }
            catch (const std::runtime_error &e)
            {
                deleteTree(notNode);
                throw e;
            }
            return notNode;
        }
        return parseNegate(ctx, depth);
    }

    static ParserNode *parseNegate(ParserContext &ctx, int depth)
    {
        checkRecursionDepth(depth, ctx.expression);
        checkEndOfExpression(ctx, nullptr);
        if (ctx.expression[ctx.index] == '-')
        {
            ctx.index++; // consume the NEGATE
            skipWhiteChars(ctx);
            checkEndOfExpression(ctx, nullptr);
            ParserNode *negateNode = new ParserNode(Operators::NEGATE, ValueType::NUMBER, nullptr);
            negateNode->depth = depth;
            try
            {
                negateNode->left = parseNegate(ctx, depth + 1); // NEGATE is right-associative -> can chain
            }
            catch (const std::runtime_error &e)
            {
                deleteTree(negateNode);
                throw e;
            }
            return negateNode;
        }
        return parseExpr(ctx, depth);
    }

    static ParserNode *parseExpr(ParserContext &ctx, int depth)
    {
        checkRecursionDepth(depth, ctx.expression);
        checkEndOfExpression(ctx, nullptr);
        if (ctx.expression[ctx.index] == '(')
        {
            ctx.index++; // consume the LEFT PARENTHESIS
            skipWhiteChars(ctx);
            checkEndOfExpression(ctx, nullptr);
            ParserNode *exprNode = new ParserNode(Operators::EXPR, ValueType::UNKNOWN, nullptr);
            exprNode->depth = depth;
            try
            {
                exprNode->left = parseOr(ctx, depth + 1); // restart the tree evaluation since it's a SUB-expresison
            }
            catch (const std::runtime_error &e)
            {
                deleteTree(exprNode);
                throw e;
            }
            checkEndOfExpression(ctx, exprNode);
            if (ctx.expression[ctx.index] != ')')
            {
                deleteTree(exprNode); // free the memory
                throw std::runtime_error(
                    std::format("MISSING RIGHT PARENTHESIS: at index {}", ctx.index));
            }
            ctx.index++; // consume the RIGHT PARENTHESIS
            skipWhiteChars(ctx);
            return exprNode;
        }
        return parseTerminal(ctx, depth);
    }

    static std::string *extractTerminalValue(ParserContext &ctx)
    {
        int startIndex = ctx.index;
        int len = ctx.expression.length();
        while (ctx.index < len && !ctx.is_unallowed(ctx.expression[ctx.index]))
        {
            ctx.index++;
        }
        if (startIndex >= ctx.index)
        {
            throw std::runtime_error(
                std::format("EXPECTED A TERMINAL VALUE AT INDEX {} IN: {}", ctx.index, ctx.expression));
        }
        return new std::string(ctx.expression.substr(startIndex, ctx.index - startIndex));
    }

    static ParserNode *parseTerminal(ParserContext &ctx, int depth)
    {
        skipWhiteChars(ctx);
        checkEndOfExpression(ctx, nullptr);
        if (ctx.expression[ctx.index] == '(')
        { // this should never happen, since the LEFT PARENTHESIS should be consumed by parseExpr, but just in case...
            return parseExpr(ctx, depth);
        }
        auto terminalValue = extractTerminalValue(ctx);
        ParserNode *terminalNode = new ParserNode(Operators::TERMINAL, ValueType::UNKNOWN, terminalValue);
        terminalNode->depth = depth;
        return terminalNode;
    }

    static ParserNode *parse(ParserContext &ctx)
    {
        return parseOr(ctx, 0);
    }

    // EVALUATION

    /*
    ParserNode* parseOr(ParserContext &ctx, int depth);
    ParserNode* parseOr(ParserNode* left, ParserContext &ctx, int depth);
    ParserNode* parseAnd(ParserContext &ctx, int depth);
    ParserNode* parseAnd(ParserNode* left, ParserContext &ctx, int depth);
    ParserNode* parseEqual(ParserContext &ctx, int depth);
    ParserNode* parseEqual(ParserNode* left, ParserContext &ctx, int depth);
    ParserNode* parseNotEqual(ParserContext &ctx, int depth);
    ParserNode* parseNotEqual(ParserNode* left, ParserContext &ctx, int depth);
    ParserNode* parseLower(ParserContext &ctx, int depth); // then / equal
    ParserNode* parseLower(ParserNode* left, ParserContext &ctx, int depth);  // then / equal
    ParserNode* parseGreater(ParserContext &ctx, int depth);
    ParserNode* parseGreater(ParserNode* left, ParserContext &ctx, int depth);
    ParserNode* parseNot(ParserContext &ctx, int depth);
    ParserNode* parseNegate(ParserContext &ctx, int depth);
    ParserNode* parseExpr(ParserContext &ctx, int depth);
    ParserNode* parseTerminal(ParserContext &ctx, int depth);


    NOTES:

    ParserContext *pctx;
    ParserNode *currentNode;
    // if equal / not-equal comparison .......
    bool subexpressionsTypesMismatch = (currentNode->left->valueType != currentNode->right->valueType);
        if(isEqual ){
            if (subexpressionsTypesMismatch){
                if(ALLOW_ETHEROGENEOUS_COMPARISONS){
                pctx->resultType = ValueType::BOOLEAN;
                pctx->result = false;
                }else {
                        throw std::runtime_error(
                            std::format("Unequal type comparison depth ({}) exceeded in: {}", depth, expression));
                        }
                }
        }else {
            if(subexpressionsTypesMismatch){
                if(ALLOW_ETHEROGENEOUS_COMPARISONS){
                    // just a TRUE is enough to determine the result of a NOT EQUAL, no need to evaluate the whole expression
                    pctx->resultType = ValueType::BOOLEAN;
                    pctx->result = true;
                }else {
                        throw std::runtime_error(
                            std::format("Unequal type comparison depth ({}) exceeded in: {}", depth, expression));
                        }
                }
            }
        }

    */

    // start

    bool evaluate(std::string &expression, std::map<std::string, std::string> &valuesByName)
    {
        ParserContext ctx(expression);
        ParserNode *root = parse(ctx);

        printf("PARSED TREE:\n");
        printTree(std::cout, root);
        valuesByName.contains(expression); // just to avoid "unused parameter" warning, since the evaluation is not implemented yet

        // EvaluationContext evalCtx;
        // evaluate ....
        // deleteNode(root);
        // TODO: implement me :)
        return false;
    }

    // g++ -std=c++26 -Wall -Wextra -o expr_eval  expr_eval.cpp
    //     ./expr_eval
    int main(int argc, char **argv)
    {
        std::cout << "Testing expression evaluator... (argc: " << argc << ", argv: " << argv[0] << ")" << std::endl;
        std::map<std::string, std::string> m;
        std::string e1 = "v0 == 1";
        std::string e2 = "(v0 == 2 || v1 > 10)";
        std::string e3 = "(v0 == 2 || (v1 > 10 && v2 > 3)) && v3 == 0";
        std::string e4 = "(v0 == 2 || (v1 > 10 && v2 > 3)) && v3 == -15.000000001 && !v4";
        std::string e5 = "(v0 == 2 || (v1 > 10 && v2 > 3)) && v3 == -15.000000001 && v4";
        std::string e6 = "((v0 == 2 || (v1 > 10 && v2 > 3)) && v3 == -15.000000001 && v4) && (v5 == !v4)";
        std::string e7 = "true";
        m["v0"] = "1";
        m["v1"] = "15.55";
        m["v2"] = "-10";
        m["v3"] = "-15.000000001";
        m["v4"] = "true";
        m["v5"] = "false";
        bool testCorrect = (evaluate(e1, m) &&
                            evaluate(e2, m) &&
                            !evaluate(e3, m) &&
                            !evaluate(e4, m) &&
                            evaluate(e5, m) &&
                            evaluate(e6, m) &&
                            evaluate(e7, m));
        std::cout << (testCorrect ? "Good job!" : "Uhm, please retry!") << std::endl;
        return 0;
    }
}