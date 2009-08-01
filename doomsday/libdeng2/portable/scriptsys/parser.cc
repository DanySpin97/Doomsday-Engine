/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/Parser"
#include "de/ScriptLex"
#include "de/TokenBuffer"
#include "de/TokenRange"
#include "de/Script"
#include "de/ExpressionStatement"
#include "de/PrintStatement"
#include "de/IfStatement"
#include "de/WhileStatement"
#include "de/ForStatement"
#include "de/JumpStatement"
#include "de/AssignStatement"
#include "de/FunctionStatement"
#include "de/ArrayExpression"
#include "de/DictionaryExpression"
#include "de/ConstantExpression"
#include "de/OperatorExpression"
#include "de/NameExpression"
#include "de/BuiltInExpression"
#include "de/TextValue"
#include "de/NumberValue"
#include "de/Operator"

#include <sstream>

using std::auto_ptr;
using namespace de;

Parser::Parser()
{}

Parser::~Parser()
{}

void Parser::parse(const String& input, Script& output)
{
    // Lexical analyzer for Haw scripts.
    analyzer_ = ScriptLex(input);
    
    // Get the tokens of the first statement.
    if(nextStatement() > 0)
    {
        // Parse the bottom-level compound.
        parseCompound(output.compound());
    }

    // We're done, free the remaining tokens.
    tokens_.clear();
}

duint Parser::nextStatement()
{
    duint result = analyzer_.getStatement(tokens_);
    
    // Begin with the whole thing.
    statementRange_ = TokenRange(tokens_);

    std::cout << "Next statement: '" << statementRange_.asText() << "'";
    
    return result;
}

void Parser::parseCompound(Compound& compound)
{
    while(statementRange_.size() > 0) 
    {
        if(statementRange_.firstToken().equals("elif") || 
            (statementRange_.size() == 1 && 
                (statementRange_.firstToken().equals("end") ||
                 statementRange_.firstToken().equals("else"))))
        {
            // End of compound.
            break;
        }
        
        // We have a list of tokens, which form a statement.
        // Figure out what it is and generate the correct statement(s)
        // and expressions.
        parseStatement(compound);
    } 
}

void Parser::parseStatement(Compound& compound)
{
    const TokenBuffer::Token& firstToken = statementRange_.firstToken();

    // Built-in statements: print, if/else, for, while, def.
    if(firstToken.equals("if"))
    {
        compound.add(parseIfStatement());
    }
    else if(firstToken.equals("while"))
    {
        compound.add(parseWhileStatement());
    }
    else if(firstToken.equals("for"))
    {
        compound.add(parseForStatement());
    }
    else if(firstToken.equals("continue"))
    {
        compound.add(new JumpStatement(JumpStatement::CONTINUE));
    }
    else if(firstToken.equals("break"))
    {
        // Break may have an expression argument that tells us how many
        // nested compounds to break out of.
        Expression* breakCount = 0;
        if(statementRange_.size() > 1)
        {
            breakCount = parseExpression(statementRange_.startingFrom(1));
        }
        compound.add(new JumpStatement(JumpStatement::BREAK, breakCount));
    }
    else if(firstToken.equals("return"))
    {
        Expression* returnValue = 0;
        if(statementRange_.size() > 1)
        {
            returnValue = parseExpression(statementRange_.startingFrom(1));
        }
        compound.add(new JumpStatement(JumpStatement::RETURN, returnValue));
    }
    else if(firstToken.equals("print"))
    {
        compound.add(parsePrintStatement());
    }
    else if(firstToken.equals("def"))
    {
        compound.add(parseFunctionStatement());
    }
    else if(statementRange_.hasBracketless("=") || statementRange_.hasBracketless(":="))
    {
        compound.add(parseAssignStatement());
    }
    else 
    {
        compound.add(parseExpressionStatement());
    }

    // We've fully parsed the current set of tokens, get the next statement.
    nextStatement();
}

IfStatement* Parser::parseIfStatement()
{
    auto_ptr<IfStatement> statement(new IfStatement());

    statement->newBranch();
    statement->setBranchCondition(
        parseConditionalCompound(statement->branchCompound(), HAS_CONDITION));
    
    while(statementRange_.beginsWith("elif"))
    {
        statement->newBranch();
        statement->setBranchCondition(
            parseConditionalCompound(statement->branchCompound(), HAS_CONDITION));
    }
    
    if(statementRange_.beginsWith("else"))
    {
        parseConditionalCompound(statement->elseCompound());
    }
    
    if(statementRange_.size() != 1 || !statementRange_.firstToken().equals("end"))
    {
        throw UnexpectedTokenError("Parser::parseIfStatement", "Expected 'end', but got " + 
            statementRange_.firstToken().asText());
    }

    return statement.release(); 
}

