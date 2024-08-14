#pragma once

#if defined(_MSC_VER)
#    pragma warning(push)
#    pragma warning(disable : 4996)
#    pragma warning(disable : 4324)
#    pragma warning(disable : 4456)
#    pragma warning(disable : 4201)
#endif

// where is this defined anyway? :(
#undef MAX_PRIORITY
#include <taskflow/taskflow.hpp>

#if defined(_MSC_VER)
#    pragma warning(pop)
#endif

using Executor = tf::Executor;
using Task = tf::Task;
using Taskflow = tf::Taskflow;
using Subflow = tf::Subflow;
template<typename T>
using Future = tf::Future<T>;
