/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * https://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2022 Andrey Semashev
 */
/*!
 * \file scope/unique_resource.hpp
 *
 * This header contains definition of \c unique_resource template.
 */

#ifndef BOOST_SCOPE_UNIQUE_RESOURCE_HPP_INCLUDED_
#define BOOST_SCOPE_UNIQUE_RESOURCE_HPP_INCLUDED_

#include <type_traits>
#include <boost/core/addressof.hpp>
#include <boost/core/invoke_swap.hpp>
#include <boost/scope/unique_resource_fwd.hpp>
#include <boost/scope/detail/config.hpp>
#include <boost/scope/detail/compact_storage.hpp>
#include <boost/scope/detail/move_or_copy_assign_ref.hpp>
#include <boost/scope/detail/move_or_copy_construct_ref.hpp>
#include <boost/scope/detail/type_traits/is_swappable.hpp>
#include <boost/scope/detail/type_traits/is_nothrow_swappable.hpp>
#include <boost/scope/detail/type_traits/negation.hpp>
#include <boost/scope/detail/type_traits/conjunction.hpp>
#include <boost/scope/detail/type_traits/disjunction.hpp>
#include <boost/scope/detail/header.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

namespace boost {
namespace scope {

struct default_resource_t {};

//! Keyword representing default, unallocated resource argument
BOOST_INLINE_VARIABLE constexpr default_resource_t default_resource = {};

namespace detail {

template< typename T >
struct is_default_resource : public std::false_type { };
template< >
struct is_default_resource< default_resource_t > : public std::true_type { };
template< >
struct is_default_resource< const default_resource_t > : public std::true_type { };
template< >
struct is_default_resource< volatile default_resource_t > : public std::true_type { };
template< >
struct is_default_resource< const volatile default_resource_t > : public std::true_type { };
template< typename T >
struct is_default_resource< T& > : public is_default_resource< T >::type { };

template< typename T >
class ref_wrapper
{
private:
    T* m_value;

public:
    explicit
#if !defined(BOOST_CORE_NO_CONSTEXPR_ADDRESSOF)
    constexpr
#endif
    ref_wrapper(T& value) noexcept :
        m_value(boost::addressof(value))
    {
    }

    ref_wrapper& operator= (T& value) noexcept
    {
        m_value = boost::addressof(value);
        return *this;
    }

    ref_wrapper(T&&) = delete;
    ref_wrapper& operator= (T&&) = delete;

    operator T& () const noexcept
    {
        return *m_value;
    }

    template< typename... Args >
    void operator() (Args&&... args) const noexcept(noexcept(std::declval< T& >()(std::declval< Args&& >()...)))
    {
        (*m_value)(static_cast< Args&& >(args)...);
    }
};

template< typename T >
struct wrap_reference
{
    typedef T type;
};

template< typename T >
struct wrap_reference< T& >
{
    typedef ref_wrapper< T > type;
};

template< typename Resource, typename Traits >
struct has_custom_default_impl
{
    template< typename Tr, typename R = decltype(Tr::make_default()) >
    static typename std::is_constructible< Resource, R >::type _has_custom_default_check(int);
    template< typename Tr >
    static std::false_type _has_custom_default_check(...);

    typedef decltype(has_custom_default_impl::_has_custom_default_check< Traits >(0)) type;
};

template< typename Resource, typename Traits >
struct has_custom_default : public has_custom_default_impl< Resource, Traits >::type { };

template< typename Resource, typename Traits >
struct has_deallocated_state_impl
{
    template< typename Tr, typename Res, typename R = decltype(!!Tr::is_allocated(std::declval< Res const& >())) >
    static std::true_type _has_deallocated_state_check(int);
    template< typename Tr, typename Res >
    static std::false_type _has_deallocated_state_check(...);