WhileStatement* Parser::parseWhileStatement()
{
    // "while" expr ":" statement
    // "while" expr "\n" compound
    
    auto_ptr<WhileStatement> statement(new WhileStatement());
    statement->setCondition(parseConditionalCompound(statement->compound(), HAS_CONDITION));
    return statement.release();
}

ForStatement* Parser::parseForStatement()
{
    // "for" by-ref-name-expr "in" expr ":" statement
    // "for" by-ref-name-expr "in" expr "\n" compound
    
    dint colonPos = statementRange_.find(":");
    dint inPos = statementRange_.find("in");
    if(inPos < 0 || (colonPos > 0 && inPos > colonPos))
    {
        throw MissingTokenError("Parser::parseForStatement",
            "Expected 'in' to follow " + statementRange_.firstToken().asText());
    }
    
    auto_ptr<NameExpression> iter(parseNameExpression(statementRange_.between(1, inPos),
        NAME_BY_REFERENCE | ALLOW_NEW_VARIABLES | LOOKUP_LOCAL_ONLY));
    Expression* iterable = parseExpression(statementRange_.between(inPos + 1, colonPos));
    
    auto_ptr<ForStatement> statement(new ForStatement(iter.release(), iterable));

    // Parse the statements of the method.
    parseConditionalCompound(statement->compound(), IGNORE_EXTRA_BEFORE_COLON);
    
    return statement.release();
}

PrintStatement* Parser::parsePrintStatement()
{    
    // Parse the arguments of the print statement, as a standard argument list.
    ArrayExpression* args = parseArray(statementRange_.startingFrom(1));
    return new PrintStatement(args);    
}

FunctionStatement* Parser::parseFunctionStatement()
{
    dint pos = statementRange_.find("(");
    if(pos < 0)
    {
        throw MissingTokenError("Parser::parseMethodStatement",
            "Expected arguments for " + statementRange_.firstToken().asText());
    }

    // The function must have a name that is not already in use in the scope.
    auto_ptr<FunctionStatement> statement(new FunctionStatement(
        parseNameExpression(statementRange_.between(1, pos), 
        LOOKUP_LOCAL_ONLY | NAME_BY_REFERENCE | REQUIRE_NEW_VARIABLE)));

    // Collect the argument names.
    TokenRange argRange = statementRange_.between(pos + 1, statementRange_.closingBracket(pos));
    if(!argRange.empty())
    {
        // The arguments are comma-separated.
        TokenRange delim = argRange.undefinedRange();
        while(argRange.getNextDelimited(",", delim))
        {
            if(delim.size() == 1 && delim.firstToken().type() == TokenBuffer::Token::IDENTIFIER)
            {
                // Just the name of the argument.
                statement->addArgument(delim.firstToken().str());
            }
            else if(delim.size() >= 3 && 
                delim.token(0).type() == TokenBuffer::Token::IDENTIFIER &&
                delim.token(1).equals("="))
            {
                // Argument with a default value.
                statement->addArgument(delim.firstToken().str(),
                    parseExpression(delim.startingFrom(2)));
            }
            else
            {
                throw UnexpectedTokenError("Parser::parseFunctionStatement",
                    "'" + delim.asText() + "' was unexpected in argument definition at " +
                    argRange.firstToken().asText());
            }
        }
    }

    // Parse the statements of the function.
    parseConditionalCompound(statement->compound(), IGNORE_EXTRA_BEFORE_COLON);
    
    return statement.release();
}

AssignStatement* Parser::parseAssignStatement()
{
    ExpressionFlags flags = ALLOW_NEW_VARIABLES | NAME_BY_REFERENCE | LOOKUP_LOCAL_ONLY;
    dint pos = statementRange_.find("=");
    if(pos < 0)
    {
        pos = statementRange_.find(":=");
        flags &= ~LOOKUP_LOCAL_ONLY;
    }
    
    // Has indices been specified?
    AssignStatement::Indices indices;
    dint nameEndPos = pos;
    dint bracketPos = pos - 1;
    try
    {
        while(statementRange_.token(bracketPos).equals("]"))
        {
            dint startPos = statementRange_.openingBracket(bracketPos);
            nameEndPos = startPos;
            Expression* indexExpr = parseExpression(
                statementRange_.between(startPos + 1, bracketPos));
            indices.push_back(indexExpr);
            bracketPos = nameEndPos - 1;
        }

        auto_ptr<NameExpression> lValue(parseNameExpression(statementRange_.endingTo(nameEndPos), flags));
        auto_ptr<Expression> rValue(parseExpression(statementRange_.startingFrom(pos + 1)));

        AssignStatement* st = new AssignStatement(lValue.get(), indices, rValue.get());

        lValue.release();
        rValue.release();
        
        return st;
    }
    catch(const Error& err)
    {
        // Cleanup.
        for(AssignStatement::Indices::iterator i = indices.begin(); i != indices.end(); ++i)
        {
            delete *i;
        }
        err.raise();
        return 0; // not reached
    }
}

