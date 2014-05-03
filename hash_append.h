//----------------------------- hash_append.h ----------------------------------
// 
// This software is in the public domain.  The only restriction on its use is
// that no one can remove it from the public domain by claiming ownership of it,
// including the original authors.
// 
// There is no warranty of correctness on the software contained herein.  Use
// at your own risk.
// 
//------------------------------------------------------------------------------

#ifndef HASH_APPEND
#define HASH_APPEND

#include <cstddef>
#include <type_traits>
#include <utility>
#include <tuple>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <system_error>

#include "siphash.h"  // the current default hasher

// Everything in namespace xstd, excluding those items in xstd::detail,
//  is proposed.

// C++14 is assumed below because std::index_sequence_for makes hash_append
// 	for tuple just so easy.  So in for a penny, in for a pound...

namespace xstd
{

namespace detail
{

//  Standards-worthy utilities, but not for this proposal...

template <bool ...> struct static_and;

template <bool B0, bool ... Bp>
struct static_and<B0, Bp...>
    : public std::integral_constant<bool, B0 && static_and<Bp...>{}>
{
};

template <>
struct static_and<>
    : public std::true_type
{
};

template <std::size_t ...> struct static_sum;

template <std::size_t S0, std::size_t ...SN>
struct static_sum<S0, SN...>
    : public std::integral_constant<std::size_t, S0 + static_sum<SN...>{}>
{
};

template <>
struct static_sum<>
    : public std::integral_constant<std::size_t, 0>
{
};

}  // detail

// is_contiguously_hashable<T>

// A type T is contiguously hashable if for all combinations of two values of
// 	a type, say x and y, if x == y, then it must also be true that
// 	memcmp(addressof(x), addressof(y), sizeof(T)) == 0. I.e. if x == y,
// 	then x and y have the same bit pattern representation.

template <class T>
struct is_contiguously_hashable
    : public std::integral_constant<bool, std::is_integral<T>{} || 
                                          std::is_enum<T>{}     ||
                                          std::is_pointer<T>{}>
{
};

// is_contiguously_hashable<std::pair<T, U>>

template <class T, class U>
struct is_contiguously_hashable<std::pair<T, U>>
    : public std::integral_constant<bool, is_contiguously_hashable<T>{} && 
                                          is_contiguously_hashable<U>{} &&
                                          sizeof(T) + sizeof(U) == sizeof(std::pair<T, U>)>
{
};

// is_contiguously_hashable<std::tuple<T...>>

template <class ...T>
struct is_contiguously_hashable<std::tuple<T...>>
    : public std::integral_constant<bool,
            detail::static_and<is_contiguously_hashable<T>{}...>{} && 
            detail::static_sum<sizeof(T)...>{} == sizeof(std::tuple<T...>)>
{
};

// is_contiguously_hashable<T[N]>

template <class T, std::size_t N>
struct is_contiguously_hashable<T[N]>
    : public std::integral_constant<bool, is_contiguously_hashable<T>{}>
{
};

// is_contiguously_hashable<std::array<T, N>>

template <class T, std::size_t N>
struct is_contiguously_hashable<std::array<T, N>>
    : public std::integral_constant<bool, is_contiguously_hashable<T>{} && 
                                          sizeof(T)*N == sizeof(std::array<T, N>)>
{
};

// template <class Hasher, class T>
// void
// hash_append(Hasher& h, T const& t);
// 
// Each type to be hashed must either be contiguously hashable, or overload
// 	hash_append to expose its hashable bits to a Hasher.

// scalars

template <class Hasher, class T>
inline
std::enable_if_t
<
    is_contiguously_hashable<T>{}
>
hash_append(Hasher& h, T const& t) noexcept
{
    h(std::addressof(t), sizeof(t));
}

template <class Hasher, class T>
inline
std::enable_if_t
<
    std::is_floating_point<T>{}
>
hash_append(Hasher& h, T t) noexcept
{
    if (t == 0)
        t = 0;
    h(&t, sizeof(t));
}

template <class Hasher>
inline
void
hash_append(Hasher& h, std::nullptr_t) noexcept
{
    void const* p = nullptr;
    h(&p, sizeof(p));
}

// Forward declarations for ADL purposes

template <class Hasher, class T, std::size_t N>
std::enable_if_t
<
    !is_contiguously_hashable<T>{}
>
hash_append(Hasher& h, T (&a)[N]) noexcept;

template <class Hasher, class CharT, class Traits, class Alloc>
std::enable_if_t
<
    !is_contiguously_hashable<CharT>{} 
>
hash_append(Hasher& h, std::basic_string<CharT, Traits, Alloc> const& s) noexcept;

template <class Hasher, class CharT, class Traits, class Alloc>
std::enable_if_t
<
    is_contiguously_hashable<CharT>{} 
>
hash_append(Hasher& h, std::basic_string<CharT, Traits, Alloc> const& s) noexcept;

template <class Hasher, class T, class U>
std::enable_if_t
<
    !is_contiguously_hashable<std::pair<T, U>>{} 
>
hash_append (Hasher& h, std::pair<T, U> const& p) noexcept;

template <class Hasher, class T, class Alloc>
std::enable_if_t
<
    !is_contiguously_hashable<T>{}
>
hash_append(Hasher& h, std::vector<T, Alloc> const& v) noexcept;

template <class Hasher, class T, class Alloc>
std::enable_if_t
<
    is_contiguously_hashable<T>{}
>
hash_append(Hasher& h, std::vector<T, Alloc> const& v) noexcept;

template <class Hasher, class T, std::size_t N>
std::enable_if_t
<
    !is_contiguously_hashable<std::array<T, N>>{}
>
hash_append(Hasher& h, std::array<T, N> const& a) noexcept;

template <class Hasher, class ...T>
std::enable_if_t
<
    !is_contiguously_hashable<std::tuple<T...>>{}
>
hash_append(Hasher& h, std::tuple<T...> const& t) noexcept;

template <class Hasher, class Key, class T, class Hash, class Pred, class Alloc>
void
hash_append(Hasher& h, std::unordered_map<Key, T, Hash, Pred, Alloc> const& m);

template <class Hasher, class Key, class Hash, class Pred, class Alloc>
void
hash_append(Hasher& h, std::unordered_set<Key, Hash, Pred, Alloc> const& s);

template <class Hasher, class T0, class T1, class ...T>
void
hash_append (Hasher& h, T0 const& t0, T1 const& t1, T const& ...t) noexcept;

// c-array

template <class Hasher, class T, std::size_t N>
std::enable_if_t
<
    !is_contiguously_hashable<T>{}
>
hash_append(Hasher& h, T (&a)[N]) noexcept
{
    for (auto const& t : a)
        hash_append(h, t);
}

// basic_string

template <class Hasher, class CharT, class Traits, class Alloc>
inline
std::enable_if_t
<
    !is_contiguously_hashable<CharT>{} 
>
hash_append(Hasher& h, std::basic_string<CharT, Traits, Alloc> const& s) noexcept
{
    for (auto c : s)
        hash_append(h, c);
    hash_append(h, s.size());
}

template <class Hasher, class CharT, class Traits, class Alloc>
inline
std::enable_if_t
<
    is_contiguously_hashable<CharT>{} 
>
hash_append(Hasher& h, std::basic_string<CharT, Traits, Alloc> const& s) noexcept
{
    h(s.data(), s.size()*sizeof(CharT));
    hash_append(h, s.size());
}

// pair

template <class Hasher, class T, class U>
inline
std::enable_if_t
<
    !is_contiguously_hashable<std::pair<T, U>>{}
>
hash_append (Hasher& h, std::pair<T, U> const& p) noexcept
{
    hash_append (h, p.first, p.second);
}

// vector

template <class Hasher, class T, class Alloc>
inline
std::enable_if_t
<
    !is_contiguously_hashable<T>{}
>
hash_append(Hasher& h, std::vector<T, Alloc> const& v) noexcept
{
    for (auto const& t : v)
        hash_append(h, t);
    hash_append(h, v.size());
}

template <class Hasher, class T, class Alloc>
inline
std::enable_if_t
<
    is_contiguously_hashable<T>{}
>
hash_append(Hasher& h, std::vector<T, Alloc> const& v) noexcept
{
    h(v.data(), v.size()*sizeof(T));
    hash_append(h, v.size());
}

// array

template <class Hasher, class T, std::size_t N>
std::enable_if_t
<
    !is_contiguously_hashable<std::array<T, N>>{}
>
hash_append(Hasher& h, std::array<T, N> const& a) noexcept
{
    for (auto const& t : a)
        hash_append(h, t);
}

// tuple

namespace detail
{

inline
void
for_each_item(...) noexcept
{
}

template <class Hasher, class T>
inline
int
hash_one(Hasher& h, T const& t) noexcept
{
    hash_append(h, t);
    return 0;
}

template <class Hasher, class ...T, std::size_t ...I>
inline
void
tuple_hash(Hasher& h, std::tuple<T...> const& t, std::index_sequence<I...>) noexcept
{
    for_each_item(hash_one(h, std::get<I>(t))...);
}

}  // detail

template <class Hasher, class ...T>
inline
std::enable_if_t
<
    !is_contiguously_hashable<std::tuple<T...>>{}
>
hash_append(Hasher& h, std::tuple<T...> const& t) noexcept
{
    detail::tuple_hash(h, t, std::index_sequence_for<T...>{});
}

// variadic

template <class Hasher, class T0, class T1, class ...T>
inline
void
hash_append (Hasher& h, T0 const& t0, T1 const& t1, T const& ...t) noexcept
{
    hash_append(h, t0);
    hash_append(h, t1, t...);
}

// error_code

template <class HashAlgorithm>
inline
void
hash_append(HashAlgorithm& h, std::error_code const& ec)
{
    hash_append(h, ec.value(), &ec.category());
}

// uhash

template <class Hasher = acme::siphash>
struct uhash
{
    using result_type = typename Hasher::result_type;

    template <class T>
    result_type
    operator()(T const& t) const noexcept
    {
        Hasher h;
        hash_append(h, t);
        return static_cast<result_type>(h);
    }
};

}  // xstd

#endif  // HASH_APPEND
