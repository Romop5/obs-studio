/*****************************************************************************
Copyright (C) 2019 by Roman Dobias <rom.dobias@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/
#include <obs-module.h>
#include <graphics/matrix4.h>
#include <graphics/quat.h>


#define SETTING_XSTRENGTH                  "Xstrength"
#define SETTING_YSTRENGTH                  "Ystrength"

#define TEXT_XSTRENGTH                     obs_module_text("X Strength")
#define TEXT_YSTRENGTH                     obs_module_text("Y Strength")

struct mirror_filter_data {
	obs_source_t                   *context;

	gs_effect_t                    *effect;

	gs_eparam_t                    *x_strength_param;
	gs_eparam_t                    *y_strength_param;

    float                           x_strength;
    float                           y_strength;
};

/*
 * As the functions' namesake, this provides the internal name of your Filter,
 * which is then translated/referenced in the "data/locale" files.
 */
static const char *mirror_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Mirroring");
}

/*
 * This function is called (see bottom of this file for more details)
 * whenever the OBS filter interface changes. So when the user is messing
 * with a slider this function is called to update the internal settings
 * in OBS, and hence the settings being passed to the CPU/GPU.
 */
static void mirror_filter_update(void *data, obs_data_t *settings)
{
	struct mirror_filter_data *filter = data;

	/* Build our Gamma numbers. */
	
	filter->x_strength = obs_data_get_double(settings, SETTING_XSTRENGTH);
	filter->y_strength = obs_data_get_double(settings, SETTING_YSTRENGTH);
}

/*
 * Since this is C we have to be careful when destroying/removing items from
 * OBS. Jim has added several useful functions to help keep memory leaks to
 * a minimum, and handle the destruction and construction of these filters.
 */
static void mirror_filter_destroy(void *data)
{
	struct mirror_filter_data *filter = data;

	if (filter->effect) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect);
		obs_leave_graphics();
	}

	bfree(data);
}

/*
 * When you apply a filter OBS creates it, and adds it to the source. OBS also
 * starts rendering it immediately. This function doesn't just 'create' the
 * filter, it also calls the render function (farther below) that contains the
 * actual rendering code.
 */
static void *mirror_filter_create(obs_data_t *settings,
	obs_source_t *context)
{
	/*
	* Because of limitations of pre-c99 compilers, you can't create an
	* array that doesn't have a known size at compile time. The below
	* function calculates the size needed and allocates memory to
	* handle the source.
	*/
	struct mirror_filter_data *filter =
		bzalloc(sizeof(struct mirror_filter_data));

	/*
	 * By default the effect file is stored in the ./data directory that
	 * your filter resides in.
	 */
	char *effect_path = obs_module_file("mirror_filter.effect");

	filter->context = context;

	/* Here we enter the GPU drawing/shader portion of our code. */
	obs_enter_graphics();

	/* Load the shader on the GPU. */
	filter->effect = gs_effect_create_from_file(effect_path, NULL);

	/* If the filter is active pass the parameters to the filter. */
	if (filter->effect) {
		filter->x_strength_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_XSTRENGTH);
		filter->y_strength_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_YSTRENGTH);
	}

	obs_leave_graphics();

	bfree(effect_path);

	/*
	 * If the filter has been removed/deactivated, destroy the filter
	 * and exit out so we don't crash OBS by telling it to update
	 * values that don't exist anymore.
	 */
	if (!filter->effect) {
		mirror_filter_destroy(filter);
		return NULL;
	}

	/*
	 * It's important to call the update function here. If we don't
	 * we could end up with the user controlled sliders and values
	 * updating, but the visuals not updating to match.
	 */
	mirror_filter_update(filter, settings);
	return filter;
}

/* This is where the actual rendering of the filter takes place. */
static void mirror_filter_render(void *data, gs_effect_t *effect)
{
	struct mirror_filter_data *filter = data;

	if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
			OBS_ALLOW_DIRECT_RENDERING))
		return;

	/* Now pass the interface variables to the .effect file. */
	gs_effect_set_float(filter->x_strength_param, filter->x_strength);
	gs_effect_set_float(filter->y_strength_param, filter->y_strength);

	obs_source_process_filter_end(filter->context, filter->effect, 0, 0);

	UNUSED_PARAMETER(effect);
}

/*
 * This function sets the interface. the types (add_*_Slider), the type of
 * data collected (int), the internal name, user-facing name, minimum,
 * maximum and step values. While a custom interface can be built, for a
 * simple filter like this it's better to use the supplied functions.
 */
static obs_properties_t *mirror_filter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_float_slider(props, SETTING_XSTRENGTH,
			TEXT_XSTRENGTH, 1.0, 10.0, 0.01);
	obs_properties_add_float_slider(props, SETTING_YSTRENGTH,
			TEXT_YSTRENGTH, 1.0, 10.0, 0.01);

	UNUSED_PARAMETER(data);
	return props;
}

/*
 * As the functions' namesake, this provides the default settings for any
 * options you wish to provide a default for. Try to select defaults that
 * make sense to the end user, or that don't effect the data.
 * *NOTE* this function is completely optional, as is providing a default
 * for any particular setting.
 */
static void mirror_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, SETTING_XSTRENGTH, 1.0);
	obs_data_set_default_double(settings, SETTING_YSTRENGTH, 1.0);
}

/*
 * So how does OBS keep track of all these plug-ins/filters? How does OBS know
 * which function to call when it needs to update a setting? Or a source? Or
 * what type of source this is?
 *
 * OBS does it through the obs_source_info_struct. Notice how variables are
 * assigned the name of a function? Notice how the function name has the
 * variable name in it? While not mandatory, it helps a ton for you (and those
 * reading your code) to follow this convention.
 */
struct obs_source_info mirror_filter = {
	.id = "mirror_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = mirror_filter_name,
	.create = mirror_filter_create,
	.destroy = mirror_filter_destroy,
	.video_render = mirror_filter_render,
	.update = mirror_filter_update,
	.get_properties = mirror_filter_properties,
	.get_defaults = mirror_filter_defaults
};