    typedef decltype(has_deallocated_state_impl::_has_deallocated_state_check< Traits, Resource >(0)) type;
};

template< typename Resource, typename Traits >
struct has_deallocated_state : public has_deallocated_state_impl< Resource, Traits >::type { };

template< typename Resource, typename Traits, bool = has_custom_default< Resource, Traits >::value >
class resource_holder :
    public detail::compact_storage< typename wrap_reference< Resource >::type >
{
public:
    typedef Resource resource_type;
    typedef typename wrap_reference< Resource >::type internal_resource_type;
    typedef Traits traits_type;
    typedef detail::compact_storage< internal_resource_type > resource_base;

public:
    template<
        bool Requires = std::is_default_constructible< internal_resource_type >::value,
        typename = typename std::enable_if< Requires >::type
    >
    constexpr resource_holder() noexcept(std::is_nothrow_default_constructible< internal_resource_type >::value) :
        resource_base()
    {
    }

    template<
        typename R,
        typename = typename std::enable_if< std::is_constructible< internal_resource_type, R >::value >::type
    >
    explicit resource_holder(R&& res) noexcept(std::is_nothrow_constructible< internal_resource_type, R >::value) :
        resource_base(static_cast< R&& >(res))
    {
    }

    template<
        typename R,
        typename D,
        typename = typename std::enable_if< std::is_constructible< internal_resource_type, R >::value >::type
    >
    explicit resource_holder(R&& res, D&& del, bool allocated) noexcept(std::is_nothrow_constructible< internal_resource_type, R >::value) :
        resource_holder(static_cast< R&& >(res), static_cast< D&& >(del), allocated, typename std::is_nothrow_constructible< resource_type, R >::type())
    {
    }

    resource_type& get() noexcept
    {
        return resource_base::get();
    }

    resource_type const& get() const noexcept
    {
        return resource_base::get();
    }

    internal_resource_type& get_internal() noexcept
    {
        return resource_base::get();
    }

    internal_resource_type const& get_internal() const noexcept
    {
        return resource_base::get();
    }

private:
    template< typename R, typename D >
    explicit resource_holder(R&& res, D&& del, bool allocated, std::true_type) noexcept :
        resource_base(static_cast< R&& >(res))
    {
    }

    template< typename R, typename D >
    explicit resource_holder(R&& res, D&& del, bool allocated, std::false_type) try :
        resource_base(static_cast< R&& >(res))
    {
    }
    catch (...)
    {
        if (allocated)
            del(static_cast< R&& >(res));
    }
};

template< typename Resource, typename Traits >
class resource_holder< Resource, Traits, true > :
    public detail::compact_storage< typename wrap_reference< Resource >::type >
{
public:
    typedef Resource resource_type;
    typedef typename wrap_reference< Resource >::type internal_resource_type;
    typedef Traits traits_type;
    typedef detail::compact_storage< internal_resource_type > resource_base;

public:
    constexpr resource_holder()
        noexcept(detail::conjunction<
            std::integral_constant< bool, noexcept(traits_type::make_default()) >,
            std::is_nothrow_constructible< internal_resource_type, decltype(traits_type::make_default()) >
        >::value) :
        resource_base(traits_type::make_default())
    {
    }

    template<
        typename R,
        typename = typename std::enable_if< std::is_constructible< internal_resource_type, R >::value >::type
    >
    explicit resource_holder(R&& res) noexcept(std::is_nothrow_constructible< internal_resource_type, R >::value) :
        resource_base(static_cast< R&& >(res))
    {
    }

    template<
        typename R,
        typename D,
        typename = typename std::enable_if< std::is_constructible< internal_resource_type, R >::value >::type
    >
    explicit resource_holder(R&& res, D&& del, bool allocated) noexcept(std::is_nothrow_constructible< internal_resource_type, R >::value) :
        resource_holder(static_cast< R&& >(res), static_cast< D&& >(del), allocated, typename std::is_nothrow_constructible< internal_resource_type, R >::type())
    {
    }

    resource_type& get() noexcept
    {
        return resource_base::get();
    }

    resource_type const& get() const noexcept
    {
        return resource_base::get();
    }

    internal_resource_type& get_internal() noexcept
    {
        return resource_base::get();
    }

    internal_resource_type const& get_internal() const noexcept
    {
        return resource_base::get();
    }

private:
    template< typename R, typename D >
    explicit resource_holder(R&& res, D&& del, bool allocated, std::true_type) noexcept :
        resource_base(static_cast< R&& >(res))
    {
    }

    template< typename R, typename D >
    explicit resource_holder(R&& res, D&& del, bool allocated, std::false_type) try :
        resource_base(static_cast< R&& >(res))
    {
    }
    catch (...)
    {
        if (allocated)
            del(static_cast< R&& >(res));
    }
};

template< typename Resource, typename Deleter >
class deleter_holder :
    public detail::compact_storage< typename wrap_reference< Deleter >::type >
{
public:
    typedef Resource resource_type;
    typedef Deleter deleter_type;
    typedef typename wrap_reference< deleter_type >::type internal_deleter_type;
    typedef detail::compact_storage< internal_deleter_type > deleter_base;

public:
    template<
        bool Requires = std::is_default_constructible< internal_deleter_type >::value,
        typename = typename std::enable_if< Requires >::type
    >
    constexpr deleter_holder() noexcept(std::is_nothrow_default_constructible< internal_deleter_type >::value) :
        deleter_base()
    {
    }

    template<
        typename D,
        typename = typename std::enable_if< std::is_constructible< internal_deleter_type, D >::value >::type
    >
    explicit deleter_holder(D&& del) noexcept(std::is_nothrow_constructible< internal_deleter_type, D >::value) :
        deleter_base(static_cast< D&& >(del))
    {
    }

    template<
        typename D,
        typename = typename std::enable_if< std::is_constructible< internal_deleter_type, D >::value >::type
    >
    explicit deleter_holder(D&& del, resource_type& res, bool allocated) noexcept(std::is_nothrow_constructible< internal_deleter_type, D >::value) :
        deleter_holder(static_cast< D&& >(del), res, allocated, typename std::is_nothrow_constructible< internal_deleter_type, D >::type())
    {
    }

    deleter_type& get() noexcept
    {
        return deleter_base::get();
    }

    deleter_type const& get() const noexcept
    {
        return deleter_base::get();
    }

    internal_deleter_type& get_internal() noexcept
    {
        return deleter_base::get();
    }

    internal_deleter_type const& get_internal() const noexcept
    {
        return deleter_base::get();
    }

private:
    template< typename D >
    explicit deleter_holder(D&& del, resource_type& res, bool allocated, std::true_type) noexcept :
        deleter_base(static_cast< D&& >(del))
    {
    }

    template< typename D >
    explicit deleter_holder(D&& del, resource_type& res, bool allocated, std::false_type) try :
        deleter_base(static_cast< D&& >(del))
    {
    }
    catch (...)
    {
        if (BOOST_LIKELY(allocated))
            del(res);
    }
};

template< typename Resource, typename Traits, bool = detail::conjunction< has_deallocated_state< Resource, Traits >, has_custom_default< Resource, Traits > >::value >
struct use_deallocated_state : public std::false_type { };

template< typename Resource, typename Traits >
struct use_deallocated_state< Resource, Traits, true > :
    public detail::conjunction<
        std::integral_constant< bool, noexcept(Traits::make_default()) >,
        std::is_nothrow_assignable< Resource&, decltype(Traits::make_default()) >
    >
{
};

template< typename Resource, typename Deleter, typename Traits, bool = use_deallocated_state< Resource, Traits >::value >
class unique_resource_data :
    public detail::resource_holder< Resource, Traits >,
    public detail::deleter_holder< Resource, Deleter >
{
public:
    typedef Resource resource_type;
    typedef Deleter deleter_type;
    typedef Traits traits_type;
    typedef detail::resource_holder< resource_type, traits_type > resource_holder;
    typedef typename resource_holder::internal_resource_type internal_resource_type;
    typedef detail::deleter_holder< resource_type, deleter_type > deleter_holder;
    typedef typename deleter_holder::internal_deleter_type internal_deleter_type;

private:
    bool m_allocated;

public:
    template<
        bool Requires = detail::conjunction< std::is_default_constructible< resource_holder >, std::is_default_constructible< deleter_holder > >::value,
        typename = typename std::enable_if< Requires >::type
    >
    constexpr unique_resource_data()
        noexcept(detail::conjunction< std::is_nothrow_default_constructible< resource_holder >, std::is_nothrow_default_constructible< deleter_holder > >::value) :
        resource_holder(),
        deleter_holder(),
        m_allocated(false)
    {
    }

    unique_resource_data(unique_resource_data const&) = delete;
    unique_resource_data& operator= (unique_resource_data const&) = delete;

    template<
        bool Requires = detail::conjunction<
            std::is_constructible< internal_resource_type, typename detail::move_or_copy_construct_ref< resource_type >::type >,
            std::is_constructible< internal_deleter_type, typename detail::move_or_copy_construct_ref< deleter_type >::type >
        >::value,
        typename = typename std::enable_if< Requires >::type
    >
    unique_resource_data(unique_resource_data&& that)
        noexcept(detail::conjunction<
            std::is_nothrow_constructible< internal_resource_type, typename detail::move_or_copy_construct_ref< resource_type >::type >,
            std::is_nothrow_constructible< internal_deleter_type, typename detail::move_or_copy_construct_ref< deleter_type >::type >
        >::value) :
        unique_resource_data
        (
            static_cast< unique_resource_data&& >(that),
            typename std::is_nothrow_constructible< internal_deleter_type, typename detail::move_or_copy_construct_ref< deleter_type >::type >::type()
        )
    {
    }

    template<
        typename D,
        typename = typename std::enable_if< detail::conjunction<
            std::is_default_constructible< resource_holder >,
            std::is_constructible< deleter_holder, D >
        >::value >::type
    >
    explicit unique_resource_data(default_resource_t, D&& del)
        noexcept(detail::conjunction<
            std::is_nothrow_default_constructible< resource_holder >,
            std::is_nothrow_constructible< deleter_holder, D >
        >::value) :
        resource_holder(),
        deleter_holder(static_cast< D&& >(del)),
        m_allocated(false)
    {
    }

    template<
        typename R,
        typename D,
        typename = typename std::enable_if< detail::conjunction<
            detail::negation< detail::is_default_resource< R > >,
            std::is_constructible< resource_holder, R, D, bool >,
            std::is_constructible< deleter_holder, D, resource_type&, bool >
        >::value >::type
    >
    explicit unique_resource_data(R&& res, D&& del)
        noexcept(detail::conjunction<
            std::is_nothrow_constructible< resource_holder, R, D, bool >,
            std::is_nothrow_constructible< deleter_holder, D, resource_type&, bool >
        >::value) :
        resource_holder(static_cast< R&& >(res), static_cast< D&& >(del), true),
        deleter_holder(static_cast< D&& >(del), resource_holder::get(), true),
        m_allocated(true)
    {
    }

    template<
        bool Requires = detail::conjunction<
            std::is_assignable< internal_resource_type&, typename detail::move_or_copy_assign_ref< resource_type >::type >,
            std::is_assignable< internal_deleter_type&, typename detail::move_or_copy_assign_ref< deleter_type >::type >
        >::value
    >
    typename std::enable_if< Requires, unique_resource_data& >::type operator= (unique_resource_data&& that)
        noexcept(detail::conjunction<
            std::is_nothrow_assignable< internal_resource_type&, typename detail::move_or_copy_assign_ref< resource_type >::type >,
            std::is_nothrow_assignable< internal_deleter_type&, typename detail::move_or_copy_assign_ref< deleter_type >::type >
        >::value)
    {
        assign(static_cast< unique_resource_data&& >(that), typename std::is_nothrow_move_assignable< internal_deleter_type >::type());
        return *this;
    }

    resource_type& get_resource() noexcept
    {
        return resource_holder::get();
    }

    resource_type const& get_resource() const noexcept
    {
        return resource_holder::get();
    }

    internal_resource_type& get_internal_resource() noexcept
    {
        return resource_holder::get_internal();
    }

    internal_resource_type const& get_internal_resource() const noexcept
    {
        return resource_holder::get_internal();
    }

    deleter_type& get_deleter() noexcept
    {
        return deleter_holder::get();
    }

    deleter_type const& get_deleter() const noexcept
    {
        return deleter_holder::get();
    }

    internal_deleter_type& get_internal_deleter() noexcept
    {
        return deleter_holder::get_internal();
    }

    internal_deleter_type const& get_internal_deleter() const noexcept
    {
        return deleter_holder::get_internal();
    }

    bool is_allocated() const noexcept
    {
        return m_allocated;
    }

    void set_deallocated() noexcept
    {
        m_allocated = false;
    }

    template< typename R >
    void assign_resource(R&& res) noexcept(std::is_nothrow_assignable< internal_resource_type&, R >::value)
    {
        get_internal_resource() = static_cast< R&& >(res);
        m_allocated = true;
    }

    template<
        bool Requires = detail::conjunction<
            detail::is_swappable< internal_resource_type >,
            detail::is_swappable< internal_deleter_type >,
            detail::disjunction<
                detail::is_nothrow_swappable< internal_resource_type >,
                detail::is_nothrow_swappable< internal_deleter_type >
            >
        >::value
    >
    typename std::enable_if< Requires >::type swap(unique_resource_data& that)
        noexcept(detail::conjunction< detail::is_nothrow_swappable< internal_resource_type >, detail::is_nothrow_swappable< internal_deleter_type > >::value)
    {
        swap_impl
        (
            that,
            std::integral_constant< bool, detail::is_nothrow_swappable< internal_resource_type >::value >(),
            std::integral_constant< bool, detail::conjunction<
                detail::is_nothrow_swappable< internal_resource_type >,
                detail::is_nothrow_swappable< internal_deleter_type >
            >::value >()
        );
    }

private:
    unique_resource_data(unique_resource_data&& that, std::true_type)
        noexcept(std::is_nothrow_constructible< internal_resource_type, typename detail::move_or_copy_construct_ref< resource_type >::type >::value) :
        resource_holder(static_cast< typename detail::move_or_copy_construct_ref< resource_type >::type >(that.get_resource())),
        deleter_holder(static_cast< typename detail::move_or_copy_construct_ref< deleter_type >::type >(that.get_deleter()), resource_holder::get(), false),
        m_allocated(that.m_allocated)
    {
        that.m_allocated = false;
    }

    unique_resource_data(unique_resource_data&& that, std::false_type) try :
        resource_holder(static_cast< typename detail::move_or_copy_construct_ref< resource_type >::type >(that.get_resource())),
        deleter_holder(static_cast< typename detail::move_or_copy_construct_ref< deleter_type >::type >(that.get_deleter()), resource_holder::get(),
            std::is_nothrow_constructible< internal_resource_type, resource_type&& >::value && that.m_allocated), // don't deallocate if the resource was copy-constructed
        m_allocated(that.m_allocated)
    {
        that.m_allocated = false;
    }
    catch (...)
    {
        BOOST_IF_CONSTEXPR (std::is_nothrow_constructible< internal_resource_type, resource_type&& >::value)
        {
            // The resource was moved to this object, and the deleter constructor failed with an exception.
            // The deleter holder has invoked the deleter already, so the move source is no longer valid.
            that.m_allocated = false;
        }
    }

    void assign(unique_resource_data&& that, std::true_type)
        noexcept(std::is_nothrow_assignable< internal_resource_type&, typename detail::move_or_copy_assign_ref< resource_type >::type >::value)
    {
        get_internal_resource() = static_cast< typename detail::move_or_copy_assign_ref< resource_type >::type >(that.get_resource());
        get_internal_deleter() = static_cast< typename detail::move_or_copy_assign_ref< deleter_type >::type >(that.get_deleter());

        m_allocated = that.m_allocated;
        that.m_allocated = false;
    }

    void assign(unique_resource_data&& that, std::false_type)
    {
        get_internal_deleter() = static_cast< typename detail::move_or_copy_assign_ref< deleter_type >::type >(that.get_deleter());
        get_internal_resource() = static_cast< typename detail::move_or_copy_assign_ref< resource_type >::type >(that.get_resource());

        m_allocated = that.m_allocated;
        that.m_allocated = false;
    }

    void swap_impl(unique_resource_data& that, std::true_type, std::true_type) noexcept
    {
        boost::core::invoke_swap(get_internal_resource(), that.get_internal_resource());
        boost::core::invoke_swap(get_internal_deleter(), that.get_internal_deleter());

        const bool allocated = m_allocated;
        m_allocated = that.m_allocated;
        that.m_allocated = allocated;
    }

    void swap_impl(unique_resource_data& that, std::true_type, std::false_type)
    {
        boost::core::invoke_swap(get_internal_resource(), that.get_internal_resource());
        try
        {
            boost::core::invoke_swap(get_internal_deleter(), that.get_internal_deleter());
        }
        catch (...)
        {
            boost::core::invoke_swap(get_internal_resource(), that.get_internal_resource());
            throw;
        }

        const bool allocated = m_allocated;
        m_allocated = that.m_allocated;
        that.m_allocated = allocated;
    }

    void swap_impl(unique_resource_data& that, std::false_type, std::false_type)
    {
        boost::core::invoke_swap(get_internal_deleter(), that.get_internal_deleter());
        try
        {
            boost::core::invoke_swap(get_internal_resource(), that.get_internal_resource());
        }
        catch (...)
        {
            boost::core::invoke_swap(get_internal_deleter(), that.get_internal_deleter());
            throw;
        }

        const bool allocated = m_allocated;
        m_allocated = that.m_allocated;
        that.m_allocated = allocated;
    }
};

template< typename Resource, typename Deleter, typename Traits >
class unique_resource_data< Resource, Deleter, Traits, true > :
    public detail::resource_holder< Resource, Traits >,
    public detail::deleter_holder< Resource, Deleter >
{
public:
    typedef Resource resource_type;
    typedef Deleter deleter_type;
    typedef Traits traits_type;
    typedef detail::resource_holder< resource_type, traits_type > resource_holder;
    typedef typename resource_holder::internal_resource_type internal_resource_type;
    typedef detail::deleter_holder< resource_type, deleter_type > deleter_holder;
    typedef typename deleter_holder::internal_deleter_type internal_deleter_type;

public:
    template<
        bool Requires = detail::conjunction< std::is_default_constructible< resource_holder >, std::is_default_constructible< deleter_holder > >::value,
        typename = typename std::enable_if< Requires >::type
    >
    constexpr unique_resource_data()
        noexcept(detail::conjunction< std::is_nothrow_default_constructible< resource_holder >, std::is_nothrow_default_constructible< deleter_holder > >::value) :
        resource_holder(),
        deleter_holder()
    {
    }

    unique_resource_data(unique_resource_data const&) = delete;
    unique_resource_data& operator= (unique_resource_data const&) = delete;

    unique_resource_data(unique_resource_data&& that)
        noexcept(detail::conjunction<
            std::is_nothrow_constructible< internal_resource_type, typename detail::move_or_copy_construct_ref< resource_type >::type >,
            std::is_nothrow_constructible< internal_deleter_type, typename detail::move_or_copy_construct_ref< deleter_type >::type >
        >::value) :
        unique_resource_data
        (
            static_cast< unique_resource_data&& >(that),
            typename std::is_nothrow_constructible< internal_deleter_type, typename detail::move_or_copy_construct_ref< deleter_type >::type >::type()
        )
    {
    }

    template<
        typename D,
        typename = typename std::enable_if< detail::conjunction<
            std::is_default_constructible< resource_holder >,
            std::is_constructible< deleter_holder, D >
        >::value >::type
    >
    explicit unique_resource_data(default_resource_t, D&& del)
        noexcept(detail::conjunction<
            std::is_nothrow_default_constructible< resource_holder >,
            std::is_nothrow_constructible< deleter_holder, D >
        >::value) :
        resource_holder(),
        deleter_holder(static_cast< D&& >(del))
    {
    }

    template<
        typename R,
        typename D,
        typename = typename std::enable_if< detail::conjunction<
            detail::negation< detail::is_default_resource< R > >,
            std::is_constructible< resource_holder, R, D, bool >,
            std::is_constructible< deleter_holder, D, resource_type&, bool >
        >::value >::type
    >
    explicit unique_resource_data(R&& res, D&& del)
        noexcept(detail::conjunction<
            std::is_nothrow_constructible< resource_holder, R, D, bool >,
            std::is_nothrow_constructible< deleter_holder, D, resource_type&, bool >
        >::value) :
        unique_resource_data(static_cast< R&& >(res), static_cast< D&& >(del), traits_type::is_allocated(static_cast< R&& >(res)))
    {
    }

    template<
        bool Requires = detail::conjunction<
            std::is_assignable< internal_resource_type&, typename detail::move_or_copy_assign_ref< resource_type >::type >,
            std::is_assignable< internal_deleter_type&, typename detail::move_or_copy_assign_ref< deleter_type >::type >
        >::value
    >
    typename std::enable_if< Requires, unique_resource_data& >::type operator= (unique_resource_data&& that)
        noexcept(detail::conjunction<
            std::is_nothrow_assignable< internal_resource_type&, typename detail::move_or_copy_assign_ref< resource_type >::type >,
            std::is_nothrow_assignable< internal_deleter_type&, typename detail::move_or_copy_assign_ref< deleter_type >::type >
        >::value)
    {
        assign(static_cast< unique_resource_data&& >(that), typename std::is_nothrow_move_assignable< internal_deleter_type >::type());
        return *this;
    }

    resource_type& get_resource() noexcept
    {
        return resource_holder::get();
    }

    resource_type const& get_resource() const noexcept
    {
        return resource_holder::get();
    }

    internal_resource_type& get_internal_resource() noexcept
    {
        return resource_holder::get_internal();
    }

    internal_resource_type const& get_internal_resource() const noexcept
    {
        return resource_holder::get_internal();
    }

    deleter_type& get_deleter() noexcept
    {
        return deleter_holder::get();
    }

    deleter_type const& get_deleter() const noexcept
    {
        return deleter_holder::get();
    }

    internal_deleter_type& get_internal_deleter() noexcept
    {
        return deleter_holder::get_internal();
    }

    internal_deleter_type const& get_internal_deleter() const noexcept
    {
        return deleter_holder::get_internal();
    }

    bool is_allocated() const noexcept
    {
        return traits_type::is_allocated(get_resource());
    }

    void set_deallocated() noexcept
    {
        get_internal_resource() = traits_type::make_default();
    }

    template< typename R >
    void assign_resource(R&& res) noexcept(std::is_nothrow_assignable< internal_resource_type&, R >::value)
    {
        get_internal_resource() = static_cast< R&& >(res);
    }

    template<
        bool Requires = detail::conjunction<
            detail::is_swappable< internal_resource_type >,
            detail::is_swappable< internal_deleter_type >,
            detail::disjunction<
                detail::is_nothrow_swappable< internal_resource_type >,
                detail::is_nothrow_swappable< internal_deleter_type >
            >
        >::value
    >
    typename std::enable_if< Requires >::type swap(unique_resource_data& that)
        noexcept(detail::conjunction< detail::is_nothrow_swappable< internal_resource_type >, detail::is_nothrow_swappable< internal_deleter_type > >::value)
    {
        swap_impl
        (
            that,
            std::integral_constant< bool, detail::is_nothrow_swappable< internal_resource_type >::value >(),
            std::integral_constant< bool, detail::conjunction<
                detail::is_nothrow_swappable< internal_resource_type >,
                detail::is_nothrow_swappable< internal_deleter_type >
            >::value >()
        );
    }

private:
    unique_resource_data(unique_resource_data&& that, std::true_type)
        noexcept(std::is_nothrow_constructible< internal_resource_type, typename detail::move_or_copy_construct_ref< resource_type >::type >::value) :
        resource_holder(static_cast< typename detail::move_or_copy_construct_ref< resource_type >::type >(that.get_resource())),
        deleter_holder(static_cast< typename detail::move_or_copy_construct_ref< deleter_type >::type >(that.get_deleter()), resource_holder::get(), false)
    {
        that.set_deallocated();
    }

    unique_resource_data(unique_resource_data&& that, std::false_type) try :
        resource_holder(static_cast< typename detail::move_or_copy_construct_ref< resource_type >::type >(that.get_resource())),
        deleter_holder(static_cast< typename detail::move_or_copy_construct_ref< deleter_type >::type >(that.get_deleter()), resource_holder::get(),
            std::is_nothrow_constructible< internal_resource_type, resource_type&& >::value && is_allocated()) // don't deallocate if the resource was copy-constructed
    {
        that.set_deallocated();
    }
    catch (...)
    {
        BOOST_IF_CONSTEXPR (std::is_nothrow_constructible< internal_resource_type, resource_type&& >::value)
        {
            // The resource was moved to this object, and the deleter constructor failed with an exception.
            // The deleter holder has invoked the deleter already, so the move source is no longer valid.
            that.set_deallocated();
        }
    }

    template<
        typename R,
        typename D,
        typename = typename std::enable_if< detail::conjunction<
            std::is_constructible< resource_holder, R, D, bool >,
            std::is_constructible< deleter_holder, D, resource_type&, bool >
        >::value >::type
    >
    explicit unique_resource_data(R&& res, D&& del, bool allocated)
        noexcept(detail::conjunction<
            std::is_nothrow_constructible< resource_holder, R, D, bool >,
            std::is_nothrow_constructible< deleter_holder, D, resource_type&, bool >
        >::value) :
        resource_holder(static_cast< R&& >(res), static_cast< D&& >(del), allocated),
        deleter_holder(static_cast< D&& >(del), resource_holder::get(), allocated)
    {
    }

    void assign(unique_resource_data&& that, std::true_type)
        noexcept(std::is_nothrow_assignable< internal_resource_type&, typename detail::move_or_copy_assign_ref< resource_type >::type >::value)
    {
        get_internal_resource() = static_cast< typename detail::move_or_copy_assign_ref< resource_type >::type >(that.get_resource());
        get_internal_deleter() = static_cast< typename detail::move_or_copy_assign_ref< deleter_type >::type >(that.get_deleter());

        that.set_deallocated();
    }

    void assign(unique_resource_data&& that, std::false_type)
    {
        get_internal_deleter() = static_cast< typename detail::move_or_copy_assign_ref< deleter_type >::type >(that.get_deleter());
        get_internal_resource() = static_cast< typename detail::move_or_copy_assign_ref< resource_type >::type >(that.get_resource());

        that.set_deallocated();
    }

    void swap_impl(unique_resource_data& that, std::true_type, std::true_type) noexcept
    {
        boost::core::invoke_swap(get_internal_resource(), that.get_internal_resource());
        boost::core::invoke_swap(get_internal_deleter(), that.get_internal_deleter());
    }

    void swap_impl(unique_resource_data& that, std::true_type, std::false_type)
    {
        boost::core::invoke_swap(get_internal_resource(), that.get_internal_resource());
        try
        {
            boost::core::invoke_swap(get_internal_deleter(), that.get_internal_deleter());
        }
        catch (...)
        {
            boost::core::invoke_swap(get_internal_resource(), that.get_internal_resource());
            throw;
        }
    }

    void swap_impl(unique_resource_data& that, std::false_type, std::false_type)
    {
        boost::core::invoke_swap(get_internal_deleter(), that.get_internal_deleter());
        try
        {
            boost::core::invoke_swap(get_internal_resource(), that.get_internal_resource());
        }
        catch (...)
        {
            boost::core::invoke_swap(get_internal_deleter(), that.get_internal_deleter());
            throw;
        }
    }
};

template< typename T >
struct is_dereferenceable_impl
{
    template< typename U, typename R = decltype(*std::declval< U const& >()) >
    static std::true_type _is_dereferenceable_check(int);
    template< typename U >
    static std::false_type _is_dereferenceable_check(...);

