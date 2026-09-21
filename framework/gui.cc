#include "settings/functions.h"
#include "shader/blur.hpp"
#include "esp/esp_globals.h"
#include "esp/esp_data.h"

void c_gui::render()
{

	gui->new_frame();
	{
		// Sync UI settings into runtime ESP globals
		g_Globals.Visuals.Enable = var->c_esp.esp;
		g_Globals.EspConfig.AutoRefresh = var->c_settings.connect_lib || var->c_esp.auto_refresh;
		g_Globals.Visuals.Box = (var->c_esp.box_selection >= 0);
		g_Globals.Visuals.players_box = var->c_esp.box_selection;
		g_Globals.Visuals.Skeleton = var->c_skeleton.skeleton;
		g_Globals.Visuals.Lines = (var->c_skeleton.snaplines_selection >= 0);
		g_Globals.Visuals.EspLines = var->c_skeleton.snaplines_selection;
		g_Globals.Visuals.HealthBar = var->c_esp.healthbar;
		g_Globals.Visuals.players_healthbar = var->c_esp.healthbar_selection;
		g_Globals.Visuals.HeadDot = var->c_esp.headdot;
		g_Globals.Visuals.ESPWeapon = var->c_skeleton.weapon;
		g_Globals.Visuals.Name = var->c_skeleton.nickname;
		g_Globals.Visuals.Distance = var->c_esp.distance;
		g_Globals.Visuals.DistanceEsp = var->c_esp.max_distance;
		g_Globals.Visuals.Radar = var->c_esp.ingame_radar;

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

			// Native window dragging from ANY empty / transparent space or logo in the sidebar OR the top header
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || 
			   (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive() && ImGui::GetIO().MouseDownDuration[0] < 0.15f))
			{
				ImVec2 mouse = ImGui::GetMousePos();
				bool in_header = (mouse.y >= pos.y && mouse.y <= pos.y + SCALE(75.0f) && mouse.x >= pos.x && mouse.x <= pos.x + size.x);
				bool in_sidebar = (mouse.x >= pos.x && mouse.x <= pos.x + SCALE(110.0f) && mouse.y >= pos.y && mouse.y <= pos.y + size.y);

				if (in_header || in_sidebar)
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

						// CRITICAL: Reset ImGui mouse state immediately!
						// When Windows finishes the modal drag loop, ImGui missed WM_LBUTTONUP.
						// Resetting these fields prevents the "requires 2 clicks to drag again" issue.
						ImGuiIO& io = ImGui::GetIO();
						io.ClearInputKeys();
						io.MouseDown[0] = false;
						io.MouseClicked[0] = false;
						io.MouseDoubleClicked[0] = false;
						io.MouseDownDuration[0] = -1.0f;
						io.MouseDownDurationPrev[0] = -1.0f;
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

			float content_top = pos.y + SCALE(75.0f);
			float content_bottom = pos.y + (size.y - SCALE(15.0f));
			float content_left = pos.x + SCALE(110.0f);
			float content_right = pos.x + (size.x - SCALE(15.0f));

			// Main black panel pushed down to start at tab level, leaving top header fully transparent
			draw->add_rect_filled(draw_list, { content_left, content_top }, { content_right, content_bottom }, gui->get_clr(clr->c_window.layout), SCALE(set->c_window.rounding));
			draw->add_rect(draw_list, { content_left, content_top }, { content_right, content_bottom }, gui->get_clr(clr->c_window.stroke), SCALE(set->c_window.rounding));

			draw->rect_filled_multi_color(draw_list, { content_left + (content_right - content_left) * 0.5f, content_top }, { content_right, content_top + 1 }, gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f));
			draw->rect_filled_multi_color(draw_list, { content_left, content_top }, { content_left + (content_right - content_left) * 0.5f, content_top + 1 }, gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f));

			draw->rect_filled_multi_color(draw_list, { pos.x + size.x / 2, pos.y + size.y - 1 }, { pos.x + size.x, pos.y + size.y }, gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f));
			draw->rect_filled_multi_color(draw_list, { pos.x, pos.y + size.y - 1 }, { pos.x + size.x / 2, pos.y + size.y }, gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.2f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f));

			// 1. Top Logo in sidebar
			draw->render_text(draw_list, set->c_font.icon[2], { pos.x, pos.y + SCALE(10) }, { pos.x + SCALE(110), pos.y + SCALE(62) }, gui->get_clr(clr->c_other_clr.accent_clr), "B", 0, 0, { 0.5f, 0.5f });
			draw->rect_filled_multi_color(draw_list, { pos.x + SCALE(20), pos.y + SCALE(68) }, { pos.x + SCALE(90), pos.y + SCALE(70) },
				gui->get_clr(clr->c_other_clr.accent_clr, 0.f), gui->get_clr(clr->c_other_clr.accent_clr, 0.5f),
				gui->get_clr(clr->c_other_clr.accent_clr, 0.5f), gui->get_clr(clr->c_other_clr.accent_clr, 0.f));

			// 2. Top Middle Panel Title: Modern 3D Revolving Rotary Wave & Dual 360° Gyro Emblems
			{
				float title_center_x = (content_left + content_right) * 0.5f;
				float title_center_y = pos.y + SCALE(36.0f);

				float time = (float)ImGui::GetTime();
				ImVec4 acc = clr->c_other_clr.accent_clr;

				const char* full_title = "Mvp Cheats Aimkill";
				const int total_chars = 18;
				const int split_idx = 11; // Index where "Aimkill" begins

				float total_w = set->c_font.name->CalcTextSizeA(set->c_font.name->FontSize, FLT_MAX, -1, full_title).x;
				float start_x = title_center_x - total_w * 0.5f;
				float font_h = set->c_font.name->FontSize;
				float base_y = title_center_y - font_h * 0.5f;

				// --- Dual 360° Rotating Gyro Diamond Emblems on Left & Right ---
				auto draw_rotating_emblem = [&](ImVec2 center, float angle, ImVec4 emblem_col)
				{
					float r = SCALE(6.0f);
					float cos_a = cosf(angle);
					float sin_a = sinf(angle);

					ImVec2 p[4] = {
						{ center.x + r * cos_a, center.y + r * sin_a },
						{ center.x - r * sin_a, center.y + r * cos_a },
						{ center.x - r * cos_a, center.y - r * sin_a },
						{ center.x + r * sin_a, center.y - r * cos_a }
					};

					for (int k = 0; k < 4; ++k)
					{
						draw_list->AddLine(p[k], p[(k + 1) % 4], gui->get_clr(emblem_col, 0.85f), 1.4f);
					}

					// Inner revolving accent cross
					float r_in = r * 0.5f;
					draw_list->AddLine({ center.x - r_in * cos_a, center.y - r_in * sin_a },
					                   { center.x + r_in * cos_a, center.y + r_in * sin_a },
					                   gui->get_clr(clr->c_other_clr.white_clr, 0.9f), 1.2f);

					draw_list->AddCircleFilled(center, SCALE(1.4f), gui->get_clr(emblem_col, 1.0f));
				};

				float rot_speed = time * 2.2f;
				draw_rotating_emblem({ start_x - SCALE(20.0f), title_center_y }, rot_speed, acc);
				draw_rotating_emblem({ start_x + total_w + SCALE(20.0f), title_center_y }, -rot_speed, acc);

				// --- Letter-by-Letter 3D Revolving Rotary Wave ---
				float cur_x = start_x;
				for (int i = 0; i < total_chars; ++i)
				{
					char ch_str[2] = { full_title[i], '\0' };
					float ch_w = set->c_font.name->CalcTextSizeA(font_h, FLT_MAX, -1, ch_str).x;

					if (full_title[i] == ' ')
					{
						cur_x += ch_w;
						continue;
					}

					// Staggered rotary cylinder angle
					float angle = time * 3.2f - (float)i * 0.28f;
					float rot_y = sinf(angle) * SCALE(3.5f);
					float depth = cosf(angle); // 3D depth lighting factor (-1 to +1)

					ImVec4 char_col;
					if (i < split_idx)
					{
						// "Mvp Cheats" - Metallic Silver with dynamic rotary depth lighting
						float bright = 0.78f + (depth * 0.5f + 0.5f) * 0.22f;
						char_col = ImVec4(bright * 0.92f, bright * 0.94f, bright, 1.0f);
					}
					else
					{
						// "Aimkill" - Glowing accent with smooth cylindrical highlight
						float bright = 0.75f + (depth * 0.5f + 0.5f) * 0.35f;
						char_col = ImVec4(ImMin(acc.x * bright, 1.0f), ImMin(acc.y * bright, 1.0f), ImMin(acc.z * bright, 1.0f), 1.0f);
					}

					// Crisp, razor-sharp rendering with zero blurry ghosting
					draw_list->AddText(set->c_font.name, font_h, { cur_x, base_y + rot_y }, gui->get_clr(char_col), ch_str);
					cur_x += ch_w;
				}

				// --- Linear Laser Light Sweep Beneath Title ---
				float beam_w = (total_w + SCALE(40.0f));
				float beam_y = base_y + font_h + SCALE(7.0f);
				float half_beam = beam_w * 0.5f;

				// Subtle base guide track line
				draw->rect_filled_multi_color(draw_list,
					{ title_center_x - half_beam, beam_y },
					{ title_center_x, beam_y + SCALE(1.0f) },
					gui->get_clr(acc, 0.0f), gui->get_clr(acc, 0.35f),
					gui->get_clr(acc, 0.35f), gui->get_clr(acc, 0.0f));

				draw->rect_filled_multi_color(draw_list,
					{ title_center_x, beam_y },
					{ title_center_x + half_beam, beam_y + SCALE(1.0f) },
					gui->get_clr(acc, 0.35f), gui->get_clr(acc, 0.0f),
					gui->get_clr(acc, 0.0f), gui->get_clr(acc, 0.35f));

				// High-speed specular traveling light sweep
				float sweep_phase = fmodf(time * 0.65f, 1.0f);
				float spark_x = (title_center_x - half_beam) + sweep_phase * (beam_w);
				draw->add_rect_filled(draw_list,
					{ spark_x - SCALE(10.0f), beam_y - SCALE(0.5f) },
					{ spark_x + SCALE(10.0f), beam_y + SCALE(1.5f) },
					gui->get_clr(acc, 0.70f), SCALE(1.0f));
				draw->add_rect_filled(draw_list,
					{ spark_x - SCALE(3.0f), beam_y - SCALE(1.0f) },
					{ spark_x + SCALE(3.0f), beam_y + SCALE(2.0f) },
					gui->get_clr(clr->c_other_clr.white_clr, 0.95f), SCALE(1.0f));
			}

			// 3. Vertical tabs stacked smoothly from top to bottom
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

			gui->set_cursor_pos(SCALE(115, 75));

			float anim_dt = ImClamp(ImGui::GetIO().DeltaTime, 0.001f, 0.05f);
			var->c_selection.selection_alpha = ImClamp(var->c_selection.selection_alpha + (10.f * anim_dt * (var->c_selection.selection == var->c_selection.selection_active ? 1.f : -1.f)), 0.f, 1.f);
			if (var->c_selection.selection_alpha <= 0.05f) var->c_selection.selection_active = var->c_selection.selection;

			gui->push_style_var(ImGuiStyleVar_Alpha, var->c_selection.selection_alpha * style->Alpha);

			gui->begin_content("content", ImVec2(size.x - SCALE(130), size.y - SCALE(90)), { 15, 15 }, { 15, 15 });
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
						gui->begin_child("esp_settings");
						{
							widget->checkbox_with_key("Enable ESP", &var->c_esp.esp, &var->c_esp.esp_key, &var->c_esp.esp_holding, &var->c_esp.esp_value, &var->c_esp.esp_show_binds);

							widget->separator();

							widget->checkbox("Auto Refresh", &var->c_esp.auto_refresh);

							widget->separator();

							const float child_width = GetContentRegionAvail().x;
							widget->button("Refresh ESP", { child_width, SCALE(32) });
							if (ImGui::IsItemClicked())
							{
								FWork::Data::Refresh();
								notify->add_notify("ESP cache refreshed successfully!", 3, static_cast<notify_position>(var->c_notify.notify_position));
							}

							widget->separator();

							widget->dropdown("Box Type", &var->c_esp.box_selection, var->c_esp.box_list, var->c_esp.box_list.size());

							widget->separator();

							widget->checkbox_with_color("In-Game Radar", &var->c_esp.ingame_radar, var->c_esp.inagame_color, true);
						}
						gui->end_child();

						gui->begin_child("esp_ranges");
						{
							widget->slider_int("Max Distance", &var->c_esp.max_distance, 10, 500, 1, "%d m");

							widget->separator();

							widget->checkbox_with_color("Distance Tag", &var->c_esp.distance, var->c_esp.distance_color, true);

							widget->separator();

							widget->checkbox_with_color("Head Circle", &var->c_esp.headdot, var->c_esp.headdot_color, true);
						}
						gui->end_child();
					}
					gui->end_group();

					gui->sameline();

					gui->begin_group();
					{
						gui->begin_child("skeleton_elements");
						{
							widget->checkbox("Skeleton Bones", &var->c_skeleton.skeleton);

							widget->separator();

							widget->dropdown("Snaplines", &var->c_skeleton.snaplines_selection, var->c_skeleton.snaplines_list, var->c_skeleton.snaplines_list.size());

							widget->separator();

							widget->dropdown("Health Bar", &var->c_esp.healthbar_selection, var->c_esp.healthbar_list, var->c_esp.healthbar_list.size());

							widget->separator();

							widget->checkbox_with_color("Weapon Tag", &var->c_skeleton.weapon, var->c_skeleton.weapon_color, true);

							widget->separator();

							widget->checkbox_with_color("Nickname", &var->c_skeleton.nickname, var->c_skeleton.nickname_color, true);
						}
						gui->end_child();

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

						gui->begin_child("lib_management");
						{
							widget->checkbox("Connect Lib", &var->c_settings.connect_lib);

							widget->separator();

							const float width = GetContentRegionAvail().x;
							bool is_conn = FWork::Data::IsConnected();
							widget->button(is_conn ? "Lib: Connected" : "Connect Lib Now", { width, SCALE(35) });
							if (ImGui::IsItemClicked())
							{
								bool ok = FWork::Data::ConnectEngine();
								if (ok)
								{
									notify->add_notify("Memory Engine connected successfully!", 4, static_cast<notify_position>(var->c_notify.notify_position));
								}
								else
								{
									notify->add_notify("Failed to connect Lib! Start the game/VM first.", 4, static_cast<notify_position>(var->c_notify.notify_position));
								}
							}
						}
						gui->end_child();
					}
					gui->end_group();

					gui->sameline();

					gui->begin_group();
					{
						gui->begin_child("panel_management");
						{
							// 1. Hide panel toggle + keybind (default INSERT)
							widget->checkbox_with_key("Hide panel", &var->c_panel.enable_hide_key, &var->c_panel.hide_key, &var->c_panel.hide_holding, &var->c_panel.hide_value, &var->c_panel.hide_show_binds);

							widget->separator();

							// 2. Exit panel toggle + keybind (default END)
							widget->checkbox_with_key("Exit panel", &var->c_panel.enable_exit_key, &var->c_panel.exit_key, &var->c_panel.exit_holding, &var->c_panel.exit_value, &var->c_panel.exit_show_binds);

							widget->separator();

							const float width = GetContentRegionAvail().x;

							// Quick action buttons to Hide or Exit right now
							widget->button("Hide UI Now", { (width - style->ItemSpacing.x) / 2, SCALE(35) });
							if (ImGui::IsItemClicked())
							{
								var->c_panel.request_hide = true;
							}

							gui->sameline();

							widget->button("Exit Panel Now", { (width - style->ItemSpacing.x) / 2, SCALE(35) });
							if (ImGui::IsItemClicked())
							{
								var->c_panel.request_exit = true;
							}

							widget->separator();

							// Theme quick actions
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