ExpressionStatement* Parser::parseExpressionStatement()
{
    return new ExpressionStatement(parseExpression(statementRange_));
}

Expression* Parser::parseConditionalCompound(Compound& compound, const CompoundFlags& flags)
{
    // keyword [expr] ":" statement
    // keyword [expr] "\n" compound
    
    TokenRange range = statementRange_;
    
    // See if there is a colon on this line.
    dint colon = range.find(":");
    
    auto_ptr<Expression> condition;
    if(flags[HAS_CONDITION_BIT])
    {
        TokenRange conditionRange = range.between(1, colon);
        if(conditionRange.empty())
        {
            throw MissingTokenError("Parser::parseConditionalCompound",
                "A condition expression was expected after " + range.token(0).asText());
        }
        condition = auto_ptr<Expression>(parseExpression(conditionRange));
    }
    else if(colon > 0 && (colon > 1 && !flags[IGNORE_EXTRA_BEFORE_COLON_BIT]))
    {
        throw UnexpectedTokenError("Parser::parseConditionalCompound",
            range.token(1).asText() + " was unexpected");
    }
    
    if(colon > 0 && colon < dint(range.size()) - 1)
    {
        // The colon is not the last token. There must be a statement 
        // continuing on the same line.
        statementRange_ = statementRange_.startingFrom(colon + 1);
        parseStatement(compound);
    }
    else
    {
        parseCompound(compound);
    }
    
    return condition.release();
}

ArrayExpression* Parser::parseArray(const TokenRange& range, const char* separator)
{
    auto_ptr<ArrayExpression> exp(new ArrayExpression);
    if(range.size() > 0)
    {
        // The arguments are comma-separated.
        TokenRange delim = range.undefinedRange();
        while(range.getNextDelimited(separator, delim))
        {
            exp->add(parseExpression(delim));
        }
    }
    return exp.release();
}

Expression* Parser::parseExpression(const TokenRange& fullRange, const ExpressionFlags& flags)
{
    TokenRange range = fullRange;
    
    if(!range.size())
    {
        // Empty expression yields a None value.
        return ConstantExpression::None();
    }
    
    // We can ignore extra parenthesis around the range.
    while(range.firstToken().equals("(") && range.closingBracket(0) == range.size() - 1)
    {
        range = range.shrink(1);
    }

    TokenRange leftSide = range.between(0, 0);
    TokenRange rightSide = leftSide;
    
    // Locate the lowest-ranking operator.
    Operator op = findLowestOperator(range, leftSide, rightSide);
    
    if(op == NONE)
    {
        // This is a constant or a variable reference.
        return parseTokenExpression(range, flags);
    }
    else if(op == ARRAY)
    {
        return parseArrayExpression(range);
    }
    else if(op == DICTIONARY)
    {
        return parseDictionaryExpression(range);
    }
    else if(op == CALL)
    {
        return parseCallExpression(leftSide, rightSide);
    }
    else
    {
        std::cout << "parsing operator expression: " << operatorToText(op) << "\n";
        
        // Left side is empty with unary operators.
        // The right side inherits the flags of the expression (e.g., name-by-reference).
        return parseOperatorExpression(op, leftSide, rightSide, flags);
    }
}

ArrayExpression* Parser::parseArrayExpression(const TokenRange& range)
{
    if(!range.firstToken().equals("[") ||
        range.closingBracket(0) != range.size() - 1)
    {
        throw MissingTokenError("Parser::parseArrayExpression",
            "Expected brackets for the array expression beginning at " + 
            range.firstToken().asText());
    }    
    return parseArray(range.between(1, range.size() - 1));
}

