/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/Process"
#include "de/Variable"
#include "de/ArrayValue"
#include "de/TextValue"
#include "de/NoneValue"
#include "de/TryStatement"
#include "de/CatchStatement"

#include <sstream>

using namespace de;
using std::auto_ptr;
    
/// If execution continues for longer than this, a HangError is thrown.
static TimeDelta const MAX_EXECUTION_TIME = 10;

Process::Process(Record *externalGlobalNamespace) : _state(Stopped), _workingPath("/")
{
    // Push the first context on the stack. This bottommost context
    // is never popped from the stack. Its namespace is the global namespace
    // of the process.
    _stack.push_back(new Context(Context::BaseProcess, this, externalGlobalNamespace));
}

Process::Process(Script const &script) : _state(Stopped), _workingPath("/")
{
    clear();

    // If a script is provided, start running it automatically.
    run(script);
}

Process::~Process()
{
    clearStack();
}

void Process::clear()
{
    _state = Stopped;
    clearStack();
    _stack.push_back(new Context(Context::BaseProcess, this));
    _workingPath = "/";
}

void Process::clearStack(duint downToLevel)
{
    while(depth() > downToLevel)
    {
        delete _stack.back();
        _stack.pop_back();
    }
}

dsize Process::depth() const
{
    return _stack.size();
}

void Process::run(Script const &script)
{
    run(script.firstStatement());
    
    // Set up the automatic variables.
    Record &ns = globals();
    if(ns.has("__file__"))
    {
        ns["__file__"].set(TextValue(script.path()));
    }
    else
    {
        ns.add(new Variable("__file__", new TextValue(script.path()), Variable::AllowText));
    }
}

void Process::run(Statement const *firstStatement)
{
    if(_state != Stopped)
    {
        throw NotStoppedError("Process::run", "Process must be stopped first");
    }
    _state = Running;

    // Make sure the stack is clear except for the process context.
    clearStack(1);

    context().start(firstStatement);
}

void Process::suspend(bool suspended)
{
    if(_state == Stopped)
    {
        throw SuspendError("Process:suspend", 
            "Stopped processes cannot be suspended or resumed");
    }    
    
    _state = (suspended? Suspended : Running);
}

void Process::stop()
{
    _state = Stopped;
    
    // Clear the context stack, apart from the bottommost context, which 
    // represents the process itself.
    DENG2_FOR_EACH_REVERSE(ContextStack, i, _stack)
    {
        if(*i != _stack[0])
        {
            delete *i;
        }
    }
    DENG2_ASSERT(!_stack.empty());

    // Erase all but the first context.
    _stack.erase(_stack.begin() + 1, _stack.end());
    
    // This will reset any half-done evaluations, but it won't clear the namespace.
    context().reset();
}

void Process::execute()
{
    if(_state == Suspended || _state == Stopped)
    {
        // The process is not active.
        return;
    }

    // We will execute until this depth is complete.
    duint startDepth = depth();
    if(startDepth == 1)
    {
        // Mark the start time.
        _startedAt = Time();
    }

    // Execute the next command(s).
    while(_state == Running && depth() >= startDepth)
    {
        try
        {
            if(!context().execute())
            {
                finish();
            }
            if(_startedAt.since() > MAX_EXECUTION_TIME)
            {
                /// @throw HangError  Execution takes too long.
                throw HangError("Process::execute", 
                    "Script execution takes too long, or is stuck in an infinite loop");
            }
        }
        catch(Error const &err)
        {
            //std::cerr << "Caught " << err.asText() << " at depth " << depth() << "\n";
            
            // Fast-forward to find a suitable catch statement.
            if(jumpIntoCatch(err))
            {
                // Suitable catch statement was found. The current statement is now
                // pointing at the catch compound's first statement.
                continue;
            }
        
            if(startDepth > 1)
            {
                // Pop this context off, it has not caught the exception.
                delete popContext();
                throw;
            }
            else
            {
                // Exception uncaught by all contexts, script execution stops.
                LOG_SCR_NOTE("Stopping process: ") << err.asText();
                stop();
            }
        }
    }
}

