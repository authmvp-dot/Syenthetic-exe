#include "settings/functions.h"
#include "shader/blur.hpp"

void c_gui::render()
{

	gui->new_frame();
	{
		notify->setup_notify();

		ImVec2 menu_size = SCALE(set->c_window.window_size);
		gui->set_next_window_pos(ImVec2(0, 0));
		gui->set_next_window_size(menu_size);

		gui->begin({ "NAME" }, { 0 }, set->c_window.window_flags | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
		{
			const ImVec2 pos = GetWindowPos();
			const ImVec2 size = GetWindowSize();

			float start_tab_y = pos.y + SCALE(80);
			float tab_h = SCALE(62);
			float tab_spacing = SCALE(14);
			float tab_w = SCALE(76);
			float tab_x = pos.x + (SCALE(110) - tab_w) * 0.5f;

			// Native window dragging from ANY empty / transparent space or logo in the sidebar
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				ImVec2 mouse = ImGui::GetMousePos();
				if (mouse.x >= pos.x && mouse.x <= pos.x + SCALE(110) &&
				    mouse.y >= pos.y && mouse.y <= pos.y + size.y)
				{
					bool on_tab = false;
					for (int t = 0; t < 3; ++t)
					{
						float ty = start_tab_y + t * (tab_h + tab_spacing);
						if (mouse.x >= tab_x && mouse.x <= tab_x + tab_w &&
						    mouse.y >= ty && mouse.y <= ty + tab_h)
						{
							on_tab = true;
							break;
						}
					}

					if (!on_tab && g_hwnd)
					{
						ReleaseCapture();
						SendMessage(g_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
					}
				}
			}

			ImDrawList* draw_list = GetWindowDrawList();
			ImGuiStyle* style = &GetStyle();

			{
				style->WindowBorderSize = SCALE(set->c_window.border_size);
				style->WindowRounding = SCALE(set->c_window.rounding);
				style->WindowPadding = SCALE(set->c_window.padding);

				style->ScrollbarSize = SCALE(set->c_window.scrollbar_size);
				style->ItemSpacing = SCALE(set->c_window.item_spacing);
			}

			draw->add_rect_filled(draw_list, { pos.x, pos.y }, { pos.x + size.x, pos.y + size.y }, gui->get_clr(clr->c_window.general_layout), SCALE(set->c_window.general_rounding));
			draw->add_rect(draw_list, { pos.x, pos.y }, { pos.x + size.x, pos.y + size.y }, gui->get_clr(clr->c_window.general_stroke), SCALE(set->c_window.general_rounding));

			draw->add_rect_filled(draw_list, { pos.x + SCALE(110), pos.y + SCALE(15) }, { pos.x + (size.x - SCALE(15)), pos.y + (size.y - SCALE(15)) }, gui->get_clr(clr->c_window.layout), SCALE(set->c_window.rounding));
			draw->add_rect(draw_list, { pos.x + SCALE(110), pos.y + SCALE(15) }, { pos.x + (size.x - SCALE(15)), pos.y + (size.y - SCALE(15)) }, gui->get_clr(clr->c_window.stroke), SCALE(set->c_window.rounding));

			draw->rect_filled_multi_color(draw_list, { pos.x + size.x / 2, pos.y }, { pos.x + size.x, pos.y + 1 }, gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f));
			draw->rect_filled_multi_color(draw_list, { pos.x, pos.y }, { pos.x + size.x / 2, pos.y + 1 }, gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f));

			draw->rect_filled_multi_color(draw_list, { pos.x + size.x / 2, pos.y + size.y - 1 }, { pos.x + size.x, pos.y + size.y }, gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f));
			draw->rect_filled_multi_color(draw_list, { pos.x, pos.y + size.y - 1 }, { pos.x + size.x / 2, pos.y + size.y }, gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f));

			// 1. Top Logo in sidebar
			draw->render_text(draw_list, set->c_font.icon[2], { pos.x, pos.y + SCALE(10) }, { pos.x + SCALE(110), pos.y + SCALE(62) }, gui->get_clr(clr->c_other_clr.accent_clr), "B", 0, 0, { 0.5f, 0.5f });
			draw->rect_filled_multi_color(draw_list, { pos.x + SCALE(20), pos.y + SCALE(68) }, { pos.x + SCALE(90), pos.y + SCALE(70) },
				gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.5f),
				gui->get_clr(clr->c_other_clr.accent_clr, 0.5f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f));

			// 2. Vertical tabs stacked smoothly from top to bottom
			for (int i = 0; i < 3; i++)
			{
				float cur_y = start_tab_y + i * (tab_h + tab_spacing);

				ImRect tab_rect(ImVec2(tab_x, cur_y), ImVec2(tab_x + tab_w, cur_y + tab_h));
				ImGuiID tab_id = ImGui::GetID(("##sidebar_tab_" + std::to_string(i)).c_str());

				gui->set_cursor_pos(ImVec2(tab_x - pos.x, cur_y - pos.y));
				ItemSize(tab_rect, 0);
				if (!ItemAdd(tab_rect, tab_id))
					continue;

				bool hovered = false, held = false;
				bool pressed = ImGui::ButtonBehavior(tab_rect, tab_id, &hovered, &held);
				if (pressed)
				{
					var->c_selection.selection = i;
				}

				struct s_side_tab { float anim; float hover; };
				s_side_tab* st = gui->anim_container(&st, tab_id);
				bool is_active = (var->c_selection.selection == i);
				st->anim = ImLerp(st->anim, is_active ? 1.f : 0.f, gui->fixed_speed(14.f));
				st->hover = ImLerp(st->hover, hovered ? 1.f : 0.f, gui->fixed_speed(14.f));

				// Button background with smooth accent glow
				ImVec4 bg_col = ImLerp(clr->c_window.layout, clr->c_other_clr.accent_clr, st->anim * 0.22f);
				if (st->hover > 0.01f && !is_active)
					bg_col = ImLerp(bg_col, clr->c_element.layout, st->hover * 0.5f);
				draw->add_rect_filled(draw_list, tab_rect.Min, tab_rect.Max, gui->get_clr(bg_col), SCALE(8.f));

				// Button border
				ImVec4 border_col = ImLerp(clr->c_window.stroke, clr->c_other_clr.accent_clr, st->anim * 0.85f);
				if (st->hover > 0.01f && !is_active)
					border_col = ImLerp(border_col, clr->c_other_clr.accent_clr, st->hover * 0.4f);
				draw->add_rect(draw_list, tab_rect.Min, tab_rect.Max, gui->get_clr(border_col), SCALE(8.f));

				// Horizontal glowing indicator bar across the width (at bottom of tab)
				if (st->anim > 0.01f)
				{
					float bar_pad = SCALE(14.f) * (1.f - st->anim) + SCALE(10.f);
					draw->add_rect_filled(draw_list,
						{ tab_rect.Min.x + bar_pad, tab_rect.Max.y - SCALE(5.5f) },
						{ tab_rect.Max.x - bar_pad, tab_rect.Max.y - SCALE(2.5f) },
						gui->get_clr(clr->c_other_clr.accent_clr, st->anim), SCALE(1.5f));
				}

				// Tab icon
				ImVec4 inactive_icon = ImLerp(ImVec4(0.62f, 0.62f, 0.74f, 1.0f), ImVec4(1.f, 1.f, 1.f, 1.f), st->hover);
				ImVec4 icon_col = is_active ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImLerp(inactive_icon, ImVec4(1.f, 1.f, 1.f, 1.f), st->anim);
				draw->render_text(draw_list, set->c_font.icon[1], tab_rect.Min, tab_rect.Max - ImVec2(0, SCALE(2.f)), gui->get_clr(icon_col), var->c_selection.selection_icon[i].c_str(), 0, 0, { 0.5f, 0.48f });
			}

			gui->set_cursor_pos(SCALE(115, 15));

			float anim_dt = ImClamp(ImGui::GetIO().DeltaTime, 0.001f, 0.05f);
			var->c_selection.selection_alpha = ImClamp(var->c_selection.selection_alpha + (10.f * anim_dt * (var->c_selection.selection == var->c_selection.selection_active ? 1.f : -1.f)), 0.f, 1.f);
			if (var->c_selection.selection_alpha <= 0.05f) var->c_selection.selection_active = var->c_selection.selection;

			gui->push_style_var(ImGuiStyleVar_Alpha, var->c_selection.selection_alpha * style->Alpha);

			gui->begin_content("content", ImVec2(size.x - SCALE(130), size.y - SCALE(30)), { 15, 15 }, { 15, 15 });
			{
				if (var->c_selection.selection_active == 0) // Ragebot
				{
					gui->begin_group();
					{
						gui->begin_child("ragebot");
						{
							if (widget->checkbox_with_key("Enable ragebot", &var->c_ragebot.ragebot, &var->c_ragebot.rage_key, &var->c_ragebot.rage_holding, &var->c_ragebot.rage_value, &var->c_ragebot.rage_show_binds))
							{
								notify->add_notify("You have successfully summoned a notification!", 4, static_cast<notify_position>(var->c_notify.notify_position));
							};

							widget->separator();

							widget->checkbox("Silent aimbot", &var->c_ragebot.silent_aimbot);

							widget->separator();

							widget->checkbox("Hit chance", &var->c_ragebot.hit_chance);

							widget->separator();

							widget->slider_int("Field of view", &var->c_ragebot.fov, -180, 180, 1, "%d\xC2\xB0");
						}
						gui->end_child();

						gui->begin_child("other");
						{
							widget->dropdown("Body aimbot", &var->c_other.body_selection, var->c_other.body_list, var->c_other.body_list.size());

							widget->separator();

							widget->dropdown("Safe points", &var->c_other.points_selection, var->c_other.points_list, var->c_other.points_list.size());
						}
						gui->end_child();

						gui->begin_child("recoil");
						{
							widget->checkbox_with_key("Enable recoil", &var->c_recoil.enable_recoil, &var->c_recoil.recoil_key, &var->c_recoil.recoil_holding, &var->c_recoil.recoil_value, &var->c_recoil.recoil_show_binds);

							widget->separator();

							widget->slider_int("Smoothness", &var->c_recoil.smoothness, 0, 100, 1, "%d%%");
						}
						gui->end_child();
					}
					gui->end_group();

					gui->sameline();

					gui->begin_group();
					{
						gui->begin_child("move");
						{
							widget->slider_int("Hit chance", &var->c_move.hit_chance, 0, 100, 1, "%d%%");

							widget->separator();

							widget->slider_int("Max misses", &var->c_move.max_misses, 0, 100, 1, "%d%%");

							widget->separator();

							widget->checkbox_with_key("Static point scale", &var->c_move.point_scale, &var->c_move.point_key, &var->c_move.point_holding, &var->c_move.point_value, &var->c_move.point_show_binds);

							widget->separator();

							widget->checkbox("Head safety if lethal", &var->c_move.head_safety);
						}
						gui->end_child();

						gui->begin_child("trigger");
						{
							widget->checkbox_with_key("Enable triggerbot", &var->c_trigger.enable_trigger, &var->c_trigger.trigger_key, &var->c_trigger.trigger_holding, &var->c_trigger.trigger_value, &var->c_trigger.trigger_show_binds);

							widget->separator();

							widget->checkbox("Enable trigger in smoke", &var->c_trigger.trigger_in_smoke);

							widget->separator();

							widget->button("Button", { GetContentRegionAvail().x, SCALE(35) });
						}
						gui->end_child();

						gui->begin_child("settings");
						{
							widget->slider_float("Pitch", &var->c_settings.pitch, 0.f, 1.f, 0.1f, "%.3f");

							widget->separator();

							widget->slider_float("Yaw", &var->c_settings.yaw, 0.f, 1.f, 0.1f, "%.3f");
						}
						gui->end_child();
					}
					gui->end_group();
				}
				else if (var->c_selection.selection_active == 1) // Visuals
				{
					gui->begin_group();
					{
						gui->begin_child("esp");
						{
							widget->checkbox_with_key("Enable ESP", &var->c_esp.esp, &var->c_esp.esp_key, &var->c_esp.esp_holding, &var->c_esp.esp_value, &var->c_esp.esp_show_binds);

							widget->separator();

							widget->checkbox("Through walls", &var->c_esp.through_walls);

							widget->separator();

							widget->dropdown("Dynamic tracer", &var->c_esp.tracers_selection, var->c_esp.tracers_list, var->c_esp.tracers_list.size());

							widget->separator();

							widget->checkbox("Dynamic boxes", &var->c_esp.dynamic_boxes);

							widget->separator();

							widget->checkbox_with_color("In-Game radar", &var->c_esp.ingame_radar, var->c_esp.inagame_color, true);
						}
						gui->end_child();

						gui->begin_child("glow");
						{
							widget->checkbox_with_key("Enable glow", &var->c_glow.glow, &var->c_glow.glow_key, &var->c_glow.glow_holding, &var->c_glow.glow_value, &var->c_glow.glow_show_binds);

							widget->separator();

							widget->slider_int("The power of brightness", &var->c_glow.power, 0, 100, 1, "%d%%");
						}
						gui->end_child();

						gui->begin_child("attachments");
						{
							widget->checkbox_with_color("Attachments", &var->c_attachments.attachments, var->c_attachments.attachments_color, true);

							widget->separator();

							widget->checkbox_with_key("Visible teammates", &var->c_attachments.teammates, &var->c_attachments.teammates_key, &var->c_attachments.teammates_holding, &var->c_attachments.teammates_value, &var->c_attachments.teammates_show_binds);
						}
						gui->end_child();
					}
					gui->end_group();

					gui->sameline();

					gui->begin_group();
					{
						gui->begin_child("chams");
						{
							widget->checkbox_with_key("Enable chams", &var->c_chams.chams, &var->c_chams.chams_key, &var->c_chams.chams_holding, &var->c_chams.chams_value, &var->c_chams.chams_show_binds);

							widget->separator();

							widget->checkbox_with_color("Backtrack", &var->c_chams.backtrack, var->c_chams.backtrack_color, true);

							widget->separator();

							widget->checkbox_with_color("On shot", &var->c_chams.onshot, var->c_chams.onshot_color, true);

							widget->separator();

							widget->checkbox_with_color("Ragdolls", &var->c_chams.ragdolls, var->c_chams.ragdolls_color, true);
						}
						gui->end_child();

						gui->begin_child("skeleton");
						{
							widget->checkbox("Skeleton", &var->c_skeleton.skeleton);

							widget->separator();

							widget->dropdown("Snaplines", &var->c_skeleton.snaplines_selection, var->c_skeleton.snaplines_list, var->c_skeleton.snaplines_list.size());
						
							widget->separator();

							widget->checkbox_with_color("Weapon", &var->c_skeleton.weapon, var->c_skeleton.weapon_color, true);

							widget->separator();

							widget->checkbox_with_color("Nickname", &var->c_skeleton.nickname, var->c_skeleton.nickname_color, true);
						}
						gui->end_child();
					}
					gui->end_group();
				}
				else if (var->c_selection.selection_active == 2) // Settings
				{
					gui->begin_group();
					{
						gui->begin_child("theme");
						{
							static bool theme_color_enabled = true;
							widget->checkbox_with_color("UI Theme Color", &theme_color_enabled, (float*)&clr->c_other_clr.accent_clr, false);

							widget->separator();

							widget->slider_int("UI Brightness", &var->c_glow.power, 0, 100, 1, "%d%%");

							widget->separator();

							static char buf[128] = "Default User";
							widget->text_field("Custom Tag", "M", buf, 128, { GetContentRegionAvail().x, SCALE(35) });
						}
						gui->end_child();
					}
					gui->end_group();

					gui->sameline();

					gui->begin_group();
					{
						gui->begin_child("dpi");
						{
							widget->slider_int("DPI", &var->c_dpi.dpi_saved, 100, 200, 1, "%d%%");

							if (var->c_dpi.dpi != var->c_dpi.dpi_saved / 100.f && IsMouseReleased(ImGuiMouseButton_Left)) {
								notify->add_notify("You have successfully set the menu size", 4, static_cast<notify_position>(var->c_notify.notify_position));
								var->c_dpi.dpi_changed = true;
							}

							widget->separator();

							const float width = GetContentRegionAvail().x;

							widget->button("Reset Color", { (width - style->ItemSpacing.x) / 2, SCALE(35) });
							if (ImGui::IsItemClicked())
							{
								clr->c_other_clr.accent_clr = ImColor(142, 132, 255, 255);
							}

							gui->sameline();

							widget->button("Max Bright", { (width - style->ItemSpacing.x) / 2, SCALE(35) });
							if (ImGui::IsItemClicked())
							{
								var->c_glow.power = 100;
							}
						}
						gui->end_child();
					}
					gui->end_group();
				}
			}
			gui->end_content();

			gui->pop_style_var(1);

		}
		gui->end();

	}
	gui->end_frame();

}