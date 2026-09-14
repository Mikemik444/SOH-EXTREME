#include "SohMenu.h"
#include <soh/Notification/Notification.h>
#include "SohGui.hpp"
#include "soh/OTRGlobals.h"
#include "soh/util.h"
#include <soh/Network/Sail/Sail.h>
#include <soh/Network/Archipelago/ArchipelagoClient.h>
#include <soh/Enhancements/randomizer/randomizerEnums/RandomizerInf.h>
#include <soh/Network/CrowdControl/CrowdControl.h>

extern "C" {
extern s32 Flags_GetRandomizerInf(RandomizerInf flag);
}

namespace SohGui {

extern std::shared_ptr<SohMenu> mSohMenu;
using namespace UIWidgets;

void SohMenu::AddMenuNetwork() {
    // Add Network Menu
    AddMenuEntry("Network", CVAR_SETTING("Menu.NetworkSidebarSection"));
    WidgetPath path;

    // Archipelago / SOH-EXTREME
    path = { "Network", "Archipelago", SECTION_COLUMN_1 };
    AddSidebarEntry("Network", path.sidebarName, 3);
    AddWidget(path,
              "Connect this SOH-EXTREME build directly to an Archipelago server. "
              "Use the SOH-EXTREME APWorld on the server so custom Souls, Shovel, "
              "Flow of Time and Song Notes use matching IDs.",
              WIDGET_TEXT);
    AddWidget(path, "Server", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        ImGui::BeginDisabled(ArchipelagoClient::GetInstance().IsEnabled());
        CVarInputString("##APServer", CVAR_REMOTE_ARCHIPELAGO("ServerAddress"),
                        InputOptions().Color(THEME_COLOR).PlaceholderText("archipelago.gg:38281")
                            .DefaultValue("archipelago.gg:38281").Size(ImVec2(ImGui::GetFontSize() * 24, 0))
                            .LabelPosition(LabelPositions::None));
        ImGui::EndDisabled();
    });
    AddWidget(path, "Slot Name", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        ImGui::BeginDisabled(ArchipelagoClient::GetInstance().IsEnabled());
        CVarInputString("##APSlot", CVAR_REMOTE_ARCHIPELAGO("SlotName"),
                        InputOptions().Color(THEME_COLOR).PlaceholderText("Player1")
                            .DefaultValue("").Size(ImVec2(ImGui::GetFontSize() * 24, 0))
                            .LabelPosition(LabelPositions::None));
        ImGui::EndDisabled();
    });
    AddWidget(path, "Password", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        ImGui::BeginDisabled(ArchipelagoClient::GetInstance().IsEnabled());
        CVarInputString("##APPassword", CVAR_REMOTE_ARCHIPELAGO("Password"),
                        InputOptions().Color(THEME_COLOR).PlaceholderText("optional")
                            .DefaultValue("").Size(ImVec2(ImGui::GetFontSize() * 24, 0))
                            .LabelPosition(LabelPositions::None));
        ImGui::EndDisabled();
    });
    AddWidget(path, "Connect##Archipelago", WIDGET_BUTTON)
        .PreFunc([](WidgetInfo& info) {
            auto& ap = ArchipelagoClient::GetInstance();
            std::string slot = CVarGetString(CVAR_REMOTE_ARCHIPELAGO("SlotName"), "");

            if (ap.IsAuthenticated()) {
                info.name = "Disconnect";
            } else if (ap.IsConnectionRefused()) {
                // One click retries a refused connection instead of forcing the user to
                // click Disconnect and then Connect again.
                info.name = "Retry Connection";
            } else if (ap.IsEnabled()) {
                info.name = "Cancel Connection";
            } else {
                info.name = "Connect";
            }
            info.options->disabled = !ap.IsEnabled() && slot.empty();
        })
        .Callback([](WidgetInfo& info) {
            auto& ap = ArchipelagoClient::GetInstance();

            if (ap.IsConnectionRefused()) {
                ap.Disable();
                ap.Enable();
            } else {
                ap.Toggle();
            }

            if (ap.IsEnabled()) {
                CVarSetInteger(CVAR_REMOTE_ARCHIPELAGO("Enabled"), 1);
            } else {
                CVarClear(CVAR_REMOTE_ARCHIPELAGO("Enabled"));
            }
            Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        });
    AddWidget(path, "Status##Archipelago", WIDGET_TEXT).PreFunc([](WidgetInfo& info) {
        // Do not append an ImGui ##ID here: WIDGET_TEXT renders the string literally,
        // which is why the old menu visibly showed "##Archipelago".
        info.name = "Status: " + ArchipelagoClient::GetInstance().GetStatusText();
    });

    AddWidget(path, "Collected Song Notes", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "##ArchipelagoSongNotes", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        struct SongRow { const char* name; int count; };
        static constexpr SongRow songs[] = {
            {"Zelda's Lullaby", 6}, {"Epona's Song", 6}, {"Saria's Song", 6}, {"Sun's Song", 6},
            {"Song of Time", 6}, {"Song of Storms", 6}, {"Minuet of Forest", 6}, {"Bolero of Fire", 8},
            {"Serenade of Water", 5}, {"Requiem of Spirit", 6}, {"Nocturne of Shadow", 7}, {"Prelude of Light", 6},
        };

        int offset = 0;
        int total = 0;
        for (const auto& song : songs) {
            int have = 0;
            for (int i = 0; i < song.count; ++i) {
                if (Flags_GetRandomizerInf(static_cast<RandomizerInf>(
                        static_cast<int>(RAND_INF_SONG_NOTE_0) + offset + i))) {
                    ++have;
                    ++total;
                }
            }
            ImGui::Text("%s: %d/%d", song.name, have, song.count);
            ImGui::SameLine();
            for (int i = 0; i < song.count; ++i) {
                const int note = offset + i;
                const bool owned = Flags_GetRandomizerInf(static_cast<RandomizerInf>(
                    static_cast<int>(RAND_INF_SONG_NOTE_0) + note));
                ImGui::SameLine();
                if (owned) ImGui::Text("[X%02d]", note + 1);
                else ImGui::TextDisabled("[ %02d]", note + 1);
            }
            offset += song.count;
        }
        ImGui::Text("Total: %d / 74", total);
    });

    AddWidget(path, "Archipelago Chat", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "##ArchipelagoChat", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        auto& ap = ArchipelagoClient::GetInstance();
        const auto messages = ap.GetChatMessages();

        ImGui::BeginChild("##APChatLog", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 10.0f),
                          ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& line : messages) {
            ImGui::TextWrapped("%s", line.c_str());
        }
        if (!messages.empty() && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        static char chatInput[512] = {};
        const bool canChat = ap.IsAuthenticated() && ap.IsGameplaySessionActive();
        ImGui::BeginDisabled(!canChat);
        ImGui::SetNextItemWidth(-ImGui::GetFrameHeightWithSpacing() * 2.5f);
        bool send = ImGui::InputText("##APChatInput", chatInput, sizeof(chatInput), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        send |= ImGui::Button("Send##APChat");
        if (send && chatInput[0] != '\0') {
            ap.SendChatMessage(chatInput);
            chatInput[0] = '\0';
            ImGui::SetKeyboardFocusHere(-1);
        }
        ImGui::EndDisabled();
        if (!canChat) {
            ImGui::TextDisabled("Chat becomes active after an AP randomizer save is started or loaded.");
        }
    });

    // Sail
    path = { "Network", "Sail", SECTION_COLUMN_1 };
    AddSidebarEntry("Network", path.sidebarName, 3);

    AddWidget(path,
              "Sail is a networking protocol designed to facilitate remote "
              "control of the Ship of Harkinian client. It is intended to "
              "be utilized alongside a Sail server, for which we provide a "
              "few straightforward implementations on our GitHub. The current "
              "implementations available allow integration with Twitch chat "
              "and SAMMI Bot, feel free to contribute your own!\n"
              "\n"
              "Click this button to copy the link to the Sail Github "
              "page to your clipboard.",
              WIDGET_TEXT);
    AddWidget(path, ICON_FA_CLIPBOARD "##Sail", WIDGET_BUTTON)
        .Callback([](WidgetInfo& info) {
            ImGui::SetClipboardText("https://github.com/HarbourMasters/sail");
            Notification::Emit({
                .message = "Copied to clipboard",
            });
        })
        .Options(ButtonOptions().Tooltip("https://github.com/HarbourMasters/sail"));
    AddWidget(path, "Host & Port", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        ImGui::BeginDisabled(Sail::Instance->isEnabled || CVarGetInteger(CVAR_SETTING("DisableChanges"), 0));
        ImGui::Text("%s", info.name.c_str());
        CVarInputString("##HostSail", CVAR_REMOTE_SAIL("Host"),
                        InputOptions()
                            .Color(THEME_COLOR)
                            .PlaceholderText("127.0.0.1")
                            .DefaultValue("127.0.0.1")
                            .Size(ImVec2(ImGui::GetFontSize() * 15, 0))
                            .LabelPosition(LabelPositions::None));
        ImGui::SameLine();
        ImGui::Text(":");
        ImGui::SameLine();
        CVarInputInt("##PortSail", CVAR_REMOTE_SAIL("Port"),
                     InputOptions()
                         .Color(THEME_COLOR)
                         .PlaceholderText("43384")
                         .DefaultValue("43384")
                         .Size(ImVec2(ImGui::GetFontSize() * 5, 0))
                         .LabelPosition(LabelPositions::None));
        ImGui::EndDisabled();
    });
    AddWidget(path, "Enable##Sail", WIDGET_BUTTON)
        .PreFunc([](WidgetInfo& info) {
            std::string host = CVarGetString(CVAR_REMOTE_SAIL("Host"), "127.0.0.1");
            uint16_t port = CVarGetInteger(CVAR_REMOTE_SAIL("Port"), 43384);
            info.options->disabled = !(!SohUtils::IsStringEmpty(host) && port > 1024 && port < 65535);
            if (Sail::Instance->isEnabled) {
                info.name = "Disable##Sail";
            } else {
                info.name = "Enable##Sail";
            }
        })
        .Callback([](WidgetInfo& info) {
            if (Sail::Instance->isEnabled) {
                CVarClear(CVAR_REMOTE_SAIL("Enabled"));
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                Sail::Instance->Disable();
            } else {
                CVarSetInteger(CVAR_REMOTE_SAIL("Enabled"), 1);
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                Sail::Instance->Enable();
            }
        });
    AddWidget(path, "Connecting...##Sail", WIDGET_TEXT).PreFunc([](WidgetInfo& info) {
        info.isHidden = !Sail::Instance->isEnabled;
        if (Sail::Instance->isConnected) {
            info.name = "Connected##Sail";
        } else {
            info.name = "Connecting...##Sail";
        }
    });

    path.sidebarName = "Crowd Control";
    AddSidebarEntry("Network", path.sidebarName, 3);
    path.column = SECTION_COLUMN_1;

    AddWidget(path, "About Crowd Control", WIDGET_SEPARATOR_TEXT);
    AddWidget(path,
              "Crowd Control is a platform that allows viewers to interact "
              "with a streamer's game in real time.\n"
              "\n"
              "Please head over to www.crowdcontrol.live for more information!",
              WIDGET_TEXT);

    AddWidget(path, "Connect to Crowd Control", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Host & Port", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        ImGui::BeginDisabled(CrowdControl::Instance->isEnabled || CVarGetInteger(CVAR_SETTING("DisableChanges"), 0));
        ImGui::Text("%s", info.name.c_str());
        CVarInputString("##HostCrowdControl", CVAR_REMOTE_CROWD_CONTROL("Host"),
                        InputOptions()
                            .Color(THEME_COLOR)
                            .PlaceholderText("127.0.0.1")
                            .DefaultValue("127.0.0.1")
                            .Size(ImVec2(ImGui::GetFontSize() * 15, 0))
                            .LabelPosition(LabelPositions::None));
        ImGui::SameLine();
        ImGui::Text(":");
        ImGui::SameLine();
        CVarInputInt("##PortCrowdControl", CVAR_REMOTE_CROWD_CONTROL("Port"),
                     InputOptions()
                         .Color(THEME_COLOR)
                         .PlaceholderText("43384")
                         .DefaultValue("43384")
                         .Size(ImVec2(ImGui::GetFontSize() * 5, 0))
                         .LabelPosition(LabelPositions::None));
        ImGui::EndDisabled();
    });
    AddWidget(path, "Enable##CrowdControl", WIDGET_BUTTON)
        .PreFunc([](WidgetInfo& info) {
            std::string host = CVarGetString(CVAR_REMOTE_CROWD_CONTROL("Host"), "127.0.0.1");
            uint16_t port = CVarGetInteger(CVAR_REMOTE_CROWD_CONTROL("Port"), 43384);
            info.options->disabled = !(!SohUtils::IsStringEmpty(host) && port > 1024 && port < 65535);
            if (CrowdControl::Instance->isEnabled) {
                info.name = "Disable##CrowdControl";
            } else {
                info.name = "Enable##CrowdControl";
            }
        })
        .Callback([](WidgetInfo& info) {
            if (CrowdControl::Instance->isEnabled) {
                CVarClear(CVAR_REMOTE_CROWD_CONTROL("Enabled"));
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                CrowdControl::Instance->Disable();
            } else {
                CVarSetInteger(CVAR_REMOTE_CROWD_CONTROL("Enabled"), 1);
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                CrowdControl::Instance->Enable();
            }
        });
    AddWidget(path, "Connecting...", WIDGET_TEXT).PreFunc([](WidgetInfo& info) {
        info.isHidden = !CrowdControl::Instance->isEnabled;
        if (CrowdControl::Instance->isConnected) {
            info.name = "Connected";
        } else {
            info.name = "Connecting...";
        }
    });
    AddWidget(path, "Additional Settings", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Enemy Name Tags", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_REMOTE_CROWD_CONTROL("EnemyNameTags"))
        .RaceDisable(true)
        .Options(CheckboxOptions().Tooltip(
            "When viewers spawn enemies, the enemy will have a name tag above them with the viewer's name."));
    AddWidget(path, "Spawned Enemies Ignored Ingame", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_REMOTE_CROWD_CONTROL("SpawnedEnemiesIgnoredIngame"))
        .RaceDisable(true)
        .Options(CheckboxOptions().Tooltip("Enemies spawned by CrowdControl won't be considered for \"clear enemy "
                                           "rooms\", so they don't need to be killed to complete these rooms."));
    path.sidebarName = "Anchor";
    AddSidebarEntry("Network", path.sidebarName, 2);
}

} // namespace SohGui
