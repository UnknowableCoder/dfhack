#include <vector>
#include <charconv>
#include <algorithm>

#include "PluginManager.h"

#include "modules/Gui.h"

#include "df/gamest.h"
#include "df/viewscreen_new_regionst.h"
#include "df/world.h"
#include "df/world_generatorst.h"

DFHACK_PLUGIN("worldgen-tools");
DFHACK_PLUGIN_IS_ENABLED(wgtools_enabled);

REQUIRE_GLOBAL(world);
REQUIRE_GLOBAL(cur_year);

static DFHack::command_result wgtools_start(DFHack::color_ostream& out)
{
    wgtools_enabled = true;
    return DFHack::CR_OK;
}

static DFHack::command_result wgtools_stop(DFHack::color_ostream& out)
{
    wgtools_enabled = false;
    return DFHack::CR_OK;
}

static constexpr int32_t stagecount_begin = 31;
static constexpr int32_t stagecount_end   = 32;

static DFHack::command_result wgtools_immediate_set_pause(DFHack::color_ostream& out, bool pause, df::viewscreen_new_regionst* scr = nullptr)
{
    if (!scr)
    {
        scr = DFHack::Gui::getViewscreenByType<df::viewscreen_new_regionst>(-1);
    }
    if (!scr || scr->stage_count < stagecount_begin || scr->stage_count >= stagecount_end)
    {
        return DFHack::CR_OK;
    }

    scr->abort_world_gen_dialogue = static_cast<int8_t>(pause);

    return DFHack::CR_OK;
}

static uint32_t states_to_pause;
static std::vector<uint32_t> years_to_pause;
static bool pause_every_year;

static DFHack::command_result wgtools_handle_pause_config(DFHack::color_ostream& out, std::vector <std::string>& commands, uint32_t command_start)
{
	auto get_state = [](const std::string& str)
		{
#define STATEGETTER(X) else if (str == #X) return static_cast<uint32_t>(df::world_generatorst::T_state::X)
			if (str == "None" || str == "Initializing")
			{
				return static_cast<uint32_t>(0);
			}
			STATEGETTER(PreparingElevation);
			STATEGETTER(SettingTemperature);
            STATEGETTER(RunningRivers);
			STATEGETTER(FormingLakesAndMinerals);
			STATEGETTER(GrowingVegetation);
			STATEGETTER(VerifyingTerrain);
			STATEGETTER(ImportingWildlife);
			STATEGETTER(RecountingLegends);
			STATEGETTER(Finalizing);
            else if (str == "PlacingCaves")
            {
                return static_cast<uint32_t>(df::world_generatorst::T_state::Finalizing) + 1;
            }
            else if (str == "PlacingGoodEvil")
            {
                return static_cast<uint32_t>(df::world_generatorst::T_state::Finalizing) + 2;
            }
            else if (str == "MakingMegabeasts" || str == "PlacingMegabeasts")
            {
                return static_cast<uint32_t>(df::world_generatorst::T_state::Finalizing) + 3;
            }
            else if (str == "MakingOtherBeasts" || str == "PlacingOtherBeasts")
            {
                return static_cast<uint32_t>(df::world_generatorst::T_state::Finalizing) + 4;
            }
            else if (str == "MakingCavePops" || str == "PlacingCavePops")
            {
                return static_cast<uint32_t>(df::world_generatorst::T_state::Finalizing) + 5;
            }
            else if (str == "MakingCaveCivs" || str == "PlacingCaveCivs")
            {
                return static_cast<uint32_t>(df::world_generatorst::T_state::Finalizing) + 6;
            }
            else if (str == "MakingCivs" || str == "PlacingCivs")
            {
                return static_cast<uint32_t>(df::world_generatorst::T_state::Finalizing) + 7;
            }
            else
            {
                return static_cast<uint32_t>(0);
            }
#undef STATEGETTER
		};


    for (unsigned i = command_start; i < commands.size(); ++i)
    {
        const auto& this_command = commands[i];

        if (this_command == "clear" || this_command == "none")
        {
            states_to_pause = 0;
            years_to_pause.clear();
            pause_every_year = false;
            continue;
        }
        else if (this_command == "allstages")
        {
            states_to_pause = 0xFFFFFFFFU;
            continue;
        }
        else if (this_command == "yearly")
        {
            pause_every_year = true;
            years_to_pause.clear();
            continue;
        }
        else if (this_command == "all")
        {
            states_to_pause = 0xFFFFFFFFU;
            pause_every_year = true;
            years_to_pause.clear();
            continue;
        }

        const uint32_t state = get_state(this_command);
        if (state > 0)
        {
            states_to_pause |= 1U << (state - 1);
            continue;
        }

        if (pause_every_year)
        {
            continue;
        }

        uint32_t year = 0xFFFFFFFFU;


        std::from_chars(this_command.c_str(), this_command.c_str() + this_command.size(), year);
        //Possibly other ways of doing str -> int conversion, not sure about the throwing std::stoul...

        if (year < 0xFFFFFFFFU)
        {
            auto it = std::lower_bound(years_to_pause.begin(), years_to_pause.end(), year);

            if (it == years_to_pause.end() || *it != year)
            {
                years_to_pause.insert(it, year);
            }

        }
    }

    constexpr unsigned state_conv_numchars = sizeof(states_to_pause) * CHAR_BIT;

    char state_conv[state_conv_numchars + 1] = {};

    for (uint32_t i = 0; i < state_conv_numchars; ++i)
    {
        state_conv[i] = '0' + ((states_to_pause >> i) & 1);
    }


    state_conv[state_conv_numchars] = '\0';

    out.print("Current autopause status: %s |", state_conv);

    if (pause_every_year)
    {
        out.print(" every year");
    }
    else
    {
        out.print(" %u | ", years_to_pause.size());
        for (const auto& year : years_to_pause)
        {
            out.print(" %u", year);
        }
    }
    out.print("\n");

    return DFHack::CR_OK;
}

