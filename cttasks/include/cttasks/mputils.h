#pragma once

#include <tuple>
#include <assert.h>

namespace olagarro
{
namespace ct_tasks
{

template <std::size_t I> struct VisitImplementation
{
    template <typename Type, typename Visitor> static void visit(Type& tuple, std::size_t index, Visitor visitor)
    {
        if(index == I - 1)
        {
            visitor(index, std::get<I - 1>(tuple));
        }
        else
        {
            VisitImplementation<I - 1>::visit(tuple, index, visitor);
        }
    }
};

template <> struct VisitImplementation<0>
{
    template <typename Type, typename Visitor> static void visit(Type&, std::size_t, Visitor) { assert(false); }
};

template <typename Visitor, typename... Types>
void visit_tuple_at(std::tuple<Types...> const& tuple, std::size_t index, Visitor visitor)
{
    VisitImplementation<sizeof...(Types)>::visit(tuple, index, visitor);
}

template <typename Visitor, typename... Types>
void visit_tuple_at(std::tuple<Types...>& tuple, std::size_t index, Visitor visitor)
{
    VisitImplementation<sizeof...(Types)>::visit(tuple, index, visitor);
}

template <typename Visitor, typename... Types> void for_each_in_tuple(std::tuple<Types...>& tuple, Visitor visitor)
{
    for(auto i{0u}; i < sizeof...(Types); ++ i)
    {
        visit_tuple_at(tuple, i, visitor);
    }
}

} // namespace ct_tasks
} // namespace olagarro