    typedef decltype(is_dereferenceable_impl::_is_dereferenceable_check< T >(0)) type;
};

template< typename T >
struct is_dereferenceable : public is_dereferenceable_impl< T >::type { };
template< >
struct is_dereferenceable< void* > : public std::false_type { };
template< >
struct is_dereferenceable< const void* > : public std::false_type { };
template< >
struct is_dereferenceable< volatile void* > : public std::false_type { };
template< >
struct is_dereferenceable< const volatile void* > : public std::false_type { };
template< >
struct is_dereferenceable< void*& > : public std::false_type { };
template< >
struct is_dereferenceable< const void*& > : public std::false_type { };
template< >
struct is_dereferenceable< volatile void*& > : public std::false_type { };
template< >
struct is_dereferenceable< const volatile void*& > : public std::false_type { };
template< >
struct is_dereferenceable< void* const& > : public std::false_type { };
template< >
struct is_dereferenceable< const void* const& > : public std::false_type { };
template< >
struct is_dereferenceable< volatile void* const& > : public std::false_type { };
template< >
struct is_dereferenceable< const volatile void* const& > : public std::false_type { };
template< >
struct is_dereferenceable< void* volatile& > : public std::false_type { };
template< >
struct is_dereferenceable< const void* volatile& > : public std::false_type { };
template< >
struct is_dereferenceable< volatile void* volatile& > : public std::false_type { };
template< >
struct is_dereferenceable< const volatile void* volatile& > : public std::false_type { };
template< >
struct is_dereferenceable< void* const volatile& > : public std::false_type { };
template< >
struct is_dereferenceable< const void* const volatile& > : public std::false_type { };
template< >
struct is_dereferenceable< volatile void* const volatile& > : public std::false_type { };
template< >
struct is_dereferenceable< const volatile void* const volatile& > : public std::false_type { };

template< typename T, bool = detail::is_dereferenceable< T >::value >
struct dereference_traits { };
template< typename T >
struct dereference_traits< T, true >
{
    typedef decltype(*std::declval< T const& >()) result_type;
    static constexpr bool is_noexcept = noexcept(*std::declval< T const& >());
};

} // namespace detail

/*!
 * \brief RAII wrapper for automatically reclaiming arbitrary resources.
 *
 * A \c unique_resource object exclusively owns wrapped resource and invokes
 * the deleter function object on it on destruction. The wrapped resource can have
 * any type that is:
 *
 * \li Move-constructible, where the move constructor is marked as `noexcept`, or
 * \li Copy-constructible, or
 * \li An lvalue reference to an object type.
 *
 * The deleter must be a function object type that is callable on an lvalue
 * of the resource type. The deleter must be copy-constructible.
 *
 * An optional resource traits template parameter may be specified. Resource
 * traits can be used to optimize \c unique_resource implementation when
 * the following conditions are met:
 *
 * \li There is at least one value of the resource type that is considered
 *     unallocated (that is, no allocated resource shall be equal to one of
 *     the unallocated resource values). The unallocated resource values need not
 *     be deallocated using the deleter.
 * \li One of the unallocated resource values can be considered the default.
 *     Constructing the default resource value and assigning it to a resource
 *     object (whether allocated or not) shall not throw exceptions.
 * \li Resource objects can be tested for being unallocated. Such a test shall
 *     not throw exceptions.
 *
 * If specified, the resource traits must be a class that has the following
 * public static members:
 *
 * \li `Resource make_default() noexcept` - must return the default resource
 *      value.
 * \li `bool is_allocated(Resource const& res) noexcept` - must
 *     return \c true if \c res is not one of the unallocated resource values
 *     and \c false otherwise.
 *
 * Note that `is_allocated(make_default())` must always return \c false.
 *
 * When conforming resource traits are specified, \c unique_resource will be able
 * to avoid storing additional indication of whether the owned resource object
 * needs to be deallocated with the deleter on destruction. It will use the default
 * resource value to initialize the owned resource object when \c unique_resource
 * is not in the allocated state. Additionally, it will be possible to construct
 * \c unique_resource with unallocated resource values, which will create
 * \c unique_resource objects in deallocated state (the deleter will not be called
 * on unallocated resource values).
 *
 * \tparam Resource Resource type.
 * \tparam Deleter Resource deleter function object type.
 * \tparam Traits Optional resource traits type.
 */
template< typename Resource, typename Deleter, typename Traits BOOST_SCOPE_DETAIL_DOC(= void) >
class unique_resource
{
public:
    //! Resource type
    typedef Resource resource_type;
    //! Deleter type
    typedef Deleter deleter_type;
    //! Resource traits
    typedef Traits traits_type;

//! \cond
private:
    typedef detail::unique_resource_data< resource_type, deleter_type, traits_type > data;
    typedef typename data::internal_resource_type internal_resource_type;
    typedef typename data::internal_deleter_type internal_deleter_type;

