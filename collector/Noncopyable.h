#ifndef __UTILS_H__
#define __UTILS_H__

// boost::noncopyable header path was moved with Boost 1.56
#include <boost/version.hpp>

#if BOOST_VERSION >= 105600
#include <boost/core/noncopyable.hpp>
#else
#include <boost/noncopyable.hpp>
#endif

#endif /* __UTILS_H__ */
