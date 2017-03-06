#include <main/dispatcher.hh>

bool
SQLDispatcher::canDo(LEX *const lex) const
{
    return handlers.end() != handlers.find(extract(lex));
}

const SQLHandler &
SQLDispatcher::dispatch(LEX *const lex) const
{
    auto it = handlers.find(extract(lex));
    assert(handlers.end() != it);

    assert(it->second);
    return *it->second;
}

long long
SQLDispatcher::extract(LEX *const lex) const
{
    return lex->sql_command;
}

bool
AlterDispatcher::canDo(LEX *const lex) const
{
    // there must be a command for us to do
    if (0 == lex->alter_info.flags) {
        return false;
    }

    long long flags = lex->alter_info.flags;
    for (const auto &it : handlers) {
        flags -= lex->alter_info.flags & it.first;
    }

    return 0 == flags;
}

std::vector<AlterSubHandler *>
AlterDispatcher::dispatch(LEX *const lex) const
{
    std::vector<AlterSubHandler *> out;
    for (const auto &it : handlers) {
        const long long extract = lex->alter_info.flags & it.first;
        if (extract) {
            auto it_handler = handlers.find(extract);
            assert(handlers.end() != it_handler && it_handler->second);
            out.push_back(it_handler->second.get());
        }
    }

    return out;
}