bool Process::jumpIntoCatch(Error const &err)
{
    dint level = 0;

    // Proceed along default flow.
    for(context().proceed(); context().current(); context().proceed())
    {
        Statement const *statement = context().current();
        TryStatement const *tryStatement = dynamic_cast<TryStatement const *>(statement);
        if(tryStatement)
        {
            // Encountered a nested try statement.
            ++level;
            continue;
        }
        CatchStatement const *catchStatement = dynamic_cast<CatchStatement const *>(statement);
        if(catchStatement)
        {
            if(!level)
            {
                // This might be the catch for us.
                if(catchStatement->matches(err))
                {
                    catchStatement->executeCatch(context(), err);
                    return true;
                }
            }
            if(catchStatement->isFinal() && level > 0)
            {
                // A sequence of catch statements has ended.
                --level;
            }
        }
    }
    
    // Failed to find a catch statement that matches the given error.
    return false;
}

Context &Process::context(duint downDepth)
{
    DENG2_ASSERT(downDepth < depth());
    return **(_stack.rbegin() + downDepth);
}

Context *Process::popContext()
{
    Context *topmost = _stack.back();
    _stack.pop_back();

    // Pop a global namespace as well, if present.
    if(context().type() == Context::GlobalNamespace)
    {
        delete _stack.back();
        _stack.pop_back();
    }    
    
    return topmost;
}

void Process::finish(Value *returnValue)
{
    DENG2_ASSERT(depth() >= 1);

    // Move one level downwards in the context stack.
    if(depth() > 1)
    {
        // Finish the topmost context.
        std::auto_ptr<Context> topmost(popContext());
        if(topmost->type() == Context::FunctionCall)
        {
            // Return value to the new topmost level.
            context().evaluator().pushResult(returnValue? returnValue : new NoneValue);
        }
        delete topmost.release();
    }
    else
    {
        DENG2_ASSERT(_stack.back()->type() == Context::BaseProcess);
        
        // This was the last level.
        _state = Stopped;
    }   
}

String const &Process::workingPath() const
{
    return _workingPath;
}

void Process::setWorkingPath(String const &newWorkingPath)
{
    _workingPath = newWorkingPath;
}

void Process::call(Function const &function, ArrayValue const &arguments)
{
    // First map the argument values.
    Function::ArgumentValues argValues;
    function.mapArgumentValues(arguments, argValues);
    
    if(function.isNative())
    {
        // Do a native function call.
        context().evaluator().pushResult(function.callNative(context(), argValues));
    }
    else
    {
        // If the function resides in another process's namespace, push
        // that namespace on the stack first.
        if(function.globals() && function.globals() != &globals())
        {
            _stack.push_back(new Context(Context::GlobalNamespace, this, function.globals()));
        }
        
        // Create a new context.
        _stack.push_back(new Context(Context::FunctionCall, this));
        
        // Create local variables for the arguments in the new context.
        Function::ArgumentValues::const_iterator b = argValues.begin();
        Function::Arguments::const_iterator a = function.arguments().begin();
        for(; b != argValues.end() && a != function.arguments().end(); ++b, ++a)
        {
            context().names().add(new Variable(*a, (*b)->duplicate()));
        }
        
        // This should never be called if the process is suspended.
        DENG2_ASSERT(_state != Suspended);

        if(_state == Running)
        {
            // Execute the function as part of the currently running process.
            context().start(function.compound().firstStatement());
            execute();
        }
        else if(_state == Stopped)
        {
            // We'll execute just this one function.
            _state = Running;
            context().start(function.compound().firstStatement());
            execute();
            _state = Stopped;
        }
    }
}

void Process::namespaces(Namespaces &spaces) const
{
    spaces.clear();
    
    DENG2_FOR_EACH_CONST_REVERSE(ContextStack, i, _stack)
    {
        Context &context = **i;
        spaces.push_back(&context.names());
        if(context.type() == Context::GlobalNamespace)
        {
            // This shadows everything below.
            break;
        }
    }
}

Record &Process::globals()
{
    return _stack[0]->names();
}