    data m_data;

//! \endcond
public:
    /*!
     * \brief Constructs an unallocated unique resource guard.
     *
     * **Requires:** Default \c Resource value can be constructed and \c Deleter is default-constructible.
     *
     * **Effects:** Initializes the \c Resource object with the default resource value. Default-constructs
     *              the \c Deleter object.
     *
     * **Throws:** Nothing, unless construction of \c Resource or \c Deleter throws.
     *
     * \post `this->allocated() == false`
     */
    //! \cond
    template<
        bool Requires = std::is_default_constructible< data >::value,
        typename = typename std::enable_if< Requires >::type
    >
    //! \endcond
    constexpr unique_resource() noexcept(BOOST_SCOPE_DETAIL_DOC_HIDDEN(std::is_nothrow_default_constructible< data >::value))
    {
    }

    /*!
     * \brief Constructs an unallocated unique resource guard with the given deleter.
     *
     * **Requires:** Default \c Resource value can be constructed and \c Deleter is constructible from \a del.
     *
     * **Effects:** Initializes the \c Resource value with the default resource value. If \c Deleter is nothrow
     *              constructible from `D&&` then constructs \c Deleter from `std::forward< D >(del)`,
     *              otherwise constructs from `del`.
     *
     * **Throws:** Nothing, unless construction of \c Resource or \c Deleter throws.
     *
     * \param res A tag argument indicating default resource value.
     * \param del Resource deleter function object.
     *
     * \post `this->allocated() == false`
     */
    template<
        typename D
        //! \cond
        , typename = typename std::enable_if<
            std::is_constructible< data, default_resource_t, typename detail::move_or_copy_construct_ref< D, deleter_type >::type >::value
        >::type
        //! \endcond
    >
    unique_resource(default_resource_t res, D&& del)
        noexcept(BOOST_SCOPE_DETAIL_DOC_HIDDEN(
            std::is_nothrow_constructible<
                data,
                default_resource_t,
                typename detail::move_or_copy_construct_ref< D, deleter_type >::type
            >::value
        )) :
        m_data
        (
            res,
            static_cast< typename detail::move_or_copy_construct_ref< D, deleter_type >::type >(del)
        )
    {
    }