DictionaryExpression* Parser::parseDictionaryExpression(const TokenRange& range)
{
    if(!range.firstToken().equals("{") ||
        range.closingBracket(0) != range.size() - 1)
    {
        throw MissingTokenError("Parser::parseDictionaryExpression",
            "Expected brackets for the dictionary expression beginning at " + 
            range.firstToken().asText());
    }
    TokenRange shrunk = range.shrink(1);
    
    auto_ptr<DictionaryExpression> exp(new DictionaryExpression);
    if(shrunk.size() > 0)
    {
        // The arguments are comma-separated.
        TokenRange delim = shrunk.undefinedRange();
        while(shrunk.getNextDelimited(",", delim))
        {
            dint colonPos = delim.findBracketless(":");
            if(colonPos < 0)
            {
                throw MissingTokenError("Parser::parseDictionaryExpression",
                    "Colon is missing from '" + delim.asText() + "' at " +
                    delim.firstToken().asText());
            }
            
            auto_ptr<Expression> key(parseExpression(delim.endingTo(colonPos)));
            Expression* value = parseExpression(delim.startingFrom(colonPos + 1));
            exp->add(key.release(), value);
        }
    }
    return exp.release();
}

Expression* Parser::parseCallExpression(const TokenRange& nameRange, const TokenRange& argumentRange)
{
    std::cerr << "call name: " << nameRange.asText() << "\n";
    std::cerr << "call args: " << argumentRange.asText() << "\n";
    
    if(!argumentRange.firstToken().equals("(") || 
         argumentRange.closingBracket(0) < argumentRange.size() - 1)
    {
        throw SyntaxError("Parser::parseCallExpression",
            "Call arguments must be enclosed in parenthesis for " + 
            argumentRange.firstToken().asText());
    }
    
    // Parse the arguments, with possible labels included.
    // The named arguments are evaluated by a dictionary which is always
    // included as the first expression in the array.
    auto_ptr<ArrayExpression> args(new ArrayExpression);
    DictionaryExpression* namedArgs = new DictionaryExpression;
    args->add(namedArgs);
    
    TokenRange argsRange = argumentRange.shrink(1);
    if(!argsRange.empty())
    {
        // The arguments are comma-separated.
        TokenRange delim = argsRange.undefinedRange();
        while(argsRange.getNextDelimited(",", delim))
        {
            if(delim.has("="))
            {
                // A label is included.
                if(delim.size() < 3 || 
                    delim.firstToken().type() != TokenBuffer::Token::IDENTIFIER ||
                    !delim.token(1).equals("="))
                {
                    throw UnexpectedTokenError("Parser::parseCallExpression",
                        "Labeled argument '" + delim.asText() + "' is malformed");
                }
                // Create a dictionary entry for this.
                Expression* value = parseExpression(delim.startingFrom(2));
                namedArgs->add(new ConstantExpression(new TextValue(delim.firstToken().str())), value);
            }
            else
            {
                // Unlabeled argument.
                args->add(parseExpression(delim));
            }
        }
    }
    
    // Check for some built-in methods, which are usable everywhere.
    if(nameRange.size() == 1)
    {
        if(nameRange.firstToken().equals("len"))
        {
            return new BuiltInExpression(BuiltInExpression::LENGTH, args.release());
        }
        if(nameRange.firstToken().equals("dictkeys"))
        {
            return new BuiltInExpression(BuiltInExpression::DICTIONARY_KEYS, args.release());
        }
        if(nameRange.firstToken().equals("dictvalues"))
        {
            return new BuiltInExpression(BuiltInExpression::DICTIONARY_VALUES, args.release());
        }
    }
    auto_ptr<NameExpression> identifier(parseNameExpression(nameRange, NAME_BY_REFERENCE));
    return new OperatorExpression(CALL, identifier.release(), args.release());
}

OperatorExpression* Parser::parseOperatorExpression(Operator op, const TokenRange& leftSide, 
    const TokenRange& rightSide, const ExpressionFlags& rightFlags)
{
    std::cerr << "left: " << leftSide.asText() << ", right: " << rightSide.asText() << "\n";
    
    if(leftSide.empty())
    {
        // Must be unary.
        auto_ptr<Expression> operand(parseExpression(rightSide));
        OperatorExpression* x = new OperatorExpression(op, operand.get());
        operand.release();
        return x;
    }
    else
    {
        ExpressionFlags leftFlags = NAME_BY_VALUE;
        if(leftOperandByReference(op))
        {
            leftFlags = NAME_BY_REFERENCE;
        }
            
        // Binary operation.
        auto_ptr<Expression> leftOperand(parseExpression(leftSide, leftFlags));
        auto_ptr<Expression> rightOperand(op == SLICE? parseArray(rightSide, ":") :
            parseExpression(rightSide, rightFlags));
        OperatorExpression* x = new OperatorExpression(op, leftOperand.get(), rightOperand.get());
        rightOperand.release();
        leftOperand.release();
        return x;
    }
}

