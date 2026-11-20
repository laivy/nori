#pragma once
#include <memory>

#include <nori/io/connection.hpp>
#include <nori/io/context.hpp>
#include <nori/io/strand.hpp>
#include <nori/io/task_handle.hpp>

namespace nori::io
{
class context;
class strand;
class coroutine_entry;
class connection;

std::shared_ptr<context> acquire(context_handle handle);
std::shared_ptr<strand> acquire(strand_handle handle);

connection_handle insert(context_handle ctx, std::shared_ptr<connection> state);
task_handle insert(context_handle ctx, std::shared_ptr<coroutine_entry> owner);
void release(task_handle task);
void finish(task_handle task);
} // namespace nori::io