    /*!
     * \brief Constructs a unique resource guard with the given resource and a default-constructed deleter.
     *
     * **Requires:** \c Resource is constructible from \a res and \c Deleter is default-constructible.
     *
     * **Effects:** Constructs the unique resource object as if by calling
     *              `unique_resource(std::forward< R >(res), Deleter())`.
     *
     * **Throws:** Nothing, unless construction of \c Resource or \c Deleter throws.
     *
     * \param res Resource object.
     */
    template<
        typename R
        //! \cond
        , typename = typename std::enable_if< detail::conjunction<
            std::is_nothrow_default_constructible< deleter_type >,
            std::is_constructible< data, typename detail::move_or_copy_construct_ref< R, resource_type >::type, typename detail::move_or_copy_construct_ref< deleter_type >::type >,
            detail::disjunction< detail::negation< std::is_reference< resource_type > >, std::is_reference< R > > // prevent binding lvalue-reference resource to an rvalue
        >::value >::type
        //! \endcond
    >
    explicit unique_resource(R&& res)
        noexcept(BOOST_SCOPE_DETAIL_DOC_HIDDEN(
            std::is_nothrow_constructible<
                data,
                typename detail::move_or_copy_construct_ref< R, resource_type >::type,
                typename detail::move_or_copy_construct_ref< deleter_type >::type
            >::value
        )) :
        m_data
        (
            static_cast< typename detail::move_or_copy_construct_ref< R, resource_type >::type >(res),
            static_cast< typename detail::move_or_copy_construct_ref< deleter_type >::type >(deleter_type())
        )
    {
    }

