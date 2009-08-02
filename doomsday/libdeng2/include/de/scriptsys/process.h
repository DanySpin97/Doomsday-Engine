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

#ifndef LIBDENG2_PROCESS_H
#define LIBDENG2_PROCESS_H

#include "../Script"
#include "../Time"
#include "../Context"
#include "../Function"
#include "../String"

#include <list>

namespace de
{
    class Variable;
    class ArrayValue;
    
    /**
     * Runs a script. The process maintains the execution environment, including things
     * like local variables and keeping track of which statement is being executed.
     * 
     * @ingroup script
     */
    class Process
    {
    public:
        /// The process is running while an operation is attempted that requires the 
        /// process to be stopped. @ingroup errors
        DEFINE_ERROR(NotStoppedError);
        
        /// Suspending or resuming fails. @ingroup errors
        DEFINE_ERROR(SuspendError);
        
        /// Execution is taking too long to complete. @ingroup errors
        DEFINE_ERROR(HangError);
        
        /// A process is always in one of these states.
        enum State {
            RUNNING,    /**< The process is running normally. */
            SUSPENDED,  /**< The process has been suspended and will
                         *   not continue running until restored.  A
                         *   process cannot restore itself from a
                         *   suspended state. */
            STOPPED     /**< The process has reached the end of the
                         *   script or has been terminated. */
        };
        
        typedef std::list<Record*> Namespaces;

    public:
        /**
         * Constructs a new process. The process is initialized to RUNNING state.
         *
         * @param script  Script to run.
         */
        Process(const Script& script);
        
        virtual ~Process();

        State state() const { return state_; }

        /// Determines the current depth of the call stack.
        duint depth() const;

        /**
         * Starts running the given script. Note that the process must be
         * in the FINISHED state for this to be a valid operation.
         *
         * @param script  Script to run.
         */
        void run(const Script& script);

        /**
         * Suspends or resumes execution of the script.
         */
        void suspend(bool suspended = true);
        
        /**
         * Stops the execution of the script. The process is set to the 
         * FINISHED state, which means it must be rerun with a new script.
         */
        void stop();

        /*
         * Execute the next command(s) in the script.
         *
         * @param timeBox  If defined, execution will continue at most for this
         *                 period of time. By default execution continues until
         *                 the script leaves RUNNING state.
         */
        void execute(const Time::Delta& timeBox = 0);

        /**
         * Finish the execution of the topmost context. Execution will 
         * continue from the second topmost context.
         * 
         * @param returnValue  Value to use as the return value from the 
         *                     context. Takes ownership of the value.
         */
        void finish(Value* returnValue = 0);

        /**
         * Changes the working path of the process. File system access within the 
         * process is relative to the working path.
         *
         * @param newWorkingPath  New working path for the process.
         */
        void setWorkingPath(const String& newWorkingPath);

        /**
         * Returns the current working path.
         */
        const String& workingPath() const;

        /**
         * Return an execution context. By default returns the topmost context.
         *
         * @param downDepth  How many levels to go down. There are depth() levels
         *                   in the context stack.
         *
         * @return  Context at @a downDepth.
         */
        Context& context(duint downDepth = 0);
                
        /// A method call.
        void call(Function& function, const ArrayValue& arguments);
        
        /**
         * Collects the namespaces currently visible. This includes the process's
         * own stack and the global namespaces.
         *
         * @param spaces  The namespaces are collected here. The order is important:
         *                earlier namespaces shadow the subsequent ones.
         */
        void namespaces(Namespaces& spaces);
        
    private:
        State state_;

        // The execution environment.
        typedef std::vector<Context*> ContextStack;
        ContextStack stack_;
        
        /// This is the current working folder of the process. Relative paths
        /// given to workingFile() are located in relation to this
        /// folder. Initial value is the root folder.
        String workingPath_;

        /// @c true, if the process has not executed a single statement yet.
        bool firstExecute_;
        
        /// Time when execution was started at depth 1.
        Time startedAt_;
    };
}

#endif /* LIBDENG2_PROCESS_H */