NameExpression* Parser::parseNameExpression(const TokenRange& range, const ExpressionFlags& flags)
{
    NameExpression::Flags nameFlags;
    
    if(flags[NAME_BY_VALUE_BIT]) nameFlags |= NameExpression::BY_VALUE;
    if(flags[NAME_BY_REFERENCE_BIT]) nameFlags |= NameExpression::BY_REFERENCE;
    if(flags[LOOKUP_LOCAL_ONLY_BIT]) nameFlags |= NameExpression::LOCAL_ONLY;
    if(flags[ALLOW_NEW_VARIABLES_BIT]) nameFlags |= NameExpression::NEW_VARIABLE;
    if(flags[REQUIRE_NEW_VARIABLE_BIT]) nameFlags |= NameExpression::NOT_IN_SCOPE;
    
    return new NameExpression(parseExpression(range), nameFlags);
}

Expression* Parser::parseTokenExpression(const TokenRange& range, const ExpressionFlags& flags)
{
    if(!range.size())
    {
        throw MissingTokenError("Parser::parseTokenExpression",
            "Expected tokens, but got nothing -- near " +
            range.buffer().at(range.tokenIndex(0)).asText());
    }

    const TokenBuffer::Token& token = range.token(0);

    if(token.type() == TokenBuffer::Token::KEYWORD)
    {
        if(token.equals("True"))
        {
            return ConstantExpression::True();
        }
        else if(token.equals("False"))
        {
            return ConstantExpression::False();
        }
        else if(token.equals("None"))
        {
            return ConstantExpression::None();
        }
    }

    switch(token.type())
    {
    case TokenBuffer::Token::IDENTIFIER:
        if(range.size() == 1)
        {
            return new NameExpression(new ConstantExpression(new TextValue(range.token(0).str())), flags);
        }
        else
        {
            throw UnexpectedTokenError("Parser::parseTokenExpression", 
                "Unexpected token " + range.token(1).asText());
        }
        
    case TokenBuffer::Token::LITERAL_STRING_APOSTROPHE:
    case TokenBuffer::Token::LITERAL_STRING_QUOTED:
    case TokenBuffer::Token::LITERAL_STRING_LONG:
        return new ConstantExpression(
            new TextValue(ScriptLex::unescapeStringToken(token)));

    case TokenBuffer::Token::LITERAL_NUMBER:
        return new ConstantExpression(
            new NumberValue(ScriptLex::tokenToNumber(token)));
        
    default:
        throw UnexpectedTokenError("Parser::parseTokenExpression",
            token.asText() + " which was identified as " +
            TokenBuffer::Token::typeToText(token.type()) + " was unexpected");
    }
}

