///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Mediator.hpp"
#include "Renderer.hpp"
#include "imGuiUi/ImGuiUi.hpp"

class GuiRendererConcreteMediator : public IMediator {
public:
    GuiRendererConcreteMediator(Renderer& renderer, ImGuiUI& imguiUI)
        : renderer(renderer), imguiUI(imguiUI) {}
    void update();

    void notify(EventType event);

private:
    Renderer& renderer;
    ImGuiUI& imguiUI;

    //The idea here is to convert in frame X and export in frame X+1
    enum class BatchSubstate { Idle, Loading, Converting, Exporting };
    BatchSubstate batchSubstate = BatchSubstate::Idle;
    ImGuiUI::BatchItem* currentJob = nullptr;  // pointer returned by UI queue
    int framesSinceDispatch = 0;               // simple GPU """"barrier"""" between convert and export
    void startBatchJob(ImGuiUI::BatchItem* job, ImGuiUI& ui);
    void finishBatchJobSuccess(ImGuiUI& ui);
    void finishBatchJobFail(ImGuiUI& ui, const std::string& what);
};