static bool reject_this_worldgen;

static DFHack::command_result wgtools_reject(DFHack::color_ostream& out, df::viewscreen_new_regionst* scr = nullptr)
{
    if (!scr)
    {
        scr = DFHack::Gui::getViewscreenByType<df::viewscreen_new_regionst>(-1);
    }
    if (!scr || scr->stage_count < stagecount_begin || scr->stage_count >= stagecount_end || !df::global::world)
    {
        out.printerr("Can only reject during worldgen!\n");
        return DFHack::CR_WRONG_USAGE;
    }

    df::world_generatorst& wg = df::global::world->worldgen_status;

    if (wg.state <= df::world_generatorst::T_state::RecountingLegends || !wg.finished_prehistory)
    {
        reject_this_worldgen = true;
        return wgtools_immediate_set_pause(out, false, scr);
    }
    else
    {
        out.printerr("Can only reject before finishing civilization placement!\n");
        return DFHack::CR_WRONG_USAGE;
    }
}

DFhackCExport DFHack::command_result plugin_onupdate(DFHack::color_ostream& out)
{
    if (!wgtools_enabled)
    {
        return DFHack::CR_OK;
    }

    if (!df::global::world)
    {
        return DFHack::CR_OK;
    }

    auto scr = DFHack::Gui::getViewscreenByType<df::viewscreen_new_regionst>(-1);

    if (!scr || scr->abort_world_gen_dialogue)
    {
        return DFHack::CR_OK;
    }

    static uint32_t year_to_check = 0;
    static uint32_t previous_state = 0;
    static uint32_t prev_extra_stage = 0;

    if (scr->stage_count < stagecount_begin)
    {
        year_to_check = 0;
        previous_state = 0;
        prev_extra_stage = 0;
        reject_this_worldgen = false;
        return DFHack::CR_OK;
    }

    if (scr->stage_count >= stagecount_end)
    {
        return DFHack::CR_OK;
    }

    df::world_generatorst& wg = df::global::world->worldgen_status;

    const uint32_t this_state = static_cast<int32_t>(wg.state);

    if (this_state == 0 || previous_state > this_state)
    {
        previous_state = 0;
        prev_extra_stage = 0;
        reject_this_worldgen = false;
    }
    if (this_state <= static_cast<uint32_t>(df::world_generatorst::T_state::RecountingLegends))
    {
        year_to_check = 0;
        prev_extra_stage = 0;
    }

    if (this_state > 0 && this_state != previous_state)
    {
        const uint32_t state_mask = (1u << (this_state + 1)) - (1u << (previous_state + 1));
        previous_state = this_state;
        if (states_to_pause & state_mask)
        {
            return wgtools_immediate_set_pause(out, true, scr);
        }
    }

    if (this_state > static_cast<uint32_t>(df::world_generatorst::T_state::RecountingLegends))
    {
        if (!wg.finished_prehistory)
        {
            const uint32_t current_extra_stage = static_cast<uint32_t>(wg.prehistory_initialized) +
                                                 static_cast<uint32_t>(wg.placed_caves          ) +
                                                 static_cast<uint32_t>(wg.placed_good_evil      ) +
                                                 static_cast<uint32_t>(wg.placed_megabeasts     ) +
                                                 static_cast<uint32_t>(wg.placed_other_beasts   ) +
                                                 static_cast<uint32_t>(wg.made_cave_pops        ) +
                                                 static_cast<uint32_t>(wg.made_cave_civs        ) +
                                                 static_cast<uint32_t>(wg.finished_prehistory   );
            
            if (reject_this_worldgen)
            {
                wg.civs_left_to_place = 100000;
                if (wg.made_cave_civs && wg.civ_count > 0)
                {
                    reject_this_worldgen = false;
                }
            }

            if (current_extra_stage > prev_extra_stage)
            {
                const uint32_t state_mask = ((1u << current_extra_stage) - (1u << prev_extra_stage)) << static_cast<uint32_t>(df::world_generatorst::T_state::Finalizing);
                prev_extra_stage = current_extra_stage;

                if (states_to_pause & state_mask)
                {
                    return wgtools_immediate_set_pause(out, true, scr);
                }
            }
            else
            {
                return DFHack::CR_OK;
            }
        }
        if (!df::global::cur_year)
        {
            return DFHack::CR_OK;
        }
        if (pause_every_year)
        {
            const uint32_t this_year = static_cast<uint32_t>(*df::global::cur_year);

            if (this_year != year_to_check)
            {
                year_to_check = this_year;
                return wgtools_immediate_set_pause(out, true, scr);
            }
        }
        else if (year_to_check < years_to_pause.size())
        {
            const uint32_t this_year = static_cast<uint32_t>(*df::global::cur_year);

            if (this_year >= years_to_pause[year_to_check])
            {
                while (year_to_check < years_to_pause.size() && years_to_pause[year_to_check] <= this_year)
                {
                    ++year_to_check;
                }
                return wgtools_immediate_set_pause(out, true, scr);
            }
        }
    }

    return DFHack::CR_OK;
}