Operator Parser::findLowestOperator(const TokenRange& range, TokenRange& leftSide, TokenRange& rightSide)
{
    enum {
        MAX_RANK            = 0x7fffffff,
        RANK_MEMBER         = 13,
        RANK_CALL           = 14,
        RANK_INDEX          = 14,
        RANK_SLICE          = 14,
        RANK_DOT            = 15,
        RANK_ARRAY          = MAX_RANK - 1,
        RANK_DICTIONARY     = RANK_ARRAY,
        RANK_PARENTHESIS    = MAX_RANK - 1
    };
    enum Direction {
        LEFT_TO_RIGHT,
        RIGHT_TO_LEFT
    };

    Operator previousOp = NONE;
    Operator lowestOp = NONE;
    int lowestRank = MAX_RANK;

    for(duint i = 0, continueFrom = 0; i < range.size(); i = continueFrom)
    {
        continueFrom = i + 1;
        
        int rank = MAX_RANK;
        Operator op = NONE;
        Direction direction = LEFT_TO_RIGHT;
        
        const TokenBuffer::Token& token = range.token(i);
        
        if(token.equals("("))
        {
            continueFrom = range.closingBracket(i) + 1;
            if(previousOp == NONE && i > 0)
            {
                // The previous token was not an operator, but there 
                // was something before this one. It must be a function
                // call.
                op = CALL;
                rank = RANK_CALL;
            }
            else
            {
                // It's parenthesis. Skip past it.
                op = PARENTHESIS;
                rank = RANK_PARENTHESIS;
            }
        }
        else if(token.equals("["))
        {
            continueFrom = range.closingBracket(i) + 1;
            if((previousOp == NONE || previousOp == PARENTHESIS ||
                previousOp == INDEX || previousOp == SLICE) && i > 0)
            {
                if(range.between(i + 1, continueFrom - 1).has(":"))
                {
                    op = SLICE;
                    rank = RANK_SLICE;
                }
                else
                {
                    op = INDEX;
                    rank = RANK_INDEX;
                }
            }
            else
            {
                op = ARRAY;
                rank = RANK_ARRAY;
            }
        }
        else if(token.equals("{"))
        {
            continueFrom = range.closingBracket(i) + 1;
            op = DICTIONARY;
            rank = RANK_DICTIONARY;
        }
        else
        {
            const struct {
                const char* token;
                Operator op;
                int rank;
                Direction direction;
            } rankings[] = { // Ascending order.
                { "+=",     PLUS_ASSIGN,        0,          RIGHT_TO_LEFT },
                { "-=",     MINUS_ASSIGN,       0,          RIGHT_TO_LEFT },
                { "*=",     MULTIPLY_ASSIGN,    0,          RIGHT_TO_LEFT },
                { "/=",     DIVIDE_ASSIGN,      0,          RIGHT_TO_LEFT },
                { "%=",     MODULO_ASSIGN,      0,          RIGHT_TO_LEFT },
                { "not",    NOT,                3,          RIGHT_TO_LEFT },
                { "in",     IN,                 4,          LEFT_TO_RIGHT },
                { "==",     EQUAL,              5,          LEFT_TO_RIGHT },
                { "!=",     NOT_EQUAL,          5,          LEFT_TO_RIGHT },
                { "<",      LESS,               6,          LEFT_TO_RIGHT },
                { ">",      GREATER,            6,          LEFT_TO_RIGHT },
                { "<=",     LEQUAL,             6,          LEFT_TO_RIGHT },
                { ">=",     GEQUAL,             6,          LEFT_TO_RIGHT },
                { "+",      PLUS,               9,          LEFT_TO_RIGHT },
                { "-",      MINUS,              9,          LEFT_TO_RIGHT },
                { "*",      MULTIPLY,           10,         LEFT_TO_RIGHT },
                { "/",      DIVIDE,             10,         LEFT_TO_RIGHT },
                { "%",      MODULO,             10,         LEFT_TO_RIGHT },
                { ".",      DOT,                RANK_DOT,   LEFT_TO_RIGHT },
                { 0,        NONE,               MAX_RANK,   LEFT_TO_RIGHT }
            };            
            
            // Operator precedence:
            // .
            // function call
            // member
            // []
            // ()
            // * / %
            // + -
            // & | ^
            // << >>
            // < > <= >= 
            // == !=
            // in
            // not
            // and
            // or
         
            // Check the rankings table.
            for(int k = 0; rankings[k].token; ++k)
            {
                if(token.equals(rankings[k].token))
                {
                    op = rankings[k].op;
                    rank = rankings[k].rank;
                    direction = rankings[k].direction;
                    
                    if(op == DOT) // && previousOp == NONE)
                    {
                        op = MEMBER;
                        rank = RANK_MEMBER;
                        direction = LEFT_TO_RIGHT;
                    }
                    else if(!i || (previousOp != NONE && previousOp != PARENTHESIS &&
                        previousOp != CALL && previousOp != INDEX && previousOp != SLICE &&
                        previousOp != ARRAY && previousOp != DICTIONARY))
                    {
                        // There already was an operator before this one.
                        // Must be unary?
                        if(op == PLUS || op == MINUS)
                        {
                            rank += 100;
                            //std::cerr << "Rank boost for " << rankings[k].token << "\n";
                        }
                    }
                    break;
                }
            }
        }
        
        if(op != NONE && 
            ((direction == LEFT_TO_RIGHT && rank <= lowestRank) ||
             (direction == RIGHT_TO_LEFT && rank < lowestRank)))
        {
            lowestOp = op;
            lowestRank = rank;
            leftSide = range.endingTo(i);
            if(op == INDEX || op == SLICE)
            {
                rightSide = range.between(i + 1, continueFrom - 1);
            }
            else
            {
                rightSide = range.startingFrom(op == CALL || op == ARRAY || 
                    op == DICTIONARY? i : i + 1);
            }
        }
        
        previousOp = op;       
    }
    
    return lowestOp;
}