    /*!
     * \brief Constructs a unique resource guard with the given resource and deleter.
     *
     * **Requires:** \c Resource is constructible from \a res and \c Deleter is constructible from \a del.
     *
     * **Effects:** If \c Resource is nothrow constructible from `R&&` then constructs \c Resource
     *              from `std::forward< R >(res)`, otherwise constructs from `res`. If \c Deleter
     *              is nothrow constructible from `D&&` then constructs \c Deleter from
     *              `std::forward< D >(del)`, otherwise constructs from `del`.
     *
     *              If construction of \c Resource or \c Deleter throws and \a res is not an unallocated resource
     *              value, invokes \a del on \a res (if \c Resource construction failed) or the constructed
     *              \c Resource object (if \c Deleter construction failed).
     *
     * **Throws:** Nothing, unless construction of \c Resource or \c Deleter throws.
     *
     * \param res Resource object.
     * \param del Resource deleter function object.
     *
     * \post If \a res is an unallocated resource value then `this->allocated() == false`, otherwise
     *       `this->allocated() == true`.
     */
    template<
        typename R,
        typename D
        //! \cond
        , typename = typename std::enable_if< detail::conjunction<
            std::is_constructible< data, typename detail::move_or_copy_construct_ref< R, resource_type >::type, typename detail::move_or_copy_construct_ref< D, deleter_type >::type >,
            detail::disjunction< detail::negation< std::is_reference< resource_type > >, std::is_reference< R > > // prevent binding lvalue-reference resource to an rvalue
        >::value >::type
        //! \endcond
    >
    unique_resource(R&& res, D&& del)
        noexcept(BOOST_SCOPE_DETAIL_DOC_HIDDEN(
            std::is_nothrow_constructible<
                data,
                typename detail::move_or_copy_construct_ref< R, resource_type >::type,
                typename detail::move_or_copy_construct_ref< D, deleter_type >::type
            >::value
        )) :
        m_data
        (
            static_cast< typename detail::move_or_copy_construct_ref< R, resource_type >::type >(res),
            static_cast< typename detail::move_or_copy_construct_ref< D, deleter_type >::type >(del)
        )
    {
    }

