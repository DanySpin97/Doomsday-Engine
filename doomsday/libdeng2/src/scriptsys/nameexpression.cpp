/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/NameExpression"
#include "de/Evaluator"
#include "de/Process"
#include "de/TextValue"
#include "de/RefValue"
#include "de/RecordValue"
#include "de/Writer"
#include "de/Reader"
#include "de/App"
#include "de/Module"

using namespace de;

NameExpression::NameExpression()
{}

NameExpression::NameExpression(String const &identifier, Flags const &flags) 
    : _identifier(identifier)
{
    setFlags(flags);
}

NameExpression::~NameExpression()
{}

Value *NameExpression::evaluate(Evaluator &evaluator) const
{
    //LOG_AS("NameExpression::evaluate");
    //std::cout << "NameExpression::evaluator: " << _flags.to_string() << "\n";
    //LOG_DEBUG("path = %s, scope = %x") << _path << evaluator.names();
    
    // Collect the namespaces to search.
    Evaluator::Namespaces spaces;
    evaluator.namespaces(spaces);
    
    Record *foundInNamespace = 0;
    Variable *variable = 0;
    
    DENG2_FOR_EACH(Evaluator::Namespaces, i, spaces)
    {
        Record &ns = **i;
        if(ns.hasMember(_identifier))
        {
            // The name exists in this namespace.
            variable = &ns[_identifier];
            foundInNamespace = &ns;
            break;
        }
        /*
        if(ns.hasSubrecord(_identifier))
        {
            // The name exists in this namespace (as a record).
            record = &ns.subrecord(_identifier);
            foundInNamespace = &ns;
        }
        */
        if(flags().testFlag(LocalOnly))
        {
            break;
        }
    }

    if(flags().testFlag(ThrowawayIfInScope) && variable)
    {
        foundInNamespace = 0;
        variable = &evaluator.context().throwaway();
    }

    // If a new variable/record is required and one is in scope, we cannot continue.
    if(flags().testFlag(NotInScope) && variable)
    {
        throw AlreadyExistsError("NameExpression::evaluate", 
            "Identifier '" + _identifier + "' already exists");
    }
    
    // Should we delete the identifier?
    if(flags().testFlag(Delete))
    {
        if(!variable) // && !record)
        {
            throw NotFoundError("NameExpression::evaluate",
                "Cannot delete nonexistent identifier '" + _identifier + "'");
        }
        DENG2_ASSERT(foundInNamespace != 0);
        //if(variable)
        //{

        delete foundInNamespace->remove(*variable);

        /*}
        else if(record)
        {
            delete foundInNamespace->remove(_identifier);
        }*/
        return new NoneValue();
    }

    // Create a new subrecord in the namespace? ("record xyz")
    if(flags().testFlag(NewSubrecord))
    {
        // Replaces existing member with this identifier.
        Record &record = spaces.front()->addRecord(_identifier);

        return new RecordValue(&record);
    }

    // If nothing is found and we are permitted to create new variables, do so.
    // Occurs when assigning into new variables.
    if(!variable /*&& !record*/ && flags().testFlag(NewVariable))
    {
        variable = new Variable(_identifier);

        // Add it to the local namespace.
        spaces.front()->add(variable);
    }

    // Should we import a namespace?
    if(flags() & Import)
    {
        Record *record = &App::importModule(_identifier,
            evaluator.process().globals()["__file__"].value().asText());

        // Overwrite any existing member with this identifier.
        spaces.front()->add(variable = new Variable(_identifier));

        if(flags().testFlag(ByValue))
        {
            // Take a copy of the record ("import record").
            *variable = new RecordValue(new Record(*record), RecordValue::OwnsRecord);
        }
        else
        {
            // The variable will merely reference the module.
            *variable = new RecordValue(record);
        }

        return new RecordValue(record);

        /*
        // Take a copy if requested ("import record").
        if(flags().testFlag(NewRecord))
        {
            record = &spaces.front()->add(_identifier, new Record(*record));
        }*/
    }
    
    if(variable)
    {
        // Variables can be referred to by reference or value.
        if(flags() & ByReference)
        {
            // Reference to the variable.
            return new RefValue(variable);
        }
        else
        {
            // Variables evaluate to their values.
            return variable->value().duplicate();
        }
    }

    /*
    // We may be permitted to create a new record.
    if(flags() & NewRecord)
    {
        if(!record)
        {
            // Add it to the local namespace.
            record = &spaces.front()->addRecord(_identifier);
        }
        else
        {
            // Create a variable referencing the record.
            spaces.front()->add(new Variable(_identifier, new RecordValue(record)));
        }
    }
    if(record)
    {
        // Records can only be referenced.
        return new RecordValue(record);
    }
    */
    
    throw NotFoundError("NameExpression::evaluate", "Identifier '" + _identifier + 
        "' does not exist");
}

void NameExpression::operator >> (Writer &to) const
{
    to << SerialId(NAME);

    Expression::operator >> (to);

    to << _identifier;
}

void NameExpression::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if(id != NAME)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized expression was invalid.
        throw DeserializationError("NameExpression::operator <<", "Invalid ID");
    }

    Expression::operator << (from);

    from >> _identifier;
}
