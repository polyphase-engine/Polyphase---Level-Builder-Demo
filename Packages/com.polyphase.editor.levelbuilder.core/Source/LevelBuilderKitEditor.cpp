#include "LevelBuilderKitEditor.h"

#if EDITOR

#include "LBKitRegistry.h"
#include "LBKitTypes.h"
#include "LBKitJson.h"
#include "LevelBuilderRegistry.h"

#include "Plugins/EditorUIHooks.h"
#include "Plugins/PolyphaseEngineAPI.h"
#include "imgui.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    EditorUIHooks* sHooks = nullptr;
    const char* kWindowId   = "level_builder_kit_editor";
    const char* kWindowName = "Kit Editor";

    // Status row at the bottom of the window. Set by mutations; cleared
    // on next mutation. Color encodes severity (green = ok, amber =
    // warning, red = error).
    std::string sStatus;
    ImVec4      sStatusColor{0.7f, 0.7f, 0.7f, 1.0f};

    void SetStatusOk(const std::string& m)
    {
        sStatus = m;
        sStatusColor = ImVec4(0.30f, 0.85f, 0.40f, 1.0f);
    }
    void SetStatusWarn(const std::string& m)
    {
        sStatus = m;
        sStatusColor = ImVec4(1.00f, 0.70f, 0.20f, 1.0f);
    }
    void SetStatusErr(const std::string& m)
    {
        sStatus = m;
        sStatusColor = ImVec4(1.00f, 0.40f, 0.40f, 1.0f);
    }

    // Local edit buffer for the active piece — flushed to core on Apply
    // (size via Kit_SetPieceSize; name / asset / category / iconPath
    // via the R4 mutation surface).
    struct PieceEdit
    {
        std::string assetKey;     // identity — change → reseed
        std::string kitName;
        char        name[128]      = {0};
        char        assetName[128] = {0};
        char        category[64]   = {0};
        char        iconPath[256]  = {0};
        float       size[3]        = {1,1,1};
        bool        dirty          = false;
    };
    PieceEdit& Edit() { static PieceEdit e; return e; }

    void SeedEditFrom(const LBPieceInfo& pi, const std::string& kitName)
    {
        PieceEdit& e = Edit();
        e.assetKey = pi.assetName ? pi.assetName : "";
        e.kitName  = kitName;
        std::snprintf(e.name,      sizeof(e.name),      "%s", pi.name      ? pi.name      : "");
        std::snprintf(e.assetName, sizeof(e.assetName), "%s", pi.assetName ? pi.assetName : "");
        std::snprintf(e.category,  sizeof(e.category),  "%s", pi.category  ? pi.category  : "");
        std::snprintf(e.iconPath,  sizeof(e.iconPath),  "%s", pi.iconPath  ? pi.iconPath  : "");
        e.size[0] = pi.size[0]; e.size[1] = pi.size[1]; e.size[2] = pi.size[2];
        e.dirty = false;
    }

    // ---- Section: kit picker + new / delete ----
    void DrawKitPickerRow()
    {
        LBKitRegistry& reg = LBKitRegistry::Get();
        const std::string& active = reg.GetActiveKitName();
        const char* preview = active.empty() ? "<none>" : active.c_str();

        ImGui::SetNextItemWidth(220);
        if (ImGui::BeginCombo("##ke_kit_combo", preview))
        {
            for (int i = 0; i < reg.GetKitCount(); ++i)
            {
                const std::string& name = reg.GetKitNameAt(i);
                bool sel = (name == active);
                if (ImGui::Selectable(name.c_str(), sel))
                    reg.SetActiveKit(name);
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();

        static char sNewKitName[64] = {0};
        ImGui::SetNextItemWidth(180);
        ImGui::InputTextWithHint("##ke_new_name", "New kit name", sNewKitName, sizeof(sNewKitName));
        ImGui::SameLine();
        if (ImGui::Button("+ New Kit") && sNewKitName[0])
        {
            LBKit* k = reg.CreateKit(sNewKitName);
            if (k)
            {
                reg.SetActiveKit(sNewKitName);
                SetStatusOk(std::string("Created kit: ") + sNewKitName);
                sNewKitName[0] = 0;
            }
            else
            {
                SetStatusErr("CreateKit failed (name empty or already exists)");
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(active.empty());
        if (ImGui::Button("- Delete Kit"))
        {
            std::string name = active;
            reg.RemoveKit(name);
            SetStatusWarn(std::string("Deleted kit from registry: ") + name +
                          " (source file on disk is NOT touched).");
        }
        ImGui::EndDisabled();
    }

    // ---- Section: kit metadata ----
    void DrawKitMetaSection(int activeKitIdx)
    {
        if (activeKitIdx < 0) { ImGui::TextDisabled("No active kit."); return; }

        LBKit* kit = LBKitRegistry::Get().FindKitByIndex(activeKitIdx);
        if (!kit) { ImGui::TextDisabled("No active kit."); return; }

        // Pull current values directly from kit fields. Save via
        // SetKitName + SetMetaString. Apply-on-edit so the user sees
        // changes immediately in other UIs.
        char nameBuf[128];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", kit->name.c_str());
        ImGui::SetNextItemWidth(280);
        if (ImGui::InputText("Kit name", nameBuf, sizeof(nameBuf),
                             ImGuiInputTextFlags_EnterReturnsTrue))
        {
            if (nameBuf[0] && nameBuf != kit->name)
            {
                if (LBKitRegistry::Get().RenameKit(kit->name, nameBuf))
                    SetStatusOk(std::string("Renamed kit → ") + nameBuf);
                else
                    SetStatusErr("Rename failed (name empty or collision)");
            }
        }

        // Edit each metadata string field. EnterReturnsTrue commits on
        // Enter; otherwise the user can click Apply at the bottom.
        auto editField = [&](const char* label, std::string& field, LBKitMetaField id, int width = 280)
        {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "%s", field.c_str());
            ImGui::SetNextItemWidth((float)width);
            if (ImGui::InputText(label, buf, sizeof(buf),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                field = buf;
                SetStatusOk(std::string("Set ") + label);
                (void)id;   // kit->meta fields are public — no setter needed
            }
        };

        // Metadata fields are flat on LBKit (see LBKitTypes.h) — no
        // nested ".meta" struct.
        editField("Kit ID##ke_kid",       kit->kitId,                  LBKitMeta_KitId);
        editField("Version##ke_ver",      kit->kitVersion,             LBKitMeta_KitVersion);
        editField("Author##ke_author",    kit->author,                 LBKitMeta_Author);
        editField("Author URL##ke_aurl",  kit->authorUrl,              LBKitMeta_AuthorUrl);
        editField("License##ke_lic",      kit->license,                LBKitMeta_License);
        editField("License URL##ke_lurl", kit->licenseUrl,             LBKitMeta_LicenseUrl);
        editField("Homepage##ke_home",    kit->homepage,               LBKitMeta_Homepage);
        editField("Preview image##ke_pv", kit->previewImage,           LBKitMeta_PreviewImage);

        char descBuf[1024];
        std::snprintf(descBuf, sizeof(descBuf), "%s", kit->description.c_str());
        ImGui::SetNextItemWidth(420);
        if (ImGui::InputTextMultiline("Description##ke_desc", descBuf, sizeof(descBuf),
                                      ImVec2(420, 80)))
            kit->description = descBuf;
    }

    // ---- Section: pieces list + add/remove + per-piece editor ----
    void DrawPiecesSection(int activeKitIdx)
    {
        if (activeKitIdx < 0) { ImGui::TextDisabled("No active kit."); return; }

        LBKit* kit = LBKitRegistry::Get().FindKitByIndex(activeKitIdx);
        if (!kit) return;

        const int pieceCount = (int)kit->pieces.size();
        static int sSelected = -1;
        if (sSelected >= pieceCount) sSelected = pieceCount - 1;

        // ---- Pieces list ----
        ImGui::Text("Pieces (%d)", pieceCount);
        ImGui::SameLine();
        if (ImGui::SmallButton("+ Add"))
        {
            LBPiece p;
            p.name      = "NewPiece";
            p.assetName = "NewPiece";
            p.size[0] = p.size[1] = p.size[2] = 1.0f;
            kit->pieces.push_back(std::move(p));
            sSelected = (int)kit->pieces.size() - 1;
            SetStatusOk("Added piece (NewPiece)");
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(sSelected < 0 || sSelected >= pieceCount);
        if (ImGui::SmallButton("- Remove"))
        {
            std::string name = kit->pieces[sSelected].name;
            kit->pieces.erase(kit->pieces.begin() + sSelected);
            if (sSelected >= (int)kit->pieces.size()) sSelected = (int)kit->pieces.size() - 1;
            SetStatusWarn(std::string("Removed piece: ") + name);
        }
        ImGui::EndDisabled();

        ImGui::SetNextItemWidth(280);
        if (ImGui::BeginListBox("##ke_pieces_list", ImVec2(280, 180)))
        {
            for (int i = 0; i < pieceCount; ++i)
            {
                const auto& p = kit->pieces[i];
                char lbl[160];
                std::snprintf(lbl, sizeof(lbl), "%s (%s)",
                              p.name.c_str(), p.assetName.c_str());
                bool sel = (sSelected == i);
                if (ImGui::Selectable(lbl, sel))
                    sSelected = i;
            }
            ImGui::EndListBox();
        }

        // ---- Per-piece editor ----
        if (sSelected < 0 || sSelected >= pieceCount)
        {
            ImGui::TextDisabled("Select a piece to edit.");
            return;
        }
        const auto& pi_view = kit->pieces[sSelected];
        // Snapshot into edit buffer on selection change.
        PieceEdit& s = Edit();
        std::string assetKey = pi_view.assetName;
        if (s.assetKey != assetKey || s.kitName != kit->name)
        {
            LBPieceInfo info{};
            info.name      = pi_view.name.c_str();
            info.assetName = pi_view.assetName.c_str();
            info.category  = pi_view.category.c_str();
            info.iconPath  = pi_view.iconPath.c_str();
            info.size[0] = pi_view.size[0];
            info.size[1] = pi_view.size[1];
            info.size[2] = pi_view.size[2];
            SeedEditFrom(info, kit->name);
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Selected Piece");

        if (ImGui::InputText("Display name##ke_pn", s.name, sizeof(s.name)))  s.dirty = true;
        if (ImGui::InputText("Asset name##ke_pa",   s.assetName, sizeof(s.assetName))) s.dirty = true;
        if (ImGui::InputText("Category##ke_pc",     s.category, sizeof(s.category)))   s.dirty = true;
        if (ImGui::InputText("Icon (rel. to kit folder)##ke_pi",
                             s.iconPath, sizeof(s.iconPath)))                    s.dirty = true;
        if (ImGui::DragFloat3("Size (w,h,d)##ke_psz", s.size, 0.05f,
                              0.001f, 1000.0f, "%.3f"))                          s.dirty = true;

        ImGui::Spacing();
        ImGui::BeginDisabled(!s.dirty);
        if (ImGui::Button("Apply##ke_apply"))
        {
            // Push edits via the R4 mutation surface — keeps everything
            // consistent (registry by-name maps, palette mirrors, etc).
            LBPiece& p = kit->pieces[sSelected];
            p.name      = s.name;
            p.assetName = s.assetName;
            p.category  = s.category;
            p.iconPath  = s.iconPath;
            p.size[0] = s.size[0]; p.size[1] = s.size[1]; p.size[2] = s.size[2];
            s.dirty = false;
            s.assetKey = s.assetName;
            SetStatusOk("Applied piece edits (in-memory only — Save Kit to persist).");
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("Sockets: edit in the Modular brush → Piece Properties accordion.");
    }

    // ---- Window body ----
    void Draw(void* /*ud*/)
    {
        LBKitRegistry& reg = LBKitRegistry::Get();
        int activeKitIdx = reg.FindKitIndex(reg.GetActiveKitName());

        DrawKitPickerRow();
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Kit Metadata##ke_meta",
                                    ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawKitMetaSection(activeKitIdx);
        }

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Pieces##ke_pieces",
                                    ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawPiecesSection(activeKitIdx);
        }

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::BeginDisabled(activeKitIdx < 0);
        if (ImGui::Button("Save Kit", ImVec2(120, 0)))
        {
            LBKit* kit = reg.FindKitByIndex(activeKitIdx);
            if (kit && !kit->sourceFile.empty())
            {
                std::string err;
                if (LBKitJson::SaveToFile(kit->sourceFile, *kit, err))
                    SetStatusOk(std::string("Saved to ") + kit->sourceFile);
                else
                    SetStatusErr(std::string("Save failed: ") + err);
            }
            else
            {
                SetStatusErr("Kit has no source file (in-memory only). "
                             "Reload from disk first or assign a kit.json path.");
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Reload From Disk", ImVec2(160, 0)))
        {
            reg.ScanProjectKits();
            SetStatusOk("Reloaded kits from disk.");
        }

        if (!sStatus.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(sStatusColor, "%s", sStatus.c_str());
        }
    }

    void OnOpenMenu(void* /*ud*/)
    {
        if (!sHooks) return;
        if (sHooks->IsWindowOpen(kWindowId)) sHooks->CloseWindow(kWindowId);
        else                                 sHooks->OpenWindow(kWindowId);
    }
}

namespace LevelBuilderKitEditor
{
    void Register(EditorUIHooks* hooks, uint64_t hookId)
    {
        if (!hooks) return;
        sHooks = hooks;

        // Dockable window — separate from the main Level Builder window
        // so artists can dock the Kit Editor wherever fits their workflow
        // (often a wide bottom dock for the pieces list).
        hooks->RegisterWindow(hookId, kWindowName, kWindowId, &Draw, nullptr);

        if (hooks->AddAddonsMenuItem)
            hooks->AddAddonsMenuItem(hookId, "Level Builder/Open Kit Editor",
                                     &OnOpenMenu, nullptr);
    }

    void Unregister()
    {
        sHooks = nullptr;
    }
}

#endif // EDITOR
