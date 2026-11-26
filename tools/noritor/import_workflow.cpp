#include "pch.hpp"

#include <array>
#include <format>
#include <utility>

#include <nori/asset.hpp>
#include <nori/core/encoding.hpp>
#include <nori/io.hpp>
#include <nori/resource/image.hpp>
#include <nori/resource/operations.hpp>

#include "context.hpp"
#include "import_workflow.hpp"

namespace nori::editor
{
import_workflow::import_workflow(context* ctx, nori::io::context_handle main_context, nori::io::context_handle worker_context) :
    context_{ctx},
    main_context_{main_context},
    worker_context_{worker_context}
{
}

void import_workflow::start_image(std::filesystem::path path, resource_request_target request)
{
    if (!nori::io::co_spawn(main_context_, import_image(std::move(path), std::move(request))))
    {
        context_->set_status("이미지 작업 시작 실패: 작업을 제출할 수 없습니다.");
    }
}

void import_workflow::start_model(std::filesystem::path path, resource_request_target request)
{
    if (!nori::io::co_spawn(main_context_, import_model(std::move(path), std::move(request))))
    {
        context_->set_status("FBX 작업 시작 실패: 작업을 제출할 수 없습니다.");
    }
}

void import_workflow::start_replacement(std::filesystem::path path, resource_request_target request)
{
    if (!nori::io::co_spawn(main_context_, replace_image(std::move(path), std::move(request))))
    {
        context_->set_status("이미지 작업 시작 실패: 작업을 제출할 수 없습니다.");
    }
}

nori::io::awaitable<void> import_workflow::import_image(std::filesystem::path path, resource_request_target request)
{
    {
        const document_editor* const target_document{context_->documents().find_root(request.document_root)};
        if (!target_document || target_document->closing())
        {
            co_return;
        }
        const auto parent{resolve_property(request.document_root, request.path)};
        if (!parent || *parent != request.expected)
        {
            co_return;
        }
    }

    auto image{co_await nori::io::run_on(worker_context_, [path = std::move(path)]()
    {
        return nori::asset::cook_image(path, nori::resource::image_format::rgba8_unorm);
    })};
    if (!image)
    {
        context_->set_status(std::format("이미지 불러오기 실패: {}", image.error().diagnostics));
        co_return;
    }

    document_editor* const target_document{context_->documents().find_root(request.document_root)};
    if (!target_document || target_document->closing())
    {
        co_return;
    }
    const auto parent{resolve_property(request.document_root, request.path)};
    if (!parent || *parent != request.expected)
    {
        co_return;
    }

    const auto effect{target_document->create(*parent, target_document->unique_child_name(*parent), std::move(*image))};
    if (!effect)
    {
        report(effect.error());
        co_return;
    }
    context_->workspace(request.document_root).hierarchy().open_nodes.insert(*parent);
}

nori::io::awaitable<void> import_workflow::import_model(std::filesystem::path path, resource_request_target request)
{
    {
        const document_editor* const target_document{context_->documents().find_root(request.document_root)};
        if (!target_document || target_document->closing())
        {
            co_return;
        }
        const auto parent{resolve_property(request.document_root, request.path)};
        if (!parent || *parent != request.expected)
        {
            co_return;
        }
    }

    const std::string base_name{nori::core::path_to_utf8(path.stem())};
    auto model{co_await nori::io::run_on(worker_context_, [path = std::move(path)]()
    {
        return nori::asset::cook_model(path);
    })};
    if (!model)
    {
        const std::string diagnostics{model.error().diagnostics.empty() ? std::string{nori::asset::to_string(model.error().code)} : std::format("{}: {}", nori::asset::to_string(model.error().code), model.error().diagnostics)};
        context_->set_status(std::format("FBX 모델 가져오기 실패: {}", diagnostics));
        co_return;
    }

    document_editor* const target_document{context_->documents().find_root(request.document_root)};
    if (!target_document || target_document->closing())
    {
        co_return;
    }
    const auto parent{resolve_property(request.document_root, request.path)};
    if (!parent || *parent != request.expected)
    {
        co_return;
    }

    const std::string name{target_document->unique_child_name(*parent, base_name.empty() ? "Model" : base_name)};
    const auto effect{target_document->import_model(*parent, name, std::move(*model))};
    if (!effect)
    {
        report(effect.error());
        co_return;
    }
    context_->workspace(request.document_root).hierarchy().open_nodes.insert(*parent);
}

nori::io::awaitable<void> import_workflow::replace_image(std::filesystem::path path, resource_request_target request)
{
    {
        const document_editor* const target_document{context_->documents().find_root(request.document_root)};
        if (!target_document || target_document->closing())
        {
            co_return;
        }
        const auto target{resolve_property(request.document_root, request.path)};
        if (!target || *target != request.expected)
        {
            co_return;
        }
    }

    auto image{co_await nori::io::run_on(worker_context_, [path = std::move(path)]()
    {
        return nori::asset::cook_image(path, nori::resource::image_format::rgba8_unorm);
    })};
    if (!image)
    {
        context_->set_status(std::format("이미지 불러오기 실패: {}", image.error().diagnostics));
        co_return;
    }

    document_editor* const target_document{context_->documents().find_root(request.document_root)};
    if (!target_document || target_document->closing())
    {
        co_return;
    }
    const auto target{resolve_property(request.document_root, request.path)};
    if (!target || *target != request.expected)
    {
        co_return;
    }

    const std::array targets{*target};
    if (const auto res{target_document->set_value(targets, std::move(*image))}; !res)
    {
        report(res.error());
    }
}

void import_workflow::report(nori::resource::error_code error)
{
    context_->set_status(std::string{nori::resource::to_string(error)});
}
} // namespace nori::editor