    unique_resource(unique_resource const&) = delete;
    unique_resource& operator= (unique_resource const&) = delete;

    /*!
     * \brief Move-constructs a unique resource guard.
     *
     * **Requires:** \c Resource and \c Deleter are move-constructible.
     *
     * **Effects:** If \c Resource is nothrow move-constructible then move-constructs \c Resource,
     *              otherwise copy-constructs. If \c Deleter is nothrow move-constructible then move-constructs
     *              \c Deleter, otherwise copy-constructs. Deactivates the moved-from unique resource object.
     *
     *              If `that.allocated()` was \c true prior to the operation and constructing \c Deleter
     *              throws after \c Resource is move-constructed, invokes the original deleter stored in \a that
     *              on the resource and deactivates \a that before returning with the exception. In other
     *              exceptional cases \a that is left intact.
     *
     * \note This logic ensures that the resource is not leaked in case of an exception.
     *
     * **Throws:** Nothing, unless construction of \c Resource or \c Deleter throws.
     *
     * \param that Move source.
     *
     * \post Let \c allocated be equal to `that.allocated()` prior to the operation. Then
     *       `this->allocated() == allocated` and `that.allocated() == false`.
     */
    //! \cond
    template<
        bool Requires = std::is_move_constructible< data >::value,
        typename = typename std::enable_if< Requires >::type
    >
    //! \endcond
    unique_resource(unique_resource&& that) noexcept(BOOST_SCOPE_DETAIL_DOC_HIDDEN(std::is_nothrow_move_constructible< data >::value)) :
        m_data(static_cast< data&& >(that.m_data))
    {
    }

    /*!
     * \brief Move-assigns a unique resource guard.
     *
     * **Requires:** \c Resource and \c Deleter are move-assignable.
     *
     * **Effects:** Calls `this->reset()`. Then, if \c Deleter is nothrow move-assignable, move-assigns
     *              the \c Deleter object first and the \c Resource object next. Otherwise, move-assigns
     *              the objects in reverse order. Lastly, deactivates the moved-from unique resource object.
     *
     *              If an exception is thrown, \a that is left intact.
     *
     * \note The different orders of assignment ensure that in case of an exception the resource is not leaked
     *       and ramains owned by the move source.
     *
     * **Throws:** Nothing, unless assignment of \c Resource or \c Deleter throws.
     *
     * \param that Move source.
     *
     * \post Let \c allocated be equal to `that.allocated()` prior to the operation. Then
     *       `this->allocated() == allocated` and `that.allocated() == false`.
     */
#if !defined(BOOST_SCOPE_DOXYGEN)
    template< bool Requires = std::is_move_assignable< data >::value >
    typename std::enable_if< Requires, unique_resource& >::type
#else
    unique_resource&
#endif
    operator= (unique_resource&& that)
        noexcept(BOOST_SCOPE_DETAIL_DOC_HIDDEN(std::is_nothrow_move_assignable< data >::value))
    {
        reset();
        m_data = static_cast< data&& >(that.m_data);
        return *this;
    }

    /*!
     * \brief Invokes `reset()` and destroys the resource.
     *
     * **Throws:** Nothing, unless invoking the deleter throws.
     */
    ~unique_resource() noexcept(noexcept(std::declval< deleter_type& >()(std::declval< resource_type& >())))
    {
        reset();
    }

    /*!
     * \brief Returns \c true if the resource is allocated and to be reclaimed by the deleter, otherwise \c false.
     *
     * **Throws:** Nothing.
     */
    bool allocated() const noexcept
    {
        return m_data.is_allocated();
    }

    /*!
     * \brief Returns a reference to the resource object.
     *
     * **Throws:** Nothing.
     */
    resource_type const& get() const noexcept
    {
        return m_data.get_resource();
    }

    /*!
     * \brief Returns a reference to the deleter object.
     *
     * **Throws:** Nothing.
     */
    deleter_type const& get_deleter() const noexcept
    {
        return m_data.get_deleter();
    }

    /*!
     * \brief Marks the resource as unallocated. Does not call the deleter if the resource was previously allocated.
     *
     * **Throws:** Nothing.
     *
     * \post `this->allocated() == false`
     */
    void release() noexcept
    {
        m_data.set_deallocated();
    }

    /*!
     * \brief If the resource is allocated, calls the deleter function on it and marks the resource as unallocated.
     *
     * **Throws:** Nothing, unless invoking the deleter throws.
     *
     * \post `this->allocated() == false`
     */
    void reset() noexcept(noexcept(std::declval< deleter_type& >()(std::declval< resource_type& >())))
    {
        if (BOOST_LIKELY(m_data.is_allocated()))
        {
            m_data.get_deleter()(m_data.get_resource());
            m_data.set_deallocated();
        }
    }

    /*!
     * \brief Assigns a new resource object to the unique resource wrapper.
     *
     * **Effects:** Calls `this->reset()`. Then, if \c Resource is nothrow assignable from `R&&`,
     *              assigns `std::forward< R >(res)` to the stored resource object, otherwise assigns
     *              `res`.
     *
     *              If \a res is not an unallocated resource value and an exception is thrown during the operation,
     *              invokes the stored deleter on \a res before returning with the exception.
     *
     * **Throws:** Nothing, unless invoking the deleter throws.
     *
     * \param res Resource object to assign.
     *
     * \post `this->allocated() == false`
     */
    template< typename R >
#if !defined(BOOST_SCOPE_DOXYGEN)
    typename std::enable_if< detail::conjunction<
        std::is_assignable< internal_resource_type&, typename detail::move_or_copy_assign_ref< R, resource_type >::type >,
        detail::disjunction< detail::negation< std::is_reference< resource_type > >, std::is_reference< R > > // prevent binding lvalue-reference resource to an rvalue
    >::value >::type
#else
    void
#endif
    reset(R&& res)
        noexcept(BOOST_SCOPE_DETAIL_DOC_HIDDEN(
            detail::conjunction<
                std::integral_constant< bool, noexcept(std::declval< deleter_type& >()(std::declval< resource_type& >())) >,
                std::is_nothrow_assignable< internal_resource_type&, typename detail::move_or_copy_assign_ref< R, resource_type >::type >
            >::value
        ))
    {
        reset_impl
        (
            static_cast< typename detail::move_or_copy_assign_ref< R, resource_type >::type >(res),
            typename detail::conjunction<
                std::integral_constant< bool, noexcept(std::declval< deleter_type& >()(std::declval< resource_type& >())) >,
                std::is_nothrow_assignable< internal_resource_type&, typename detail::move_or_copy_assign_ref< R, resource_type >::type >
            >::type()
        );
    }

