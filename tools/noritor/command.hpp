#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <nori/asset.hpp>
#include <nori/resource/binary.hpp>
#include <nori/resource/error_code.hpp>
#include <nori/resource/handle.hpp>
#include <nori/resource/image.hpp>
#include <nori/resource/value.hpp>

namespace nori::editor
{
class temporary_storage;

using revision = std::uint64_t;

using property_snapshot = std::variant<
    std::monostate,
    bool,
    std::int32_t,
    std::int64_t,
    float,
    std::string,
    nori::resource::image,
    nori::resource::binary
>;

struct resource_request_target
{
    nori::resource::handle document_root;
    nori::resource::handle expected;
    std::string path;
};

struct command_effect
{
    std::vector<nori::resource::handle> created;
    std::vector<nori::resource::handle> updated;
    std::vector<nori::resource::handle> invalidated;
};

struct command_execution
{
    command_effect effect;
    nori::resource::result<void> result;
    bool changed;
};

struct command_result
{
    std::string description;
    command_execution execution;
};

class command
{
public:
    virtual ~command() = default;

    virtual command_execution execute() = 0;
    virtual command_execution undo() = 0;
    virtual std::string description() const = 0;
    virtual const void* merge_type() const { return nullptr; }
    virtual bool merge(const command& newer);
    virtual bool net_noop() const;
};

nori::resource::result<nori::resource::handle> resolve_property(nori::resource::handle document_root, std::string_view subpath);
nori::resource::result<std::string> relative_property_path(nori::resource::handle document_root, nori::resource::handle target);
nori::resource::result<property_snapshot> copy_property_snapshot(nori::resource::handle target);
bool is_primitive_type(nori::resource::property_type type);

template <typename T>
class create_command final : public command
{
public:
    create_command(nori::resource::handle document_root, nori::resource::handle parent, std::string name, T initial_value);

    command_execution execute() override;
    command_execution undo() override;
    std::string description() const override;

private:
    nori::resource::handle document_root_;
    nori::resource::handle parent_;
    std::string parent_path_;
    std::string name_;
    T initial_value_;
};

class import_model_command final : public command
{
public:
    import_model_command(nori::resource::handle document_root, nori::resource::handle parent, std::string name, nori::asset::cooked_model model);

    command_execution execute() override;
    command_execution undo() override;
    std::string description() const override;

private:
    nori::resource::handle document_root_;
    nori::resource::handle parent_;
    std::string parent_path_;
    std::string name_;
    nori::asset::cooked_model model_;
};

struct copy_source
{
    nori::resource::handle source;
    nori::resource::handle parent;
    std::string name;
    std::optional<std::size_t> position;
};

class copy_command final : public command
{
public:
    copy_command(temporary_storage* temporary, nori::resource::handle document_root, std::vector<copy_source> sources);
    ~copy_command() override;

    command_execution execute() override;
    command_execution undo() override;
    std::string description() const override;

private:
    struct entry
    {
        nori::resource::handle backup;
        nori::resource::handle overwritten_backup;
        std::string parent_path;
        std::string name;
        std::optional<std::size_t> position;
        std::optional<std::size_t> overwritten_position;
    };

    command_execution prepare();
    command_execution materialize();

    temporary_storage* temporary_;
    nori::resource::handle document_root_;
    std::vector<copy_source> sources_;
    std::vector<entry> entries_;
};

class remove_command final : public command
{
public:
    remove_command(temporary_storage* temporary, nori::resource::handle document_root, std::vector<nori::resource::handle> targets);
    ~remove_command() override;

    command_execution execute() override;
    command_execution undo() override;
    std::string description() const override;

private:
    struct entry
    {
        nori::resource::handle backup;
        std::string parent_path;
        std::string name;
        std::size_t position;
    };

    command_execution prepare();
    command_execution remove();

    temporary_storage* temporary_;
    nori::resource::handle document_root_;
    std::vector<nori::resource::handle> targets_;
    std::vector<entry> entries_;
};

class move_command final : public command
{
public:
    struct entry
    {
        nori::resource::handle source;
        nori::resource::handle destination;
        std::optional<std::size_t> position;
    };

    move_command(nori::resource::handle document_root, std::vector<entry> entries);

    command_execution execute() override;
    command_execution undo() override;
    std::string description() const override;

private:
    struct prepared_entry
    {
        std::string before_path;
        std::string after_path;
        std::string before_parent_path;
        std::string after_parent_path;
        std::size_t before_position;
        std::optional<std::size_t> after_position;
    };

    command_execution prepare();
    command_execution move(bool forward);

    nori::resource::handle document_root_;
    std::vector<entry> inputs_;
    std::vector<prepared_entry> entries_;
};

class set_name_command final : public command
{
public:
    set_name_command(nori::resource::handle document_root, nori::resource::handle target, std::string new_name);

    command_execution execute() override;
    command_execution undo() override;
    std::string description() const override;

private:
    command_execution set_name(std::string_view source, std::string_view destination);

    nori::resource::handle document_root_;
    nori::resource::handle target_;
    std::string parent_path_;
    std::string old_name_;
    std::string new_name_;
};

class set_type_command final : public command
{
public:
    set_type_command(nori::resource::handle document_root, std::vector<nori::resource::handle> targets, nori::resource::property_type after);

    command_execution execute() override;
    command_execution undo() override;
    std::string description() const override;

private:
    struct entry
    {
        std::string path;
        property_snapshot before;
    };

    command_execution apply_after();
    command_execution apply_before();

    nori::resource::handle document_root_;
    std::vector<nori::resource::handle> targets_;
    std::vector<entry> entries_;
    property_snapshot after_;
};

template <typename T>
class set_value_command final : public command
{
public:
    set_value_command(nori::resource::handle document_root, std::vector<nori::resource::handle> targets, T after);

    command_execution execute() override;
    command_execution undo() override;
    std::string description() const override;
    const void* merge_type() const override
    {
        static int token{};
        return &token;
    }
    bool merge(const command& newer) override;
    bool net_noop() const override;

private:
    struct entry
    {
        std::string path;
        T before;
    };

    command_execution apply_after();
    command_execution apply_before();

    nori::resource::handle document_root_;
    std::vector<nori::resource::handle> targets_;
    std::vector<entry> entries_;
    T after_;
};

struct history_entry
{
    std::unique_ptr<command> command;
    revision before_revision;
    revision after_revision;
};

class command_manager
{
public:
    command_manager();

    command_result execute(std::unique_ptr<command> new_command);
    command_result undo();
    command_result redo();

    bool can_undo() const;
    bool can_redo() const;
    bool dirty() const;

    void mark_saved();
    void break_merge();
    void clear();

    static constexpr std::size_t max_size{256};

private:
    std::deque<history_entry> undo_stack_;
    std::deque<history_entry> redo_stack_;
    revision current_revision_;
    std::optional<revision> saved_revision_;
    revision next_revision_;
    bool merge_allowed_;
};
} // namespace nori::editor