static DFHack::command_result wgtools_statecheck(DFHack::color_ostream& out, df::viewscreen_new_regionst* scr = nullptr)
{
    if (!scr)
    {
        scr = DFHack::Gui::getViewscreenByType<df::viewscreen_new_regionst>(-1);
    }
    if (!scr || scr->stage_count < stagecount_begin || scr->stage_count >= stagecount_end)
    {
        return DFHack::CR_OK;
    }

    df::world_generatorst& wg = df::global::world->worldgen_status;

    out.print("%d %d %d %d %d", (int)wg.state, wg.num_rejects, (int)wg.rejection_reason, wg.new_dimx, wg.new_dimy);
    for (int i = 0; i < 53; ++i)
    {
        if (i % 5 == 0)
        {
            out.print("\n");
        }
        out.print("(%d %d) ", wg.skip_reject[i], wg.reject_type[i]);
    }
    out.print("\n%d %d %d", wg.lake_x, wg.geoindex, wg.max_geo_index);
    for (int i = 0; i < 100; ++i)
    {
        if (i % 5 == 0)
        {
            out.print("\n");
        }
        out.print("%p ", wg.geo_layers[i]);
    }
    for (int i = 0; i < 100; ++i)
    {
        if (i % 5 == 0)
        {
            out.print("\n");
        }
        out.print("(%d %d) ", (int)wg.placement_freq[i], (int)wg.placement_parent[i]);
    }
    out.print("\n %d %d %d %d %d %d\n", wg.have_logged_parameters, wg.finalized_civ_mats, wg.finalized_art, wg.finalized_uniforms, wg.finalized_sites, wg.rampage_num);

    out.print("%u %u %u %u\n", (unsigned)wg.entities.size(), (unsigned)wg.sites.size(), (unsigned)wg.riverstart.x.size(), (unsigned)wg.riverstart.y.size());

    out.print("%d %d %d\n", wg.rivers_total, wg.rivers_cur, (int)wg.last_used_valid);

    out.print("%s\n%s\n%s\n%s\n%s\n", wg.last_param_set.c_str(), wg.last_seed.c_str(), wg.last_history_seed.c_str(), wg.last_name_seed.c_str(), wg.last_creature_seed.c_str());

    out.print("%08d ", wg.finished_prehistory * 1 + wg.made_cave_civs * 10 + wg.made_cave_pops * 100 + wg.placed_other_beasts * 1000 + wg.placed_megabeasts * 10000 + wg.placed_good_evil * 100000 + wg.placed_caves * 1000000 + wg.prehistory_initialized * 10000000);

    out.print("%u %u %u\n", (unsigned)wg.mythical_site.size(), (unsigned)wg.caves.size(), (unsigned)wg.orig_cave.size());

    out.print("%d %04d %u %u %d %d", wg.current_bandit_num, wg.just_continued * 1 + wg.skip_controllable * 10 + wg.placed * 100 + wg.have_placed_controllable * 1000, (unsigned)wg.entity_raws.size(), (unsigned)wg.entity_race.size(), wg.civ_count, wg.civs_left_to_place);

    for (int i = 0; i < 10; ++i)
    {
        if (i % 2 == 0)
        {
            out.print("\n");
        }
        out.print("(%u %u %u) ", (unsigned)wg.good_regions[i].size(), (unsigned)wg.normal_regions[i].size(), (unsigned)wg.evil_regions[i].size());
    }

    out.print("\n%u %u %u %u %u %u\n",
        (unsigned)wg.cave_choice_x.size(), (unsigned)wg.cave_choice_y.size(),
        (unsigned)wg.final_cave_choice_x.size(), (unsigned)wg.final_cave_choice_y.size(),
        (unsigned)wg.o_final_cave_choice_x.size(), (unsigned)wg.o_final_cave_choice_y.size());

    out.print("%d %d %d\n", wg.mountain_cave_left, wg.non_mountain_cave_left, wg.mythical_site_left);

    out.print("%u %u %u %d\n", (unsigned)wg.candidate_demon_c.size(), (unsigned)wg.candidate_demon_cc.size(), (unsigned)wg.libraries.size(), wg.book_count);

    out.print("%u %u %u %u\n", (unsigned)wg.temples.size(), (unsigned)wg.holy_relics.size(), (unsigned)wg.disaster_site.size(), (unsigned)wg.building_usage_move_check_hfid.size());

    out.print("%d %u %u %u %u\n", wg.prepare_civs_step, (unsigned)wg.move_civ.size(), (unsigned)wg.move_civ_ll.size(), (unsigned)wg.move_site.size(), (unsigned)wg.move_subsite.size());

    out.print("%u %u %u %u\n", (unsigned)wg.move_religion.size(), (unsigned)wg.move_wgwg.size(), (unsigned)wg.move_beast.size(), (unsigned)wg.move_civ_actor.size(), (unsigned)wg.move_plotter_actor.size());

    out.print("%d %d %d %u %u %d %d\n", wg.predator_num, wg.lph_num, wg.wk, (unsigned)wg.text_box.word.size(), (unsigned)wg.text_box.link.size(), wg.text_box.current_width, wg.text_box.max_y);

    out.print("%u %d %u\n", wg.last_chronicle_add_time, wg.last_event_id_added, (unsigned)wg.mythical_sphere.size());

    return DFHack::CR_OK;
}