    /*!
     * \brief Invokes indirection on the resource object.
     *
     * **Requires:** \c Resource is dereferenceable.
     *
     * **Effects:** Returns a reference to the resource object as if by calling `get()`.
     *
     * \note If \c Resource is not a pointer type, the compiler will invoke its `operator->`.
     *       Such call sequence will continue until a pointer is obtained.
     *
     * **Throws:** Nothing. Note that any implicit subsequent calls to other `operator->`
     *             functions that are caused by this call may have different throw conditions.
     */
#if !defined(BOOST_SCOPE_DOXYGEN)
    template< bool Requires = detail::is_dereferenceable< resource_type >::value >
    typename std::enable_if< Requires, resource_type const& >::type
#else
    resource_type const&
#endif
    operator-> () const noexcept
    {
        return get();
    }

    /*!
     * \brief Dereferences the resource object.
     *
     * **Requires:** \c Resource is dereferenceable.
     *
     * **Effects:** Returns the result of dereferencing the resource object as if by calling `*get()`.
     *
     * **Throws:** Nothing, unless dereferencing the resource object throws.
     */
#if !defined(BOOST_SCOPE_DOXYGEN)
    template< bool Requires = detail::is_dereferenceable< resource_type >::value >
    typename detail::dereference_traits< resource_type, Requires >::result_type
#else
    auto
#endif
    operator* () const
        noexcept(BOOST_SCOPE_DETAIL_DOC_HIDDEN(detail::dereference_traits< resource_type, Requires >::is_noexcept))
    {
        return *get();
    }

    /*!
     * \brief Swaps two unique resource wrappers.
     *
     * **Requires:** \c Resource and \c Deleter are swappable. At least one of \c Resource and \c Deleter
     *               is nothrow swappable.
     *
     * **Effects:** Swaps the resource objects and deleter objects stored in `*this` and `that`
     *              as if by calling unqualified `swap` in a context where `std::swap` is
     *              found by overload resolution.
     *
     * **Throws:** Nothing, unless swapping the resource objects or deleters throw.
     *
     * \param that Unique resource wrapper to swap with.
     */
#if !defined(BOOST_SCOPE_DOXYGEN)
    template< bool Requires = detail::is_swappable< data >::value >
    typename std::enable_if< Requires >::type
#else
    void
#endif
    swap(unique_resource& that)
        noexcept(BOOST_SCOPE_DETAIL_DOC_HIDDEN(detail::is_nothrow_swappable< data >::value))
    {
        m_data.swap(that.m_data);
    }

    /*!
     * \brief Swaps two unique resource wrappers.
     *
     * **Effects:** As if `left.swap(right)`.
     */
#if !defined(BOOST_SCOPE_DOXYGEN)
    template< bool Requires = detail::is_swappable< data >::value >
    friend typename std::enable_if< Requires >::type
#else
    friend void
#endif
    swap(unique_resource& left, unique_resource& right)
        noexcept(BOOST_SCOPE_DETAIL_DOC_HIDDEN(detail::is_nothrow_swappable< data >::value))
    {
        left.swap(right);
    }

//! \cond
private:
    //! Assigns a new resource object to the unique resource wrapper.
    template< typename R >
    void reset_impl(R&& res, std::true_type) noexcept
    {
        reset();
        m_data.assign_resource(static_cast< R&& >(res));
    }

    //! Assigns a new resource object to the unique resource wrapper.
    template< typename R >
    void reset_impl(R&& res, std::false_type)
    {
        try
        {
            reset();
            m_data.assign_resource(static_cast< R&& >(res));
        }
        catch (...)
        {
            m_data.get_deleter()(static_cast< R&& >(res));
            throw;
        }
    }
//! \endcond
};

#if !defined(BOOST_NO_CXX17_DEDUCTION_GUIDES)
template<
    typename Resource,
    typename Deleter,
    typename = typename std::enable_if< !detail::is_default_resource< Resource >::value >::type
>
unique_resource(Resource&&, Deleter&&) -> unique_resource<
    typename std::remove_cv< typename std::remove_reference< Resource >::type >::type,
    typename std::remove_cv< typename std::remove_reference< Deleter >::type >::type
>;
#endif // !defined(BOOST_NO_CXX17_DEDUCTION_GUIDES)

/*!
 * \brief Checks if the resource is valid and creates a \c unique_resource wrapper.
 *
 * **Effects:** If the resource \a res is not equal to \a invalid, creates a unique resource wrapper
 *              that is in allocated state and owns \a res. Otherwise creates a unique resource wrapper
 *              in deallocated state.
 *
 * \note This function does not call \a del if \a res is equal to \a invalid.
 *
 * **Throws:** Nothing, unless \c unique_resource constructor throws.
 *
 * \param res Resource to wrap.
 * \param invalid An invalid value for the resource.
 * \param del A deleter to invoke on the resource to free it.
 */
template< typename Resource, typename Deleter, typename Invalid >
inline unique_resource< typename std::decay< Resource >::type, typename std::decay< Deleter >::type >
make_unique_resource_checked(Resource&& res, Invalid const& invalid, Deleter&& del)
    noexcept(BOOST_SCOPE_DETAIL_DOC_HIDDEN(
        detail::conjunction<
            std::is_nothrow_constructible< typename std::decay< Resource >::type, typename detail::move_or_copy_construct_ref< Resource, typename std::decay< Resource >::type >::type >,
            std::is_nothrow_constructible< typename std::decay< Deleter >::type, typename detail::move_or_copy_construct_ref< Deleter, typename std::decay< Deleter >::type >::type >
        >::value
    ))
{
    typedef unique_resource< typename std::decay< Resource >::type, typename std::decay< Deleter >::type > unique_resource_t;
    if (!(res == invalid))
        return unique_resource_t(static_cast< Resource&& >(res), static_cast< Deleter&& >(del));
    else
        return unique_resource_t(default_resource_t(), static_cast< Deleter&& >(del));
}

} // namespace scope
} // namespace boost

#include <boost/scope/detail/footer.hpp>

#endif // BOOST_SCOPE_UNIQUE_RESOURCE_HPP_INCLUDED_