DFhackCExport DFHack::command_result wgtools_general_command(DFHack::color_ostream& out, std::vector <std::string>& commands)
{
    if (commands.size() < 1)
    {
        return DFHack::CR_OK;
    }

    if (commands[0] == "start" || commands[0] == "enable")
    {
        return wgtools_start(out);
    }
    else if (commands[0] == "stop" || commands[0] == "disable")
    {
        return wgtools_stop(out);
    }
    if (commands[0] == "autopause")
    {
        return wgtools_handle_pause_config(out, commands, 1);
    }
    else if (commands[0] == "reject")
    {
        return wgtools_reject(out);
    }
    else if (commands[0] == "pause")
    {
        return wgtools_immediate_set_pause(out, true);
    }
    else if (commands[0] == "unpause")
    {
        return wgtools_immediate_set_pause(out, false);
    }
    else if (commands[0] == "statecheck")
    {
        return wgtools_statecheck(out);
    }

    return DFHack::CR_OK;
}

DFhackCExport DFHack::command_result wgtools_autopause_comand(DFHack::color_ostream& out, std::vector <std::string>& commands)
{
    return wgtools_handle_pause_config(out, commands, 0);
}

DFhackCExport DFHack::command_result wgtools_reject_command(DFHack::color_ostream& out, std::vector <std::string>& commands)
{
    if (commands.size() > 0)
    {
        return DFHack::CR_WRONG_USAGE;
    }
    return wgtools_reject(out);
}

DFhackCExport DFHack::command_result wgtools_pause_command(DFHack::color_ostream& out, std::vector <std::string>& commands)
{
    if (commands.size() > 1)
    {
        return DFHack::CR_WRONG_USAGE;
    }
    if (commands.size() == 0 || commands[0] == "true" || commands[0] == "1")
    {
        return wgtools_immediate_set_pause(out, true);
    }
    else if (commands.size() > 0 && (commands[0] == "false" || commands[0] == "0"))
    {
        return wgtools_immediate_set_pause(out, false);
    }
    return DFHack::CR_WRONG_USAGE;
}

DFhackCExport DFHack::command_result plugin_init(DFHack::color_ostream& out, std::vector <DFHack::PluginCommand>& commands)
{
    commands.push_back(DFHack::PluginCommand("worldgen-tools", "Useful tools for controlling the process of world generation.", wgtools_general_command));

    commands.push_back(DFHack::PluginCommand("worldgen-autopause", "Configure automatic pausing of world generation.", wgtools_autopause_comand));

    commands.push_back(DFHack::PluginCommand("worldgen-reject", "Reject the current world.", wgtools_reject_command));

    commands.push_back(DFHack::PluginCommand("worldgen-pause", "Pause current world generation.", wgtools_pause_command));

    states_to_pause = 0;
    pause_every_year = false;
    reject_this_worldgen = false;

    wgtools_enabled = true;

    return DFHack::CR_OK;
}